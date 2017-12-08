#include "push_disable_llvm_warnings.hpp"
#include <llvm/IR/Constant.h>
#include <llvm/IR/LLVMContext.h>
#include "pop_llvm_warnings.hpp"

#include "assert.hpp"
#include "keywords.hpp"
#include "lang_types.hpp"
#include "mangling.hpp"

#include "code_builder.hpp"

namespace U
{

namespace CodeBuilderPrivate
{

void CodeBuilder::TryGenerateDefaultConstructor( Class& the_class, const Type& class_type )
{
	// Search for explicit default constructor.
	if( const NamesScope::InsertedName* const constructors_name=
		the_class.members.GetThisScopeName( Keyword( Keywords::constructor_ ) ) )
	{
		const OverloadedFunctionsSet* const constructors= constructors_name->second.GetFunctionsSet();
		U_ASSERT( constructors != nullptr );
		for( const FunctionVariable& constructor : *constructors )
		{
			const Function& constructor_type= *constructor.type.GetFunctionType();

			U_ASSERT( constructor_type.args.size() >= 1u && constructor_type.args.front().type == class_type );
			if( ( constructor_type.args.size() == 1u ) )
			{
				the_class.is_default_constructible= true;
				return;
			}
		};
	}

	// Generating of default constructor disabled, if class have other explicit constructors, except copy constructors.
	if( the_class.have_explicit_noncopy_constructors )
		return;

	// Generate default constructor, if all fields is default constructible.
	bool all_fields_is_default_constructible= true;

	the_class.members.ForEachInThisScope(
		[&]( const NamesScope::InsertedName& member )
		{
			const ClassField* const field= member.second.GetClassField();
			if( field == nullptr )
				return;

			if( !field->type.IsDefaultConstructible() )
				all_fields_is_default_constructible= false;
		} );

	if( !all_fields_is_default_constructible )
		return;

	// Generate function

	Function constructor_type;
	constructor_type.return_type= void_type_;
	constructor_type.args.emplace_back();
	constructor_type.args.back().type= class_type;
	constructor_type.args.back().is_mutable= true;
	constructor_type.args.back().is_reference= true;

	std::vector<llvm::Type*> args_llvm_types;
	args_llvm_types.push_back( llvm::PointerType::get( class_type.GetLLVMType(), 0u ) );

	constructor_type.llvm_function_type=
		llvm::FunctionType::get(
			fundamental_llvm_types_.void_,
			llvm::ArrayRef<llvm::Type*>( args_llvm_types.data(), args_llvm_types.size() ),
			false );

	llvm::Function* const llvm_constructor_function=
		llvm::Function::Create(
			constructor_type.llvm_function_type,
			llvm::Function::LinkageTypes::LinkOnceODRLinkage,
			MangleFunction( the_class.members, Keyword( Keywords::constructor_ ), constructor_type, true ),
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
	this_llvm_value->setName( KeywordAscii( Keywords::this_ ) );

	the_class.members.ForEachInThisScope(
		[&]( const NamesScope::InsertedName& member )
		{
			const ClassField* const field= member.second.GetClassField();
			if( field == nullptr )
				return;

			Variable field_variable;
			field_variable.type= field->type;
			field_variable.value_type= ValueType::Reference;

			llvm::Value* index_list[2];
			index_list[0]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(0u) ) );
			index_list[1]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(field->index) ) );
			field_variable.llvm_value=
				function_context.llvm_ir_builder.CreateGEP( this_llvm_value, llvm::ArrayRef<llvm::Value*> ( index_list, 2u ) );

			ApplyEmptyInitializer( member.first, FilePos()/*TODO*/, field_variable, function_context );
		} );

	function_context.llvm_ir_builder.CreateRetVoid();
	function_context.alloca_ir_builder.CreateBr( function_context.function_basic_block );

	// Add generated constructor
	FunctionVariable constructor_variable;
	constructor_variable.type= std::move( constructor_type );
	constructor_variable.have_body= true;
	constructor_variable.is_this_call= true;
	constructor_variable.is_generated= true;
	constructor_variable.llvm_function= llvm_constructor_function;

	if( NamesScope::InsertedName* const constructors_name=
		the_class.members.GetThisScopeName( Keyword( Keywords::constructor_ ) ) )
	{
		OverloadedFunctionsSet* const constructors= constructors_name->second.GetFunctionsSet();
		U_ASSERT( constructors != nullptr );
		constructors->push_back( std::move( constructor_variable ) );
	}
	else
	{
		OverloadedFunctionsSet constructors;
		constructors.push_back( std::move( constructor_variable ) );
		the_class.members.AddName( Keyword( Keywords::constructor_ ), std::move( constructors ) );
	}

	// After default constructor generation, class is default-constructible.
	the_class.is_default_constructible= true;
}

