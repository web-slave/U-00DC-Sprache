#pragma once
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include "push_disable_llvm_warnings.hpp"
#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include "pop_llvm_warnings.hpp"

#include "lang_types.hpp"
#include "program_string.hpp"
#include "syntax_elements.hpp"

namespace U
{

namespace CodeBuilderPrivate
{

struct Function;
struct Array;

struct Class;
typedef std::shared_ptr<Class> ClassPtr;
typedef std::weak_ptr<Class> ClassWeakPtr;

class NamesScope;
typedef std::shared_ptr<NamesScope> NamesScopePtr;

struct TypeTemplate;
typedef std::shared_ptr<TypeTemplate> TypeTemplatePtr;

struct FundamentalType final
{
	U_FundamentalType fundamental_type;
	llvm::Type* llvm_type;

	FundamentalType( U_FundamentalType fundamental_type= U_FundamentalType::Void, llvm::Type* llvm_type= nullptr );
};

struct TemplateDependentType
{
	size_t index;
	llvm::Type* llvm_type;

	TemplateDependentType( size_t in_index, llvm::Type* in_llvm_type );
};

// Stub for type of non-variable "Values".
enum class NontypeStub
{
	OverloadedFunctionsSet,
	ThisOverloadedMethodsSet,
	TypeName,
	Namespace,
	TypeTemplate,
	TemplateDependentValue,
	YetNotDeducedTemplateArg,
	ErrorValue,
};

bool operator==( const FundamentalType& r, const FundamentalType& l );
bool operator!=( const FundamentalType& r, const FundamentalType& l );

bool operator==( const TemplateDependentType& r, const TemplateDependentType& l );
bool operator!=( const TemplateDependentType& r, const TemplateDependentType& l );

class Type final
{
public:
	Type()= default;
	Type( const Type& other );
	Type( Type&& )= default;

	Type& operator=( const Type& other );
	Type& operator=( Type&& )= default;

	// Construct from different type kinds.
	Type( FundamentalType fundamental_type );
	Type( const Function& function_type );
	Type( Function&& function_type );
	Type( const Array& array_type );
	Type( Array&& array_type );
	Type( ClassPtr class_type );
	Type( NontypeStub nontype_strub );
	Type( TemplateDependentType template_dependent_type );

	// Get different type kinds.
	FundamentalType* GetFundamentalType();
	const FundamentalType* GetFundamentalType() const;
	Function* GetFunctionType();
	const Function* GetFunctionType() const;
	Array* GetArrayType();
	const Array* GetArrayType() const;
	ClassPtr GetClassType() const;
	TemplateDependentType* GetTemplateDependentType();
	const TemplateDependentType* GetTemplateDependentType() const;

	// TODO - does this method needs?
	size_t SizeOf() const;

	bool IsIncomplete() const;
	bool IsDefaultConstructible() const;
	bool IsCopyConstructible() const;
	bool HaveDestructor() const;
	bool CanBeConstexpr() const;

	llvm::Type* GetLLVMType() const;
	ProgramString ToString() const;

private:
	friend bool operator==( const Type&, const Type&);

	typedef std::unique_ptr<Function> FunctionPtr;
	typedef std::unique_ptr<Array> ArrayPtr;

	boost::variant<
		FundamentalType,
		FunctionPtr,
		ArrayPtr,
		ClassPtr,
		NontypeStub,
		TemplateDependentType> something_;
};

bool operator==( const Type& r, const Type& l );
bool operator!=( const Type& r, const Type& l );

struct Function final
{
	struct Arg
	{
		Type type;
		bool is_reference;
		bool is_mutable;
	};

	Type return_type;
	bool return_value_is_reference= false;
	bool return_value_is_mutable= false;
	std::vector<Arg> args;

	llvm::FunctionType* llvm_function_type= nullptr;
};

bool operator==( const Function::Arg& r, const Function::Arg& l );
bool operator!=( const Function::Arg& r, const Function::Arg& l );
bool operator==( const Function& r, const Function& l );
bool operator!=( const Function& r, const Function& l );

struct Array final
{
	// "size" in case, when size is not known yet, when size depends on template parameter, for example.
	static constexpr size_t c_undefined_size= std::numeric_limits<size_t>::max();

	Type type;
	size_t size= c_undefined_size;

