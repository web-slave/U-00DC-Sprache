#include "tests.hpp"

namespace U
{

U_TEST( ImportsTest0 )
{
	static const char c_program_text_a[]=
	R"(
		fn Bar() : i32
		{
			return  586;
		}
	)";

	static const char c_program_text_root[]=
	R"(
		import "a"

		fn Foo() : i32
		{
			return Bar();
		}
	)";

	BuildMultisourceProgram(
		{
			{ "a"_SpC, c_program_text_a },
			{ "root"_SpC, c_program_text_root }
		},
		"root"_SpC );
}

U_TEST( ImportsTest1_FunctionPrototypeInOneFileAndBodyInAnother )
{
	static const char c_program_text_a[]=
	R"(
		fn Bar() : i32;
	)";

	static const char c_program_text_root[]=
	R"(
		import "a"

		fn Bar() : i32
		{
			return  586;
		}

		fn Foo() : i32
		{
			return Bar();
		}
	)";

	const EnginePtr engine=
		CreateEngine(
			BuildMultisourceProgram(
				{
					{ "a"_SpC, c_program_text_a },
					{ "root"_SpC, c_program_text_root }
				},
				"root"_SpC ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( static_cast<uint64_t>(586) == result_value.IntVal.getLimitedValue() );
}

U_TEST( ImportsTest2_FunctionsWithDifferentSignaturesInDifferentFiles )
{
	static const char c_program_text_a[]=
	R"(
		fn Bar( f32 x ) : i32
		{
			return 8854;
		}
	)";

	static const char c_program_text_root[]=
	R"(
		import "a"

		fn Bar( i32 x ) : i32
		{
			return 984364;
		}

		fn Foo() : i32
		{
			return Bar( 0.0f );
		}
	)";

	const EnginePtr engine=
		CreateEngine(
			BuildMultisourceProgram(
				{
					{ "a"_SpC, c_program_text_a },
					{ "root"_SpC, c_program_text_root }
				},
				"root"_SpC ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( static_cast<uint64_t>(8854) == result_value.IntVal.getLimitedValue() );
}

U_TEST( ImportsTest3_MultipleInportOfSameFile_Test0 )
{
	static const char c_program_text_a[]=
	R"(
		fn Bar() : i32
		{
			return  586;
		}
	)";

	static const char c_program_text_root[]=
	R"(
		import "a"
		import "a" // Should discard this import.

		fn Foo() : i32
		{
			return Bar();
		}
	)";

	BuildMultisourceProgram(
		{
			{ "a"_SpC, c_program_text_a },
			{ "root"_SpC, c_program_text_root }
		},
		"root"_SpC );
}

U_TEST( ImportsTest4_MultipleInportOfSameFile_Test1 )
{
	static const char c_program_text_a[]=
	R"(
		struct X{}
	)";

	static const char c_program_text_b[]=
	R"(
		import "a"
		struct Y{}
	)";

	static const char c_program_text_root[]=
	R"(
		import "a"
		import "b" // "b" already contains "a", so, it does not loads twice.
	)";

	BuildMultisourceProgram(
		{
			{ "a"_SpC, c_program_text_a },
			{ "b"_SpC, c_program_text_b },
			{ "root"_SpC, c_program_text_root }
		},
		"root"_SpC );
}

