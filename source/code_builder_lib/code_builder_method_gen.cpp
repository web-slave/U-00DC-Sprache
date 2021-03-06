#include "push_disable_llvm_warnings.hpp"
#include <llvm/IR/Constant.h>
#include <llvm/IR/LLVMContext.h>
#include "pop_llvm_warnings.hpp"

#include "../lex_synt_lib/assert.hpp"
#include "../lex_synt_lib/keywords.hpp"
#include "error_reporting.hpp"
#include "mangling.hpp"

#include "code_builder.hpp"

namespace U
{

namespace CodeBuilderPrivate
{

void CodeBuilder::TryGenerateDefaultConstructor( Class& the_class, const Type& class_type )
{
	// Search for explicit default constructor.
	FunctionVariable* prev_constructor_variable= nullptr;
	if( Value* const constructors_value=
		the_class.members.GetThisScopeValue( Keyword( Keywords::constructor_ ) ) )
	{
		OverloadedFunctionsSet* const constructors= constructors_value->GetFunctionsSet();
		U_ASSERT( constructors != nullptr );
		for( FunctionVariable& constructor : constructors->functions )
		{
			if( IsDefaultConstructor( *constructor.type.GetFunctionType(), class_type ) )
			{
				if( constructor.is_generated )
				{
					U_ASSERT(!constructor.have_body);
					prev_constructor_variable= &constructor;
				}
				else
				{
					if( !constructor.is_deleted )
						the_class.is_default_constructible= true;
					return;
				}
			}
		};
	}

	// Generating of default constructor disabled, if class have other explicit constructors, except copy constructors.
	if( the_class.have_explicit_noncopy_constructors && prev_constructor_variable == nullptr )
		return;

	// Generate default constructor, if all fields is default constructible.
	bool all_fields_is_default_constructible= true;

	the_class.members.ForEachValueInThisScope(
		[&]( const Value& value )
		{
			const ClassField* const field= value.GetClassField();
			if( field == nullptr )
				return;
			if( field->class_.lock()->class_ != &the_class )
				return; // Skip fields of parent classes.

			if( field->syntax_element->initializer == nullptr &&
				( field->is_reference || !field->type.IsDefaultConstructible() ) )
				all_fields_is_default_constructible= false;
		} );

	if( the_class.base_class != nullptr && !the_class.base_class->class_->is_default_constructible )
		all_fields_is_default_constructible= false;

	if( !all_fields_is_default_constructible )
	{
		if( prev_constructor_variable != nullptr )
			REPORT_ERROR( MethodBodyGenerationFailed, the_class.members.GetErrors(), prev_constructor_variable->prototype_file_pos );
		return;
	}

	// Generate function

	Function constructor_type;
	constructor_type.return_type= void_type_for_ret_;
	constructor_type.args.emplace_back();
	constructor_type.args.back().type= class_type;
	constructor_type.args.back().is_mutable= true;
	constructor_type.args.back().is_reference= true;

	ArgsVector<llvm::Type*> args_llvm_types;
	args_llvm_types.push_back( class_type.GetLLVMType()->getPointerTo() );

	constructor_type.llvm_function_type=
		llvm::FunctionType::get(
			fundamental_llvm_types_.void_for_ret,
			llvm::ArrayRef<llvm::Type*>( args_llvm_types.data(), args_llvm_types.size() ),
			false );

	llvm::Function* const llvm_constructor_function=
		llvm::Function::Create(
			constructor_type.llvm_function_type,
			llvm::Function::LinkageTypes::ExternalLinkage,
			MangleFunction( the_class.members, Keyword( Keywords::constructor_ ), constructor_type ),
			module_.get() );

	SetupGeneratedFunctionLinkageAttributes( *llvm_constructor_function );
	llvm_constructor_function->addAttribute( 1u, llvm::Attribute::NonNull ); // this is nonnull

	FunctionContext function_context(
		constructor_type.return_type,
		constructor_type.return_value_is_mutable,
		constructor_type.return_value_is_reference,
		llvm_context_,
		llvm_constructor_function );
	StackVariablesStorage function_variables_storage( function_context );
	llvm::Value* const this_llvm_value= llvm_constructor_function->args().begin();
	this_llvm_value->setName( Keyword( Keywords::this_ ) );

	if( the_class.base_class != nullptr )
	{
		Variable base_variable;
		base_variable.type= the_class.base_class;
		base_variable.value_type= ValueType::Reference;

		base_variable.llvm_value=
			function_context.llvm_ir_builder.CreateGEP(
				this_llvm_value,
				{ GetZeroGEPIndex(), GetFieldGEPIndex( 0u /*base class is allways first field */ ) } );

		ApplyEmptyInitializer( Keyword( Keywords::base_ ), FilePos()/*TODO*/, base_variable, the_class.members, function_context );
	}

	for( const std::string& field_name : the_class.fields_order )
	{
		if( field_name.empty() )
			continue;

		const ClassField& field= *the_class.members.GetThisScopeValue( field_name )->GetClassField();

		if( field.is_reference )
		{
			U_ASSERT( field.syntax_element->initializer != nullptr ); // Can initialize reference field only with class field initializer.
			Variable variable;
			variable.type= class_type;
			variable.value_type= ValueType::Reference;
			variable.llvm_value= this_llvm_value;
			InitializeReferenceClassFieldWithInClassIninitalizer( variable, field, function_context );
		}
		else
		{
			Variable field_variable;
			field_variable.type= field.type;
			field_variable.value_type= ValueType::Reference;

			field_variable.llvm_value=
				function_context.llvm_ir_builder.CreateGEP( this_llvm_value, { GetZeroGEPIndex(), GetFieldGEPIndex( field.index ) } );

			if( field.syntax_element->initializer != nullptr )
				InitializeClassFieldWithInClassIninitalizer( field_variable, field, function_context );
			else
				ApplyEmptyInitializer( field_name, FilePos()/*TODO*/, field_variable, the_class.members, function_context );
		}
	}

	SetupVirtualTablePointers( this_llvm_value, the_class, function_context );

	function_context.llvm_ir_builder.CreateRetVoid();
	function_context.alloca_ir_builder.CreateBr( function_context.function_basic_block );

	// Add generated constructor
	FunctionVariable constructor_variable;
	constructor_variable.type= std::move( constructor_type );
	constructor_variable.have_body= true;
	constructor_variable.is_this_call= true;
	constructor_variable.is_generated= true;
	constructor_variable.is_constructor= true;
	constructor_variable.constexpr_kind= the_class.can_be_constexpr ? FunctionVariable::ConstexprKind::ConstexprComplete : FunctionVariable::ConstexprKind::NonConstexpr;
	constructor_variable.llvm_function= llvm_constructor_function;

	if( prev_constructor_variable != nullptr )
		*prev_constructor_variable= std::move(constructor_variable);
	else
	{
		if( Value* const constructors_value=
			the_class.members.GetThisScopeValue( Keyword( Keywords::constructor_ ) ) )
		{
			OverloadedFunctionsSet* const constructors= constructors_value->GetFunctionsSet();
			U_ASSERT( constructors != nullptr );
			constructors->functions.push_back( std::move( constructor_variable ) );
		}
		else
		{
			OverloadedFunctionsSet constructors;
			constructors.functions.push_back( std::move( constructor_variable ) );
			the_class.members.AddName( Keyword( Keywords::constructor_ ), std::move( constructors ) );
		}
	}

	// After default constructor generation, class is default-constructible.
	the_class.is_default_constructible= true;
}

void CodeBuilder::TryGenerateCopyConstructor( Class& the_class, const Type& class_type )
{
	// Search for explicit copy constructor.
	FunctionVariable* prev_constructor_variable= nullptr;
	if( Value* const constructors_value=
		the_class.members.GetThisScopeValue( Keyword( Keywords::constructor_ ) ) )
	{
		OverloadedFunctionsSet* const constructors= constructors_value->GetFunctionsSet();
		U_ASSERT( constructors != nullptr );
		for( FunctionVariable& constructor : constructors->functions )
		{
			if( IsCopyConstructor( *constructor.type.GetFunctionType(), class_type ) )
			{
				if( constructor.is_generated )
				{
					U_ASSERT(!constructor.have_body);
					prev_constructor_variable= &constructor;
				}
				else
				{
					if( !constructor.is_deleted )
						the_class.is_copy_constructible= true;
					return;
				}
			}
		}
	}

	if( prev_constructor_variable == nullptr && the_class.kind != Class::Kind::Struct )
		return; // Do not generate copy-constructor for classes. Generate it only if "=default" explicitly specified for this method.

	bool all_fields_is_copy_constructible= true;

	the_class.members.ForEachValueInThisScope(
		[&]( const Value& value )
		{
			const ClassField* const field= value.GetClassField();
			if( field == nullptr )
				return;

			if( !field->is_reference && !field->type.IsCopyConstructible() )
				all_fields_is_copy_constructible= false;
		} );

	if( the_class.base_class != nullptr && !the_class.base_class->class_->is_copy_constructible )
		all_fields_is_copy_constructible= false;

	if( !all_fields_is_copy_constructible )
	{
		if( prev_constructor_variable != nullptr )
			REPORT_ERROR( MethodBodyGenerationFailed, the_class.members.GetErrors(), prev_constructor_variable->prototype_file_pos );
		return;
	}

	// Generate copy-constructor
	Function constructor_type;
	constructor_type.return_type= void_type_for_ret_;
	constructor_type.args.resize(2u);
	constructor_type.args[0].type= class_type;
	constructor_type.args[0].is_mutable= true;
	constructor_type.args[0].is_reference= true;
	constructor_type.args[1].type= class_type;
	constructor_type.args[1].is_mutable= false;
	constructor_type.args[1].is_reference= true;

	// Generate default reference pollution for copying.
	for( size_t i= 0u; i < class_type.ReferencesTagsCount(); ++i )
	{
		Function::ReferencePollution pollution;
		pollution.dst.first= 0u;
		pollution.dst.second= i;
		pollution.src.first= 1u;
		pollution.src.second= i;
		constructor_type.references_pollution.emplace(pollution);
	}

	ArgsVector<llvm::Type*> args_llvm_types;
	args_llvm_types.push_back( class_type.GetLLVMType()->getPointerTo() );
	args_llvm_types.push_back( class_type.GetLLVMType()->getPointerTo() );

	constructor_type.llvm_function_type=
		llvm::FunctionType::get(
			fundamental_llvm_types_.void_for_ret,
			llvm::ArrayRef<llvm::Type*>( args_llvm_types.data(), args_llvm_types.size() ),
			false );

	llvm::Function* const llvm_constructor_function=
		llvm::Function::Create(
			constructor_type.llvm_function_type,
			llvm::Function::LinkageTypes::ExternalLinkage,
			MangleFunction( the_class.members, Keyword( Keywords::constructor_ ), constructor_type ),
			module_.get() );

	SetupGeneratedFunctionLinkageAttributes( *llvm_constructor_function );
	llvm_constructor_function->addAttribute( 1u, llvm::Attribute::NonNull ); // this is nonnull
	llvm_constructor_function->addAttribute( 2u, llvm::Attribute::NonNull ); // and src is nonnull

	FunctionContext function_context(
		constructor_type.return_type,
		constructor_type.return_value_is_mutable,
		constructor_type.return_value_is_reference,
		llvm_context_,
		llvm_constructor_function );

	llvm::Value* const this_llvm_value= &*llvm_constructor_function->args().begin();
	this_llvm_value->setName( Keyword( Keywords::this_ ) );
	llvm::Value* const src_llvm_value= &*std::next(llvm_constructor_function->args().begin());
	src_llvm_value->setName( "src" );

	if( the_class.base_class != nullptr )
	{
		llvm::Value* const index_list[2] { GetZeroGEPIndex(),  GetFieldGEPIndex(  0u /*base class is allways first field */ ) };
		BuildCopyConstructorPart(
			function_context.llvm_ir_builder.CreateGEP( this_llvm_value, index_list ),
			function_context.llvm_ir_builder.CreateGEP( src_llvm_value , index_list ),
			the_class.base_class,
			function_context );
	}

	for( const std::string& field_name : the_class.fields_order )
	{
		if( field_name.empty() )
			continue;

		const ClassField& field= *the_class.members.GetThisScopeValue( field_name )->GetClassField();

		llvm::Value* const index_list[2] { GetZeroGEPIndex(), GetFieldGEPIndex( field.index ) };
		llvm::Value* const src= function_context.llvm_ir_builder.CreateGEP( src_llvm_value , index_list );
		llvm::Value* const dst= function_context.llvm_ir_builder.CreateGEP( this_llvm_value, index_list );

		if( field.is_reference )
		{
			// Create simple load-store for references.
			llvm::Value* const val= function_context.llvm_ir_builder.CreateLoad( src );
			function_context.llvm_ir_builder.CreateStore( val, dst );
		}
		else
		{
			U_ASSERT( field.type.IsCopyConstructible() );
			BuildCopyConstructorPart( dst, src, field.type, function_context );
		}

	}

	SetupVirtualTablePointers( this_llvm_value, the_class, function_context );

	function_context.llvm_ir_builder.CreateRetVoid();
	function_context.alloca_ir_builder.CreateBr( function_context.function_basic_block );

	// Add generated constructor
	FunctionVariable constructor_variable;
	constructor_variable.type= std::move( constructor_type );
	constructor_variable.have_body= true;
	constructor_variable.is_this_call= true;
	constructor_variable.is_generated= true;
	constructor_variable.is_constructor= true;
	constructor_variable.constexpr_kind= the_class.can_be_constexpr ? FunctionVariable::ConstexprKind::ConstexprComplete : FunctionVariable::ConstexprKind::NonConstexpr;
	constructor_variable.llvm_function= llvm_constructor_function;

	if( prev_constructor_variable != nullptr )
		*prev_constructor_variable= std::move(constructor_variable);
	else
	{
		if( Value* const constructors_value=
			the_class.members.GetThisScopeValue( Keyword( Keywords::constructor_ ) ) )
		{
			OverloadedFunctionsSet* const constructors= constructors_value->GetFunctionsSet();
			U_ASSERT( constructors != nullptr );
			constructors->functions.push_back( std::move( constructor_variable ) );
		}
		else
		{
			OverloadedFunctionsSet constructors;
			constructors.functions.push_back( std::move( constructor_variable ) );
			the_class.members.AddName( Keyword( Keywords::constructor_ ), std::move( constructors ) );
		}
	}

	// After default constructor generation, class is copy-constructible.
	the_class.is_copy_constructible= true;
}

FunctionVariable CodeBuilder::GenerateDestructorPrototype( Class& the_class, const Type& class_type )
{
	Function destructor_type;

	destructor_type.return_type= void_type_for_ret_;
	destructor_type.args.resize(1u);
	destructor_type.args[0].type= class_type;
	destructor_type.args[0].is_mutable= true;
	destructor_type.args[0].is_reference= true;

	llvm::Type* const this_llvm_type= class_type.GetLLVMType()->getPointerTo();
	destructor_type.llvm_function_type=
		llvm::FunctionType::get(
			fundamental_llvm_types_.void_for_ret,
			llvm::ArrayRef<llvm::Type*>( &this_llvm_type, 1u ),
			false );

	FunctionVariable destructor_function;
	destructor_function.type= destructor_type;
	destructor_function.type= destructor_type;
	destructor_function.is_generated= true;
	destructor_function.is_this_call= true;
	destructor_function.have_body= false;

	destructor_function.llvm_function=
		llvm::Function::Create(
			destructor_type.llvm_function_type,
			llvm::Function::LinkageTypes::ExternalLinkage,
			MangleFunction( the_class.members, Keyword( Keywords::destructor_ ), destructor_type ),
			module_.get() );

	return destructor_function;
}

void CodeBuilder::GenerateDestructorBody( Class& the_class, const Type& class_type, FunctionVariable& destructor_function )
{
	const Function& destructor_type= *destructor_function .type.GetFunctionType();

	llvm::Value* const this_llvm_value= &*destructor_function .llvm_function->args().begin();
	this_llvm_value->setName( Keyword( Keywords::this_ ) );

	Variable this_;
	this_.type= class_type;
	this_.location= Variable::Location::Pointer;
	this_.value_type= ValueType::Reference;
	this_.llvm_value= this_llvm_value;

	FunctionContext function_context(
		destructor_type.return_type,
		destructor_type.return_value_is_mutable,
		destructor_type.return_value_is_reference,
		llvm_context_,
		destructor_function .llvm_function );
	function_context.this_= &this_;

	CallMembersDestructors( function_context, the_class.members.GetErrors(), the_class.body_file_pos );
	function_context.alloca_ir_builder.CreateBr( function_context.function_basic_block );
	function_context.llvm_ir_builder.CreateRetVoid();

	SetupGeneratedFunctionLinkageAttributes( *destructor_function.llvm_function );

	destructor_function.have_body= true;
}

void CodeBuilder::TryGenerateDestructor( Class& the_class, const Type& class_type )
{
	// Search for explicit destructor.
	if( Value* const destructor_value=
		the_class.members.GetThisScopeValue( Keyword( Keywords::destructor_ ) ) )
	{
		OverloadedFunctionsSet* const destructors= destructor_value->GetFunctionsSet();
		if( destructors->functions.empty() )
			return; // destructors may be invalid in case of error.

		FunctionVariable& destructor_function= destructors->functions.front();
		if( destructor_function.is_generated && !destructor_function.have_body )
			GenerateDestructorBody( the_class, class_type, destructor_function ); // Finish generating pre-generated destructor.

		the_class.have_destructor= true;
		return;
	}

	// SPRACHE_TODO - maybe not generate default destructor for classes, that have no fields with destructors?
	// SPRACHE_TODO - maybe mark generated destructor for this cases as "empty"?

	// Generate destructor.

	FunctionVariable destructor_variable= GenerateDestructorPrototype( the_class, class_type );
	GenerateDestructorBody( the_class, class_type, destructor_variable );

	// TODO - destructor have no overloads. Maybe store it as FunctionVariable, not as FunctionsSet?
	OverloadedFunctionsSet destructors;
	destructors.functions.push_back( std::move( destructor_variable ) );

	the_class.members.AddName(
		Keyword( Keywords::destructor_ ),
		Value( std::move( destructors ) ) );

	// Say "we have destructor".
	the_class.have_destructor= true;
}

void CodeBuilder::TryGenerateCopyAssignmentOperator( Class& the_class, const Type& class_type )
{
	const std::string op_name= OverloadedOperatorToString( OverloadedOperator::Assign );

	// Search for explicit assignment operator.
	FunctionVariable* prev_operator_variable= nullptr;
	if( Value* const assignment_operator_value=
		the_class.members.GetThisScopeValue( op_name ) )
	{
		OverloadedFunctionsSet* const operators= assignment_operator_value->GetFunctionsSet();
		for( FunctionVariable& op : operators->functions )
		{

			// SPRACHE_TODO - support assignment operator with value src argument.
			if( IsCopyAssignmentOperator( *op.type.GetFunctionType(), class_type ) )
			{
				if( op.is_generated )
				{
					U_ASSERT( !op.have_body );
					prev_operator_variable= &op;
				}
				else
				{
					if( !op.is_deleted )
						the_class.is_copy_assignable= true;
					return;
				}
			}
		}
	}

	if( prev_operator_variable == nullptr && the_class.kind != Class::Kind::Struct )
		return; // Do not generate copy-assignement operator for classes. Generate it only if "=default" explicitly specified for this method.

	bool all_fields_is_copy_assignable= true;

	the_class.members.ForEachValueInThisScope(
		[&]( const Value& member )
		{
			const ClassField* const field= member.GetClassField();
			if( field == nullptr )
				return;

			// We can not generate assignment operator for classes with references, for classes with immutable fields, for classes with noncopyable fields.
			if( field->is_reference || !field->type.IsCopyAssignable() || !field->is_mutable )
				all_fields_is_copy_assignable= false;
		} );

	if( the_class.base_class != nullptr && !the_class.base_class->class_->is_copy_assignable )
		all_fields_is_copy_assignable= false;

	if( !all_fields_is_copy_assignable )
	{
		if( prev_operator_variable != nullptr )
			REPORT_ERROR( MethodBodyGenerationFailed, the_class.members.GetErrors(), prev_operator_variable->prototype_file_pos );
		return;
	}

	// Generate assignment operator
	Function op_type;
	op_type.return_type= void_type_for_ret_;
	op_type.args.resize(2u);
	op_type.args[0].type= class_type;
	op_type.args[0].is_mutable= true;
	op_type.args[0].is_reference= true;
	op_type.args[1].type= class_type;
	op_type.args[1].is_mutable= false;
	op_type.args[1].is_reference= true;

	// Generate default reference pollution for copying.
	for( size_t i= 0u; i < class_type.ReferencesTagsCount(); ++i )
	{
		Function::ReferencePollution pollution;
		pollution.dst.first= 0u;
		pollution.dst.second= i;
		pollution.src.first= 1u;
		pollution.src.second= i;
		op_type.references_pollution.emplace(pollution);
	}

	ArgsVector<llvm::Type*> args_llvm_types;
	args_llvm_types.push_back( class_type.GetLLVMType()->getPointerTo() );
	args_llvm_types.push_back( class_type.GetLLVMType()->getPointerTo() );

	op_type.llvm_function_type=
		llvm::FunctionType::get(
			fundamental_llvm_types_.void_for_ret,
			llvm::ArrayRef<llvm::Type*>( args_llvm_types.data(), args_llvm_types.size() ),
			false );

	llvm::Function* const llvm_op_function=
		llvm::Function::Create(
			op_type.llvm_function_type,
			llvm::Function::LinkageTypes::ExternalLinkage,
			MangleFunction( the_class.members, op_name, op_type ),
			module_.get() );

	SetupGeneratedFunctionLinkageAttributes( *llvm_op_function );
	llvm_op_function->addAttribute( 1u, llvm::Attribute::NonNull ); // this is nonnull
	llvm_op_function->addAttribute( 2u, llvm::Attribute::NonNull ); // and src is nonnull

	FunctionContext function_context(
		op_type.return_type,
		op_type.return_value_is_mutable,
		op_type.return_value_is_reference,
		llvm_context_,
		llvm_op_function );

	llvm::Value* const this_llvm_value= &*llvm_op_function->args().begin();
	this_llvm_value->setName( Keyword( Keywords::this_ ) );
	llvm::Value* const src_llvm_value= &*std::next(llvm_op_function->args().begin());
	src_llvm_value->setName( "src" );

	if( the_class.base_class != nullptr )
	{
		llvm::Value* index_list[2];
		index_list[0]= GetZeroGEPIndex();
		index_list[1]= GetFieldGEPIndex(  0u /*base class is allways first field */ );
		BuildCopyAssignmentOperatorPart(
			function_context.llvm_ir_builder.CreateGEP( this_llvm_value, index_list ),
			function_context.llvm_ir_builder.CreateGEP( src_llvm_value , index_list ),
			the_class.base_class,
			function_context );
	}

	for( const std::string& field_name : the_class.fields_order )
	{
		if( field_name.empty() )
			continue;

		const ClassField& field= *the_class.members.GetThisScopeValue( field_name )->GetClassField();
		U_ASSERT( field.type.IsCopyAssignable() );

		llvm::Value* const index_list[2] { GetZeroGEPIndex(), GetFieldGEPIndex( field.index ) };
		BuildCopyAssignmentOperatorPart(
			function_context.llvm_ir_builder.CreateGEP( this_llvm_value, index_list ),
			function_context.llvm_ir_builder.CreateGEP( src_llvm_value , index_list ),
			field.type,
			function_context );
	}

	function_context.alloca_ir_builder.CreateBr( function_context.function_basic_block );
	function_context.llvm_ir_builder.CreateRetVoid();

	// Add generated assignment operator
	FunctionVariable op_variable;
	op_variable.type= std::move( op_type );
	op_variable.have_body= true;
	op_variable.is_this_call= true;
	op_variable.is_generated= true;
	op_variable.constexpr_kind= the_class.can_be_constexpr ? FunctionVariable::ConstexprKind::ConstexprComplete : FunctionVariable::ConstexprKind::NonConstexpr;
	op_variable.llvm_function= llvm_op_function;

	if( prev_operator_variable != nullptr )
		*prev_operator_variable= std::move( op_variable );
	else
	{
		if( Value* const operators_value= the_class.members.GetThisScopeValue( op_name ) )
		{
			OverloadedFunctionsSet* const operators= operators_value->GetFunctionsSet();
			U_ASSERT( operators != nullptr );
			operators->functions.push_back( std::move( op_variable ) );
		}
		else
		{
			OverloadedFunctionsSet operators;
			operators.functions.push_back( std::move( op_variable ) );
			the_class.members.AddName( op_name , std::move( operators ) );
		}
	}

	// After operator generation, class is copy-assignable.
	the_class.is_copy_assignable= true;
}

void CodeBuilder::BuildCopyConstructorPart(
	llvm::Value* const dst, llvm::Value* const src,
	const Type& type,
	FunctionContext& function_context )
{
	if( type.GetFundamentalType() != nullptr || type.GetEnumType() != nullptr || type.GetFunctionPointerType() != nullptr )
	{
		// Create simple load-store.
		if( src->getType() == dst->getType() )
			function_context.llvm_ir_builder.CreateStore( function_context.llvm_ir_builder.CreateLoad( src ), dst );
		else if( src->getType() == dst->getType()->getPointerElementType() )
			function_context.llvm_ir_builder.CreateStore( src, dst );
		else U_ASSERT( false );
	}
	else if( const Array* const array_type_ptr= type.GetArrayType() )
	{
		const Array& array_type= *array_type_ptr;

		GenerateLoop(
			array_type.size,
			[&](llvm::Value* const counter_value)
			{
				llvm::Value* index_list[2];
				index_list[0]= GetZeroGEPIndex();
				index_list[1]= counter_value;

				BuildCopyConstructorPart(
					function_context.llvm_ir_builder.CreateGEP( dst, index_list ),
					function_context.llvm_ir_builder.CreateGEP( src, index_list ),
					array_type.type,
					function_context );
			},
			function_context);
	}
	else if( const Tuple* const tuple_type= type.GetTupleType() )
	{
		for( const Type& element_type : tuple_type->elements )
		{
			llvm::Value* const index_list[2]{ GetZeroGEPIndex(), GetFieldGEPIndex( size_t(&element_type - tuple_type->elements.data()) ) };
			BuildCopyConstructorPart(
				function_context.llvm_ir_builder.CreateGEP( dst, index_list ),
				function_context.llvm_ir_builder.CreateGEP( src, index_list ),
				element_type,
				function_context );
		}
	}
	else if( const ClassProxyPtr class_type_proxy= type.GetClassTypeProxy() )
	{
		const Type filed_class_type= class_type_proxy;
		const Class& class_type= *class_type_proxy->class_;

		// Search copy constructor.
		const Value* constructor_value=
			class_type.members.GetThisScopeValue( Keyword( Keywords::constructor_ ) );
		U_ASSERT( constructor_value != nullptr );
		const OverloadedFunctionsSet* const constructors_set= constructor_value->GetFunctionsSet();
		U_ASSERT( constructors_set != nullptr );

		const FunctionVariable* constructor= nullptr;;
		for( const FunctionVariable& candidate_constructor : constructors_set->functions )
		{
			const Function& constructor_type= *candidate_constructor.type.GetFunctionType();

			if( constructor_type.args.size() == 2u &&
				constructor_type.args.back().type == filed_class_type && !constructor_type.args.back().is_mutable )
			{
				constructor= &candidate_constructor;
				break;
			}
		}
		U_ASSERT( constructor != nullptr );

		// Call it.
		function_context.llvm_ir_builder.CreateCall(constructor->llvm_function, { dst, src } );
	}
	else
		U_ASSERT(false);
}

void CodeBuilder::BuildCopyAssignmentOperatorPart(
	llvm::Value* const dst, llvm::Value* const src,
	const Type& type,
	FunctionContext& function_context )
{
	if( type.GetFundamentalType() != nullptr || type.GetEnumType() != nullptr || type.GetFunctionPointerType() != nullptr )
	{
		// Create simple load-store.
		if( src->getType() == dst->getType() )
			function_context.llvm_ir_builder.CreateStore( function_context.llvm_ir_builder.CreateLoad( src ), dst );
		else if( src->getType() == dst->getType()->getPointerElementType() )
			function_context.llvm_ir_builder.CreateStore( src, dst );
		else U_ASSERT( false );
	}
	else if( const Array* const array_type_ptr= type.GetArrayType() )
	{
		const Array& array_type= *array_type_ptr;

		GenerateLoop(
			array_type.size,
			[&](llvm::Value* const counter_value)
			{
				llvm::Value* index_list[2];
				index_list[0]= GetZeroGEPIndex();
				index_list[1]= counter_value;

				BuildCopyAssignmentOperatorPart(
					function_context.llvm_ir_builder.CreateGEP( dst, index_list ),
					function_context.llvm_ir_builder.CreateGEP( src, index_list ),
					array_type.type,
					function_context );
			},
			function_context);
	}
	else if( const Tuple* const tuple_type= type.GetTupleType() )
	{
		for( const Type& element_type : tuple_type->elements )
		{
			llvm::Value* const index_list[2]{ GetZeroGEPIndex(), GetFieldGEPIndex( size_t(&element_type - tuple_type->elements.data()) ) };
			BuildCopyAssignmentOperatorPart(
				function_context.llvm_ir_builder.CreateGEP( dst, index_list ),
				function_context.llvm_ir_builder.CreateGEP( src, index_list ),
				element_type,
				function_context );
		}
	}
	else if( const ClassProxyPtr class_type_proxy= type.GetClassTypeProxy() )
	{
		const Type filed_class_type= class_type_proxy;
		const Class& class_type= *class_type_proxy->class_;

		// Search copy-assignment aoperator.
		const Value* op_value=
			class_type.members.GetThisScopeValue( "=" );
		U_ASSERT( op_value != nullptr );
		const OverloadedFunctionsSet* const operators_set= op_value->GetFunctionsSet();
		U_ASSERT( operators_set != nullptr );

		const FunctionVariable* op= nullptr;;
		for( const FunctionVariable& candidate_op : operators_set->functions )
		{
			const Function& op_type= *candidate_op .type.GetFunctionType();

			if( op_type.args[0u].type == type &&  op_type.args[0u].is_mutable && op_type.args[0u].is_reference &&
				op_type.args[1u].type == type && !op_type.args[1u].is_mutable && op_type.args[1u].is_reference )
			{
				op= &candidate_op;
				break;
			}
		}
		U_ASSERT( op != nullptr );

		// Call it.
		function_context.llvm_ir_builder.CreateCall( op->llvm_function, { dst, src } );
	}
	else
		U_ASSERT(false);
}

void CodeBuilder::CopyBytes_r(
	llvm::Value* const src, llvm::Value* const dst,
	const llvm::Type* const llvm_type,
	FunctionContext& function_context )
{
	if( llvm_type->isIntegerTy() || llvm_type->isFloatingPointTy() || llvm_type->isPointerTy() )
	{
		// Create simple load-store.
		if( src->getType() == dst->getType() )
			function_context.llvm_ir_builder.CreateStore( function_context.llvm_ir_builder.CreateLoad( src ), dst );
		else if( src->getType() == dst->getType()->getPointerElementType() )
			function_context.llvm_ir_builder.CreateStore( src, dst );
		else U_ASSERT(false);
	}
	else if( llvm_type->isArrayTy() )
	{
		GenerateLoop(
			llvm_type->getArrayNumElements(),
			[&](llvm::Value* const counter_value)
			{
				llvm::Value* const index_list[2]{ GetZeroGEPIndex(), counter_value };
				CopyBytes_r(
					function_context.llvm_ir_builder.CreateGEP( src, index_list ),
					function_context.llvm_ir_builder.CreateGEP( dst, index_list ),
					llvm_type->getArrayElementType(),
					function_context );
			},
			function_context );
	}
	else if( llvm_type->isStructTy() )
	{
		for( unsigned int i= 0u; i < llvm_type->getStructNumElements(); ++i )
		{
			llvm::Value* const index_list[2]{ GetZeroGEPIndex(), GetFieldGEPIndex(i) };
			CopyBytes_r(
				function_context.llvm_ir_builder.CreateGEP( src, index_list ),
				function_context.llvm_ir_builder.CreateGEP( dst, index_list ),
				llvm_type->getStructElementType(i),
				function_context );
		}
	}
	else
		U_ASSERT(false);
}

void CodeBuilder::CopyBytes(
	llvm::Value* const src, llvm::Value* const dst,
	const Type& type,
	FunctionContext& function_context )
{
	return CopyBytes_r( src, dst, type.GetLLVMType(), function_context );
}

void CodeBuilder::MoveConstantToMemory(
	llvm::Value* const ptr, llvm::Constant* const constant,
	FunctionContext& function_context )
{
	llvm::Value* index_list[2];
	index_list[0]= GetZeroGEPIndex();

	llvm::Type* const type= constant->getType();
	if( type->isStructTy() )
	{
		llvm::StructType* const struct_type= llvm::dyn_cast<llvm::StructType>(type);
		for( unsigned int i= 0u; i < struct_type->getNumElements(); ++i )
		{
			index_list[1]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, i ) );
			MoveConstantToMemory(
				function_context.llvm_ir_builder.CreateGEP( ptr, index_list ),
				constant->getAggregateElement(i),
				function_context );
		}
	}
	else if( type->isArrayTy() )
	{
		llvm::ArrayType* const array_type= llvm::dyn_cast<llvm::ArrayType>(type);
		for( unsigned int i= 0u; i < array_type->getNumElements(); ++i )
		{
			index_list[1]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, i ) );
			MoveConstantToMemory(
				function_context.llvm_ir_builder.CreateGEP( ptr, index_list ),
				constant->getAggregateElement(i),
				function_context );
		}
	}
	else if( type->isIntegerTy() || type->isFloatingPointTy() )
		function_context.llvm_ir_builder.CreateStore( constant, ptr );
	else U_ASSERT(false);
}

bool CodeBuilder::IsDefaultConstructor( const Function& function_type, const Type& base_class )
{
	return
		function_type.args.size() == 1u &&
		function_type.args[0].type == base_class &&  function_type.args[0].is_mutable && function_type.args[0].is_reference;
}

bool CodeBuilder::IsCopyConstructor( const Function& function_type, const Type& base_class )
{
	return
		function_type.args.size() == 2u &&
		function_type.args[0].type == base_class &&  function_type.args[0].is_mutable && function_type.args[0].is_reference &&
		function_type.args[1].type == base_class && !function_type.args[1].is_mutable && function_type.args[1].is_reference;
}

bool CodeBuilder::IsCopyAssignmentOperator( const Function& function_type, const Type& base_class )
{
	return
		function_type.args.size() == 2u &&
		function_type.args[0].type == base_class &&  function_type.args[0].is_mutable && function_type.args[0].is_reference &&
		function_type.args[1].type == base_class && !function_type.args[1].is_mutable && function_type.args[1].is_reference;
}

} // namespace CodeBuilderPrivate

} //namespace U
