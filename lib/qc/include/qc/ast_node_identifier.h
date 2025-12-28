#ifndef QD_QC_AST_NODE_IDENTIFIER_H
#define QD_QC_AST_NODE_IDENTIFIER_H

#include "ast_node.h"
#include <string>
#include <vector>

namespace Qd {
	// Cast direction for implicit casts
	enum class CastDirection {
		NONE,
		INT_TO_FLOAT, // casti -> castf
		FLOAT_TO_INT  // castf -> casti
	};

	class AstNodeIdentifier : public IAstNode {
	public:
		AstNodeIdentifier(const std::string& name)
			: mName(name), mParent(nullptr), mAbortOnError(false), mCheckError(false), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::IDENTIFIER;
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

		void setAbortOnError(bool abort) {
			mAbortOnError = abort;
		}

		bool abortOnError() const {
			return mAbortOnError;
		}

		void setCheckError(bool check) {
			mCheckError = check;
		}

		bool checkError() const {
			return mCheckError;
		}

		// Set which parameter positions need implicit casts
		void setParameterCasts(const std::vector<CastDirection>& casts) {
			mParameterCasts = casts;
		}

		const std::vector<CastDirection>& parameterCasts() const {
			return mParameterCasts;
		}

		// Method call support
		void setIsMethodCall(bool isMethod) {
			mIsMethodCall = isMethod;
		}

		bool isMethodCall() const {
			return mIsMethodCall;
		}

		void setReceiverType(const std::string& type) {
			mReceiverType = type;
		}

		const std::string& receiverType() const {
			return mReceiverType;
		}

		void setMethodInputParamCount(size_t count) {
			mMethodInputParamCount = count;
		}

		size_t methodInputParamCount() const {
			return mMethodInputParamCount;
		}

		void setMethodReceiverPositionFromTop(size_t pos) {
			mMethodReceiverPositionFromTop = pos;
		}

		size_t methodReceiverPositionFromTop() const {
			return mMethodReceiverPositionFromTop;
		}

	private:
		std::string mName;
		IAstNode* mParent;
		bool mAbortOnError;
		bool mCheckError;
		size_t mLine;
		size_t mColumn;
		std::vector<CastDirection> mParameterCasts; // Which parameters need casts (indexed from bottom of stack)
		bool mIsMethodCall = false;					// True if this is a struct method call
		std::string mReceiverType;					// Struct type of receiver for method calls
		size_t mMethodInputParamCount = 0;			// Number of explicit input params (excluding receiver)
		size_t mMethodReceiverPositionFromTop = 0;	// Receiver position from top of stack (0 = on top)
	};
}

#endif
