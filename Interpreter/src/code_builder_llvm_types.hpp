#pragma once
#include <memory>
#include <vector>
#include <map>

#include <boost/variant.hpp>

#include "push_disable_llvm_warnings.hpp"
#include <llvm/IR/Function.h>
#include "pop_llvm_warnings.hpp"

#include "lang_types.hpp"
#include "program_string.hpp"

namespace Interpreter
{

namespace CodeBuilderLLVMPrivate
{

struct Function;
typedef std::unique_ptr<Function> FunctionPtr;

struct Array;
typedef std::unique_ptr<Array> ArrayPtr;

struct Class;
typedef std::shared_ptr<Class> ClassPtr;

struct FundamentalType final
{
	U_FundamentalType fundamental_type;
	llvm::Type* llvm_type;

	FundamentalType( U_FundamentalType fundamental_type= U_FundamentalType::Void, llvm::Type* llvm_type= nullptr );
};

bool operator==( const FundamentalType& r, const FundamentalType& l );
bool operator!=( const FundamentalType& r, const FundamentalType& l );

struct Type final
{
	boost::variant<
		FundamentalType,
		FunctionPtr,
		ArrayPtr,
		ClassPtr> one_of_type_kind;

	Type()= default;
	Type( const Type& other );
	Type( Type&& )= default;

	Type& operator=( const Type& other );
	Type& operator=( Type&& )= default;

	// TODO - does this method needs?
	size_t SizeOf() const;

	llvm::Type* GetLLVMType() const;
	ProgramString ToString() const;
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
	Type type;
	size_t size= 0u;

	llvm::ArrayType* llvm_type= nullptr;
};

bool operator==( const Array& r, const Array& l );
bool operator!=( const Array& r, const Array& l );

struct Class final
{
	Class();
	~Class();

	Class( const Class& )= delete;
	Class( Class&& )= delete;

	Class& operator=( const Class& )= delete;
	Class& operator=( Class&& )= delete;

	struct Field
	{
		ProgramString name;
		Type type;
		unsigned int index;
	};

	const Field* GetField( const ProgramString& name );

	ProgramString name;
	std::vector<Field> fields;

	llvm::StructType* llvm_type;
};

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
		Global,
		PointerToStack,
		LLVMRegister,
	};

	Location location;
	ValueType value_type;
	Type type;

	llvm::Value* llvm_value;
};

struct Name final
{
	// If ptr not null - name is class, else - variable
	ClassPtr class_;
	Variable variable;
};

class NamesScope final
{
public:

	typedef std::map< ProgramString, Name > NamesMap;
	typedef NamesMap::value_type InsertedName;

	NamesScope( const NamesScope* prev= nullptr );

	const InsertedName* AddName( const ProgramString& name, Variable variable );
	const InsertedName* AddName( const ProgramString& name, const ClassPtr& class_ );
	const InsertedName* AddName( const ProgramString& name, const Name name_value );

	const InsertedName* GetName( const ProgramString& name ) const;

private:
	const NamesScope* const prev_;
	NamesMap names_map_;
};

class ProgramError final : public std::exception
{
public:
	virtual ~ProgramError() override{}

	virtual const char* what() const noexcept override
	{
		return "ProgramError";
	}
};

const ProgramString& GetFundamentalTypeName( U_FundamentalType fundamental_type );
const char* GetFundamentalTypeNameASCII( U_FundamentalType fundamental_type );

} //namespace CodeBuilderLLVMPrivate

} // namespace Interpreter
