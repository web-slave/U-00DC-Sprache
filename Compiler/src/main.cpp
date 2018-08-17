#include <cstdio>
#include <cstring>
#include <iostream>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include "push_disable_llvm_warnings.hpp"
#include <llvm/ADT/Triple.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetMachine.h>
#include "pop_llvm_warnings.hpp"

#include "assert.hpp"
#include "code_builder.hpp"
#include "source_graph_loader.hpp"


static bool ReadFileRaw( const char* const name, std::string& out_file_content )
{
	std::FILE* const f= std::fopen( name, "rb" );
	if( f == nullptr )
		return false;

	std::fseek( f, 0, SEEK_END );
	const size_t file_size= size_t(std::ftell( f ));
	std::fseek( f, 0, SEEK_SET );

	out_file_content.resize(file_size);

	size_t read_total= 0u;
	bool read_error= false;
	do
	{
		const size_t read= std::fread( const_cast<char*>(out_file_content.data()) + read_total, 1, file_size - read_total, f );
		if( std::ferror(f) != 0 )
		{
			read_error= true;
			break;
		}
		if( read == 0 )
			break;

		read_total+= read;
	} while( read_total < file_size );

	std::fclose(f);

	return !read_error;
}

static bool ReadFile( const char* const name, U::ProgramString& out_file_content )
{
	std::string file_content_raw;
	if( !ReadFileRaw( name, file_content_raw ) )
		return false;

	out_file_content= U::DecodeUTF8( file_content_raw );
	return true;
}

namespace U
{

namespace fs= boost::filesystem;

class VfsOverSystemFS final : public IVfs
{
	struct PrivateTag{};

public:
	static std::shared_ptr<VfsOverSystemFS> Create( const std::vector<const char*> include_dirs )
	{
		std::vector<fs::path> result_include_dirs;

		bool all_ok= true;
		for( const char* const include_dir : include_dirs )
		{
			try
			{
				fs::path dir_path{ std::string(include_dir) };
				dir_path.make_preferred();
				if( !fs::exists( dir_path ) )
				{
					std::cout << "include dir \"" << include_dir << "\" does not exists." << std::endl;
					all_ok= false;
					continue;
				}

				result_include_dirs.push_back( std::move( dir_path ) );
			}
			catch( const std::exception& e )
			{
				std::cout << e.what() << std::endl;
				all_ok= false;
			}
		}

		if( !all_ok )
			return nullptr;

		return std::make_shared<VfsOverSystemFS>( std::move(result_include_dirs), PrivateTag() );
	}

	VfsOverSystemFS( std::vector<fs::path> include_dirs, PrivateTag )
		: include_dirs_(std::move(include_dirs))
	{}

public:
	virtual boost::optional<LoadFileResult> LoadFileContent( const Path& file_path, const Path& full_parent_file_path ) override
	{
		try
		{
			fs::path result_path= GetFullFilePathInternal( file_path, full_parent_file_path );
			if( result_path.empty() )
				return boost::none;

			LoadFileResult result;
			// TODO - maybe use here native format of path string?
			if( !ReadFile( result_path.string<std::string>().c_str(), result.file_content ) )
				return boost::none;

			result.full_file_path= ToProgramString( result_path.string<std::string>().c_str() );
			return std::move(result);
		}
		catch( const std::exception& e )
		{
			std::cout << e.what() << std::endl;
		}

		return boost::none;
	}

	virtual Path GetFullFilePath( const Path& file_path, const Path& full_parent_file_path ) override
	{
		return ToProgramString( GetFullFilePathInternal( file_path, full_parent_file_path ).string<std::string>().c_str() );
	}

private:
	fs::path GetFullFilePathInternal( const Path& file_path, const Path& full_parent_file_path )
	{
		try
		{
			const fs::path file_path_r( ToStdString(file_path) );
			fs::path result_path;

			if( full_parent_file_path.empty() )
				result_path= file_path_r;
			else if( !file_path.empty() && file_path[0] == '/' )
			{
				// If file path is absolute, like "/some_lib/some_file.u" search file in include dirs.
				// Return real file system path to first existent file.
				for( const fs::path& include_dir : include_dirs_ )
				{
					fs::path full_file_path= include_dir / file_path_r;
					if( fs::exists( full_file_path ) )
					{
						result_path= std::move(full_file_path);
						break;
					}
				}
			}
			else
			{
				const fs::path base_dir= fs::path( ToStdString(full_parent_file_path) ).parent_path();
				result_path= base_dir / file_path_r;
			}
			return result_path.make_preferred();
		}
		catch( const std::exception& e )
		{
			std::cout << e.what() << std::endl;
		}

		return fs::path();
	}

private:
	const std::vector<fs::path> include_dirs_;
};

} // namespace U

