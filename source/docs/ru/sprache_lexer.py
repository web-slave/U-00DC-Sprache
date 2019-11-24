from pygments.lexer import RegexLexer
from pygments import token

# Simple regexp-based lexer for Ü.

class SpracheLexer(RegexLexer):
	name = 'u_spr'
	aliases = ['u_spr']
	filenames = ['*.u']
	
	tokens = {
		'root': [
			# Comments
			(r'//[^\n]*\n', token.Comment),
			# Strings
			(r'\"([^\\\"]|(\\n)|(\\r)|(\\t)|(\\b)|(\\f)|(\\\")|(\\0)|(\\\\)|(\\u[0-9a-fA-F]{4,4}))*\"', token.String),
			# Whitespaces
			(r'[\ \t\n\r]+', token.Whitespace),
			# Numbers
			(r'0b[0-1]+(\.[0-1]+)?', token.Number),
			(r'0o[0-7]+(\.[0-7]+)?', token.Number),
			(r'0x[0-9a-fA-F]+(\.[0-9a-fA-F]+)?', token.Number),
			(r'[0-9]+(\.[0-9]+)?(e((-)|(\+))?[0-9]+)?', token.Number),
			# Keywords
			(r'fn[^A-Za-z_0-9]', token.Keyword),
			(r'op[^A-Za-z_0-9]', token.Keyword),
			(r'var[^A-Za-z_0-9]', token.Keyword),
			(r'auto[^A-Za-z_0-9]', token.Keyword),
			(r'lock_temps[^A-Za-z_0-9]', token.Keyword),
			(r'return[^A-Za-z_0-9]', token.Keyword),
			(r'while[^A-Za-z_0-9]', token.Keyword),
			(r'break[^A-Za-z_0-9]', token.Keyword),
			(r'continue[^A-Za-z_0-9]', token.Keyword),
			(r'if[^A-Za-z_0-9]', token.Keyword),
			(r'static_if[^A-Za-z_0-9]', token.Keyword),
			(r'enable_if[^A-Za-z_0-9]', token.Keyword),
			(r'else[^A-Za-z_0-9]', token.Keyword),
			(r'move[^A-Za-z_0-9]', token.Keyword),
			(r'select[^A-Za-z_0-9]', token.Keyword),
			(r'tup[^A-Za-z_0-9]', token.Keyword),
			(r'struct[^A-Za-z_0-9]', token.Keyword),
			(r'class[^A-Za-z_0-9]', token.Keyword),
			(r'final[^A-Za-z_0-9]', token.Keyword),
			(r'polymorph[^A-Za-z_0-9]', token.Keyword),
			(r'interface[^A-Za-z_0-9]', token.Keyword),
			(r'abstract[^A-Za-z_0-9]', token.Keyword),
			(r'ordered[^A-Za-z_0-9]', token.Keyword),
			(r'nomangle[^A-Za-z_0-9]', token.Keyword),
			(r'virtual[^A-Za-z_0-9]', token.Keyword),
			(r'override[^A-Za-z_0-9]', token.Keyword),
			(r'pure[^A-Za-z_0-9]', token.Keyword),
			(r'namespace[^A-Za-z_0-9]', token.Keyword),
			(r'public[^A-Za-z_0-9]', token.Keyword),
			(r'private[^A-Za-z_0-9]', token.Keyword),
			(r'protected[^A-Za-z_0-9]', token.Keyword),
			(r'void[^A-Za-z_0-9]', token.Keyword),
			(r'bool[^A-Za-z_0-9]', token.Keyword),
			(r'i8[^A-Za-z_0-9]', token.Keyword),
			(r'u8[^A-Za-z_0-9]', token.Keyword),
			(r'i16[^A-Za-z_0-9]', token.Keyword),
			(r'u16[^A-Za-z_0-9]', token.Keyword),
			(r'i32[^A-Za-z_0-9]', token.Keyword),
			(r'u32[^A-Za-z_0-9]', token.Keyword),
			(r'i64[^A-Za-z_0-9]', token.Keyword),
			(r'u64[^A-Za-z_0-9]', token.Keyword),
			(r'i128[^A-Za-z_0-9]', token.Keyword),
			(r'u128[^A-Za-z_0-9]', token.Keyword),
			(r'f32[^A-Za-z_0-9]', token.Keyword),
			(r'f64[^A-Za-z_0-9]', token.Keyword),
			(r'char8[^A-Za-z_0-9]', token.Keyword),
			(r'char16[^A-Za-z_0-9]', token.Keyword),
			(r'char32[^A-Za-z_0-9]', token.Keyword),
			(r'size_type[^A-Za-z_0-9]', token.Keyword),
			(r'true[^A-Za-z_0-9]', token.Keyword),
			(r'false[^A-Za-z_0-9]', token.Keyword),
			(r'mut[^A-Za-z_0-9]', token.Keyword),
			(r'imut[^A-Za-z_0-9]', token.Keyword),
			(r'constexpr[^A-Za-z_0-9]', token.Keyword),
			(r'zero_init[^A-Za-z_0-9]', token.Keyword),
			(r'uninitialized[^A-Za-z_0-9]', token.Keyword),
			(r'this[^A-Za-z_0-9]', token.Keyword),
			(r'base[^A-Za-z_0-9]', token.Keyword),
			(r'constructor[^A-Za-z_0-9]', token.Keyword),
			(r'destructor[^A-Za-z_0-9]', token.Keyword),
			(r'conversion_constructor[^A-Za-z_0-9]', token.Keyword),
			(r'static_assert[^A-Za-z_0-9]', token.Keyword),
			(r'halt[^A-Za-z_0-9]', token.Keyword),
			(r'safe[^A-Za-z_0-9]', token.Keyword),
			(r'unsafe[^A-Za-z_0-9]', token.Keyword),
			(r'type[^A-Za-z_0-9]', token.Keyword),
			(r'typeinfo[^A-Za-z_0-9]', token.Keyword),
			(r'typeof[^A-Za-z_0-9]', token.Keyword),
			(r'template[^A-Za-z_0-9]', token.Keyword),
			(r'enum[^A-Za-z_0-9]', token.Keyword),
			(r'cast_ref[^A-Za-z_0-9]', token.Keyword),
			(r'cast_ref_unsafe[^A-Za-z_0-9]', token.Keyword),
			(r'cast_imut[^A-Za-z_0-9]', token.Keyword),
			(r'cast_mut[^A-Za-z_0-9]', token.Keyword),
			(r'import[^A-Za-z_0-9]', token.Keyword),
			(r'default[^A-Za-z_0-9]', token.Keyword),
			(r'delete[^A-Za-z_0-9]', token.Keyword),
			(r'for[^A-Za-z_0-9]', token.Keyword),
			# Identifiers
			(r'[a-zA-Z][a-zA-Z_0-9]*', token.Name),
			# Other lexems
			(r'(\()|(\)|(\[)|(\])|(\{)|(\}))', token.Punctuation),
			(r'[,.:;?=+\-*/%<>&|\^~!\']', token.Operator),
			(r'(</)|(/>)|(::)|(\+\+)|(--)|(==)|(!=)|(<=)|(>=)|(&&)|(\|\|)|(\+=)|(-=)|(\*=)|(/=)|(%=)|(&=)|(\|=)|(\^=)|(<<)|(>>)|(<-)|(->)', token.Operator),
			(r'(<<=)|(>>=)|(\.\.\.)', token.Operator),
		]
	}