U_TEST( ImportsTest5_ImportContentOfNamespace )
{
	static const char c_program_text_a[]=
	R"(
		namespace X
		{
			fn Bar() : i32 { return 44512; }
		}
	)";

	static const char c_program_text_root[]=
	R"(
		import "a"
		fn Foo() : i32
		{
			return X::Bar();
		}
	)";

	const EnginePtr engine=
		CreateEngine(
			BuildMultisourceProgram(
				{
					{ "a"_SpC, c_program_text_a },
					{ "root"_SpC, c_program_text_root }
				},
				"root"_SpC ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( static_cast<uint64_t>(44512) == result_value.IntVal.getLimitedValue() );
}

U_TEST( ImportsTest6_ImportClass )
{
	static const char c_program_text_a[]=
	R"(
		struct X
		{
			fn Foo() : i32;
		}
	)";

	static const char c_program_text_root[]=
	R"(
		import "a"

		fn X::Foo() : i32 // Add body for function prototype from imported class.
		{
			return 8826545;
		}

		fn Foo() : i32
		{
			return X::Foo();
		}
	)";

	const EnginePtr engine=
		CreateEngine(
			BuildMultisourceProgram(
				{
					{ "a"_SpC, c_program_text_a },
					{ "root"_SpC, c_program_text_root }
				},
				"root"_SpC ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( static_cast<uint64_t>(8826545) == result_value.IntVal.getLimitedValue() );
}

U_TEST( ImportsTest7_ImportFileWithFunctionPrototypeAfterFileWithFunctionBody )
{
	static const char c_program_text_a[]=
	R"(
		fn Bar() : i32;
	)";

	static const char c_program_text_b[]=
	R"(
		import "a"
		fn Bar() : i32 { return 11145; }
	)";

	static const char c_program_text_root[]=
	R"(
		import "b" // function with body
		import "a" // only prototype
		fn Foo() : i32
		{
			return Bar();
		}
	)";

	const EnginePtr engine=
		CreateEngine(
			BuildMultisourceProgram(
				{
					{ "a"_SpC, c_program_text_a },
					{ "b"_SpC, c_program_text_b },
					{ "root"_SpC, c_program_text_root }
				},
				"root"_SpC ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( static_cast<uint64_t>(11145) == result_value.IntVal.getLimitedValue() );
}

U_TEST( ImportsTest8_ImportFileWithFunctionBodyAfterFileWithFunctionPrototype )
{
	static const char c_program_text_a[]=
	R"(
		fn Bar() : i32;
	)";

	static const char c_program_text_b[]=
	R"(
		import "a"
		fn Bar() : i32 { return 5641289; }
	)";

	static const char c_program_text_root[]=
	R"(
		import "a" // only prototype
		import "b" // function with body
		fn Foo() : i32
		{
			return Bar();
		}
	)";

	const EnginePtr engine=
		CreateEngine(
			BuildMultisourceProgram(
				{
					{ "a"_SpC, c_program_text_a },
					{ "b"_SpC, c_program_text_b },
					{ "root"_SpC, c_program_text_root }
				},
				"root"_SpC ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( static_cast<uint64_t>(5641289) == result_value.IntVal.getLimitedValue() );
}

U_TEST( ImportsTest9_ImportFileWithClassPrototypeAfterFileWithClassBody )
{
	static const char c_program_text_a[]=
	R"(
		struct FFF;
	)";

	static const char c_program_text_b[]=
	R"(
		import "a"
		struct FFF
		{
			fn Foo( imut this ) : i32 { return 123567845; }
		}
	)";

	static const char c_program_text_root[]=
	R"(
		import "b" // class body
		import "a" // only prototype
		fn Foo() : i32
		{
			var FFF fff;
			return fff.Foo();
		}
	)";

	const EnginePtr engine=
		CreateEngine(
			BuildMultisourceProgram(
				{
					{ "a"_SpC, c_program_text_a },
					{ "b"_SpC, c_program_text_b },
					{ "root"_SpC, c_program_text_root }
				},
				"root"_SpC ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( static_cast<uint64_t>(123567845) == result_value.IntVal.getLimitedValue() );
}

U_TEST( ImportsTest10_ImportFileWithClassBodyAfterFileWithClassprototype )
{
	static const char c_program_text_a[]=
	R"(
		struct FFF;
	)";

	static const char c_program_text_b[]=
	R"(
		import "a"
		struct FFF
		{
			fn Foo( imut this ) : i32 { return 84562; }
		}
	)";

	static const char c_program_text_root[]=
	R"(
		import "a" // only prototype
		import "b" // class body
		fn Foo() : i32
		{
			var FFF fff;
			return fff.Foo();
		}
	)";

	const EnginePtr engine=
		CreateEngine(
			BuildMultisourceProgram(
				{
					{ "a"_SpC, c_program_text_a },
					{ "b"_SpC, c_program_text_b },
					{ "root"_SpC, c_program_text_root }
				},
				"root"_SpC ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( static_cast<uint64_t>(84562) == result_value.IntVal.getLimitedValue() );
}

U_TEST( ImportsTest11_MultipleImportOfSameGeneratedTemplateClass )
{
	static const char c_program_text_a[]=
	R"(
		template</ type T />
		struct TypeLimits</ T />
		{
			fn Zero() : T { return T(0); }
		}
	)";

	static const char c_program_text_b[]=
	R"(
		import "a"
		fn Bar() : i32
		{
			return TypeLimits</ i32 />::Zero();
		}
	)";

	static const char c_program_text_c[]=
	R"(
		import "a"
		fn Baz() : i32
		{
			return TypeLimits</ i32 />::Zero();
		}
	)";

	static const char c_program_text_root[]=
	R"(
		import "b"
		import "c" // Each import contains same instance of template class "TypeLimits".
		fn Foo() : i32
		{
			return 5474 + Bar() + Baz() * 42;
		}
	)";

	const EnginePtr engine=
		CreateEngine(
			BuildMultisourceProgram(
				{
					{ "a"_SpC, c_program_text_a },
					{ "b"_SpC, c_program_text_b },
					{ "c"_SpC, c_program_text_c },
					{ "root"_SpC, c_program_text_root }
				},
				"root"_SpC ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( static_cast<uint64_t>(5474) == result_value.IntVal.getLimitedValue() );
}

U_TEST( ImportsTest12_MultipleImportOfSameGeneratedTemplateClass )
{
	static const char c_program_text_a[]=
	R"(
		template</ type T />
		struct Box</ T />
		{
			T t;
		}
	)";

	static const char c_program_text_b[]=
	R"(
		import "a"
		type f_box= Box</ f32 />;
	)";

	static const char c_program_text_c[]=
	R"(
		import "a"
		type f32_box= Box</ f32 />;
	)";

	static const char c_program_text_root[]=
	R"(
		import "b"
		import "c" // Each import contains same instance of template class "Box</ f32 />".
		fn Foo() : i32
		{
			var f_box box0{ .t= 54125.1f };
			var f32_box box1( box0 ); // f_box and f32_box are same types - so, copy constructor call must be ok.
			return i32( box1.t );
		}
	)";

	const EnginePtr engine=
		CreateEngine(
			BuildMultisourceProgram(
				{
					{ "a"_SpC, c_program_text_a },
					{ "b"_SpC, c_program_text_b },
					{ "c"_SpC, c_program_text_c },
					{ "root"_SpC, c_program_text_root }
				},
				"root"_SpC ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( static_cast<uint64_t>(54125) == result_value.IntVal.getLimitedValue() );
}

U_TEST( ImportsTest13_MultipleImportOfSameGeneratedTemplateClassInsideTemplateClass )
{
	static const char c_program_text_a[]=
	R"(
		template</ type T />
		struct Box</ T />
		{
			template</ type U />
			struct Pair</ U />
			{
				T first;
				U second;
			}
			T t;
		}
	)";

	static const char c_program_text_b[]=
	R"(
		import "a"
		type b_pair= Box</ i32 />::Pair</ f32 />;
	)";

	static const char c_program_text_c[]=
	R"(
		import "a"
		type c_pair= Box</ i32 />::Pair</ f32 />;
	)";

	static const char c_program_text_root[]=
	R"(
		import "b"
		import "c" // Each import contains same instance of template class "Box</ i32 />::Pair</ f32 />".
		fn Foo() : i32
		{
			var b_pair p0{ .first= 42, .second= 3.14f };
			var c_pair p1( p0 ); // c_pair and b_pair are same types - so, copy constructor call must be ok.
			return p1.first - i32(p1.second);
		}
	)";

	const EnginePtr engine=
		CreateEngine(
			BuildMultisourceProgram(
				{
					{ "a"_SpC, c_program_text_a },
					{ "b"_SpC, c_program_text_b },
					{ "c"_SpC, c_program_text_c },
					{ "root"_SpC, c_program_text_root }
				},
				"root"_SpC ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );

	U_TEST_ASSERT( static_cast<uint64_t>(42 - 3) == result_value.IntVal.getLimitedValue() );
}

