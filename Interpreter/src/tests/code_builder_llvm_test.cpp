#include <cstdlib>
#include <iostream>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/Interpreter.h>

#include "../code_builder_llvm.hpp"
#include "../lexical_analyzer.hpp"
#include "../syntax_analyzer.hpp"

#include "code_builder_llvm_test.hpp"

namespace Interpreter
{

static std::unique_ptr<llvm::Module> BuildProgram( const char* const text )
{
	const LexicalAnalysisResult lexical_analysis_result=
		LexicalAnalysis( ToProgramString( text ) );

	for( const std::string& lexical_error_message : lexical_analysis_result.error_messages )
		std::cout << lexical_error_message << "\n";
	U_ASSERT( lexical_analysis_result.error_messages.empty() );

	const SyntaxAnalysisResult syntax_analysis_result=
		SyntaxAnalysis( lexical_analysis_result.lexems );

	for( const std::string& syntax_error_message : syntax_analysis_result.error_messages )
		std::cout << syntax_error_message << "\n";
	U_ASSERT( syntax_analysis_result.error_messages.empty() );

	CodeBuilderLLVM::BuildResult build_result=
		CodeBuilderLLVM().BuildProgram( syntax_analysis_result.program_elements );

	U_ASSERT( build_result.error_messages.empty() );

	for( const std::string& error_message : build_result.error_messages )
		std::cout << error_message << "\n";

	return std::move( build_result.module );
}

static llvm::ExecutionEngine* CreateEngine(
	std::unique_ptr<llvm::Module> module, const bool needs_dump= false )
{
	U_ASSERT( module != nullptr );

	if( needs_dump )
		module->dump();

	llvm::EngineBuilder builder( std::move(module) );
	llvm::ExecutionEngine* const engine= builder.create();

	U_ASSERT( engine != nullptr );
	return engine;
}

static void SimpleProgramTest()
{
	static const char c_program_text[]=
	"\
	fn Foo( a : i32, b : i32, c : i32 ) : i32\
	{\
		return a + b + c;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	U_i32 arg0= 100500, arg1= 1488, arg2= 42;

	llvm::Function* function= engine->FindFunctionNamed( "Foo" );
	U_ASSERT( function != nullptr );

	llvm::GenericValue args[3];
	args[0].IntVal= llvm::APInt( 32, arg0 );
	args[1].IntVal= llvm::APInt( 32, arg1 );
	args[2].IntVal= llvm::APInt( 32, arg2 );

	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>( args, 3 ) );

	U_ASSERT( static_cast<uint64_t>( arg0 + arg1 + arg2 ) == result_value.IntVal.getLimitedValue() );
}

static void BasicBinaryOperationsTest()
{
	static const char c_program_text[]=
	"\
	fn Foo( a : i32, b : i32, c : i32 ) : i32\
	{\
		return a * a + b / b - c;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* function= engine->FindFunctionNamed( "Foo" );
	U_ASSERT( function != nullptr );

	int arg0= 77, arg1= 1488, arg2= 42;

	llvm::GenericValue args[3];
	args[0].IntVal= llvm::APInt( 32, arg0 );
	args[1].IntVal= llvm::APInt( 32, arg1 );
	args[2].IntVal= llvm::APInt( 32, arg2 );

	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>( args, 3 ) );

	U_ASSERT( static_cast<uint64_t>( arg0 * arg0 + arg1 / arg1 - arg2 ) == result_value.IntVal.getLimitedValue() );
}

static void VariablesTest()
{
	static const char c_program_text[]=
	"\
	fn Foo( a : i32, b : i32 ) : i32\
	{\
		let tmp : i32;\
		tmp = a - b;\
		return tmp;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* function= engine->FindFunctionNamed( "Foo" );
	U_ASSERT( function != nullptr );

	int arg0= 1488, arg1= 77;

	llvm::GenericValue args[2];
	args[0].IntVal= llvm::APInt( 32, arg0 );
	args[1].IntVal= llvm::APInt( 32, arg1 );

	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>( args, 2 ) );

	U_ASSERT( static_cast<uint64_t>( arg0 - arg1 ) == result_value.IntVal.getLimitedValue() );
}

