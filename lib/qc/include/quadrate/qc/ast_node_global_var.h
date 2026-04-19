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
		AstNodeGlobalVar(
				const std::string& name, const std::string& typeName, const char* value, bool isPublic = false)
			: mName(name), mTypeName(typeName), mValue(value), mIsPublic(isPublic) {
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

		const char* value() const {
			return mValue.c_str();
		}

		bool isPublic() const {
			return mIsPublic;
		}

	private:
		std::string mName;
		std::string mTypeName;
		std::string mValue;
		bool mIsPublic;
	};
}

#endif
