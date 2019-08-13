#include <cstdlib>
#include <iostream>

#include "tests.hpp"

namespace U
{

U_TEST(ExpectedInitializerTest0)
{
	// Expected initializer for fundamental variable.
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var i32 x;
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ExpectedInitializer );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(ExpectedInitializerTest1)
{
	// Expected initializer for array.
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var [ i32, 1024 ] x;
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ExpectedInitializer );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(ExpectedInitializerTest2)
{
	// Expected initializer for struct.
	static const char c_program_text[]=
	R"(
		struct S{ i32 x; }
		fn Foo()
		{
			var S s;
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ExpectedInitializer );
	U_TEST_ASSERT( error.file_pos.line == 5u );
}

U_TEST(ArrayInitializerForNonArrayTest0)
{
	// Array initializer for fundamental type.
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var i32 x[ 5, 6, 7 ];
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ArrayInitializerForNonArray );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(ArrayInitializerForNonArrayTest1)
{
	// Array initializer for structes.
	static const char c_program_text[]=
	R"(
		struct C{}
		fn Foo()
		{
			var C x[ 5, 6, 7 ];
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ArrayInitializerForNonArray );
	U_TEST_ASSERT( error.file_pos.line == 5u );
}

U_TEST(ArrayInitializersCountMismatchTest0)
{
	// Not enough initializers.
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var [ i32, 3u32 ] x[ 1 ];
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ArrayInitializersCountMismatch );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(ArrayInitializersCountMismatchTest1)
{
	// Too much initializers.
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var [ i32, 3u32 ] x[ 1, 2, 3, 4, 5 ];
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ArrayInitializersCountMismatch );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}


U_TEST(FundamentalTypesHaveConstructorsWithExactlyOneParameterTest0)
{
	// Not enough parameters in constructor.
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var i32 x();
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::FundamentalTypesHaveConstructorsWithExactlyOneParameter );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(FundamentalTypesHaveConstructorsWithExactlyOneParameterTest1)
{
	// Too much parameters in constructor.
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var i32 x( 0, 1, 2 );
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::FundamentalTypesHaveConstructorsWithExactlyOneParameter );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(ReferencesHaveConstructorsWithExactlyOneParameterTest0)
{
	// Not enough parameters in constructor.
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var i32 & x();
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ReferencesHaveConstructorsWithExactlyOneParameter );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(ReferencesHaveConstructorsWithExactlyOneParameterTest1)
{
	// Too much parameters in constructor.
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var i32 z= 0;
			var i32 & x( z, z );
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ReferencesHaveConstructorsWithExactlyOneParameter );
	U_TEST_ASSERT( error.file_pos.line == 5u );
}

U_TEST(UnsupportedInitializerForReferenceTest0)
{
	// Array initializer for reference.
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var i32 z= 0;
			var i32 & x[ z ];
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::UnsupportedInitializerForReference );
	U_TEST_ASSERT( error.file_pos.line == 5u );
}

U_TEST(ConstructorInitializerForUnsupportedTypeTest0)
{
	// Constructor initializer for array.
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var [ i32, 2u32 ] x( 0, 1, 2 );
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ConstructorInitializerForUnsupportedType );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(StructInitializerForNonStructTest0)
{
	// Struct initializer for array.
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var [ i32, 2u32 ] x{};
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::StructInitializerForNonStruct );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(StructInitializerForNonStructTest1)
{
	// Struct initializer for class. For classes initialization only constructors must be used.
	static const char c_program_text[]=
	R"(
		class FF{}
		fn Foo()
		{
			var FF ff{};
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::StructInitializerForNonStruct );
	U_TEST_ASSERT( error.file_pos.line == 5u );
}

U_TEST(InitializerForNonfieldStructMemberTest0)
{
	// Struct initializer for array.
	static const char c_program_text[]=
	R"(
		struct S
		{
			i32 x;
			fn Foo( this ){}
		}
		fn Foo()
		{
			var S s{ .x= 0, .Foo= 0 };
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::InitializerForNonfieldStructMember );
	U_TEST_ASSERT( error.file_pos.line == 9u );
}

U_TEST(DuplicatedStructMemberInitializerTest0)
{
	static const char c_program_text[]=
	R"(
		struct Point{ i32 x; i32 y; }
		fn Foo()
		{
			var Point point{ .x= 42, .y= 34, .x= 0 };
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::DuplicatedStructMemberInitializer );
	U_TEST_ASSERT( error.file_pos.line == 5u );
}

U_TEST(DuplicatedStructMemberInitializerTest1)
{
	static const char c_program_text[]=
	R"(
		class A polymorph{}
		class B : A
		{
			fn constructor() ( base(), base() ) {}   // duplicated intitializer for base class
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::DuplicatedStructMemberInitializer );
	U_TEST_ASSERT( error.file_pos.line == 5u );
}

