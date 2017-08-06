#pragma once
#include "code_builder_types.hpp"

namespace U
{

namespace CodeBuilderPrivate
{

// Mangling with Itanium ABI rules.

std::string MangleFunction(
	const NamesScope& parent_scope,
	const ProgramString& function_name,
	const Function& function_type );

std::string MangleClass(
	const NamesScope& parent_scope,
	const ProgramString& class_name );

} // namespace CodeBuilderPrivate

} // namespace U