	llvm::ArrayType* llvm_type= nullptr;

	size_t ArraySizeOrZero() const { return size == c_undefined_size ? 0u : size; }
};

bool operator==( const Array& r, const Array& l );
bool operator!=( const Array& r, const Array& l );

struct FunctionVariable final
{
	Type type; // Function type 100%
	bool have_body= true;
	bool is_this_call= false;
	bool is_generated= false;
	bool return_value_is_sret= false;

	llvm::Function* llvm_function= nullptr;

	// TODO - fill this.
	FilePos file_pos;
};

// Set of functions with same name, but different signature.
typedef std::vector<FunctionVariable> OverloadedFunctionsSet;

enum class ValueType
{
	Value,
	Reference,
	ConstReference,
};

struct Variable final
{
	enum class Location
	{
		Pointer,
		LLVMRegister,
	};

	Type type;
	Location location= Location::Pointer;
	ValueType value_type= ValueType::ConstReference;
	llvm::Value* llvm_value= nullptr;

	// Exists only for constant expressions of fundamental types.
	llvm::Constant* constexpr_value= nullptr;
};

struct ClassField final
{
	Type type;
	unsigned int index= 0u;
	ClassWeakPtr class_;
};

// "this" + functions set of class of "this"
struct ThisOverloadedMethodsSet final
{
	Variable this_;
	OverloadedFunctionsSet overloaded_methods_set;
};

struct TemplateDependentValue final
{};

struct YetNotDeducedTemplateArg final
{};

struct ErrorValue final
{};

class Value final
{
public:
	Value();
	Value( Variable variable, const FilePos& file_pos );
	Value( FunctionVariable function_variable );
	Value( OverloadedFunctionsSet functions_set );
	Value( Type type, const FilePos& file_pos );
	Value( ClassField class_field, const FilePos& file_pos );
	Value( ThisOverloadedMethodsSet class_field );
	Value( const NamesScopePtr& namespace_, const FilePos& file_pos );
	Value( const TypeTemplatePtr& type_template, const FilePos& file_pos );
	Value( TemplateDependentValue template_dependent_value );
	Value( YetNotDeducedTemplateArg yet_not_deduced_template_arg );
	Value( ErrorValue error_value );

	const Type& GetType() const;

	// Fundamental, class, array types
	Variable* GetVariable();
	const Variable* GetVariable() const;
	// Function types
	FunctionVariable* GetFunctionVariable();
	const FunctionVariable* GetFunctionVariable() const;
	// Function set stub type
	OverloadedFunctionsSet* GetFunctionsSet();
	const OverloadedFunctionsSet* GetFunctionsSet() const;
	// Typename
	Type* GetTypeName();
	const Type* GetTypeName() const;
	// Class fields
	const ClassField* GetClassField() const;
	// This + methods set
	ThisOverloadedMethodsSet* GetThisOverloadedMethodsSet();
	const ThisOverloadedMethodsSet* GetThisOverloadedMethodsSet() const;
	// Namespace
	NamesScopePtr GetNamespace() const;
	// Class Template
	TypeTemplatePtr GetTypeTemplate() const;
	// Template-dependent value
	TemplateDependentValue* GetTemplateDependentValue();
	const TemplateDependentValue* GetTemplateDependentValue() const;
	// Yet not deduced template arg
	YetNotDeducedTemplateArg* GetYetNotDeducedTemplateArg();
	const YetNotDeducedTemplateArg* GetYetNotDeducedTemplateArg() const;
	// Error value
	ErrorValue* GetErrorValue();
	const ErrorValue* GetErrorValue() const;

private:
	boost::variant<
		Variable,
		FunctionVariable,
		OverloadedFunctionsSet,
		Type,
		ClassField,
		ThisOverloadedMethodsSet,
		NamesScopePtr,
		TypeTemplatePtr,
		TemplateDependentValue,
		YetNotDeducedTemplateArg,
		ErrorValue > something_;

