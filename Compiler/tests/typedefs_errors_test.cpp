#include "tests.hpp"

namespace U
{

U_TEST( NameNotFound_ForTypedef_Test0 )
{
	static const char c_program_text[]=
	R"(
		type T= wtf;
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::NameNotFound );
	U_TEST_ASSERT( error.file_pos.line == 2u );
}

U_TEST( NameNotFound_ForTypedef_Test1 )
{
	static const char c_program_text[]=
	R"(
		type T= [ wtf, 4 ];
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::NameNotFound );
	U_TEST_ASSERT( error.file_pos.line == 2u );
}

U_TEST( NameIsNotTypeName_ForTypedef_Test0 )
{
	static const char c_program_text[]=
	R"(
		namespace SSS{}
		type T= SSS;
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::NameIsNotTypeName );
	U_TEST_ASSERT( error.file_pos.line == 3u );
}

U_TEST( NameIsNotTypeName_ForTypedef_Test1 )
{
	static const char c_program_text[]=
	R"(
		fn Foo();
		type T= Foo;
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::NameIsNotTypeName );
	U_TEST_ASSERT( error.file_pos.line == 3u );
}

U_TEST( Redefinition_ForTypedef_Test0 )
{
	static const char c_program_text[]=
	R"(
		struct SSS{}
		type SSS= i32;
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::Redefinition );
	U_TEST_ASSERT( error.file_pos.line == 3u );
}

U_TEST( Redefinition_ForTypedef_Test1 )
{
	static const char c_program_text[]=
	R"(
		fn Foo(){}
		type Foo= bool;
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::Redefinition );
	U_TEST_ASSERT( error.file_pos.line == 3u );
}

U_TEST( NameNotFound_ForTypedefTemplate_Test0 )
{
	static const char c_program_text[]=
	R"(
		template</ type T />
		type Box</ T />= Can</ T />;    // "Can" - unknown
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::NameNotFound );
	U_TEST_ASSERT( error.file_pos.line == 3u );
}

U_TEST( NameNotFound_ForTypedefTemplate_Test1 )
{
	static const char c_program_text[]=
	R"(
		template</ type T />
		type Unbox</ Box</ T /> />= T;    // "Box" - unknown
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::NameNotFound );
	U_TEST_ASSERT( error.file_pos.line == 3 );
}

U_TEST( TemplateArgumentNotUsedInSignature_ForTypedefTemplate_Test0 )
{
	static const char c_program_text[]=
	R"(
		template</ type T />
		type Box</ />= i32;
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::TemplateArgumentNotUsedInSignature );
	U_TEST_ASSERT( error.file_pos.line == 2u );
}

} // namespace U
