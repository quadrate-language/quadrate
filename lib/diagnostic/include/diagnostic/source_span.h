#ifndef QD_DIAGNOSTIC_SOURCE_SPAN_H
#define QD_DIAGNOSTIC_SOURCE_SPAN_H

#include <cstdint>

namespace Qd {
	struct SourceSpan {
		int32_t column;
		int32_t length;
		int32_t line;
		int32_t offset;
	};
}

#endif