	// File_pos used as unique id for entry, needed for imports merging.
	// Two values are 100% same, if their file_pos are identical.
	// Not for all values file_pos required, so, fill it with zeros for it.
	FilePos file_pos_= { 0u, 0u, 0u };
};

// "Class" of function argument in terms of overloading.
enum class ArgOverloadingClass
{
	// Value-args (both mutable and immutable), immutable references.
	ImmutableReference,
	// Mutable references.
	MutalbeReference,
	// SPRACHE_TODO - add class for move-references here
};

ArgOverloadingClass GetArgOverloadingClass( bool is_reference, bool is_mutable );
ArgOverloadingClass GetArgOverloadingClass( ValueType value_type );
ArgOverloadingClass GetArgOverloadingClass( const Function::Arg& arg );

class NamesScope final
{
public:

	typedef std::map< ProgramString, Value > NamesMap;
	typedef NamesMap::value_type InsertedName;

	NamesScope(
		ProgramString name,
		const NamesScope* parent );

	NamesScope( const NamesScope&)= delete;
	NamesScope& operator=( const NamesScope&)= delete;

	bool IsAncestorFor( const NamesScope& other ) const;
	const ProgramString& GetThisNamespaceName() const;
	void SetThisNamespaceName( ProgramString name );

	// Returns nullptr, if name already exists in this scope.
	InsertedName* AddName( const ProgramString& name, Value value );

	// Resolve simple name only in this scope.
	InsertedName* GetThisScopeName( const ProgramString& name ) const;
	InsertedName& GetTemplateDependentValue();

	const NamesScope* GetParent() const;
	const NamesScope* GetRoot() const;
	void SetParent( const NamesScope* parent );

	template<class Func>
	void ForEachInThisScope( const Func& func ) const
	{
		for( const InsertedName& inserted_name : names_map_ )
			func( inserted_name );
	}

	// TODO - maybe add for_each in all scopes?

private:
	ProgramString name_;
	const NamesScope* parent_;
	NamesMap names_map_;
};

struct NameResolvingKey final
{
	const ComplexName::Component* components;
	size_t component_count;
};

struct NameResolvingKeyHasher
{
	size_t operator()( const NameResolvingKey& key ) const;
	bool operator()( const NameResolvingKey& a, const NameResolvingKey& b ) const;
};

bool NameResolvingKeyCompare( const NameResolvingKey& a, const NameResolvingKey& b );

struct ResolvingCacheValue final
{
	NamesScope::InsertedName name;
	NamesScope* parent_namespace;
	size_t name_components_cut;
};

typedef
	std::unordered_map<
		NameResolvingKey,
		ResolvingCacheValue,
		NameResolvingKeyHasher,
		NameResolvingKeyHasher > ResolvingCache;

typedef boost::variant< Variable, Type > TemplateParameter;

struct Class final
{
	Class( const ProgramString& name, const NamesScope* parent_scope );
	~Class();

	Class( const Class& )= delete;
	Class( Class&& )= delete;

	Class& operator=( const Class& )= delete;
	Class& operator=( Class&& )= delete;

	NamesScope members;
	size_t field_count= 0u;
	bool is_incomplete= true;
	bool have_explicit_noncopy_constructors= false;
	bool is_default_constructible= false;
	bool is_copy_constructible= false;
	bool have_destructor= false;

	llvm::StructType* llvm_type;

	struct BaseTemplate
	{
		TypeTemplatePtr class_template;
		std::vector<TemplateParameter> template_parameters;
	};

	// Exists only for classes, generated from class templates.
	boost::optional<BaseTemplate> base_template;
};

struct TypeTemplate final
{
	struct TemplateParameter
	{
		ProgramString name;
		const ComplexName* type_name= nullptr; // Exists for value parameters.
	};

	// Sorted in order of first parameter usage in signature.
	std::vector< TemplateParameter > template_parameters;

	std::vector< const ComplexName* > signature_arguments;
	std::vector< const ComplexName* > default_signature_arguments;
	size_t first_optional_signature_argument= ~0u;

	// Store syntax tree element for instantiation.
	// Syntax tree must live longer, than this struct.
	const TemplateBase* syntax_element= nullptr;

	ResolvingCache resolving_cache;
};

typedef boost::variant< int, Type, Variable > DeducibleTemplateParameter; // int means not deduced
typedef std::vector<DeducibleTemplateParameter> DeducibleTemplateParameters;

const ProgramString& GetFundamentalTypeName( U_FundamentalType fundamental_type );
const char* GetFundamentalTypeNameASCII( U_FundamentalType fundamental_type );

} //namespace CodeBuilderLLVMPrivate

} // namespace U
