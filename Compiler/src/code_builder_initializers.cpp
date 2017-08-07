#include <set>

#include "push_disable_llvm_warnings.hpp"
#include <llvm/IR/Constant.h>
#include <llvm/IR/LLVMContext.h>
#include "pop_llvm_warnings.hpp"

#include "assert.hpp"
#include "keywords.hpp"
#include "lang_types.hpp"

#include "code_builder.hpp"

namespace U
{

namespace CodeBuilderPrivate
{

void CodeBuilder::ApplyInitializer_r(
	const Variable& variable,
	const IInitializer* const initializer,
	NamesScope& block_names,
	FunctionContext& function_context )
{
	// SPRACHE_TODO - allow missing initializers for types with default constructor.
	if( initializer == nullptr )
	{
		// TODO - set file_pos
		errors_.push_back( ReportExpectedInitializer( FilePos() ) );
		return;
	}

	if( const ArrayInitializer* const array_initializer=
		dynamic_cast<const ArrayInitializer*>(initializer) )
	{
		ApplyArrayInitializer( variable, *array_initializer, block_names, function_context );
	}
	else if( const StructNamedInitializer* const struct_named_initializer=
		dynamic_cast<const StructNamedInitializer*>(initializer) )
	{
		ApplyStructNamedInitializer( variable, *struct_named_initializer, block_names, function_context );
	}
	else if( const ConstructorInitializer* const constructor_initializer=
		dynamic_cast<const ConstructorInitializer*>(initializer) )
	{
		ApplyConstructorInitializer( variable, *constructor_initializer, block_names, function_context );
	}
	else if( const ExpressionInitializer* const expression_initializer=
		dynamic_cast<const ExpressionInitializer*>(initializer) )
	{
		ApplyExpressionInitializer( variable, *expression_initializer, block_names, function_context );
	}
	else if( const ZeroInitializer* const zero_initializer=
		dynamic_cast<const ZeroInitializer*>(initializer) )
	{
		ApplyZeroInitializer( variable, *zero_initializer, block_names, function_context );
	}
	else
	{
		U_ASSERT(false);
	}
}

void CodeBuilder::ApplyArrayInitializer(
	const Variable& variable,
	const ArrayInitializer& initializer,
	NamesScope& block_names,
	FunctionContext& function_context )
{
	const ArrayPtr* const array_type_ptr= boost::get<ArrayPtr>( &variable.type.one_of_type_kind );
	if( array_type_ptr == nullptr )
	{
		errors_.push_back( ReportArrayInitializerForNonArray( initializer.file_pos_ ) );
		return;
	}
	U_ASSERT( *array_type_ptr != nullptr );
	const Array& array_type= **array_type_ptr;

	if( initializer.initializers.size() != array_type.size )
	{
		errors_.push_back(
			ReportArrayInitializersCountMismatch(
				initializer.file_pos_,
				array_type.size,
				initializer.initializers.size() ) );
		return;
		// SPRACHE_TODO - add array continious initializers.
	}

	Variable array_member= variable;
	array_member.type= array_type.type;
	array_member.location= Variable::Location::Pointer;

	// Make first index = 0 for array to pointer conversion.
	llvm::Value* index_list[2];
	index_list[0]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(0u) ) );

	for( size_t i= 0u; i < initializer.initializers.size(); i++ )
	{
		index_list[1]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(i) ) );
		array_member.llvm_value=
			function_context.llvm_ir_builder.CreateGEP( variable.llvm_value, llvm::ArrayRef<llvm::Value*> ( index_list, 2u ) );

		ApplyInitializer_r( array_member, initializer.initializers[i].get(), block_names, function_context );
	}
}

void CodeBuilder::ApplyStructNamedInitializer(
	const Variable& variable,
	const StructNamedInitializer& initializer,
	NamesScope& block_names,
	FunctionContext& function_context )
{
	const ClassPtr* const class_type_ptr= boost::get<ClassPtr>( &variable.type.one_of_type_kind );
	if( class_type_ptr == nullptr )
	{
		errors_.push_back( ReportStructInitializerForNonStruct( initializer.file_pos_ ) );
		return;
	}
	U_ASSERT( *class_type_ptr != nullptr );
	const Class& class_type= **class_type_ptr;

	std::set<ProgramString> initialized_members_names;

	Variable struct_member= variable;
	struct_member.location= Variable::Location::Pointer;
	// Make first index = 0 for array to pointer conversion.
	llvm::Value* index_list[2];
	index_list[0]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(0u) ) );

	for( const StructNamedInitializer::MemberInitializer& member_initializer : initializer.members_initializers )
	{
		if( initialized_members_names.count( member_initializer.name ) != 0 )
		{
			errors_.push_back( ReportDuplicatedStructMemberInitializer( initializer.file_pos_, member_initializer.name ) );
			continue;
		}

		const NamesScope::InsertedName* const class_member= class_type.members.GetThisScopeName( member_initializer.name );
		if( class_member == nullptr )
		{
			errors_.push_back( ReportNameNotFound( initializer.file_pos_, member_initializer.name ) );
			continue;
		}
		const ClassField* const field= class_member->second.GetClassField();
		if( field == nullptr )
		{
			errors_.push_back( ReportInitializerForNonfieldStructMember( initializer.file_pos_, member_initializer.name ) );
			continue;
		}

		initialized_members_names.insert( member_initializer.name );

		struct_member.type= field->type;
		index_list[1]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(field->index) ) );
		struct_member.llvm_value=
			function_context.llvm_ir_builder.CreateGEP( variable.llvm_value, llvm::ArrayRef<llvm::Value*> ( index_list, 2u ) );

		ApplyInitializer_r( struct_member, member_initializer.initializer.get(), block_names, function_context );
	}

	U_ASSERT( initialized_members_names.size() <= class_type.field_count );
	class_type.members.ForEachInThisScope(
		[&]( const NamesScope::InsertedName& class_member )
		{
			if( const ClassField* const field = class_member.second.GetClassField() )
			{
				U_UNUSED(field);
				// SPRACHE_TODO - allow missed initialziers for default-constructed classes.
				if( initialized_members_names.count( class_member.first ) == 0 )
					errors_.push_back(ReportMissingStructMemberInitializer( initializer.file_pos_, class_member.first ) );
			}
		});
}

