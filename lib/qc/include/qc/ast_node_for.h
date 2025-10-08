#ifndef QD_QC_AST_NODE_FOR_H
#define QD_QC_AST_NODE_FOR_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeForStatement : public IAstNode {
	public:
		AstNodeForStatement(const std::string& loopVar)
			: mLoopVar(loopVar), mParent(nullptr), mStart(nullptr), mEnd(nullptr), mStep(nullptr), mBody(nullptr) {
		}

		~AstNodeForStatement() {
			if (mStart) {
				delete mStart;
			}
			if (mEnd) {
				delete mEnd;
			}
			if (mStep) {
				delete mStep;
			}
			if (mBody) {
				delete mBody;
			}
		}

		IAstNode::Type type() const override {
			return Type::WhileStatement;
		}

		size_t childCount() const override {
			size_t count = 0;
			if (mStart) {
				count++;
			}
			if (mEnd) {
				count++;
			}
			if (mStep) {
				count++;
			}
			if (mBody) {
				count++;
			}
			return count;
		}

		IAstNode* child(size_t index) const override {
			size_t currentIndex = 0;
			if (mStart && index == currentIndex++) {
				return mStart;
			}
			if (mEnd && index == currentIndex++) {
				return mEnd;
			}
			if (mStep && index == currentIndex++) {
				return mStep;
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

		const std::string& loopVar() const {
			return mLoopVar;
		}

		void setStart(IAstNode* start) {
			mStart = start;
		}

		void setEnd(IAstNode* end) {
			mEnd = end;
		}

		void setStep(IAstNode* step) {
			mStep = step;
		}

		void setBody(IAstNode* body) {
			mBody = body;
		}

		IAstNode* start() const {
			return mStart;
		}

		IAstNode* end() const {
			return mEnd;
		}

		IAstNode* step() const {
			return mStep;
		}

		IAstNode* body() const {
			return mBody;
		}

	private:
		std::string mLoopVar;
		IAstNode* mParent;
		IAstNode* mStart;
		IAstNode* mEnd;
		IAstNode* mStep;
		IAstNode* mBody;
	};
}

#endif
