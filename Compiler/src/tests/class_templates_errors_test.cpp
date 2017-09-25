#include "tests.hpp"

namespace U
{

U_TEST( InvalidValueAsTemplateArgumentTest0 )
{
	static const char c_program_text[]=
	R"(
		fn Bar(){}
		template</ i32 x /> struct S</ x />{}
		fn Foo()
		{
			var S</ Bar /> s; // Overloaded functions set as template argument not allowed.
		}
	)";

	const CodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::InvalidValueAsTemplateArgument );
	U_TEST_ASSERT( error.file_pos.line == 6u );
}

U_TEST( InvalidTypeOfTemplateVariableArgumentTest0 )
{
	static const char c_program_text[]=
	R"(
		template</ i32 x /> struct S</ x />{}
		fn Foo()
		{
			var S</ 0.5f /> s; // float type template arguments forbidden.
		}
	)";

	const CodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::InvalidTypeOfTemplateVariableArgument );
	U_TEST_ASSERT( error.file_pos.line == 5u );
}

// TODO - InvalidTypeOfTemplateVariableArgument for arrays, structs.

U_TEST( ClassPrepass_ErrorsTest0 )
{
	static const char c_program_text[]=
	R"(
		template</ type T />
		class CC</ T />
		{
			fn Foo()
			{
				var i32 x= 0.25f; // all components are not template dependnent - generate error.
			}
		}
	)";

	const CodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::TypesMismatch );
	U_TEST_ASSERT( error.file_pos.line == 7u );
}

U_TEST( ClassPrepass_ErrorsTest1 )
{
	static const char c_program_text[]=
	R"(
		template</ type T />
		class CC</ T />
		{
			fn Foo()
			{
				var [ i32, 42 ] s{}; // struct initializer for non-struct.
			}
		}
	)";

	const CodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::StructInitializerForNonStruct );
	U_TEST_ASSERT( error.file_pos.line == 7u );
}

U_TEST( ClassPrepass_ErrorsTest2 )
{
	static const char c_program_text[]=
	R"(
		template</ type T />
		class CC</ T />
		{
			fn Foo()
			{
				var i32 something_mutable= 34;
				auto constexpr x= something_mutable;
			}
		}
	)";

	const CodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::VariableInitializerIsNotConstantExpression );
	U_TEST_ASSERT( error.file_pos.line == 8u );
}

U_TEST( DeclarationShadowsTemplateArgument_Test0 )
{
	static const char c_program_text[]=
	R"(
		template</ type T />
		class CC</ T />
		{
			i32 T; // class field
		}

		template</ type T />
		class DD</ T />
		{
			fn T(){} // function
			fn Bar()
			{
				{ var i32 T= 0; } // variable
				{ auto T= 0; } // auto-variable
			}
			fn Foo( f64 T ){} // function aargument
		}

		template</ type T />
		class EE</ T />
		{
			class T{} // class
		}

		template</ type T />
		class FF</ T />
		{
			// TODO - turn on this, when class templates inside classes will be supported.
			//template</ T size /> class T</ size />{} // class template argument
		}
	)";

	const CodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( build_result.errors.size() >= /* 7u */ 6u );

	U_TEST_ASSERT( build_result.errors[0].code == CodeBuilderErrorCode::DeclarationShadowsTemplateArgument );
	U_TEST_ASSERT( build_result.errors[0].file_pos.line == 5u );
	U_TEST_ASSERT( build_result.errors[1].code == CodeBuilderErrorCode::DeclarationShadowsTemplateArgument );
	U_TEST_ASSERT( build_result.errors[1].file_pos.line == 11u );
	U_TEST_ASSERT( build_result.errors[2].code == CodeBuilderErrorCode::DeclarationShadowsTemplateArgument );
	U_TEST_ASSERT( build_result.errors[2].file_pos.line == 14u );
	U_TEST_ASSERT( build_result.errors[3].code == CodeBuilderErrorCode::DeclarationShadowsTemplateArgument );
	U_TEST_ASSERT( build_result.errors[3].file_pos.line == 15u );
	U_TEST_ASSERT( build_result.errors[4].code == CodeBuilderErrorCode::DeclarationShadowsTemplateArgument );
	U_TEST_ASSERT( build_result.errors[4].file_pos.line == 17u );
	U_TEST_ASSERT( build_result.errors[5].code == CodeBuilderErrorCode::DeclarationShadowsTemplateArgument );
	U_TEST_ASSERT( build_result.errors[5].file_pos.line == 23u );
	//U_TEST_ASSERT( build_result.errors[6].code == CodeBuilderErrorCode::DeclarationShadowsTemplateArgument );
	//U_TEST_ASSERT( build_result.errors[6].file_pos.line == 29u );
}

U_TEST( NameNotFound_ForClassTemplateSingatureArguments_Test0 )
{
	static const char c_program_text[]=
	R"(
		template</  /> class CC</ SSSS /> {} // Name in signature argument not known yet.

		struct SSSS{}
	)";

	const CodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );


	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::NameNotFound );
	U_TEST_ASSERT( error.file_pos.line == 2u );
}

U_TEST( NameNotFound_ForClassTemplateArguments_Test0 )
{
	static const char c_program_text[]=
	R"(
		template</ SSS xx /> class CC</ xx /> {} // Name in type of value-argument not known yet.

		struct SSS{}
	)";

	const CodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );


	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::NameNotFound );
	U_TEST_ASSERT( error.file_pos.line == 2u );
}

U_TEST( ValueIsNotTemplateTest0 )
{
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			auto this_is_not_template= 0;
			var this_is_not_template</ i32 /> some_var;
		}
	)";

	const CodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );


	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ValueIsNotTemplate );
	U_TEST_ASSERT( error.file_pos.line == 5u );
}

U_TEST( TemplateInstantiationRequiredTest0 )
{
	static const char c_program_text[]=
	R"(
		template</ type T /> struct Box</ T /> { struct Tag{} T t; }

		fn Foo()
		{
			var Box::Tag x;
		}
	)";

	const CodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );


	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::TemplateInstantiationRequired );
	U_TEST_ASSERT( error.file_pos.line == 6u );
}

} // namespace U
