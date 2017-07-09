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
	if( initializer == nullptr )
	{
		if( boost::get<FundamentalType>( &variable.type.one_of_type_kind ) != nullptr )
		{
			// TODO -set file_pos
			errors_.push_back( ReportExpectedInitializer( FilePos() ) );
			throw ProgramError();
		}
		return;
	}

	if( const ArrayInitializer* const array_initializer=
		dynamic_cast<const ArrayInitializer*>(initializer) )
	{
		const ArrayPtr* const array_type_ptr= boost::get<ArrayPtr>( &variable.type.one_of_type_kind );
		if( array_type_ptr == nullptr )
		{
			// Array initializer for non-arrays
			throw ProgramError();
		}
		U_ASSERT( *array_type_ptr != nullptr );
		const Array& array_type= **array_type_ptr;

		if( array_initializer->initializers.size() != array_type.size )
		{
			// element count in initializer does not match array size.
			throw ProgramError();
			// SPRACHE_TODO - add array continious initializers.
		}

		Variable array_member= variable;
		array_member.type= array_type.type;
		array_member.location= Variable::Location::Pointer;

		// Make first index = 0 for array to pointer conversion.
		llvm::Value* index_list[2];
		index_list[0]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(0u) ) );

		for( size_t i= 0u; i < array_initializer->initializers.size(); i++ )
		{
			index_list[1]= llvm::Constant::getIntegerValue( fundamental_llvm_types_.i32, llvm::APInt( 32u, uint64_t(i) ) );
			array_member.llvm_value=
				function_context.llvm_ir_builder.CreateGEP( variable.llvm_value, llvm::ArrayRef<llvm::Value*> ( index_list, 2u ) );

			ApplyInitializer_r( array_member, array_initializer->initializers[i].get(), block_names, function_context );
		}
	}
	else if( const StructNamedInitializer* const struct_named_initializer=
		dynamic_cast<const StructNamedInitializer*>(initializer) )
	{
		// TODO
		U_UNUSED(struct_named_initializer);
	}
	else if( const ConstructorInitializer* const constructor_initializer=
		dynamic_cast<const ConstructorInitializer*>(initializer) )
	{
		if( const FundamentalType* const fundamental_type= boost::get<FundamentalType>( &variable.type.one_of_type_kind ) )
		{
			U_UNUSED(fundamental_type);

			if( constructor_initializer->call_operator.arguments_.size() != 1u )
			{
				// Fundamental types have constructors with exactly 1 argument
				throw ProgramError();
			}

			const Variable expression_result=
				BuildExpressionCode( *constructor_initializer->call_operator.arguments_.front(), block_names, function_context );
			if( expression_result.type != variable.type )
			{
				errors_.push_back( ReportTypesMismatch( constructor_initializer->file_pos_, variable.type.ToString(), expression_result.type.ToString() ) );
				throw ProgramError();
			}

			llvm::Value* const value_for_assignment= CreateMoveToLLVMRegisterInstruction( expression_result, function_context );
			function_context.llvm_ir_builder.CreateStore( value_for_assignment, variable.llvm_value );
		}
		else if( const ClassPtr* const class_type= boost::get<ClassPtr>( &variable.type.one_of_type_kind ) )
		{
			U_UNUSED(class_type);
			errors_.push_back( ReportNotImplemented( initializer->file_pos_, "constructors for classes" ) );
			throw ProgramError();
		}
		else
		{
			// Constructor initializer for type, that not supports constructor initialization.
			throw ProgramError();
		}
	}
	else if( const ExpressionInitializer* const expression_initializer=
		dynamic_cast<const ExpressionInitializer*>(initializer) )
	{
		if( const FundamentalType* const fundamental_type= boost::get<FundamentalType>( &variable.type.one_of_type_kind ) )
		{
			U_UNUSED(fundamental_type);

			const Variable expression_result=
				BuildExpressionCode( *expression_initializer->expression, block_names, function_context );
			if( expression_result.type != variable.type )
			{
				errors_.push_back( ReportTypesMismatch( expression_initializer->file_pos_, variable.type.ToString(), expression_result.type.ToString() ) );
				throw ProgramError();
			}

			llvm::Value* const value_for_assignment= CreateMoveToLLVMRegisterInstruction( expression_result, function_context );
			function_context.llvm_ir_builder.CreateStore( value_for_assignment, variable.llvm_value );
		}
		else
		{
			errors_.push_back( ReportNotImplemented( initializer->file_pos_, "expression initialization for nonfundamental types" ) );
			throw ProgramError();
		}
	}
}

} // namespace CodeBuilderPrivate

} // namespace U