#ifndef QD_LEXER_TOKEN_H
#define QD_LEXER_TOKEN_H

#include <diagnostic/source_span.h>
#include <string>

namespace Qd {
	struct Token {
		SourceSpan span;
		std::string type;
		std::string value;
	};
}

#endif