void CodeBuilder::TryGenerateCopyConstructor( Class& the_class, const Type& class_type )
{
	// Search for explicit copy constructor.
	if( const NamesScope::InsertedName* const constructors_name=
		the_class.members.GetThisScopeName( Keyword( Keywords::constructor_ ) ) )
	{
		const OverloadedFunctionsSet* const constructors= constructors_name->second.GetFunctionsSet();
		U_ASSERT( constructors != nullptr );
		for( const FunctionVariable& constructor : *constructors )
		{
			const Function& constructor_type= *constructor.type.GetFunctionType();

			U_ASSERT( constructor_type.args.size() >= 1u && constructor_type.args.front().type == class_type );
			if( constructor_type.args.size() == 2u &&
				constructor_type.args.back().type == class_type && !constructor_type.args.back().is_mutable )
			{
				the_class.is_copy_constructible= true;
				return;
			}
		}
	}

	bool all_fields_is_copy_constructible= true;

	the_class.members.ForEachInThisScope(
		[&]( const NamesScope::InsertedName& member )
		{
			const ClassField* const field= member.second.GetClassField();
			if( field == nullptr )
				return;

			if( !field->type.IsCopyConstructible() )
				all_fields_is_copy_constructible= false;
		} );

	if( !all_fields_is_copy_constructible )
		return;

	// Generate copy-constructor
	Function constructor_type;
	constructor_type.return_type= void_type_;
	constructor_type.args.resize(2u);
	constructor_type.args[0].type= class_type;
	constructor_type.args[0].is_mutable= true;
	constructor_type.args[0].is_reference= true;
	constructor_type.args[1].type= class_type;
	constructor_type.args[1].is_mutable= false;
	constructor_type.args[1].is_reference= true;

	std::vector<llvm::Type*> args_llvm_types;
	args_llvm_types.push_back( llvm::PointerType::get( class_type.GetLLVMType(), 0u ) );
	args_llvm_types.push_back( llvm::PointerType::get( class_type.GetLLVMType(), 0u ) );

	constructor_type.llvm_function_type=
		llvm::FunctionType::get(
			fundamental_llvm_types_.void_,
			llvm::ArrayRef<llvm::Type*>( args_llvm_types.data(), args_llvm_types.size() ),
			false );

	llvm::Function* const llvm_constructor_function=
		llvm::Function::Create(
			constructor_type.llvm_function_type,
			llvm::Function::LinkageTypes::LinkOnceODRLinkage,
			MangleFunction( the_class.members, Keyword( Keywords::constructor_ ), constructor_type, true ),
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
	this_llvm_value->setName( KeywordAscii( Keywords::this_ ) );
	llvm::Value* const src_llvm_value= &*(++llvm_constructor_function->args().begin());
	src_llvm_value->setName( "src" );

	the_class.members.ForEachInThisScope(
		[&]( const NamesScope::InsertedName& member )
		{
			const ClassField* const field= member.second.GetClassField();
			if( field == nullptr )
				return;
			U_ASSERT( field->type.IsCopyConstructible() );

			llvm::Value* index_list[2];
			index_list[0]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(0u) ) );
			index_list[1]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(field->index) ) );

			BuildCopyConstructorPart(
				function_context.llvm_ir_builder.CreateGEP( src_llvm_value , llvm::ArrayRef<llvm::Value*>( index_list, 2u ) ),
				function_context.llvm_ir_builder.CreateGEP( this_llvm_value, llvm::ArrayRef<llvm::Value*>( index_list, 2u ) ),
				field->type,
				function_context );

		} ); // For fields.

	function_context.llvm_ir_builder.CreateRetVoid();
	function_context.alloca_ir_builder.CreateBr( function_context.function_basic_block );

	// Add generated constructor
	FunctionVariable constructor_variable;
	constructor_variable.type= std::move( constructor_type );
	constructor_variable.have_body= true;
	constructor_variable.is_this_call= true;
	constructor_variable.is_generated= true;
	constructor_variable.llvm_function= llvm_constructor_function;

	if( NamesScope::InsertedName* const constructors_name=
		the_class.members.GetThisScopeName( Keyword( Keywords::constructor_ ) ) )
	{
		OverloadedFunctionsSet* const constructors= constructors_name->second.GetFunctionsSet();
		U_ASSERT( constructors != nullptr );
		constructors->push_back( std::move( constructor_variable ) );
	}
	else
	{
		OverloadedFunctionsSet constructors;
		constructors.push_back( std::move( constructor_variable ) );
		the_class.members.AddName( Keyword( Keywords::constructor_ ), std::move( constructors ) );
	}

	// After default constructor generation, class is copy-constructible.
	the_class.is_copy_constructible= true;
}