int main( const int argc, const char* const argv[])
{
	static const char help_message[]=
	R"(
Ü-Sprache compiler
Usage:
	Compiler -i [input_file] -o [output_ir_file] [--print-llvm-asm])";

	if( argc <= 1 )
	{
		std::cout << help_message << std::endl;
		return 1;
	}

	// Options
	std::vector<const char*> input_files;
	std::vector<const char*> include_directories;
	const char* output_file= nullptr;
	bool print_llvm_asm= false;
	bool produce_object_file= false;
	bool tests_output= false;

	// Parse command line
	for( int i = 1; i < argc; )
	{
		if( std::strcmp( argv[i], "-o" ) == 0 )
		{
			if( i + 1 >= argc )
			{
				std::cout << "Expeted name after\"-o\"" << std::endl;
				return 1;
			}
			output_file= argv[ i + 1 ];
			i+= 2;
		}
		else if( std::strcmp( argv[i], "--include-dir" ) == 0 )
		{
			if( i + 1 >= argc )
			{
				std::cout << "Expeted path after\"--include-dir\"" << std::endl;
				return 1;
			}
			include_directories.push_back( argv[ i + 1 ] );
			i+= 2;
		}
		else if( std::strcmp( argv[i], "--print-llvm-asm" ) == 0 )
		{
			print_llvm_asm= true;
			++i;
		}
		else if( std::strcmp( argv[i], "--tests-output" ) == 0 )
		{
			tests_output= true;
			++i;
		}
		else if( std::strcmp( argv[i], "--produce-object-file" ) == 0 )
		{
			produce_object_file= true;
			++i;
		}
		else if( std::strcmp( argv[i], "--help" ) == 0 )
		{
			std::cout << help_message << std::endl;
			return 0;
		}
		else
		{
			input_files.push_back( argv[i] );
			++i;
		}
	}

	if( input_files.empty() )
	{
		std::cout << "No input files" << std::endl;
		return 1;
	}
	if( output_file == nullptr )
	{
		std::cout << "No output file" << std::endl;
		return 1;
	}

	// Prepare target machine.
	// Currently can work only with native target.
	// TODO - allow compiler user to change target.
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetAsmPrinter();
	const std::string target_triple_str= llvm::sys::getDefaultTargetTriple();

	std::unique_ptr<llvm::TargetMachine> target_machine;
	{
		std::string error_str;
		const llvm::Target* const target= llvm::TargetRegistry::lookupTarget( target_triple_str, error_str );
		if( target == nullptr )
		{
			std::cout << "Error, selecting target: " << error_str << std::endl;
			return 1;
		}

		std::string features_str; // TODO - set features
		// TODO - set optimization level, reloc model, etc.
		target_machine.reset(
			target->createTargetMachine(
				target_triple_str,
				llvm::sys::getHostCPUName(),
				features_str,
				llvm::TargetOptions() ) );

		if( target_machine == nullptr )
		{
			std::cout << "Error, creating target machine." << std::endl;
			return 1;
		}
	}
	const llvm::DataLayout data_layout= target_machine->createDataLayout();

	const auto vfs= U::VfsOverSystemFS::Create( include_directories );
	if( vfs == nullptr )
		return 1u;

	// Compile multiple input files and link them together.
	U::SourceGraphLoader source_graph_loader( vfs );
	std::unique_ptr<llvm::Module> result_module;
	bool have_some_errors= false;
	for( const char* const input_file : input_files )
	{
		const U::SourceGraphPtr source_graph= source_graph_loader.LoadSource( U::ToProgramString( input_file ) );
		U_ASSERT( source_graph != nullptr );
		if( !source_graph->lexical_errors.empty() || !source_graph->syntax_errors.empty() )
		{
			have_some_errors= true;
			continue;
		}

		U::CodeBuilder::BuildResult build_result=
			U::CodeBuilder( target_triple_str, data_layout ).BuildProgram( *source_graph );

		if( tests_output )
		{
			// For tests we print errors as "file.u 88 NameNotFound"
			for( const U::CodeBuilderError& error : build_result.errors )
				std::cout << U::ToStdString( source_graph->nodes_storage[error.file_pos.file_index ].file_path )
					<< " " << error.file_pos.line << " " << U::CodeBuilderErrorCodeToString( error.code ) << "\n";
		}
		else
		{
			for( const U::CodeBuilderError& error : build_result.errors )
				std::cout << U::ToStdString( source_graph->nodes_storage[error.file_pos.file_index ].file_path )
					<< ":" << error.file_pos.line << ":" << error.file_pos.pos_in_line << " " << U::ToStdString( error.text ) << "\n";
		}

		if( !build_result.errors.empty() )
		{
			have_some_errors= true;
			continue;
		}

		if( result_module == nullptr )
			result_module= std::move( build_result.module );
		else
		{
			const bool not_ok=
				llvm::Linker::LinkModules( result_module.get() , build_result.module.get() );
			if( not_ok )
			{
				std::cout << "Error, linking file \"" << input_file << "\"" << std::endl;
				have_some_errors= true;
			}
		}
	}

	if( have_some_errors )
		return 1;

	// Prepare stdlib modules set.
	std::vector<std::string> asm_funcs_modules;
	asm_funcs_modules.push_back( "asm_funcs.bc" );
	asm_funcs_modules.push_back( data_layout.getPointerSizeInBits() == 32u ? "asm_funcs_32.bc" : "asm_funcs_64.bc" );

	// Link stdlib with result module.
	for( const std::string& asm_funcs_module : asm_funcs_modules )
	{
		std::string file_content;
		if( !ReadFileRaw( asm_funcs_module.c_str(), file_content ) )
		{
			std::cout << "Internal compiler error - stdlib module read error: " << asm_funcs_module << std::endl;
			return 1;
		}

		const llvm::ErrorOr<std::unique_ptr<llvm::Module>> std_lib_module=
			llvm::parseBitcodeFile(
				llvm::MemoryBufferRef( file_content, "ustlib asm file" ),
				result_module->getContext() );

		if( !std_lib_module )
		{
			std::cout << "Internal compiler error - stdlib module parse error: " << asm_funcs_module << std::endl;
			return 1;
		}

		std_lib_module.get()->setDataLayout( data_layout );
		std_lib_module.get()->setTargetTriple( target_triple_str );

		std::string err_stream_str;
		llvm::raw_string_ostream err_stream( err_stream_str );
		if( llvm::verifyModule( *std_lib_module.get(), &err_stream ) )
		{
			std::cout << "Internal compiler error - stdlib module verify error: " << asm_funcs_module << ":\n" << err_stream.str() << std::endl;
			return 1;
		}

		llvm::Linker::LinkModules( result_module.get(), std_lib_module.get().get() );
	}

	if( print_llvm_asm )
	{
		llvm::raw_os_ostream stream(std::cout);
		result_module->print( stream, nullptr );
	}

	std::error_code file_error_code;
	llvm::raw_fd_ostream out_file_stream( output_file, file_error_code, llvm::sys::fs::F_None );

	if( produce_object_file )
	{
		llvm::PassRegistry& registry= *llvm::PassRegistry::getPassRegistry();
		llvm::initializeCore(registry);
		llvm::initializeCodeGen(registry);
		llvm::initializeLoopStrengthReducePass(registry);
		llvm::initializeLowerIntrinsicsPass(registry);
		llvm::initializeUnreachableBlockElimPass(registry);

		const llvm::TargetMachine::CodeGenFileType file_type= llvm::TargetMachine::CodeGenFileType::CGFT_ObjectFile;
		const bool no_verify= true;
		llvm::AnalysisID start_before_id= nullptr;
		llvm::AnalysisID start_after_id = nullptr;
		llvm::AnalysisID stop_after_id = nullptr;

		llvm::legacy::PassManager pass_manager;

		if( target_machine->addPassesToEmitFile(
				pass_manager,
				out_file_stream,
				file_type,
				no_verify,
				start_before_id,
				start_after_id,
				stop_after_id,
				nullptr) )
		{
			std::cout << "Error, creating file emit pass." << std::endl;
			return 1;
		}

		pass_manager.run(*result_module);
	}
	else
	{
		llvm::WriteBitcodeToFile( result_module.get(), out_file_stream );
	}

	out_file_stream.flush();
	if( out_file_stream.has_error() )
	{
		std::cout << "Error while writing output file \"" << output_file << "\"" << std::endl;
		return 1;
	}

	return 0;
}
