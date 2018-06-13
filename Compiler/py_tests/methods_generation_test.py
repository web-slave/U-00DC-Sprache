from py_tests_common import *


def ClassHaveNoCopyConstructorByDefault_Test0():
	c_program_text= """
		class A{}
		fn Foo()
		{
			var A a;
			var A a_copy(a);  // Error, "A" is noncopyable
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "CouldNotSelectOverloadedFunction" )
	assert( errors_list[0].file_pos.line == 6 )


def ClassHaveNoCopyAssignementOperatorByDefault_Test0():
	c_program_text= """
		class A{}
		fn Foo()
		{
			var A mut a0, mut a1;
			a0= a1;  // Error, "A" is noncopyable
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "OperationNotSupportedForThisType" )
	assert( errors_list[0].file_pos.line == 6 )


def InvalidMethodForBodyGeneration_Test0():
	c_program_text= """
		fn Foo()= default; // Non-class function.
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "InvalidMethodForBodyGeneration" )
	assert( errors_list[0].file_pos.line == 2 )


def InvalidMethodForBodyGeneration_Test1():
	c_program_text= """
		class A
		{
			fn constructor( i32 x )= default; // Non-special constructor.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "InvalidMethodForBodyGeneration" )
	assert( errors_list[0].file_pos.line == 4 )


def InvalidMethodForBodyGeneration_Test2():
	c_program_text= """
		class A
		{
			op=( mut this, i32 x )= default; // Non-special assignemnt operator.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "InvalidMethodForBodyGeneration" )
	assert( errors_list[0].file_pos.line == 4 )


def InvalidMethodForBodyGeneration_Test3():
	c_program_text= """
		class A
		{
			fn Foo( this )= default; // Non-special method.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "InvalidMethodForBodyGeneration" )
	assert( errors_list[0].file_pos.line == 4 )


def InvalidMethodForBodyGeneration_Test4():
	c_program_text= """
		class A
		{
			op++( mut this )= default; // Non-special operator.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "InvalidMethodForBodyGeneration" )
	assert( errors_list[0].file_pos.line == 4 )