void CodeBuilder::TryGenerateDestructor( Class& the_class, const Type& class_type )
{
	// Search for explicit destructor.
	if( const NamesScope::InsertedName* const destructor_name=
		the_class.members.GetThisScopeName( Keyword( Keywords::destructor_ ) ) )
	{
		const OverloadedFunctionsSet* const destructors= destructor_name->second.GetFunctionsSet();
		U_ASSERT( destructors != nullptr && destructors->size() == 1u );
		U_UNUSED( destructors );
		the_class.have_destructor= true;
		return;
	}

	// SPRACHE_TODO - maybe not generate default destructor for classes, that have no fields with destructors?
	// SPRACHE_TODO - maybe mark generated destructor for this cases as "empty"?

	// Generate destructor.
	Function destructor_type;
	destructor_type.return_type= void_type_;
	destructor_type.args.resize(1u);
	destructor_type.args[0].type= class_type;
	destructor_type.args[0].is_mutable= true;
	destructor_type.args[0].is_reference= true;

	llvm::Type* const this_llvm_type= llvm::PointerType::get( class_type.GetLLVMType(), 0u );
	destructor_type.llvm_function_type=
		llvm::FunctionType::get(
			fundamental_llvm_types_.void_,
			llvm::ArrayRef<llvm::Type*>( &this_llvm_type, 1u ),
			false );

	llvm::Function* const llvm_destructor_function=
		llvm::Function::Create(
			destructor_type.llvm_function_type,
			llvm::Function::LinkageTypes::LinkOnceODRLinkage,
			MangleFunction( the_class.members, Keyword( Keywords::destructor_ ), destructor_type, true ),
			module_.get() );

	SetupGeneratedFunctionLinkageAttributes( *llvm_destructor_function );
	llvm_destructor_function->addAttribute( 1u, llvm::Attribute::NonNull ); // this is nonnull

	llvm::Value* const this_llvm_value= &*llvm_destructor_function->args().begin();
	this_llvm_value->setName( KeywordAscii( Keywords::this_ ) );

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
		llvm_destructor_function );
	function_context.this_= &this_;

	CallMembersDestructors( function_context );
	function_context.alloca_ir_builder.CreateBr( function_context.function_basic_block );
	function_context.llvm_ir_builder.CreateRetVoid();

	// Add generated destructor.
	FunctionVariable destructor_variable;
	destructor_variable.type= std::move( destructor_type );
	destructor_variable.have_body= true;
	destructor_variable.is_this_call= true;
	destructor_variable.is_generated= true;
	destructor_variable.llvm_function= llvm_destructor_function;

	// TODO - destructor have no overloads. Maybe store it as FunctionVariable, not as FunctionsSet?
	OverloadedFunctionsSet destructors;
	destructors.push_back( std::move( destructor_variable ) );

	the_class.members.AddName(
		Keyword( Keywords::destructor_ ),
		Value( std::move( destructors ) ) );

	// Say "we have destructor".
	the_class.have_destructor= true;
}

