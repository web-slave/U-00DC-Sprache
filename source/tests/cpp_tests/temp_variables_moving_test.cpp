#include <llvm/Support/DynamicLibrary.h>
#include "tests.hpp"

namespace U
{

static std::vector<int> g_constructirs_call_sequence;
static std::vector<int>  g_destructors_call_sequence;

static llvm::GenericValue ConstructorCalled(
	llvm::FunctionType*,
	llvm::ArrayRef<llvm::GenericValue> args )
{
	g_constructirs_call_sequence.push_back( static_cast<int>(args[0].IntVal.getLimitedValue()) );
	return llvm::GenericValue();
}

static llvm::GenericValue DestructorCalled(
	llvm::FunctionType*,
	llvm::ArrayRef<llvm::GenericValue> args )
{
	g_destructors_call_sequence.push_back( static_cast<int>(args[0].IntVal.getLimitedValue()) );
	return llvm::GenericValue();
}

static void TestPrepare()
{
	g_constructirs_call_sequence.clear();
	 g_destructors_call_sequence.clear();

	// "lle_X_" - common prefix for all external functions, called from LLVM Interpreter
	llvm::sys::DynamicLibrary::AddSymbol( "lle_X__Z17ConstructorCalledi", reinterpret_cast<void*>( &ConstructorCalled ) );
	llvm::sys::DynamicLibrary::AddSymbol(  "lle_X__Z16DestructorCalledi", reinterpret_cast<void*>( & DestructorCalled ) );
}

U_TEST(TempVariablesMovingTest0_MoveTempVariableToArgument)
{
	TestPrepare();

	static const char c_program_text[]=
	R"(
		fn ConstructorCalled(i32 x);
		fn  DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor()
				( x= 0 )
			{
				ConstructorCalled(x);
			}
			fn constructor( i32 in_x )
				( x= in_x )
			{
				ConstructorCalled(x);
			}
			fn constructor( S& imut other )
				( x= other.x )
			{
				ConstructorCalled(x);
			}
			fn destructor()
			{
				DestructorCalled(x);
			}
		}

		fn Bar( S s ){}

		fn Foo()
		{
			Bar( S(45) ); // Must move temporary variable to argument.
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_constructirs_call_sequence == std::vector<int>( { 45 } ) &&
		 g_destructors_call_sequence == std::vector<int>( { 45 } ) );
}

U_TEST(TempVariablesMovingTest1_MoveTempVariableToReturnValue)
{
	TestPrepare();

	static const char c_program_text[]=
	R"(
		fn ConstructorCalled(i32 x);
		fn  DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor()
				( x= 0 )
			{
				ConstructorCalled(x);
			}
			fn constructor( i32 in_x )
				( x= in_x )
			{
				ConstructorCalled(x);
			}
			fn constructor( S& imut other )
				( x= other.x )
			{
				ConstructorCalled(x);
			}
			fn destructor()
			{
				DestructorCalled(x);
			}
		}

		fn Bar() : S
		{
			return S(58424); // Must move temporary variable to result, not copy it.
		}

		fn Foo()
		{
			Bar();
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_constructirs_call_sequence == std::vector<int>( { 58424 } ) &&
		 g_destructors_call_sequence == std::vector<int>( { 58424 } ) );
}

U_TEST(TempVariablesMovingTest2_MoveTempVariableInExpressionInitialization)
{
	TestPrepare();

	static const char c_program_text[]=
	R"(
		fn ConstructorCalled(i32 x);
		fn  DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor()
				( x= 0 )
			{
				ConstructorCalled(x);
			}
			fn constructor( i32 in_x )
				( x= in_x )
			{
				ConstructorCalled(x);
			}
			fn constructor( S& imut other )
				( x= other.x )
			{
				ConstructorCalled(x);
			}
			fn destructor()
			{
				DestructorCalled(x);
			}
		}

		fn Foo()
		{
			var S s= S(66635); // Must move to initialized variable temp varaible.
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_constructirs_call_sequence == std::vector<int>( { 66635 } ) &&
		 g_destructors_call_sequence == std::vector<int>( { 66635 } ) );
}

U_TEST(TempVariablesMovingTest3_MoveTempVariableInAutoVariableInitialization)
{
	TestPrepare();

	static const char c_program_text[]=
	R"(
		fn ConstructorCalled(i32 x);
		fn  DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor()
				( x= 0 )
			{
				ConstructorCalled(x);
			}
			fn constructor( i32 in_x )
				( x= in_x )
			{
				ConstructorCalled(x);
			}
			fn constructor( S& imut other )
				( x= other.x )
			{
				ConstructorCalled(x);
			}
			fn destructor()
			{
				DestructorCalled(x);
			}
		}

		fn Foo()
		{
			auto s= S(11245678); // Must move to initialized variable temp varaible.
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_constructirs_call_sequence == std::vector<int>( { 11245678 } ) &&
		 g_destructors_call_sequence == std::vector<int>( { 11245678 } ) );
}

