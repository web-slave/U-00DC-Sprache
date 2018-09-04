#include <llvm/Support/DynamicLibrary.h>

#include "tests.hpp"

namespace U
{

static std::vector<int> g_destructors_call_sequence;

static llvm::GenericValue DestructorCalled(
	llvm::FunctionType*,
	llvm::ArrayRef<llvm::GenericValue> args )
{
	g_destructors_call_sequence.push_back( static_cast<int>(args[0].IntVal.getLimitedValue()) );
	return llvm::GenericValue();
}

static void DestructorTestPrepare()
{
	g_destructors_call_sequence.clear();

	// "lle_X_" - common prefix for all external functions, called from LLVM Interpreter
	llvm::sys::DynamicLibrary::AddSymbol( "lle_X__Z16DestructorCalledi", reinterpret_cast<void*>( &DestructorCalled ) );
}

U_TEST(DestructorsTest0)
{
	DestructorTestPrepare();

	// Must call destructor for variable.
	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor() ( x= 0 ) {}
			fn destructor()
			{
				x= 854;
				DestructorCalled(x);
			}
		}
		fn Foo()
		{
			var S s;
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction(
		function,
		llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( g_destructors_call_sequence.size() == 1u && g_destructors_call_sequence.front() == 854 );
}

U_TEST(DestructorsTest1)
{
	DestructorTestPrepare();

	// Must call destructors in reverse order.
	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor( i32 in_x ) ( x= in_x ) {}
			fn destructor()
			{
				DestructorCalled(x);
			}
		}
		fn Foo()
		{
			var S s0(0), s1(1), s2(2);
			var S imut s3(3);
			{ var S s4(4); } // s4 must be destructed before s5
			var S s5(5);
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction(
		function,
		llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( g_destructors_call_sequence == std::vector<int>( { 4, 5, 3, 2, 1, 0 } ) );
}

U_TEST(DestructorsTest2)
{
	DestructorTestPrepare();

	// Destructors for arguments.
	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor( i32 in_x ) ( x= in_x ) {}
			fn destructor()
			{
				DestructorCalled(x);
			}
		}
		fn Bar( S arg0, S arg1 )
		{
			var S s_local(0);
		}
		fn Foo()
		{
			Bar( S(88), S(66) );
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction(
		function,
		llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( g_destructors_call_sequence == std::vector<int>( { 0,  66, 88 } ) );
}

U_TEST(DestructorsTest3)
{
	DestructorTestPrepare();

	// Must call destructors after return.
	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor( i32 in_x ) ( x= in_x ) {}
			fn destructor() { DestructorCalled(x); }
		}
		fn Bar( bool b )
		{
			var S s0(0);
			if( b )
			{
				var S s1(1);
				return;
			}
			var S s2(2);
		}
		fn Foo()
		{
			Bar(false);
			Bar(true);
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction(
		function,
		llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_destructors_call_sequence ==
		std::vector<int>( { 2, 0,    1, 0 } ) );
}

U_TEST(DestructorsTest4)
{
	DestructorTestPrepare();

	// Must call destructors after return.
	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor( i32 in_x ) ( x= in_x ) {}
			fn destructor() { DestructorCalled(x); }
		}
		fn Bar( bool b )
		{
			var S s0(0);
			if( b )
			{
				var S s1(1);
				return;
			}
			else
			{
				return;
			}
			// Here must be no destructors code.
		}
		fn Foo()
		{
			Bar(false);
			Bar(true);
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction(
		function,
		llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_destructors_call_sequence ==
		std::vector<int>( { 0,    1, 0 } ) );
}

U_TEST(DestructorsTest5)
{
	DestructorTestPrepare();

	// Must call destructors of arguments in each return.
	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor( i32 in_x ) ( x= in_x ) {}
			fn destructor() { DestructorCalled(x); }
		}
		fn Bar( S s )
		{
			if( s.x == 0 ) { return; }
			if( s.x == 1 ) {{{ return; }}}
			if( s.x == 2 ) { return; }
			{ var S new_local(555); }
		}
		fn Foo()
		{
			Bar(S(0));
			Bar(S(1));
			Bar(S(2));
			Bar(S(3));
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction(
		function,
		llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_destructors_call_sequence ==
		std::vector<int>( { 0,  1,  2,  555, 3, } ) );
}

U_TEST(DestructorsTest6)
{
	DestructorTestPrepare();

	// Must call destructors of loop local variables before "break".
	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor( i32 in_x ) ( x= in_x ) {}
			fn destructor() { DestructorCalled(x); }
		}
		fn Foo()
		{
			var S live_longer_than_loop(-1);
			while(true)
			{
				var S s0(0);
				{
					var S s1(1), s2(2);
					var S s3(3);
					if( true )
					{
						var S s4(4); // must destroy s4, s3, s2, s1, s0
						break;
					}
				}
			}

			while(true)
			{
				var S s5(5);
				break;
			}
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction(
		function,
		llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_destructors_call_sequence ==
		std::vector<int>( { 4, 3, 2, 1, 0,  5,  -1 } ) );
}

U_TEST(DestructorsTest7)
{
	DestructorTestPrepare();

	// Must call destructors of inner loop, but not call destructors of outer loop before "break".
	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor( i32 in_x ) ( x= in_x ) {}
			fn destructor() { DestructorCalled(x); }
		}
		fn Foo()
		{
			var S s0(0);
			while(true)
			{
				var S s1(1), s2(2);
				var S s3(3);
				while(true)
				{
					var S s4(4);
					if( true )
					{
						var S s5(5), s6(6);
						break;
					}
				}
				var S s7(7);
				break;
			}
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction(
		function,
		llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_destructors_call_sequence ==
		std::vector<int>( { 6, 5, 4, 7, 3, 2, 1, 0 } ) );
}

U_TEST(DestructorsTest8)
{
	DestructorTestPrepare();

	// Explicit destructor must contains implicit calls to destructors of members.
	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor( i32 in_x ) ( x= in_x ) {}
			fn destructor() { DestructorCalled(x); }
		}
		class T
		{
			S s; i32 y;
			fn constructor( i32 x, i32 in_y ) ( s(x), y(in_y) ) {}
			fn destructor()
			{
				DestructorCalled(y);
			}
		}
		fn Foo()
		{
			var T t(111, 666);
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction(
		function,
		llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_destructors_call_sequence ==
		std::vector<int>( { 666, 111} ) );
}

U_TEST(DestructorsTest9)
{
	DestructorTestPrepare();

	// Explicit destructor must contains implicit calls to destructors of members.
	// Members destructors must be called in all return ways.
	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor( i32 in_x ) ( x= in_x ) {}
			fn destructor() { DestructorCalled(x); }
		}
		class T
		{
			S s; i32 y;
			fn constructor( i32 x, i32 in_y ) ( s(x), y(in_y) ) {}
			fn destructor()
			{
				if( ( y & 1 ) != 0 )
				{
					DestructorCalled(y);
					return;
				}
				else
				{
					DestructorCalled( -y );
				}
			}
		}
		fn Foo()
		{
			var T t0(111, 666);
			var T t1(500, 999);
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction(
		function,
		llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_destructors_call_sequence ==
		std::vector<int>( { 999, 500, -666, 111 } ) );
}

U_TEST(DestructorsTest10)
{
	DestructorTestPrepare();

	// Must call generated destructor.
	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor( i32 in_x ) ( x= in_x ) {}
			fn destructor() { DestructorCalled(x); }
		}
		class SWrapper
		{
			S s;
			fn constructor( i32 x ) ( s(x) ) {}
			// This class must have generated destructor, that calls destructor for members.
		}
		fn Foo()
		{
			var SWrapper s_wrapper(14789325);
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction(
		function,
		llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_destructors_call_sequence ==
		std::vector<int>( { 14789325 } ) );
}

U_TEST(DestructorsTest11)
{
	DestructorTestPrepare();

	// Destructors for temporaries must be called.
	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		struct S
		{
			i32 x;
			fn constructor( i32 in_x ) ( x= in_x ) {}
			fn destructor() { DestructorCalled(x); }
		}
		fn Bar( i32 x ) : S
		{
			return S(x);
		}
		fn Baz( S s ){}
		fn Fuz( S &imut s ){}
		fn Foo()
		{
			var i32 x= S(0).x + S(1).x; // Must destroy S(0) and S(1)
			var bool y= S(2).x == 0 || S(3).x == 0; // Must destroy both S(3) and S(2)
			var bool z= S(4).x == 0 && S(5).x == 0; // Must destroy only S(4)

			var [ i32, 2 ] mut arr= zero_init;
			// Type conversion to u32 must destroy temporary variable of class type.
			arr[ u32( S(6).x / 10 ) ]= S(7).x;  // Must destroy index and temporary in right part
			arr[ u32( S(8).x / 10 ) ]+= S(9).x;  // Must destroy index and temporary in right part

			auto i= Bar(10).x; // Must destroy returned from function result and move variable inside function.
			Baz( S(11) ); // Must destroy argument in function, and move temporary variable.
			Fuz( S(12) ); // Bind value to immutable reference parameter. Must destroy only temporary.

			var S nontemporary(13);
			Baz( nontemporary ); // Value copied to argument. Must call destructor for argument.

			S(14); // Simple expression - must destroy temporary.
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction(
		function,
		llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_destructors_call_sequence ==
		std::vector<int>( { 1, 0,   3, 2,   4,   6, 7,   8, 9,   10,  11,   12,   13,   14,   13 } ) );
}

U_TEST( DestructorsTest12_ShouldCorrectlyReturnValueFromDestructibleStruct )
{
	static const char c_program_text[]=
	R"(
		struct S
		{
			i32 x;
			fn destructor()
			{
				x= 0;
			}
		}

		fn Foo() : i32
		{
			var S s{ .x= 55841 };
			// Destructor set "x" to zero. We must read "x" before destructor call.
			return s.x;
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value=
		engine->runFunction(
			function,
			llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( static_cast<uint64_t>( 55841 ) == result_value.IntVal.getLimitedValue() );
}

U_TEST( DestructorsTest13_ShouldBeDesdtroyedAfterUsage0 )
{
	static const char c_program_text[]=
	R"(
		struct S
		{
			i32 x;
			fn constructor( i32 in_x )
			( x= in_x ) {}
			fn destructor()
			{
				x= 0;
			}
		}
		struct T{ i32 x; }

		fn Foo() : i32
		{
			var T t { .x= S(124586).x }; // Must destroy temporary inner variable after initialization via expression-initializer.
			return t.x;
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );
	U_TEST_ASSERT( static_cast<uint64_t>(124586) == result_value.IntVal.getLimitedValue() );
}

U_TEST( DestructorsTest14_ShouldBeDesdtroyedAfterUsage1 )
{
	static const char c_program_text[]=
	R"(
		struct S
		{
			i32 x;
			fn constructor( i32 in_x )
			( x= in_x ) {}
			fn destructor()
			{
				x= 0;
			}
		}
		struct T{ i32 x; }

		fn Foo() : i32
		{
			var T t { .x( S(4536758).x ) }; // Must destroy temporary inner variable after initialization via constructor-initializer.
			return t.x;
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );
	U_TEST_ASSERT( static_cast<uint64_t>(4536758) == result_value.IntVal.getLimitedValue() );
}

U_TEST( DestructorsTest15_ShouldBeDesdtroyedAfterUsage2 )
{
	static const char c_program_text[]=
	R"(
		struct S
		{
			i32 x;
			fn constructor( i32 in_x )
			( x= in_x ) {}
			fn destructor()
			{
				x= 0;
			}
		}

		fn Foo() : i32
		{
			var [ i32, 1 ] t[ S(985624).x ]; // Must destroy temporary inner variable after initialization of array member via expression-initializer.
			return t[0u];
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );
	U_TEST_ASSERT( static_cast<uint64_t>(985624) == result_value.IntVal.getLimitedValue() );
}

U_TEST( DestructorsTest16_ShouldBeDesdtroyedAfterUsage3 )
{
	static const char c_program_text[]=
	R"(
		struct B
		{
			bool b;
			fn constructor() ( b= true ) {}
			fn destructor() { b= false; }
		}

		fn Foo() : bool
		{
			auto r= B().b && B().b; // Must destroy both temp variables after evaluation of && or ||.
			return r; // Must return true.
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );
	U_TEST_ASSERT( static_cast<uint64_t>(1) == result_value.IntVal.getLimitedValue() );
}

U_TEST( DestructorsTest17_ShouldBeDesdtroyedAfterUsage4 )
{
	static const char c_program_text[]=
	R"(
		struct B
		{
			bool b;
			fn constructor() ( b= true ) {}
			fn destructor() { b= false; }
		}

		fn Foo() : i32
		{
			if( B().b ) // Temporary variable must be destroyed after evaluation of condition.
			{ return 5245; } // Must return in this branch of 'if'.
			return 123475;
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );
	U_TEST_ASSERT( static_cast<uint64_t>(5245) == result_value.IntVal.getLimitedValue() );
}

U_TEST( DestructorsTest18_ShouldBeDesdtroyedAfterUsage5 )
{
	static const char c_program_text[]=
	R"(
		struct B
		{
			bool b;
			fn constructor() ( b= true ) {}
			fn destructor() { b= false; }
		}

		fn Foo() : i32
		{
			while( B().b ) // Temporary variable must be destroyed after evaluation of condition.
			{ return 7698577; } // Must return in 'while'.
			return 9641;
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );
	U_TEST_ASSERT( static_cast<uint64_t>(7698577) == result_value.IntVal.getLimitedValue() );
}

U_TEST(DestructorsTest19_DestuctorForInterface)
{
	DestructorTestPrepare();

	// Must call destructor for of interface-parent.
	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);
		class A interface
		{
			fn destructor()
			{
				DestructorCalled( 5558414 );
			}
		}
		class B : A{}
		fn Foo()
		{
			var B b;
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( g_destructors_call_sequence.size() == 1u && g_destructors_call_sequence.front() == 5558414 );
}

U_TEST(DestructorsTest20_EarlyDestructorCallUsingMoveOperator)
{
	DestructorTestPrepare();

	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor( i32 in_x ) ( x= in_x ) {}
			fn destructor() { DestructorCalled(x); }
		}
		fn Foo()
		{
			var S mut s0( 99985 );
			move(s0); // Must call destructor here.
			var S s1( 8852 );
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( g_destructors_call_sequence == std::vector<int>( { 99985, 8852 } ) );
}

U_TEST(DestructorsTest21_ChangeDestructionOrderUsingMoveOperator)
{
	DestructorTestPrepare();

	static const char c_program_text[]=
	R"(
		fn DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor( i32 in_x ) ( x= in_x ) {}
			fn destructor() { DestructorCalled(x); }
		}
		fn Foo()
		{
			var S mut s0( 111 );
			var S s1( 222 );
			{
				var S s2= move(s0);
				var S s3( 333 );
				// Must destroy 333, 111 here.
			}
			// Must destroy 222 here.
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( g_destructors_call_sequence == std::vector<int>( { 333, 111, 222 } ) );
}

} // namespace U