void CodeBuilder::TryGenerateCopyAssignmentOperator( Class& the_class, const Type& class_type )
{
	static const ProgramString op_name= "="_SpC;

	// Search for explicit assignment operator.
	if( const NamesScope::InsertedName* const assignment_operator_name=
		the_class.members.GetThisScopeName( op_name ) )
	{
		const OverloadedFunctionsSet* const operators= assignment_operator_name->second.GetFunctionsSet();
		for( const FunctionVariable& op : *operators )
		{
			const Function& op_type= *op.type.GetFunctionType();

			if( op_type.args.size() != 2u )
				continue; // Can happens in error case.

			// SPRACHE_TODO - support assignment operator with value src argument.
			if(
				op_type.args[0u].type == class_type &&  op_type.args[0u].is_mutable && op_type.args[0u].is_reference &&
				op_type.args[1u].type == class_type && !op_type.args[1u].is_mutable && op_type.args[1u].is_reference )
			{
				the_class.is_copy_assignable= true;
				return;
			}
		}
	}

	bool all_fields_is_copy_assignable= true;

	the_class.members.ForEachInThisScope(
		[&]( const NamesScope::InsertedName& member )
		{
			const ClassField* const field= member.second.GetClassField();
			if( field == nullptr )
				return;

			if( !field->type.IsCopyAssignable() )
				all_fields_is_copy_assignable= false;
		} );

	if( !all_fields_is_copy_assignable )
		return;

	// Generate assignment operator
	Function op_type;
	op_type.return_type= void_type_;
	op_type.args.resize(2u);
	op_type.args[0].type= class_type;
	op_type.args[0].is_mutable= true;
	op_type.args[0].is_reference= true;
	op_type.args[1].type= class_type;
	op_type.args[1].is_mutable= false;
	op_type.args[1].is_reference= true;

	std::vector<llvm::Type*> args_llvm_types;
	args_llvm_types.push_back( llvm::PointerType::get( class_type.GetLLVMType(), 0u ) );
	args_llvm_types.push_back( llvm::PointerType::get( class_type.GetLLVMType(), 0u ) );

	op_type.llvm_function_type=
		llvm::FunctionType::get(
			fundamental_llvm_types_.void_,
			llvm::ArrayRef<llvm::Type*>( args_llvm_types.data(), args_llvm_types.size() ),
			false );

	llvm::Function* const llvm_op_function=
		llvm::Function::Create(
			op_type.llvm_function_type,
			llvm::Function::LinkageTypes::LinkOnceODRLinkage,
			MangleFunction( the_class.members, op_name, op_type, true ),
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
	this_llvm_value->setName( KeywordAscii( Keywords::this_ ) );
	llvm::Value* const src_llvm_value= &*(++llvm_op_function->args().begin());
	src_llvm_value->setName( "src" );

	the_class.members.ForEachInThisScope(
		[&]( const NamesScope::InsertedName& member )
		{
			const ClassField* const field= member.second.GetClassField();
			if( field == nullptr )
				return;
			U_ASSERT( field->type.IsCopyAssignable() );

			llvm::Value* index_list[2];
			index_list[0]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(0u) ) );
			index_list[1]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(field->index) ) );

			BuildCopyAssignmentOperatorPart(
				function_context.llvm_ir_builder.CreateGEP( src_llvm_value , llvm::ArrayRef<llvm::Value*>( index_list, 2u ) ),
				function_context.llvm_ir_builder.CreateGEP( this_llvm_value, llvm::ArrayRef<llvm::Value*>( index_list, 2u ) ),
				field->type,
				function_context );
		} ); // For fields.

	function_context.alloca_ir_builder.CreateBr( function_context.function_basic_block );
	function_context.llvm_ir_builder.CreateRetVoid();

	// Add generated assignment operator
	FunctionVariable op_variable;
	op_variable.type= std::move( op_type );
	op_variable.have_body= true;
	op_variable.is_this_call= true;
	op_variable.is_generated= true;
	op_variable.llvm_function= llvm_op_function;

	if( NamesScope::InsertedName* const operators_name= the_class.members.GetThisScopeName( op_name ) )
	{
		OverloadedFunctionsSet* const operators= operators_name->second.GetFunctionsSet();
		U_ASSERT( operators != nullptr );
		operators->push_back( std::move( op_variable ) );
	}
	else
	{
		OverloadedFunctionsSet operators;
		operators.push_back( std::move( op_variable ) );
		the_class.members.AddName( op_name , std::move( operators ) );
	}

	// After operator generation, class is copy-assignable.
	the_class.is_copy_assignable= true;
}

