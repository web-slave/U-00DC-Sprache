#pragma once
#include <cstdint>
#include <string>
#include <vector>


namespace U
{

struct FilePos
{
	unsigned short file_index;
	unsigned short line; // from 1
	unsigned short pos_in_line;
};

bool operator==( const FilePos& l, const FilePos& r );
bool operator!=( const FilePos& l, const FilePos& r );
bool operator< ( const FilePos& l, const FilePos& r );
bool operator<=( const FilePos& l, const FilePos& r );

struct Lexem
{
	enum class Type : uint8_t
	{
		None,

		Comment,

		Identifier,
		MacroIdentifier,
		MacroUniqueIdentifier,
		String,
		Number,

		LiteralSuffix, // For strings, numbers

		BracketLeft, // (
		BracketRight, // )

		SquareBracketLeft, // [
		SquareBracketRight, // ]

		BraceLeft, // {
		BraceRight, // }

		TemplateBracketLeft , // </
		TemplateBracketRight, // />

		MacroBracketLeft,  // <?
		MacroBracketRight, // ?>

		Scope, // ::

		Comma, // ,
		Dot, // .
		Colon, // :
		Semicolon, // ;
		Question, // ?

		Assignment, // =
		Plus, // +
		Minus, // -
		Star, // *
		Slash, // /
		Percent, // %

		And, // &
		Or, // |
		Xor, // ^
		Tilda, // ~
		Not, // !

		Apostrophe, // '

		Increment, // ++
		Decrement, // --

		CompareLess, // <
		CompareGreater, // >
		CompareEqual, // ==
		CompareNotEqual, // !=
		CompareLessOrEqual, // <=
		CompareGreaterOrEqual, // >=

		Conjunction, // &&
		Disjunction, // ||

		AssignAdd, // +=
		AssignSub, // -=
		AssignMul, // *=
		AssignDiv, // /=
		AssignRem, // %=
		AssignAnd, // &=
		AssignOr,  // |=
		AssignXor, // ^=

		ShiftLeft , // <<
		ShiftRight, // >>

		AssignShiftLeft , // <<=
		AssignShiftRight, // >>=

		LeftArrow,  // <-
		RightArrow, // ->

		Ellipsis, // ...

		// TODO - add other lexems.

		EndOfFile,
	};

	std::string text; // Non-empty for identifiers, strings, numbers. Empty for simple lexems.
	FilePos file_pos;
	Type type= Type::None;
};

bool operator==(const Lexem& l, const Lexem& r );
bool operator!=(const Lexem& l, const Lexem& r );

using Lexems= std::vector<Lexem>;

using LexicalErrorMessage= std::string;
using LexicalErrorMessages= std::vector<LexicalErrorMessage>;

struct LexicalAnalysisResult
{
	Lexems lexems;
	LexicalErrorMessages error_messages;
};

LexicalAnalysisResult LexicalAnalysis( const std::string& program_text, bool collect_comments= false );
LexicalAnalysisResult LexicalAnalysis( const char* program_text_data, size_t program_text_size, bool collect_comments= false );

} // namespace U
