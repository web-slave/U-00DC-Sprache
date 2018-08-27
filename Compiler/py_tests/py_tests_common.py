import sprache_compiler_tests_py_lib as tests_lib

class FilePos:
	def __init__(self):
		self.file_index= 0
		self.line= 0
		self.pos_in_line= 0


class CodeBuilderError:
	def __init__(self):
		self.error_code= ""
		self.text= ""
		self.file_pos= FilePos()


def ConvertErrors( errors_list ):
	result= []
	for error in errors_list:
		out_error= CodeBuilderError()
		out_error.error_code= error["code"]
		out_error.text= error["text"]
		out_error.file_pos.file_index= error["file_pos"]["file_index"]
		out_error.file_pos.line= error["file_pos"]["line"]
		out_error.file_pos.pos_in_line= error["file_pos"]["pos_in_line"]
		result.append( out_error )
	return result