static void NumericConstantsTest0()
{
	static const char c_program_text[]=
	"\
	fn Foo32( a : i32, b : i32 ) : i32\
	{\
		return a * 7 +  b - 22 / 4 + 458;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* function= engine->FindFunctionNamed( "Foo32" );
	U_ASSERT( function != nullptr );

	int arg0= 1488, arg1= 77;

	llvm::GenericValue args[2];
	args[0].IntVal= llvm::APInt( 32, arg0 );
	args[1].IntVal= llvm::APInt( 32, arg1 );

	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>( args, 2 ) );

	U_ASSERT( static_cast<uint64_t>( arg0 * 7 + arg1 - 22 / 4 + 458 ) == result_value.IntVal.getLimitedValue() );
}

static void NumericConstantsTest1()
{
	static const char c_program_text[]=
	"\
	fn Foo64() : i64\
	{\
		return 45783984055402i64;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* function= engine->FindFunctionNamed( "Foo64" );
	U_ASSERT( function != nullptr );


	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>() );

	U_ASSERT( static_cast<uint64_t>( 45783984055402ll ) == result_value.IntVal.getLimitedValue() );
}

static void UnaryMinusTest()
{
	static const char c_program_text[]=
	"\
	fn Foo( x : i32 ) : i32\
	{\
		let tmp : i32;\
		tmp= -x;\
		return -tmp;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* function= engine->FindFunctionNamed( "Foo" );
	U_ASSERT( function != nullptr );

	int arg_value= 54785;
	llvm::GenericValue arg;
	arg.IntVal= llvm::APInt( 32, arg_value );

	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>( arg ) );

	U_ASSERT( static_cast<uint64_t>( - - arg_value ) == result_value.IntVal.getLimitedValue() );
}

static void ArraysTest0()
{
	static const char c_program_text[]=
	"\
	fn Foo( x : i32 ) : i32\
	{\
		let tmp : [ i32, 17 ];\
		tmp[5u32]= x;\
		return tmp[5u32] + 5;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* function= engine->FindFunctionNamed( "Foo" );
	U_ASSERT( function != nullptr );

	int arg_value= 54785;
	llvm::GenericValue arg;
	arg.IntVal= llvm::APInt( 32, arg_value );

	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>( arg ) );

	U_ASSERT( static_cast<uint64_t>( arg_value + 5 ) == result_value.IntVal.getLimitedValue() );
}

static void ArraysTest1()
{
	static const char c_program_text[]=
	"\
	fn Foo( x : i32 ) : i32\
	{\
		let tmp : [ [ [ i32, 3 ], 5 ], 17 ];\
		tmp[5u32][3u32][1u32]= x;\
		return tmp[5u32][3u32][1u32] + 5;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* function= engine->FindFunctionNamed( "Foo" );
	U_ASSERT( function != nullptr );

	int arg_value= 54785;
	llvm::GenericValue arg;
	arg.IntVal= llvm::APInt( 32, arg_value );

	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>( arg ) );

	U_ASSERT( static_cast<uint64_t>( arg_value + 5 ) == result_value.IntVal.getLimitedValue() );
}

static void LogicalBinaryOperationsTest()
{
	static const char c_program_text[]=
	"\
	fn Foo( a : i32, b : i32, c : i32 ) : i32\
	{\
		return ( (a & b) ^ c ) + ( a | b | c );\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* function= engine->FindFunctionNamed( "Foo" );
	U_ASSERT( function != nullptr );

	int arg0= 77, arg1= 1488, arg2= 42;

	llvm::GenericValue args[3];
	args[0].IntVal= llvm::APInt( 32, arg0 );
	args[1].IntVal= llvm::APInt( 32, arg1 );
	args[2].IntVal= llvm::APInt( 32, arg2 );

	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>( args, 3 ) );

	U_ASSERT(
		static_cast<uint64_t>( ( (arg0 & arg1) ^ arg2 ) + ( arg0 | arg1 | arg2 ) ) ==
		result_value.IntVal.getLimitedValue() );
}

