#ifndef QD_QC_AST_NODE_GLOBAL_VAR_H
#define QD_QC_AST_NODE_GLOBAL_VAR_H

#include "ast_node_base.h"
#include <memory>
#include <string>

namespace Qd {
	/// Module-level mutable variable, initialized once at program start.
	/// Form: `var name:type = literal` (or `pub var`).
	/// Reads compile to a load of an LLVM global; `-> name` compiles to a store.
	/// Unlike `const`, the value can change across function calls.
	/// A struct-construction initializer is stored as a child AST node (see
	/// `initializerNode`) and run inside the generated module-init sequence
	/// before `main` executes.
	class AstNodeGlobalVar : public AstNodeBase<IAstNode::Type::GLOBAL_VAR_DECLARATION> {
	public:
		AstNodeGlobalVar(const std::string& name, const std::string& typeName, const char* value, bool isPublic = false)
			: mName(name), mTypeName(typeName), mValue(value), mSourceExpr(value), mIsPublic(isPublic) {
		}

		size_t childCount() const override {
			return mInitializerNode ? 1 : 0;
		}

		IAstNode* child(size_t i) const override {
			if (i == 0 && mInitializerNode) {
				return mInitializerNode.get();
			}
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

		// True when the source wrote `:type` explicitly. Lets the formatter
		// roundtrip inferred declarations (`var x = 0`) without adding an
		// annotation the user didn't write.
		bool hasExplicitType() const {
			return mHasExplicitType;
		}

		void setHasExplicitType(bool explicitType) {
			mHasExplicitType = explicitType;
		}

		// Optional AST initializer — used for struct construction
		// (`var p = Point { x = 1 y = 2 }`). When present, codegen emits a
		// null pointer for the LLVM global and a module-init sequence that
		// runs the construction and stores the resulting pointer.
		IAstNode* initializerNode() const {
			return mInitializerNode.get();
		}

		// Takes ownership of `node`.
		void setInitializerNode(IAstNode* node) {
			mInitializerNode.reset(node);
			if (node) {
				node->setParent(this);
			}
		}

	private:
		std::string mName;
		std::string mTypeName;
		std::string mValue;
		std::string mSourceExpr;
		bool mIsPublic;
		bool mHasExplicitType = true;
		std::unique_ptr<IAstNode> mInitializerNode;
	};
}

#endif
