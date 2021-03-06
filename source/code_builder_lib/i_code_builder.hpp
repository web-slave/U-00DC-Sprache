#pragma once
#include <vector>

#include "push_disable_llvm_warnings.hpp"
#include <llvm/IR/Module.h>
#include "pop_llvm_warnings.hpp"

#include "../lex_synt_lib/source_graph_loader.hpp"

namespace U
{

struct CodeBuilderError;

// Use interface class for boost of compilation.
// All tests will be not recompiled after minor changes in CodeBuilder implementation headers.
class ICodeBuilder
{
public:
	virtual ~ICodeBuilder()= default;

	struct BuildResult
	{
		std::vector<CodeBuilderError> errors;
		std::unique_ptr<llvm::Module> module;
	};

	virtual BuildResult BuildProgram( const SourceGraph& source_graph )= 0;
};

} // namespace U
