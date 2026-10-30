#ifndef QD_QC_AST_TYPES_H
#define QD_QC_AST_TYPES_H

namespace Qd {
	// Cast direction for implicit casts between numeric types
	enum class CastDirection {
		NONE,
		INT_TO_FLOAT, // casti -> castf
		FLOAT_TO_INT, // castf -> casti
		INT_TO_D128,  // casti -> castd, exact: every i64 fits in 34 digits
		// An `any` argument, checked when the call runs: it must hold the parameter's type, or
		// an i64 where the parameter takes f64 or d128, which widens
		ANY_TO_INT,
		ANY_TO_FLOAT,
		ANY_TO_D128,
		ANY_TO_STR,
		ANY_TO_PTR
	};
}

#endif // QD_QC_AST_TYPES_H
