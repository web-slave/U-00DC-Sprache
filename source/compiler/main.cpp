#include <cstdio>
#include <cstring>
#include <iostream>

#include "../lex_synt_lib/push_disable_boost_warnings.hpp"
#include <boost/filesystem/operations.hpp>
#include <boost/program_options.hpp>
#include "../lex_synt_lib/pop_boost_warnings.hpp"

#include "../code_builder_lib/push_disable_llvm_warnings.hpp"
#include <llvm/AsmParser/Parser.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/MC/SubtargetFeature.h>
#include "../code_builder_lib/pop_llvm_warnings.hpp"

#include "../lex_synt_lib/assert.hpp"
#include "../lex_synt_lib/source_graph_loader.hpp"
#include "../code_builder_lib/code_builder.hpp"

namespace fs= boost::filesystem;
namespace po = boost::program_options;

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

class VfsOverSystemFS final : public IVfs
{
	struct PrivateTag{};

public:
	static std::shared_ptr<VfsOverSystemFS> Create( const std::vector<std::string>& include_dirs )
	{
		std::vector<fs::path> result_include_dirs;

		bool all_ok= true;
		for( const std::string& include_dir : include_dirs )
		{
			try
			{
				fs::path dir_path{ include_dir };
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
		return ToProgramString( GetFullFilePathInternal( file_path, full_parent_file_path ).string<std::string>() );
	}

private:
	fs::path GetFullFilePathInternal( const Path& file_path, const Path& full_parent_file_path )
	{
		try
		{
			const fs::path file_path_r( ToUTF8(file_path) );
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
						result_path= fs::canonical( fs::path(ToUTF8(file_path.substr(1u))), include_dir );
						break;
					}
				}
			}
			else
			{
				const fs::path base_dir= fs::path( ToUTF8(full_parent_file_path) ).parent_path();
				result_path= fs::canonical( file_path_r, base_dir );
			}
			result_path.make_preferred();
			return result_path;
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

static std::string GetNativeTargetFeaturesStr()
{
	llvm::SubtargetFeatures features;

	llvm::StringMap<bool> host_features;
	if( llvm::sys::getHostCPUFeatures(host_features) )
	{
		for( auto& f : host_features )
			features.AddFeature( f.first(), f.second );
	}

	return features.getString();
}

static void PrintErrors( const U::SourceGraph& source_graph, const U::CodeBuilderErrorsContainer& errors )
{
	for( const U::CodeBuilderError& error : errors )
	{
		if( error.code == U::CodeBuilderErrorCode::TemplateContext )
		{
			U_ASSERT( error.template_context != nullptr );

			std::cerr << U::ToUTF8( source_graph.nodes_storage[ error.template_context->template_declaration_file_pos.file_index ].file_path ) << ": "
				<< "In instantiation of \"" << U::ToUTF8( error.template_context->template_name )
				<< "\" " << U::ToUTF8( error.template_context->parameters_description )
				<< "\n";

			std::cerr << U::ToUTF8( source_graph.nodes_storage[error.file_pos.file_index ].file_path )
				<< ":" << error.file_pos.line << ":" << error.file_pos.pos_in_line << ": required from here: " << "\n";
		}
		else
		{
			std::cerr << U::ToUTF8( source_graph.nodes_storage[error.file_pos.file_index ].file_path )
				<< ":" << error.file_pos.line << ":" << error.file_pos.pos_in_line << ": error: " << U::ToUTF8( error.text ) << "\n";
		}

		if( error.template_context != nullptr )
			PrintErrors( source_graph, error.template_context->errors );
	}
}

// Linked into executable resource files with standart library bitcode.
extern const char _binary_asm_funcs_bc_start;
extern const char _binary_asm_funcs_bc_end;
extern const char _binary_asm_funcs_bc_size;

extern const char _binary_asm_funcs_32_bc_start;
extern const char _binary_asm_funcs_32_bc_end;
extern const char _binary_asm_funcs_32_bc_size;

extern const char _binary_asm_funcs_64_bc_start;
extern const char _binary_asm_funcs_64_bc_end;
extern const char _binary_asm_funcs_64_bc_size;

int main( const int argc, const char* const argv[])
{
	// Options
	std::vector<std::string> input_files;
	std::vector<std::string> include_directories;
	std::string output_file;
	std::string architecture;
	fs::path compiler_data_dir= fs::system_complete( argv[0] ).parent_path(); // By default search compiler data near it`s executable.
	llvm::Reloc::Model relocation_model= llvm::Reloc::Default;
	bool produce_object_file= false;
	bool tests_output= false;
	bool print_llvm_asm= false;
	bool enable_pie= false;
	unsigned int optimization_level= 0u;
	unsigned int size_optimization_level= 0u;

	po::options_description program_options( u8"Ǖ-compiler options" );
	program_options.add_options()
		( "help,h", "produce help message" )
		( "input", po::value< std::vector< std::string> >()->composing(), "add input file" )
		( "output,o", po::value<std::string>()->required(), "set output file" )
		( "include-dir", po::value< std::vector<std::string> >(), "add include dir" )
		( "compiler-data-dir", po::value< std::string >(), "set path to compiler data directory" )
		( "produce-object-file", po::bool_switch()->default_value(false), "poduce native object file, instead of .ir file" )
		( "tests-output", po::bool_switch()->default_value(false), "print code builder errors in test mode" )
		( "print-llvm-asm", po::bool_switch()->default_value(false), "print llvm asm" )
		( "relocation-model", po::value< std::string >(), "relocation model of target" )
		( "enable-pie", po::bool_switch()->default_value(false), "assume the creation of a position independent executable" )
		( "optimization-level,O", po::value<std::string>(), "optimization level" )
		( "arch", po::value<std::string>()->default_value("native"), "target architecture" )
	;

	po::positional_options_description positional_options;
	positional_options.add( "input", -1 );

	if( argc <= 1 )
	{
		program_options.print( std::cout );
		return 0;
	}

	po::variables_map program_options_map;
	try
	{
		const auto options_parsed=
			po::command_line_parser( argc, argv )
				.options(program_options)
				.positional(positional_options)
				.allow_unregistered()
				.run();
		po::store( options_parsed, program_options_map );
		po::notify( program_options_map );
	} catch( std::exception& e )
	{
		std::cout << e.what() << std::endl;
		return 1;
	}

	if( program_options_map.count( "help" ) )
	{
		program_options.print( std::cout );
		return 0;
	}

	if( program_options_map.count( "include-dir" ) != 0 )
		include_directories= program_options_map["include-dir"].as< std::vector<std::string> >();

	if( program_options_map.count( "compiler-data-dir" ) != 0 )
		compiler_data_dir= fs::path( program_options_map["compiler-data-dir"].as<std::string>() );

	if( program_options_map.count( "produce-object-file" ) != 0 )
		produce_object_file= program_options_map[ "produce-object-file" ].as<bool>();
	if( program_options_map.count( "tests-output" ) != 0 )
		tests_output= program_options_map[ "tests-output" ].as<bool>();
	if( program_options_map.count( "print-llvm-asm" ) != 0 )
		print_llvm_asm= program_options_map[ "print-llvm-asm" ].as<bool>();

	if( program_options_map.count( "output" ) != 0 )
		output_file= program_options_map["output"].as< std::string >();

	if( program_options_map.count( "input" ) != 0 )
		input_files= program_options_map["input"].as< std::vector< std::string > >();

	if( program_options_map.count( "relocation-model" ) != 0 )
	{
		const std::string model_str= program_options_map["relocation-model"].as<std::string>();
		if( model_str == "default" ) relocation_model= llvm::Reloc::Default;
		else if( model_str == "static" ) relocation_model= llvm::Reloc::Static;
		else if( model_str == "pic" ) relocation_model= llvm::Reloc::PIC_;
		else if( model_str == "dynamic-no-pic" ) relocation_model= llvm::Reloc::DynamicNoPIC;
		else
		{
			std::cout << "Unknown relocation model: " << model_str << ". Supported relocation models are \"default\", \"static\", \"pic\", \"dynamic-no-pic\"." << std::endl;
			return 1;
		}
	}

	if( program_options_map.count( "enable-pie" ) != 0 )
		enable_pie= program_options_map[ "enable-pie" ].as<bool>();

	if( program_options_map.count( "optimization-level" ) != 0 )
	{
		const std::string& str= program_options_map["optimization-level"].as<std::string>();
		if( str == "0" )
			optimization_level= 0u;
		else if( str == "1" )
			optimization_level= 1u;
		else if( str == "2" )
			optimization_level= 2u;
		else if( str == "s" )
		{
			size_optimization_level= 1u;
			optimization_level= 2u;
		}
		else if( str == "z" )
		{
			size_optimization_level= 2u;
			optimization_level= 2u;
		}
		else
		{
			std::cout << "Unknown optimization: " << str << std::endl;
			return 1;
		}
	}

	if( program_options_map.count( "arch" ) != 0 )
		architecture= program_options_map["arch"].as<std::string>();

	if( input_files.empty() )
	{
		std::cout << "No input files" << std::endl;
		return 1;
	}
	if( output_file.empty() )
	{
		std::cout << "No output file" << std::endl;
		return 1;
	}

	// Prepare target machine.
	std::string target_triple_str;
	std::unique_ptr<llvm::TargetMachine> target_machine;
	{
		llvm::InitializeAllTargets();
		llvm::InitializeAllTargetMCs();
		llvm::InitializeAllAsmPrinters();
		llvm::InitializeAllAsmParsers();

		const llvm::Target* target= nullptr;
		std::string error_str, features_str, cpu_name;
		if( architecture == "native" )
		{
			target_triple_str= llvm::sys::getDefaultTargetTriple();
			target= llvm::TargetRegistry::lookupTarget( target_triple_str, error_str );
			features_str= GetNativeTargetFeaturesStr();
			cpu_name= llvm::sys::getHostCPUName();
		}
		else
		{
			llvm::Triple traget_triple;
			target= llvm::TargetRegistry::lookupTarget( architecture, traget_triple, error_str );
			target_triple_str= traget_triple.getTriple();
		}

		if( target == nullptr )
		{
			std::cout << "Error, selecting target: " << error_str << std::endl;

			std::string targets_list;
			for( const llvm::Target& target : llvm::TargetRegistry::targets() )
			{
				if( !targets_list.empty() )
					targets_list+= ", ";
				targets_list+= std::string(target.getName());
			}
			std::cout << "Available targets: " << targets_list << std::endl;

			return 1;
		}

		llvm::TargetOptions target_options;
		target_options.PositionIndependentExecutable= enable_pie;

		llvm::CodeGenOpt::Level code_gen_optimization_level;
		if( optimization_level >= 2u || size_optimization_level > 0u )
			code_gen_optimization_level= llvm::CodeGenOpt::Default;
		else if( optimization_level == 1u )
			code_gen_optimization_level= llvm::CodeGenOpt::Less;
		else
			code_gen_optimization_level= llvm::CodeGenOpt::None;

		target_machine.reset(
			target->createTargetMachine(
				target_triple_str,
				cpu_name,
				features_str,
				target_options,
				relocation_model,
				llvm::CodeModel::Default,
				code_gen_optimization_level ) );

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
	for( const std::string& input_file : input_files )
	{
		const U::SourceGraphPtr source_graph= source_graph_loader.LoadSource( U::ToProgramString( input_file.c_str() ) );
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
				std::cout << U::ToUTF8( source_graph->nodes_storage[error.file_pos.file_index ].file_path )
					<< " " << error.file_pos.line << " " << U::CodeBuilderErrorCodeToString( error.code ) << "\n";
		}
		else
		{
			PrintErrors( *source_graph, build_result.errors );
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
	const llvm::StringRef asm_funcs_modules[]=
	{
		llvm::StringRef(&_binary_asm_funcs_bc_start, reinterpret_cast<size_t>(&_binary_asm_funcs_bc_size)),
		( data_layout.getPointerSizeInBits() == 32u
			? llvm::StringRef(&_binary_asm_funcs_32_bc_start, reinterpret_cast<size_t>(&_binary_asm_funcs_32_bc_size))
			: llvm::StringRef(&_binary_asm_funcs_64_bc_start, reinterpret_cast<size_t>(&_binary_asm_funcs_64_bc_size)) ),
	};

	// Link stdlib with result module.
	for( const llvm::StringRef& asm_funcs_module : asm_funcs_modules )
	{
		const llvm::ErrorOr<std::unique_ptr<llvm::Module>> std_lib_module=
			llvm::parseBitcodeFile(
				llvm::MemoryBufferRef( asm_funcs_module, "ustlib asm file" ),
				result_module->getContext() );

		if( !std_lib_module )
		{
			std::cout << "Internal compiler error - stdlib module parse error" << std::endl;
			return 1;
		}

		std_lib_module.get()->setDataLayout( data_layout );
		std_lib_module.get()->setTargetTriple( target_triple_str );

		std::string err_stream_str;
		llvm::raw_string_ostream err_stream( err_stream_str );
		if( llvm::verifyModule( *std_lib_module.get(), &err_stream ) )
		{
			std::cout << "Internal compiler error - stdlib module verify error:\n" << err_stream.str() << std::endl;
			return 1;
		}

		llvm::Linker::LinkModules( result_module.get(), std_lib_module.get().get() );
	}

	if( optimization_level > 0u || size_optimization_level > 0u )
	{
		llvm::legacy::FunctionPassManager function_pass_manager( result_module.get() );
		llvm::legacy::PassManager pass_manager;

		{
			llvm::PassManagerBuilder pass_manager_builder;

			pass_manager_builder.OptLevel = optimization_level;
			pass_manager_builder.SizeLevel = size_optimization_level;

			if( optimization_level == 0u )
				pass_manager_builder.Inliner= nullptr;
			else
				pass_manager_builder.Inliner= llvm::createFunctionInliningPass( optimization_level, size_optimization_level );

			pass_manager_builder.populateFunctionPassManager(function_pass_manager);
			pass_manager_builder.populateModulePassManager(pass_manager);
		}

		// Run per-function optimizations.
		function_pass_manager.doInitialization();
		for( llvm::Function& func : *result_module )
			function_pass_manager.run(func);
		function_pass_manager.doFinalization();

		// Run optimizations for module.
		pass_manager.run( *result_module );
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

		llvm::legacy::PassManager pass_manager;

		if( target_machine->addPassesToEmitFile(
				pass_manager,
				out_file_stream,
				file_type,
				no_verify ) )
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
