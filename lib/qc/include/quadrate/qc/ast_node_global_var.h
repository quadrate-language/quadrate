#ifndef QD_QC_AST_NODE_GLOBAL_VAR_H
#define QD_QC_AST_NODE_GLOBAL_VAR_H

#include "ast_node_base.h"
#include <string>

namespace Qd {
	/// Module-level mutable variable, initialized once at program start.
	/// Form: `var name:type = literal` (or `pub var`).
	/// Reads compile to a load of an LLVM global; `-> name` compiles to a store.
	/// Unlike `const`, the value can change across function calls.
	class AstNodeGlobalVar : public AstNodeBase<IAstNode::Type::GLOBAL_VAR_DECLARATION> {
	public:
		AstNodeGlobalVar(const std::string& name, const std::string& typeName, const char* value, bool isPublic = false)
			: mName(name), mTypeName(typeName), mValue(value), mSourceExpr(value), mIsPublic(isPublic) {
		}

		size_t childCount() const override {
			return 0;
		}

		IAstNode* child(size_t) const override {
			return nullptr;
		}

		const std::string& name() const {
			return mName;
		}

		const std::string& typeName() const {
			return mTypeName;
		}

		// Resolved literal — what codegen consumes. If the source was a const
		// reference, this is the const's value; otherwise it's the literal as
		// written.
		const char* value() const {
			return mValue.c_str();
		}

		// Original initializer as written (const name or literal). Used by
		// the formatter to round-trip symbolic references.
		const std::string& sourceExpr() const {
			return mSourceExpr;
		}

		void setSourceExpr(const std::string& expr) {
			mSourceExpr = expr;
		}

		bool isPublic() const {
			return mIsPublic;
		}

	private:
		std::string mName;
		std::string mTypeName;
		std::string mValue;
		std::string mSourceExpr;
		bool mIsPublic;
	};
}

#endif