void CodeBuilder::ApplyConstructorInitializer(
	const Variable& variable,
	const ConstructorInitializer& initializer,
	NamesScope& block_names,
	FunctionContext& function_context )
{
	if( const FundamentalType* const fundamental_type= boost::get<FundamentalType>( &variable.type.one_of_type_kind ) )
	{
		U_UNUSED(fundamental_type);

		if( initializer.call_operator.arguments_.size() != 1u )
		{
			errors_.push_back( ReportFundamentalTypesHaveConstructorsWithExactlyOneParameter( initializer.file_pos_ ) );
			return;
		}

		const Value expression_result=
			BuildExpressionCode( *initializer.call_operator.arguments_.front(), block_names, function_context );
		if( expression_result.GetType() != variable.type )
		{
			errors_.push_back( ReportTypesMismatch( initializer.file_pos_, variable.type.ToString(), expression_result.GetType().ToString() ) );
			return;
		}

		llvm::Value* const value_for_assignment= CreateMoveToLLVMRegisterInstruction( *expression_result.GetVariable(), function_context );
		function_context.llvm_ir_builder.CreateStore( value_for_assignment, variable.llvm_value );
	}
	else if( const ClassPtr* const class_type= boost::get<ClassPtr>( &variable.type.one_of_type_kind ) )
	{
		U_UNUSED(class_type);
		errors_.push_back( ReportNotImplemented( initializer.file_pos_, "constructors for classes" ) );
		return;
	}
	else
	{
		errors_.push_back( ReportConstructorInitializerForUnsupportedType( initializer.file_pos_ ) );
		return;
	}
}

void CodeBuilder::ApplyExpressionInitializer(
	const Variable& variable,
	const ExpressionInitializer& initializer,
	NamesScope& block_names,
	FunctionContext& function_context )
{
	if( const FundamentalType* const fundamental_type= boost::get<FundamentalType>( &variable.type.one_of_type_kind ) )
	{
		U_UNUSED(fundamental_type);

		const Value expression_result=
			BuildExpressionCode( *initializer.expression, block_names, function_context );
		if( expression_result.GetType() != variable.type )
		{
			errors_.push_back( ReportTypesMismatch( initializer.file_pos_, variable.type.ToString(), expression_result.GetType().ToString() ) );
			return;
		}

		llvm::Value* const value_for_assignment= CreateMoveToLLVMRegisterInstruction( *expression_result.GetVariable(), function_context );
		function_context.llvm_ir_builder.CreateStore( value_for_assignment, variable.llvm_value );
	}
	else
	{
		errors_.push_back( ReportNotImplemented( initializer.file_pos_, "expression initialization for nonfundamental types" ) );
		return;
	}
}