U_TEST(InitializerDisabledBecauseClassHaveExplicitNoncopyConstructorsTest0)
{
	// Struct named initializer for struct with constructor.
	static const char c_program_text[]=
	R"(
		struct S
		{
			i32 x;
			fn constructor( i32 a ) ( x=a ) {}
		}
		fn Foo()
		{
			var S point{ .x=42 };
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::InitializerDisabledBecauseClassHaveExplicitNoncopyConstructors );
	U_TEST_ASSERT( error.file_pos.line == 9u );
}

U_TEST(InitializerDisabledBecauseClassHaveExplicitNoncopyConstructorsTest1)
{
	// Zero-initializer for struct with copy-constructor.
	static const char c_program_text[]=
	R"(
		struct S
		{
			i32 x;
			fn constructor( i32 imut a ) ( x(a) ) {}
		}
		fn Foo()
		{
			var S point= zero_init;
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::InitializerDisabledBecauseClassHaveExplicitNoncopyConstructors );
	U_TEST_ASSERT( error.file_pos.line == 9u );
}

U_TEST( InitializerForInvalidType_Test0 )
{
	// Type is invalid, because name of type not found.
	// expression initializer.
	static const char c_program_text[]=
	R"(
		var unknown_type constexpr x= 0;
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::NameNotFound );
	U_TEST_ASSERT( error.file_pos.line == 2u );
}

U_TEST( InitializerForInvalidType_Test1 )
{
	// Type is invalid, because name of type not found.
	// constructor initializer.
	static const char c_program_text[]=
	R"(
		var unknown_type constexpr x(0);
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::NameNotFound );
	U_TEST_ASSERT( error.file_pos.line == 2u );
}

U_TEST( InitializerForInvalidType_Test2 )
{
	// Type is invalid, because name of type not found.
	// struct initializer.
	static const char c_program_text[]=
	R"(
		var unknown_type constexpr x{};
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::NameNotFound );
	U_TEST_ASSERT( error.file_pos.line == 2u );
}

U_TEST( InitializerForInvalidType_Test3 )
{
	// Type is invalid, because name of type not found.
	// zero initializer.
	static const char c_program_text[]=
	R"(
		var unknown_type constexpr x= zero_init;
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::NameNotFound );
	U_TEST_ASSERT( error.file_pos.line == 2u );
}

U_TEST( InitializerForInvalidType_Test4 )
{
	// Type is invalid, because name of type not found.
	// array initializer.
	static const char c_program_text[]=
	R"(
		var [ unknown_type, 2 ] constexpr x[];
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::NameNotFound );
	U_TEST_ASSERT( error.file_pos.line == 2u );
}

U_TEST(ZeroInitializerForClass_Test0)
{
	// Struct initializer for class. For classes initialization only constructors must be used.
	static const char c_program_text[]=
	R"(
		class FF{}
		fn Foo()
		{
			var FF ff= zero_init;
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ZeroInitializerForClass );
	U_TEST_ASSERT( error.file_pos.line == 5u );
}


U_TEST(TuplesInitializersErrors_Test0)
{
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var tup( i32, bool ) t;
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ExpectedInitializer );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(TuplesInitializersErrors_Test1)
{
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var tup( f32, f64, i64 ) t();
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::ExpectedInitializer );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(TuplesInitializersErrors_Test2)
{
	static const char c_program_text[]=
	R"(
		struct S
		{
			fn constructor(){}
		}
		fn Foo()
		{
			var tup( f32, S, i64 ) t= zero_init;
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::InitializerDisabledBecauseClassHaveExplicitNoncopyConstructors );
	U_TEST_ASSERT( error.file_pos.line == 8u );
}

U_TEST(TuplesInitializersErrors_Test3)
{
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var tup( f32, bool, i64 ) t( 0.5f );
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::TupleInitializersCountMismatch );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(TuplesInitializersErrors_Test4)
{
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var tup( f32, bool, i64 ) t( 0.5f, true );
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::TupleInitializersCountMismatch );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(TuplesInitializersErrors_Test5)
{
	static const char c_program_text[]=
	R"(
		fn Foo()
		{
			var tup( f32, bool ) t( 0.5f, true, 666 );
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::TupleInitializersCountMismatch );
	U_TEST_ASSERT( error.file_pos.line == 4u );
}

U_TEST(TuplesInitializersErrors_Test6)
{
	static const char c_program_text[]=
	R"(
		struct S
		{
			fn constructor( mut this, S&imut other )= delete;
		}
		fn Foo()
		{
			var tup( f32, S ) t= zero_init;
			var tup( f32, S ) t_copy(t); // Can not copy tuple, because tuple element "struct S" is not copyable.
		}
	)";

	const ICodeBuilder::BuildResult build_result= BuildProgramWithErrors( c_program_text );

	U_TEST_ASSERT( !build_result.errors.empty() );
	const CodeBuilderError& error= build_result.errors.front();

	U_TEST_ASSERT( error.code == CodeBuilderErrorCode::OperationNotSupportedForThisType );
	U_TEST_ASSERT( error.file_pos.line == 9u );
}

} // namespace U
