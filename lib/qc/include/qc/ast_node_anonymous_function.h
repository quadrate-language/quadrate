#ifndef QD_QC_AST_NODE_ANONYMOUS_FUNCTION_H
#define QD_QC_AST_NODE_ANONYMOUS_FUNCTION_H

#include "ast_node.h"
#include <string>
#include <vector>

namespace Qd {
	/**
	 * AST node for anonymous functions (quotations/lambdas).
	 *
	 * Syntax: fn (x:i64 y:i64 -- r:i64) { body }
	 *
	 * This node represents an inline function definition that produces
	 * a function pointer when evaluated. Unlike regular function declarations,
	 * anonymous functions appear as expressions within other code.
	 *
	 * Code generation will:
	 * 1. Generate a unique name (e.g., __anon_001)
	 * 2. Emit the function definition
	 * 3. Push a pointer to that function onto the stack
	 */
	class AstNodeAnonymousFunction : public IAstNode {
	public:
		AstNodeAnonymousFunction() : mParent(nullptr), mBody(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeAnonymousFunction() {
			if (mBody) {
				delete mBody;
			}
			for (auto* param : mInputParameters) {
				delete param;
			}
			for (auto* param : mOutputParameters) {
				delete param;
			}
		}

		IAstNode::Type type() const override {
			return Type::ANONYMOUS_FUNCTION;
		}

		size_t childCount() const override {
			size_t count = 0;
			count += mInputParameters.size();
			count += mOutputParameters.size();
			if (mBody) {
				count++;
			}
			return count;
		}

		IAstNode* child(size_t index) const override {
			size_t currentIndex = 0;
			for (auto* param : mInputParameters) {
				if (index == currentIndex++) {
					return param;
				}
			}
			for (auto* param : mOutputParameters) {
				if (index == currentIndex++) {
					return param;
				}
			}
			if (mBody && index == currentIndex++) {
				return mBody;
			}
			return nullptr;
		}

		IAstNode* parent() const override {
			return mParent;
		}

		void setParent(IAstNode* parent) override {
			mParent = parent;
		}

		size_t line() const override {
			return mLine;
		}

		size_t column() const override {
			return mColumn;
		}

		void setPosition(size_t line, size_t column) override {
			mLine = line;
			mColumn = column;
		}

		void setBody(IAstNode* body) {
			mBody = body;
		}

		IAstNode* body() const {
			return mBody;
		}

		void addInputParameter(IAstNode* param) {
			mInputParameters.push_back(param);
		}

		void addOutputParameter(IAstNode* param) {
			mOutputParameters.push_back(param);
		}

		const std::vector<IAstNode*>& inputParameters() const {
			return mInputParameters;
		}

		const std::vector<IAstNode*>& outputParameters() const {
			return mOutputParameters;
		}

	private:
		IAstNode* mParent;
		IAstNode* mBody;
		std::vector<IAstNode*> mInputParameters;
		std::vector<IAstNode*> mOutputParameters;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