U_TEST(TempVariablesMovingTest4_MoveFunctionResult)
{
	TestPrepare();

	static const char c_program_text[]=
	R"(
		fn ConstructorCalled(i32 x);
		fn  DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor()
				( x= 0 )
			{
				ConstructorCalled(x);
			}
			fn constructor( i32 in_x )
				( x= in_x )
			{
				ConstructorCalled(x);
			}
			fn constructor( S& imut other )
				( x= other.x )
			{
				ConstructorCalled(x);
			}
			fn destructor()
			{
				DestructorCalled(x);
			}
		}

		fn Bar( i32 x ) : S
		{
			return S(x);
		}

		fn Foo()
		{
			auto s= Bar(88542); // Must move to initialized variable function result.
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_constructirs_call_sequence == std::vector<int>( { 88542 } ) &&
		 g_destructors_call_sequence == std::vector<int>( { 88542 } ) );
}

U_TEST(TempVariablesMovingTest5_MoveInAssignment)
{
	TestPrepare();

	static const char c_program_text[]=
	R"(
		fn ConstructorCalled(i32 x);
		fn  DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor()
				( x= 0 )
			{
				ConstructorCalled(x);
			}
			fn constructor( i32 in_x )
				( x= in_x )
			{
				ConstructorCalled(x);
			}
			fn constructor( S& imut other )
				( x= other.x )
			{
				ConstructorCalled(x);
			}
			fn destructor()
			{
				DestructorCalled(x);
			}
		}

		fn Foo()
		{
			var S mut s0(51254);
			s0= S(33241); // Must here call constructor for temp variable, call destructor for 's0' and move temp variable to 's0'.
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_constructirs_call_sequence == std::vector<int>( { 51254, 33241 } ) &&
		 g_destructors_call_sequence == std::vector<int>( { 51254, 33241 } ) );
}

U_TEST(TempVariablesMovingTest6_MoveInConstructorInitializer)
{
	TestPrepare();

	static const char c_program_text[]=
	R"(
		fn ConstructorCalled(i32 x);
		fn  DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor()
				( x= 0 )
			{
				ConstructorCalled(x);
			}
			fn constructor( i32 in_x )
				( x= in_x )
			{
				ConstructorCalled(x);
			}
			fn constructor( S& imut other )
				( x= other.x )
			{
				ConstructorCalled(x);
			}
			fn destructor()
			{
				DestructorCalled(x);
			}
		}

		fn Foo()
		{
			var S s( S(9565412) ); // Must not call copy constructor, but just move temp variable.
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_constructirs_call_sequence == std::vector<int>( { 9565412 } ) &&
		 g_destructors_call_sequence == std::vector<int>( { 9565412 } ) );
}

U_TEST(TempVariablesMovingTest7_MoveTempVariableReturningFromFunctionToAnotherFunction)
{
	TestPrepare();

	static const char c_program_text[]=
	R"(
		fn ConstructorCalled(i32 x);
		fn  DestructorCalled(i32 x);

		class S
		{
			i32 x;
			fn constructor()
				( x= 0 )
			{
				ConstructorCalled(x);
			}
			fn constructor( i32 in_x )
				( x= in_x )
			{
				ConstructorCalled(x);
			}
			fn constructor( S& imut other )
				( x= other.x )
			{
				ConstructorCalled(x);
			}
			fn destructor()
			{
				DestructorCalled(x);
			}
		}

		fn Bar() : S { return S(65214); }
		fn Baz( S s ){}

		fn Foo()
		{
			Baz(Bar());
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT(
		g_constructirs_call_sequence == std::vector<int>( { 65214 } ) &&
		 g_destructors_call_sequence == std::vector<int>( { 65214 } ) );
}

U_TEST(TempVariablesMovingTest8_MoveVariableFromFunctionResultWithMutableReferenceInside)
{
	static const char c_program_text[]=
	R"(
		struct Box
		{
			i32 &mut r;
			fn constructor( this'x', i32 &'y mut in_r ) ' x <- y '
			( r= in_r )
			{}
		}

		fn BoxIt( i32 &'a mut x ) : Box'a'
		{
			return Box(x);
		}

		fn Foo() : i32
		{
			var i32 mut i= 45;
			auto box= BoxIt(i); // Function result contains mutable reference. If we just copy it, we recieve reference protection error. But, with moving, all must be ok.
			box.r= 856;
			return box.r;
		}
	)";

	const EnginePtr engine= CreateEngine( BuildProgram( c_program_text ) );
	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );
	U_TEST_ASSERT( static_cast<uint64_t>(856) == result_value.IntVal.getLimitedValue() );
}

} // namespace U