static void BooleanBasicTest()
{
	static const char c_program_text[]=
	"\
	fn Foo( a : bool, b : bool, c : bool ) : bool\
	{\
		let unused : bool;\
		unused= false;\
		let tmp : bool;\
		tmp = a & b;\
		tmp= tmp & ( true );\
		tmp= tmp | false;\
		return tmp ^ c ;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* function= engine->FindFunctionNamed( "Foo" );
	U_ASSERT( function != nullptr );

	bool arg0= true, arg1= false, arg2= true;

	llvm::GenericValue args[3];
	args[0].IntVal= llvm::APInt( 1, arg0 );
	args[1].IntVal= llvm::APInt( 1, arg1 );
	args[2].IntVal= llvm::APInt( 1, arg2 );

	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>( args, 3 ) );

	U_ASSERT(
		static_cast<uint64_t>(  ( arg0 & arg1 ) ^ arg2  ) ==
		result_value.IntVal.getLimitedValue() );
}

static void CallTest0()
{
	static const char c_program_text[]=
	"\
	fn Bar( x : i32 ) : i32 \
	{\
		return x * x + 42;\
	}\
	fn Foo( a : i32, b : i32 ) : i32\
	{\
		return Bar( a ) + Bar( b );\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* function= engine->FindFunctionNamed( "Foo" );
	U_ASSERT( function != nullptr );

	int arg0= 77, arg1= 1488;

	llvm::GenericValue args[2];
	args[0].IntVal= llvm::APInt( 32, arg0 );
	args[1].IntVal= llvm::APInt( 32, arg1 );

	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>( args, 3 ) );

	U_ASSERT(
		static_cast<uint64_t>( arg0 * arg0 + 42 + arg1 * arg1 + 42 ) ==
		result_value.IntVal.getLimitedValue() );
}

static void CallTest1()
{
	static const char c_program_text[]=
	"\
	fn Bar( x : i32 ) : void \
	{\
		x + x;\
		return;\
	}\
	fn FullyVoid() { return; }\
	fn Foo( a : i32, b : i32 ) : i32\
	{\
		Bar( a );\
		FullyVoid();\
		return a / b;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* function= engine->FindFunctionNamed( "Foo" );
	U_ASSERT( function != nullptr );

	int arg0= 775678, arg1= 1488;

	llvm::GenericValue args[2];
	args[0].IntVal= llvm::APInt( 32, arg0 );
	args[1].IntVal= llvm::APInt( 32, arg1 );

	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>( args, 3 ) );

	U_ASSERT(
		static_cast<uint64_t>( arg0 / arg1 ) ==
		result_value.IntVal.getLimitedValue() );
}

static void EqualityOperatorsTest()
{
	static const char c_program_text[]=
	"\
	fn Foo( a : i32, b : i32, c : i32 ) : bool\
	{\
		return ( a == b ) | ( a != c ) ;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* function= engine->FindFunctionNamed( "Foo" );
	U_ASSERT( function != nullptr );

	int arg0= 77, arg1= 1488, arg2= 42;

	llvm::GenericValue args[3];
	args[0].IntVal= llvm::APInt( 32, arg0 );
	args[1].IntVal= llvm::APInt( 32, arg1 );
	args[2].IntVal= llvm::APInt( 32, arg2 );

	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>( args, 3 ) );

	U_ASSERT(
		static_cast<uint64_t>( ( arg0 == arg1 ) | ( arg0 != arg2 ) ) ==
		result_value.IntVal.getLimitedValue() );
}

