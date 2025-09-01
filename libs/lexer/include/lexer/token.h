#ifndef QD_LEXER_TOKEN_H
#define QD_LEXER_TOKEN_H

#include <diagnostic/source_span.h>
#include <string>

namespace Qd {
	enum class TokenType {
		Function,
		LBrace,
		LParen,
		RBrace,
		RParen
	};

	struct Token {
		SourceSpan span;
		TokenType type;
		std::string value;
	};
}

#endif

