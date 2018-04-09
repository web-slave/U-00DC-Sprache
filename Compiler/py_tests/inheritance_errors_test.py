from py_tests_common import *


def UsingIncompleteType_ForInheritance_Test0():
	c_program_text= """
		class A;
		class B : A {}  // Error - "A" is incomplete.
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "UsingIncompleteType" )
	assert( errors_list[0].file_pos.line == 3 )


def BaseUnavailable_Test0():
	c_program_text= """
		class A
		{
			fn constructor()
			( base() )   // error, tryying intitialize "base", but current class have no base class.
			{}
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "BaseUnavailable" )
	assert( errors_list[0].file_pos.line == 5 )


def CanNotDeriveFromThisType_Test0():
	c_program_text= """
		class A{}   // non-polymorph by-default
		class B : A{}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "CanNotDeriveFromThisType" )
	assert( errors_list[0].file_pos.line == 3 )


def CanNotDeriveFromThisType_Test1():
	c_program_text= """
		class A polymorph {}
		class B final : A{}
		class C : B {}    // Inherit from final class
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "CanNotDeriveFromThisType" )
	assert( errors_list[0].file_pos.line == 4 )


def DuplicatedParentClass_Test0():
	c_program_text= """
		class A interface {}
		class B : A, A {}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "DuplicatedParentClass" )
	assert( errors_list[0].file_pos.line == 3 )


def DuplicatedParentClass_Test1():
	c_program_text= """
		class A interface {}
		type A_alias= A;
		class B interface {}
		class C : A, B, A_alias {}   // Using direct name and alias name for same base class
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "DuplicatedParentClass" )
	assert( errors_list[0].file_pos.line == 5 )


def DuplicatedBaseClass_Test0():
	c_program_text= """
		class A polymorph {}
		class B polymorph {}
		class C : A, B {}   // error - inherit more than one non-interface class.
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "DuplicatedBaseClass" )
	assert( errors_list[0].file_pos.line == 4 )


def FieldsForInterfacesNotAllowed_Test0():
	c_program_text= """
		class A interface
		{
			f32 x;
		}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "FieldsForInterfacesNotAllowed" )
	assert( errors_list[0].file_pos.line == 3 )


def BaseClassForInterface_Test0():
	c_program_text= """
		class A polymorph {} // non-interface
		class B interface : A {}
	"""
	errors_list= ConvertErrors( tests_lib.build_program_with_errors( c_program_text ) )
	assert( len(errors_list) > 0 )
	assert( errors_list[0].error_code == "BaseClassForInterface" )
	assert( errors_list[0].file_pos.line == 3 )