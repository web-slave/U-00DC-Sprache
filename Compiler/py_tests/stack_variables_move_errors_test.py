from py_tests_common import *


def ExpectedReferenceValue_ForMove_Test0():
	c_program_text= """
		fn Foo()
		{
			auto imut x= 0;
			move(x); // Expected mutable variable.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ExpectedReferenceValue" )
	assert( errors_list[0].file_pos.line == 5 )


def ExpectedReferenceValue_ForMove_Test1():
	c_program_text= """
		fn Foo( i32 imut x )
		{
			move(x); // Expected mutable variable, got immutable argument.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ExpectedReferenceValue" )
	assert( errors_list[0].file_pos.line == 4 )


def ExpectedReferenceValue_ForMove_Test2():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			auto &mut r= x;
			move(r); // Expected variable, got reference. TODO - generate separate error.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ExpectedReferenceValue" )
	assert( errors_list[0].file_pos.line == 6 )


def AccessingMovedVariable_Test0():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			move(x);
			++x;
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "AccessingMovedVariable" )
	assert( errors_list[0].file_pos.line == 6 )


def AccessingMovedVariable_Test1():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			move(x);
			move(x);
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "AccessingMovedVariable" )
	assert( errors_list[0].file_pos.line == 6 )


def AccessingMovedVariable_Test2():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			move(x) + x;
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "AccessingMovedVariable" )
	assert( errors_list[0].file_pos.line == 5 )


def AccessingMovedVariable_Test3():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			move(x);
			x= 42; // Currently, event can not assign value to moved variable.
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "AccessingMovedVariable" )
	assert( errors_list[0].file_pos.line == 6 )


def AccessingMovedVariable_Test4():
	c_program_text= """
		fn Foo( i32 mut x )
		{
			move(x);
			--x;
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "AccessingMovedVariable" )
	assert( errors_list[0].file_pos.line == 5 )


def OuterVariableMoveInsideLoop_Test0():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			while( false ){ move(x); }
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "OuterVariableMoveInsideLoop" )
	assert( errors_list[0].file_pos.line == 5 )


def OuterVariableMoveInsideLoop_Test1():
	c_program_text= """
		fn Foo( i32 mut x )
		{
			while( false ){ move(x); }
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "OuterVariableMoveInsideLoop" )
	assert( errors_list[0].file_pos.line == 4 )


def ConditionalMove_Test0():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			if( false ) { move(x); }
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ConditionalMove" )
	assert( errors_list[0].file_pos.line == 5 )


def ConditionalMove_Test1():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			if( false ) { move(x); }
			else if( false ) {}
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ConditionalMove" )
	assert( errors_list[0].file_pos.line == 6 )


def ConditionalMove_Test2():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			if( false ) { move(x); }
			else if( false ) {}
			else { move(x); }
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ConditionalMove" )
	assert( errors_list[0].file_pos.line == 7 )


def ConditionalMove_Test3():
	c_program_text= """
		fn Foo()
		{
			auto mut x= 0;
			if( false ) { }   // Not moved in first branch
			else { move(x); }
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "ConditionalMove" )
	assert( errors_list[0].file_pos.line == 6 )