void CodeBuilder::BuildCopyConstructorPart(
	llvm::Value* const src, llvm::Value* const dst,
	const Type& type,
	FunctionContext& function_context )
{
	if( type.GetFundamentalType() != nullptr || type.GetEnumType() != nullptr )
	{
		// Create simple load-store.
		llvm::Value* const val= function_context.llvm_ir_builder.CreateLoad( src );
		function_context.llvm_ir_builder.CreateStore( val, dst );
	}
	else if( const Array* const array_type_ptr= type.GetArrayType() )
	{
		const Array& array_type= *array_type_ptr;

		GenerateLoop(
			array_type.ArraySizeOrZero(),
			[&](llvm::Value* const counter_value)
			{
				llvm::Value* index_list[2];
				index_list[0]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(0u) ) );
				index_list[1]= counter_value;

				BuildCopyConstructorPart(
					function_context.llvm_ir_builder.CreateGEP( src, llvm::ArrayRef<llvm::Value*>( index_list, 2u ) ),
					function_context.llvm_ir_builder.CreateGEP( dst, llvm::ArrayRef<llvm::Value*>( index_list, 2u ) ),
					array_type.type,
					function_context );
			},
			function_context);
	}
	else if( const ClassProxyPtr class_type_proxy= type.GetClassTypeProxy() )
	{
		const Type filed_class_type= class_type_proxy;
		const Class& class_type= *class_type_proxy->class_;

		// Search copy constructor.
		const NamesScope::InsertedName* constructor_name=
			class_type.members.GetThisScopeName( Keyword( Keywords::constructor_ ) );
		U_ASSERT( constructor_name != nullptr );
		const OverloadedFunctionsSet* const constructors_set= constructor_name->second.GetFunctionsSet();
		U_ASSERT( constructors_set != nullptr );

		const FunctionVariable* constructor= nullptr;;
		for( const FunctionVariable& candidate_constructor : *constructors_set )
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
		std::vector<llvm::Value*> llvm_args;
		llvm_args.push_back( dst );
		llvm_args.push_back( src );
		function_context.llvm_ir_builder.CreateCall(
			llvm::dyn_cast<llvm::Function>(constructor->llvm_function),
			llvm_args );
	}
	else
		U_ASSERT(false);
}

void CodeBuilder::BuildCopyAssignmentOperatorPart(
	llvm::Value* src, llvm::Value* dst,
	const Type& type,
	FunctionContext& function_context )
{
	if( type.GetFundamentalType() != nullptr || type.GetEnumType() != nullptr )
	{
		// Create simple load-store.
		llvm::Value* const val= function_context.llvm_ir_builder.CreateLoad( src );
		function_context.llvm_ir_builder.CreateStore( val, dst );
	}
	else if( const Array* const array_type_ptr= type.GetArrayType() )
	{
		const Array& array_type= *array_type_ptr;

		GenerateLoop(
			array_type.ArraySizeOrZero(),
			[&](llvm::Value* const counter_value)
			{
				llvm::Value* index_list[2];
				index_list[0]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(0u) ) );
				index_list[1]= counter_value;

				BuildCopyAssignmentOperatorPart(
					function_context.llvm_ir_builder.CreateGEP( src, llvm::ArrayRef<llvm::Value*>( index_list, 2u ) ),
					function_context.llvm_ir_builder.CreateGEP( dst, llvm::ArrayRef<llvm::Value*>( index_list, 2u ) ),
					array_type.type,
					function_context );
			},
			function_context);
	}
	else if( const ClassProxyPtr class_type_proxy= type.GetClassTypeProxy() )
	{
		const Type filed_class_type= class_type_proxy;
		const Class& class_type= *class_type_proxy->class_;

		// Search copy-assignment aoperator.
		const NamesScope::InsertedName* op_name=
			class_type.members.GetThisScopeName( "="_SpC );
		U_ASSERT( op_name != nullptr );
		const OverloadedFunctionsSet* const operators_set= op_name->second.GetFunctionsSet();
		U_ASSERT( operators_set != nullptr );

		const FunctionVariable* op= nullptr;;
		for( const FunctionVariable& candidate_op : *operators_set )
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
		std::vector<llvm::Value*> llvm_args;
		llvm_args.push_back( dst );
		llvm_args.push_back( src );
		function_context.llvm_ir_builder.CreateCall(
			llvm::dyn_cast<llvm::Function>(op->llvm_function),
			llvm_args );
	}
	else
		U_ASSERT(false);
}

} // namespace CodeBuilderPrivate

} //namespace U