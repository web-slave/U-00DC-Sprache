from py_tests_common import *


def SimpleProgramTest():
	c_program_text= """
		fn Add( i32 a, i32 b ) : i32
		{
			return a + b;
		}
	"""
	tests_lib.build_program( c_program_text )
	call_result= tests_lib.run_function( "_Z3Addii", 95, -5 )
	assert( call_result == 95 - 5 )


def LocalVariableTest():
	c_program_text= """
		fn Sub( i32 a, i32 b ) : i32
		{
			var i32 mut x= zero_init;
			x= a - b;
			return x;
		}
	"""
	tests_lib.build_program( c_program_text )
	call_result= tests_lib.run_function( "_Z3Subii", 654, 112 )
	assert( call_result == 654 - 112 )


def CreateMutableReferenceToVariableWithImmutableReference_Test0():
	c_program_text= """
		fn Foo()
		{
			var i32 mut x= zero_init;
			auto& imut ri= x;
			auto& mut  rm= x;
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 6 )


def CreateMutableReferenceToVariableWithImmutableReference_Test1():
	c_program_text= """
		fn Foo()
		{
			var i32 mut x= zero_init;
			var i32 &imut ri= x;
			var i32 &mut  rm= x;
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 6 )


def CreateMutableReferenceToVariableWithMutableReference():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			var i32 &mut r0= x;
			var i32 &mut r1= x;
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 6 )


def CreateMutableReferenceToAnotherMutableReference_Test0():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			var i32 &mut r0= x;
			var i32 &mut r1= r0; // ok
		}
	"""
	tests_lib.build_program( c_program_text )


def CreateMutableReferenceToAnotherMutableReference_Test1():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			var i32 &mut r0= x;
			var i32 &mut r1= r0; // ok
			var i32 &mut r2= r0; // Not ok
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 7 )


def AccessingVariableThatHaveMutableReference_Test0():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			var i32 &mut r= x;
			var i32 x_copy= x; // error, accessing 'x'
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "AccessingVariableThatHaveMutableReference" )
	assert( errors_list[0].file_pos.line == 6 )


def AccessingVariableThatHaveMutableReference_Test1():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			var i32 &mut r0= x;
			var i32 &mut r1= r0;
			var i32 x_copy= r0; // error, accessing 'r0', that have reference 'r1'
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "AccessingVariableThatHaveMutableReference" )
	assert( errors_list[0].file_pos.line == 7 )


def AccessingVariableThatHaveMutableReference_Test2():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			var i32 &mut r= x;
			var i32 x_copy= r; //ok, accessing end reference
		}
	"""
	tests_lib.build_program( c_program_text )