static void EqualityFloatOperatorsTest()
{
	static const char c_program_text[]=
	"\
	fn Foo( a : f32, b : f32, c : f32 ) : bool\
	{\
		return ( a == b ) | ( a != c );\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* function= engine->FindFunctionNamed( "Foo" );
	U_ASSERT( function != nullptr );

	float arg0= 77, arg1= 1488, arg2= 42;

	llvm::GenericValue args[3];
	args[0].FloatVal= arg0;
	args[1].FloatVal= arg1;
	args[2].FloatVal= arg2;

	llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>( args, 3 ) );

	U_ASSERT(
		static_cast<uint64_t>( ( arg0 == arg1 ) | ( arg0 != arg2 ) ) ==
		result_value.IntVal.getLimitedValue() );
}

static void ComparisonSignedOperatorsTest()
{
	static const char c_program_text[]=
	"\
	fn Less( a : i32, b : i32 ) : bool\
	{\
		return a < b;\
	}\
	fn LessOrEqual( a : i32, b : i32 ) : bool\
	{\
		return a <= b;\
	}\
	fn Greater( a : i32, b : i32 ) : bool\
	{\
		return a > b;\
	}\
	fn GreaterOrEqual( a : i32, b : i32 ) : bool\
	{\
		return a >= b;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	// 0 means a < b, 1 means a == b, 2 means a > b
	static const bool c_true_matrix[4][3]=
	{
		{ true , false, false }, // less
		{ true , true , false }, // less    or equal
		{ false, false, true  }, // greater
		{ false, true , true  }, // greater or equal
	};
	static const char* const c_func_names[4]=
	{
		"Less", "LessOrEqual", "Greater", "GreaterOrEqual",
	};
	for( unsigned int func_n= 0u; func_n < 4u; func_n++ )
	{
		llvm::Function* function= engine->FindFunctionNamed( c_func_names[ func_n ] );
		U_ASSERT( function != nullptr );

		llvm::GenericValue args[2];
		llvm::GenericValue result_value;

		// TODO - add more test-cases.

		// Less
		args[0].IntVal= llvm::APInt( 32, -1488 );
		args[1].IntVal= llvm::APInt( 32, 51478 );
		result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>( args, 2 ) );
		U_ASSERT( static_cast<uint64_t>( c_true_matrix[ func_n ][0] ) == result_value.IntVal.getLimitedValue() );

		// Equal
		args[0].IntVal= llvm::APInt( 32, 8596 );
		args[1].IntVal= llvm::APInt( 32, 8596 );
		result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>( args, 2 ) );
		U_ASSERT( static_cast<uint64_t>( c_true_matrix[ func_n ][1] ) == result_value.IntVal.getLimitedValue() );

		// Greater
		args[0].IntVal= llvm::APInt( 32, 6545284 );
		args[1].IntVal= llvm::APInt( 32, 6544 );
		result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>( args, 2 ) );
		U_ASSERT( static_cast<uint64_t>( c_true_matrix[ func_n ][2] ) == result_value.IntVal.getLimitedValue() );
	}
}

static void ComparisonUnsignedOperatorsTest()
{
	static const char c_program_text[]=
	"\
	fn Less( a : u32, b : u32 ) : bool\
	{\
		return a < b;\
	}\
	fn LessOrEqual( a : u32, b : u32 ) : bool\
	{\
		return a <= b;\
	}\
	fn Greater( a : u32, b : u32 ) : bool\
	{\
		return a > b;\
	}\
	fn GreaterOrEqual( a : u32, b : u32 ) : bool\
	{\
		return a >= b;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	// 0 means a < b, 1 means a == b, 2 means a > b
	static const bool c_true_matrix[4][3]=
	{
		{ true , false, false }, // less
		{ true , true , false }, // less    or equal
		{ false, false, true  }, // greater
		{ false, true , true  }, // greater or equal
	};
	static const char* const c_func_names[4]=
	{
		"Less", "LessOrEqual", "Greater", "GreaterOrEqual",
	};
	for( unsigned int func_n= 0u; func_n < 4u; func_n++ )
	{
		llvm::Function* function= engine->FindFunctionNamed( c_func_names[ func_n ] );
		U_ASSERT( function != nullptr );

		llvm::GenericValue args[2];
		llvm::GenericValue result_value;

		// TODO - add more test-cases.

		// Less
		args[0].IntVal= llvm::APInt( 32,  1488 );
		args[1].IntVal= llvm::APInt( 32, 51478 );
		result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>( args, 2 ) );
		U_ASSERT( static_cast<uint64_t>( c_true_matrix[ func_n ][0] ) == result_value.IntVal.getLimitedValue() );

		// Equal
		args[0].IntVal= llvm::APInt( 32, 8596 );
		args[1].IntVal= llvm::APInt( 32, 8596 );
		result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>( args, 2 ) );
		U_ASSERT( static_cast<uint64_t>( c_true_matrix[ func_n ][1] ) == result_value.IntVal.getLimitedValue() );

		// Greater
		args[0].IntVal= llvm::APInt( 32, 6545284 );
		args[1].IntVal= llvm::APInt( 32, 6544 );
		result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>( args, 2 ) );
		U_ASSERT( static_cast<uint64_t>( c_true_matrix[ func_n ][2] ) == result_value.IntVal.getLimitedValue() );
	}
}

