#ifndef QD_QC_AST_NODE_SCOPED_H
#define QD_QC_AST_NODE_SCOPED_H

#include "ast_node.h"
#include "ast_node_identifier.h" // For CastDirection enum
#include <string>
#include <vector>

namespace Qd {
	// Forward declaration for friend access
	class SemanticValidator;

	class AstNodeScopedIdentifier : public IAstNode {
		friend class SemanticValidator;

	public:
		AstNodeScopedIdentifier(const std::string& scope, const std::string& name)
			: mScope(scope), mName(name), mParent(nullptr), mAbortOnError(false), mCheckError(false), mLine(0),
			  mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::SCOPED_IDENTIFIER;
		}

		const std::string& scope() const {
			return mScope;
		}

		const std::string& name() const {
			return mName;
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

		const std::vector<CastDirection>& parameterCasts() const {
			return mParameterCasts;
		}

		bool isMethodCall() const {
			return mIsMethodCall;
		}

		const std::string& receiverType() const {
			return mReceiverType;
		}

		size_t methodInputParamCount() const {
			return mMethodInputParamCount;
		}

		size_t methodReceiverPositionFromTop() const {
			return mMethodReceiverPositionFromTop;
		}

	private:
		// Semantic validation setters (only accessible by SemanticValidator)
		void setParameterCasts(const std::vector<CastDirection>& casts) {
			mParameterCasts = casts;
		}

		void setIsMethodCall(bool isMethod) {
			mIsMethodCall = isMethod;
		}

		void setReceiverType(const std::string& type) {
			mReceiverType = type;
		}

		void setMethodInputParamCount(size_t count) {
			mMethodInputParamCount = count;
		}

		void setMethodReceiverPositionFromTop(size_t pos) {
			mMethodReceiverPositionFromTop = pos;
		}

		std::string mScope;
		std::string mName;
		IAstNode* mParent;
		bool mAbortOnError;
		bool mCheckError;
		size_t mLine;
		size_t mColumn;
		std::vector<CastDirection> mParameterCasts; // Which parameters need casts (indexed from bottom of stack)
		bool mIsMethodCall = false;					// True if this is a struct method call
		std::string mReceiverType;					// Receiver struct type for method calls
		size_t mMethodInputParamCount = 0;			// Number of input params excluding receiver
		size_t mMethodReceiverPositionFromTop = 0;	// Receiver position from top of stack (0 = on top)
	};
}

#endif
