#ifndef QD_QC_AST_NODE_ANONYMOUS_FUNCTION_H
#define QD_QC_AST_NODE_ANONYMOUS_FUNCTION_H

#include "ast_node.h"
#include <memory>
#include <string>
#include <vector>

namespace Qd {
	/**
	 * AST node for anonymous functions (quotations/lambdas) with optional closures.
	 *
	 * Syntax without captures: fn (x:i64 y:i64 -- r:i64) { body }
	 * Syntax with captures:    fn [a, b] (x:i64 y:i64 -- r:i64) { body }
	 *
	 * This node represents an inline function definition that produces
	 * a function pointer (or closure) when evaluated. Unlike regular function
	 * declarations, anonymous functions appear as expressions within other code.
	 *
	 * When captures are specified, the function becomes a closure that can
	 * access variables from the enclosing scope. Captured variables are
	 * copied by value at the point of closure creation.
	 *
	 * Code generation will:
	 * 1. Generate a unique name (e.g., __anon_001)
	 * 2. Emit the function definition
	 * 3. If captures: allocate closure struct, copy captures, push closure ptr
	 * 4. If no captures: push a plain function pointer onto the stack
	 */
	class AstNodeAnonymousFunction : public IAstNode {
	public:
		AstNodeAnonymousFunction() : mParent(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeAnonymousFunction() = default;

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
			for (const auto& param : mInputParameters) {
				if (index == currentIndex++) {
					return param.get();
				}
			}
			for (const auto& param : mOutputParameters) {
				if (index == currentIndex++) {
					return param.get();
				}
			}
			if (mBody && index == currentIndex++) {
				return mBody.get();
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
			mBody.reset(body);
		}

		IAstNode* body() const {
			return mBody.get();
		}

		void addInputParameter(IAstNode* param) {
			mInputParameters.emplace_back(param);
		}

		void addOutputParameter(IAstNode* param) {
			mOutputParameters.emplace_back(param);
		}

		const std::vector<std::unique_ptr<IAstNode>>& inputParameters() const {
			return mInputParameters;
		}

		const std::vector<std::unique_ptr<IAstNode>>& outputParameters() const {
			return mOutputParameters;
		}

		void addCapturedVariable(const std::string& name) {
			mCapturedVariables.push_back(name);
		}

		const std::vector<std::string>& capturedVariables() const {
			return mCapturedVariables;
		}

		bool hasCaptures() const {
			return !mCapturedVariables.empty();
		}

	private:
		IAstNode* mParent;
		std::unique_ptr<IAstNode> mBody;
		std::vector<std::unique_ptr<IAstNode>> mInputParameters;
		std::vector<std::unique_ptr<IAstNode>> mOutputParameters;
		std::vector<std::string> mCapturedVariables; // Variables captured from enclosing scope
		size_t mLine;
		size_t mColumn;
	};
}

#endif