static void ComparisonFloatOperatorsTest()
{
	static const char c_program_text[]=
	"\
	fn Less( a : f32, b : f32 ) : bool\
	{\
		return a < b;\
	}\
	fn LessOrEqual( a : f32, b : f32 ) : bool\
	{\
		return a <= b;\
	}\
	fn Greater( a : f32, b : f32 ) : bool\
	{\
		return a > b;\
	}\
	fn GreaterOrEqual( a : f32, b : f32 ) : bool\
	{\
		return a >= b;\
	}"
	;

	llvm::ExecutionEngine* const engine= CreateEngine( BuildProgram( c_program_text ) );

	// 0 means a < b, 1 means a == b, 2 means a > b
	static const bool c_true_matrix[4][3]=
	{
		{ true , false, false }, // less
		{ true , true , false }, // less    or equal
		{ false, false, true  }, // greater
		{ false, true , true  }, // greater or equal
	};
	static const char* const c_func_names[4]=
	{
		"Less", "LessOrEqual", "Greater", "GreaterOrEqual",
	};
	for( unsigned int func_n= 0u; func_n < 4u; func_n++ )
	{
		llvm::Function* function= engine->FindFunctionNamed( c_func_names[ func_n ] );
		U_ASSERT( function != nullptr );

		llvm::GenericValue args[2];
		llvm::GenericValue result_value;

		// TODO - add more test-cases.

		// Less
		args[0].FloatVal= -1488.0f;
		args[1].FloatVal=  51255.0f;
		result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>( args, 2 ) );
		U_ASSERT( static_cast<uint64_t>( c_true_matrix[ func_n ][0] ) == result_value.IntVal.getLimitedValue() );

		// Equal
		args[0].FloatVal= -0.0f;
		args[1].FloatVal= +0.0f;
		result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>( args, 2 ) );
		U_ASSERT( static_cast<uint64_t>( c_true_matrix[ func_n ][1] ) == result_value.IntVal.getLimitedValue() );

		// Greater
		args[0].FloatVal= 5482.3f;
		args[1].FloatVal= 785.2f;
		result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>( args, 2 ) );
		U_ASSERT( static_cast<uint64_t>( c_true_matrix[ func_n ][2] ) == result_value.IntVal.getLimitedValue() );
	}
}


void RunCodeBuilderLLVMTest()
{
	SimpleProgramTest();
	BasicBinaryOperationsTest();
	VariablesTest();
	NumericConstantsTest0();
	NumericConstantsTest1();
	UnaryMinusTest();
	ArraysTest0();
	ArraysTest1();
	LogicalBinaryOperationsTest();
	BooleanBasicTest();
	CallTest0();
	CallTest1();
	EqualityOperatorsTest();
	EqualityFloatOperatorsTest();
	ComparisonSignedOperatorsTest();
	ComparisonUnsignedOperatorsTest();
	ComparisonFloatOperatorsTest();
}

} // namespace Interpreter
