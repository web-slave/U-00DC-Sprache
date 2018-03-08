#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "program_string.hpp"

namespace U
{

struct FilePos
{
	unsigned short line; // from 1
	unsigned short pos_in_line;
	unsigned short file_index;
};

bool operator==( const FilePos& l, const FilePos& r );
bool operator!=( const FilePos& l, const FilePos& r );

struct Lexem
{
	enum class Type
	{
		None,

		Identifier,
		String,
		Number,

		BracketLeft, // (
		BracketRight, // )

		SquareBracketLeft, // [
		SquareBracketRight, // ]

		BraceLeft, // {
		BraceRight, // }

		TemplateBracketLeft , // </
		TemplateBracketRight, // />

		Scope, // ::

		Comma, // ,
		Dot, // .
		Colon, // :
		Semicolon, // ;

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

		LeftArrow, // <-

		// TODO - add other lexems.

		EndOfFile,
	};

	Type type= Type::None;
	ProgramString text;

	FilePos file_pos;
};

typedef std::vector<Lexem> Lexems;

typedef std::string LexicalErrorMessage;
typedef std::vector<LexicalErrorMessage> LexicalErrorMessages;

struct LexicalAnalysisResult
{
	Lexems lexems;
	LexicalErrorMessages error_messages;
};

LexicalAnalysisResult LexicalAnalysis( const ProgramString& program_text );

} // namespace U
