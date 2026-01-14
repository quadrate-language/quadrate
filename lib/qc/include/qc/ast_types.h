#ifndef QD_QC_AST_TYPES_H
#define QD_QC_AST_TYPES_H

namespace Qd {
	// Cast direction for implicit casts between numeric types
	enum class CastDirection {
		NONE,
		INT_TO_FLOAT, // casti -> castf
		FLOAT_TO_INT  // castf -> casti
	};
}

#endif // QD_QC_AST_TYPES_H
