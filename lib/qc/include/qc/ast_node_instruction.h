#ifndef QD_QC_AST_NODE_INSTRUCTION_H
#define QD_QC_AST_NODE_INSTRUCTION_H

#include "ast_node.h"
#include <string>

namespace Qd {
	/**
	 * Represents a built-in instruction (print, sq, div, dup, rot, etc.)
	 * These are distinguished from user-defined identifiers to allow proper code generation.
	 */
	class AstNodeInstruction : public IAstNode {
	public:
		AstNodeInstruction(const std::string& name)
			: mName(name), mTypeParam(), mParent(nullptr), mLine(0), mColumn(0) {
		}

		AstNodeInstruction(const std::string& name, const std::string& typeParam)
			: mName(name), mTypeParam(typeParam), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::INSTRUCTION;
		}

		size_t childCount() const override {
			return 0;
		}

		IAstNode* child(size_t) const override {
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

		const std::string& name() const {
			return mName;
		}

		const std::string& typeParam() const {
			return mTypeParam;
		}

		bool hasTypeParam() const {
			return !mTypeParam.empty();
		}

	private:
		std::string mName;
		std::string mTypeParam;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