void CodeBuilder::ApplyZeroInitializer(
	const Variable& variable,
	const ZeroInitializer& initializer,
	NamesScope& block_names,
	FunctionContext& function_context )
{
	if( const FundamentalType* const fundamental_type= boost::get<FundamentalType>( &variable.type.one_of_type_kind ) )
	{
		llvm::Value* zero_value= nullptr;
		switch( fundamental_type->fundamental_type )
		{
		case U_FundamentalType::Bool:
			zero_value=
				llvm::Constant::getIntegerValue(
					fundamental_llvm_types_.bool_,
					llvm::APInt( 1u, uint64_t(0) ) );
			break;

		case U_FundamentalType::i8:
		case U_FundamentalType::u8:
		case U_FundamentalType::i16:
		case U_FundamentalType::u16:
		case U_FundamentalType::i32:
		case U_FundamentalType::u32:
		case U_FundamentalType::i64:
		case U_FundamentalType::u64:
			zero_value=
				llvm::Constant::getIntegerValue(
					GetFundamentalLLVMType( fundamental_type->fundamental_type ),
					llvm::APInt( variable.type.SizeOf() * 8u, uint64_t(0) ) );
			break;

		case U_FundamentalType::f32:
			zero_value= llvm::ConstantFP::get( fundamental_llvm_types_.f32, 0.0 );
			break;
		case U_FundamentalType::f64:
			zero_value= llvm::ConstantFP::get( fundamental_llvm_types_.f64, 0.0 );
			break;

		case U_FundamentalType::Void:
		case U_FundamentalType::InvalidType:
		case U_FundamentalType::LastType:
			U_ASSERT(false);
			break;
		};

		function_context.llvm_ir_builder.CreateStore( zero_value, variable.llvm_value );
	}
	else if( const ArrayPtr* const array_type_ptr= boost::get<ArrayPtr>( &variable.type.one_of_type_kind ) )
	{
		U_ASSERT( *array_type_ptr != nullptr );
		const Array& array_type= **array_type_ptr;

		Variable array_member= variable;
		array_member.type= array_type.type;
		array_member.location= Variable::Location::Pointer;

		// Make first index = 0 for array to pointer conversion.
		llvm::Value* index_list[2];
		index_list[0]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(0u) ) );

		if( array_type.size <= 8u )
		{
			for( size_t i= 0u; i < array_type.size; i++ )
			{
				index_list[1]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(i) ) );
				array_member.llvm_value=
					function_context.llvm_ir_builder.CreateGEP( variable.llvm_value, llvm::ArrayRef<llvm::Value*> ( index_list, 2u ) );

				ApplyZeroInitializer( array_member, initializer, block_names, function_context );
			}
		}
		else
		{
			// There are too many code for initializer.
			// Make initialization loop.
			llvm::Value* const zero_value=
				llvm::Constant::getIntegerValue( fundamental_llvm_types_.u32, llvm::APInt( 32u, uint64_t(0) ) );
			llvm::Value* const one_value=
				llvm::Constant::getIntegerValue( fundamental_llvm_types_.u32, llvm::APInt( 32u, uint64_t(1u) ) );
			llvm::Value* const loop_count_value=
				llvm::Constant::getIntegerValue( fundamental_llvm_types_.u32, llvm::APInt( 32u, uint64_t(array_type.size) ) );
			llvm::Value* const couter_address= function_context.alloca_ir_builder.CreateAlloca( fundamental_llvm_types_.u32 );
			function_context.llvm_ir_builder.CreateStore( zero_value, couter_address );

			llvm::BasicBlock* loop_block= llvm::BasicBlock::Create( llvm_context_ );
			llvm::BasicBlock* block_after_init_loop= llvm::BasicBlock::Create( llvm_context_ );

			function_context.llvm_ir_builder.CreateBr( loop_block );
			function_context.function->getBasicBlockList().push_back( loop_block );
			function_context.llvm_ir_builder.SetInsertPoint( loop_block );

			llvm::Value* const current_counter_value= function_context.llvm_ir_builder.CreateLoad( couter_address );
			index_list[1]= current_counter_value;
			array_member.llvm_value=
				function_context.llvm_ir_builder.CreateGEP( variable.llvm_value, llvm::ArrayRef<llvm::Value*>( index_list, 2u ) );
			ApplyZeroInitializer( array_member, initializer, block_names, function_context );

			llvm::Value* const counter_value_plus_one= function_context.llvm_ir_builder.CreateAdd( current_counter_value, one_value );
			function_context.llvm_ir_builder.CreateStore( counter_value_plus_one, couter_address );
			llvm::Value* const counter_test= function_context.llvm_ir_builder.CreateICmpULT( counter_value_plus_one, loop_count_value );
			function_context.llvm_ir_builder.CreateCondBr( counter_test, loop_block, block_after_init_loop );

			function_context.function->getBasicBlockList().push_back( block_after_init_loop );
			function_context.llvm_ir_builder.SetInsertPoint( block_after_init_loop );
		}
	}
	else if( const ClassPtr* const class_type_ptr = boost::get<ClassPtr>( &variable.type.one_of_type_kind ) )
	{
		// SPRACHE_TODO - disallow zero initializers for all except structs without constructors.

		U_ASSERT( *class_type_ptr != nullptr );
		const Class& class_type= **class_type_ptr;

		Variable struct_member= variable;
		struct_member.location= Variable::Location::Pointer;
		// Make first index = 0 for array to pointer conversion.
		llvm::Value* index_list[2];
		index_list[0]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(0u) ) );

		class_type.members.ForEachInThisScope(
			[&]( const NamesScope::InsertedName& member )
			{
				const ClassField* const field= member.second.GetClassField();
				if( field == nullptr )
					return;

				struct_member.type= field->type;
				index_list[1]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(field->index) ) );
				struct_member.llvm_value=
					function_context.llvm_ir_builder.CreateGEP( variable.llvm_value, llvm::ArrayRef<llvm::Value*> ( index_list, 2u ) );

				ApplyZeroInitializer( struct_member, initializer, block_names, function_context );
			});
	}
	else
	{
		// REPORT unsupported type for zero initializer
		return;
	}
}

} // namespace CodeBuilderPrivate

} // namespace U