U_TEST( ImportsTest14_ImportFunctionTemplate0 )
{
	static const char c_program_text_a[]=
	R"(
		type I= i32;
		template</ type T /> fn ToImut( T &imut t ) : T&imut
		{
			var I i= 0;
			return t;
		}
	)";

	static const char c_program_text_root[]=
	R"(
		import "a"

		fn A( i32& mut x ) { x*= 2; }
		fn A( i32&imut x ) { }
		fn Foo() : i32
		{
			var i32 mut x= 5;
			A( ToImut(x) ); // Must select function with immutable argument.
			return x;
		}
	)";

	const EnginePtr engine=
		CreateEngine(
			BuildMultisourceProgram(
				{
					{ "a"_SpC, c_program_text_a },
					{ "root"_SpC, c_program_text_root }
				},
				"root"_SpC ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );
	U_TEST_ASSERT( static_cast<uint64_t>(5) == result_value.IntVal.getLimitedValue() );
}

U_TEST( ImportsTest14_ImportFunctionTemplate1 )
{
	static const char c_program_text_a[]=
	R"(
		type I= i32;
		template</ type T /> fn Bar( T &mut t )
		{
			t= T(586);
		}
	)";

	static const char c_program_text_b[]=
	R"(
		fn ToImut( i32 &mut t )
		{
			t= 1917;
		}
	)";

	static const char c_program_text_root[]=
	R"(
		import "a"
		import "b"

		fn Foo() : i32
		{
			var i32 mut x= 0;
			ToImut(x); // Must select non-template function from "b", rather then template from "a".
			return x;
		}
	)";

	const EnginePtr engine=
		CreateEngine(
			BuildMultisourceProgram(
				{
					{ "a"_SpC, c_program_text_a },
					{ "b"_SpC, c_program_text_b },
					{ "root"_SpC, c_program_text_root }
				},
				"root"_SpC ) );

	llvm::Function* const function= engine->FindFunctionNamed( "_Z3Foov" );
	U_TEST_ASSERT( function != nullptr );

	const llvm::GenericValue result_value= engine->runFunction( function, llvm::ArrayRef<llvm::GenericValue>() );
	U_TEST_ASSERT( static_cast<uint64_t>(1917) == result_value.IntVal.getLimitedValue() );
}

} // namespace U