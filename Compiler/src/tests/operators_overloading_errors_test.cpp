#include "tests.hpp"

namespace U
{

U_TEST( OperatorDeclarationOutsideClass_Test )
{
	static const char c_program_text[]=
	R"(
		struct S{}
		op+( S s, f32 f ) : S {}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::OperatorDeclarationOutsideClass );
	U_TEST_ASSERT( error.file_pos.line == 3u );
}

U_TEST( OperatorDoesNotHaveParentClassArguments_Test0 )
{
	static const char c_program_text[]=
	R"(
		struct S
		{
			op+( i32 i, f32 f ) : i32 {}
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::OperatorDoesNotHaveParentClassArguments );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST( OperatorDoesNotHaveParentClassArguments_Test1 )
{
	static const char c_program_text[]=
	R"(
		struct S
		{
			op+( i32 i ) : bool {}
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::OperatorDoesNotHaveParentClassArguments );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST( InvalidArgumentCountForOperator_Test )
{
	static const char c_program_text[]=
	R"(
		struct S
		{
			op*( i32 x, S &imut s, f32 f ) : bool {} // 3 arguments, expected 2
			op=( S &mut s ) {} // 1 argument, expected 1
			op=( i32 x, S &imut s, f32 f ) {} // 2 arguments,e xpected 1
			op++( i32 x, S &imut s ){} // 2 arguments, expected 1
			op--( i32 x, S &imut s, f32 f ) {}
			op!( S &imut s, f32 x ) : bool {} //  2 arguments, expected 1
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( build_result.errors.size() >= 6u );

	U_TEST_ASSERT( build_result.errors[0].code == CodeBuilderErrorCode::InvalidArgumentCountForOperator );
	U_TEST_ASSERT( build_result.errors[0].file_pos.line == 4u );
	U_TEST_ASSERT( build_result.errors[1].code == CodeBuilderErrorCode::InvalidArgumentCountForOperator );
	U_TEST_ASSERT( build_result.errors[1].file_pos.line == 5u );
	U_TEST_ASSERT( build_result.errors[2].code == CodeBuilderErrorCode::InvalidArgumentCountForOperator );
	U_TEST_ASSERT( build_result.errors[2].file_pos.line == 6u );
	U_TEST_ASSERT( build_result.errors[3].code == CodeBuilderErrorCode::InvalidArgumentCountForOperator );
	U_TEST_ASSERT( build_result.errors[3].file_pos.line == 7u );
	U_TEST_ASSERT( build_result.errors[4].code == CodeBuilderErrorCode::InvalidArgumentCountForOperator );
	U_TEST_ASSERT( build_result.errors[4].file_pos.line == 8u );
	U_TEST_ASSERT( build_result.errors[5].code == CodeBuilderErrorCode::InvalidArgumentCountForOperator );
	U_TEST_ASSERT( build_result.errors[5].file_pos.line == 9u );
}

U_TEST( InvalidReturnTypeForOperator_Test )
{
	static const char c_program_text[]=
	R"(
		struct S
		{
			op=( S &mut dst, S &imut src ) : bool {} // Expected void
			op++( S &mut a ) : i32 {} // Expected void
			op/=( S &mut dst, S &imut src ) : S &mut {} // Expected void
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( build_result.errors.size() >= 3u );

	U_TEST_ASSERT( build_result.errors[0].code == CodeBuilderErrorCode::InvalidReturnTypeForOperator );
	U_TEST_ASSERT( build_result.errors[0].file_pos.line == 4u );
	U_TEST_ASSERT( build_result.errors[1].code == CodeBuilderErrorCode::InvalidReturnTypeForOperator );
	U_TEST_ASSERT( build_result.errors[1].file_pos.line == 5u );
	U_TEST_ASSERT( build_result.errors[2].code == CodeBuilderErrorCode::InvalidReturnTypeForOperator );
	U_TEST_ASSERT( build_result.errors[2].file_pos.line == 6u );
}

U_TEST( UsingIncompleteTypeForOperator )
{
	static const char c_program_text[]=
	R"(
		struct Baz;
		fn Foo( Baz &mut baz )
		{
			++baz;
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::UsingIncompleteType );
	U_TEST_ASSERT( error.file_pos.line == 5u );
}

U_TEST( IndexationOperatorHaveFirstArgumentOfNonparentClass )
{
	static const char c_program_text[]=
	R"(
		struct S
		{
			op[]( i32 i, S &imut s ){}
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::OperatorDoesNotHaveParentClassArguments ); // TODO - use separate error code for this case
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

} // namespace U