def FunctionReferencePass_Test0():
	c_program_text= """
		fn Pass( i32& x ) : i32& { return x; }
		fn Foo()
		{
			auto mut x= 0;
			auto &imut ri= Pass(x);
			auto &mut rm= x; // Error, 'x' have reference already.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 7 )


def FunctionReferencePass_Test1():
	c_program_text= """
		fn PassFirst( i32&'f x, i32&'s y ) : i32&'f { return x; }
		fn Foo()
		{
			var i32 mut x= 0, mut y= 0;
			auto &imut ri= PassFirst( x, y );
			auto &mut rm= y; // Ok, 'y' have no references.
		}
	"""
	tests_lib.build_program( c_program_text )


def FunctionReferencePass_Test2():
	c_program_text= """
		fn Pass( i32&mut x ) : i32&mut { return x; }
		fn Foo()
		{
			auto mut x= 0;
			auto &mut r0= Pass(x);
			auto x_copy= x; // Error, accessing 'x', which have mutable rerefernce.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "AccessingVariableThatHaveMutableReference" )
	assert( errors_list[0].file_pos.line == 7 )


def ReferenceInsideStruct_Test0():
	c_program_text= """
		struct S{ i32 &imut r; }
		fn Foo()
		{
			var i32 mut x= 0;
			var S s{ .r= x };
			++x; // Error, 'x' have reference.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 7 )


def ReferenceInsideStruct_Test1():
	c_program_text= """
		struct S{ i32 &mut r; }
		fn Foo()
		{
			var i32 mut x= 0;
			auto &imut r= x;
			var S s{ .r= x }; // Error, creating mutable reference to variable, that have reference.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 7 )


def ReturnReferenceInsideStruct_Test0():
	c_program_text= """
		struct S{ i32 &imut r; }
		fn GetS( i32 &'a imut x ) : S'a'
		{
			var S s{ .r= x };
			return s;
		}
		fn Foo()
		{
			var i32 mut x= 0;
			auto &imut r= GetS(x).r;
			++x; // Error, reference to 'x' exists inside 'r'.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 12 )


def ReturnReferenceFromStruct_Test0():
	c_program_text= """
		struct S{ i32 &mut r; }
		fn GetR( S& s'x' ) : i32 &'x mut
		{
			return s.r;
		}
		fn Foo()
		{
			var i32 mut x= 0;
			var S s{ .r= x };
			var i32 &mut x_ref= GetR( s );
			++s.r; // Error, reference to 'x' exists inside 'r'.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 12 )


def ReturnReferenceFromStruct_Test1():
	c_program_text= """
		struct A { i32& mut r; }
		struct B { A& imut a; }
		fn GetR( B& b'x' ) : i32 &'x mut
		{
			return b.a.r;
		}
		fn Foo()
		{
			var i32 mut x= 0;
			var A a{ .r= x };
			var B b{ .a= a };
			var i32 &mut x_ref= GetR( b );
			++a.r; // Error, reference to 'x' exists inside 'r'.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 14 )


def ReturnReferenceFromStruct_Test2():
	c_program_text= """
		struct A { i32& mut r; }
		struct B { A& imut a; }
		fn GetR( B& b'x' ) : i32 &'x mut
		{
			return b.a.r;
		}
		fn Foo()
		{
			var i32 mut x= 0;
			var A a{ .r= x };
			auto& a_ref0= a;
			auto& a_ref1= a_ref0;
			var B b{ .a= a_ref1 };
			var i32 &mut x_ref= GetR( b );
			++a.r; // Error, reference to 'x' exists inside 'r'.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 16 )


def ReturnReferenceInsideStruct_Test0():
	c_program_text= """
		struct S{ i32& x; }
		fn GetS( i32&'r x ) : S'r'
		{
			var S s{ .x= x };
			return s;
		}
		fn Foo()
		{
			var i32 mut x= 0;
			auto s= GetS(x);
			++x; // Error, reference to 'x' exists inside 's'.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 12 )


def PollutionTest0():
	c_program_text= """
		struct S{ i32& x; }
		fn Pollution( S &mut s'dst', i32&'src x ) ' dst <- src' {}
		fn Foo()
		{
			var i32 mut x= 0, mut y= 0;
			var S mut s{ .x= x };
			Pollution( s, y );
			++y;
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 9 )


def PollutionTest1():
	c_program_text= """
		struct S{ i32& x; }
		fn Pollution( S &mut s'dst', i32&'src x ) ' dst <- src' {}
		fn Foo()
		{
			var i32 mut x= 0, mut y= 0;
			var S mut s{ .x= x };
			auto& mut s_ref= s;
			Pollution( s_ref, y );
			++y;
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 10 )


def PollutionTest2():
	c_program_text= """
		struct S{ i32& x; }
		fn Pollution( S &mut a'dst', S& b'src' ) ' dst <- src' {}
		fn Foo()
		{
			var i32 mut x= 0, mut y= 0;
			var S mut a{ .x= x };
			{
				var S mut b{ .x= y };
				Pollution( a, b ); // Now, 'a', contains reference to 'y'
			}
			++y;
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ReferenceProtectionError" )
	assert( errors_list[0].file_pos.line == 12 )