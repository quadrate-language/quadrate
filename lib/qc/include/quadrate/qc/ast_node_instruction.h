#ifndef QD_QC_AST_NODE_INSTRUCTION_H
#define QD_QC_AST_NODE_INSTRUCTION_H

#include "ast_node.h"
#include <string>

namespace Qd {
	// Forward declaration for friend access
	class SemanticValidator;

	/**
	 * Represents a built-in instruction (print, sq, div, dup, rot, etc.)
	 * These are distinguished from user-defined identifiers to allow proper code generation.
	 */
	class AstNodeInstruction : public IAstNode {
		friend class SemanticValidator;

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

		// Fallible call support - marks this instruction as a fallible function call (!)
		void setAbortOnError(bool abort) {
			mAbortOnError = abort;
		}

		bool abortOnError() const {
			return mAbortOnError;
		}

		void setPropagateOnError(bool propagate) {
			mPropagateOnError = propagate;
		}

		bool propagateOnError() const {
			return mPropagateOnError;
		}

		// `call` only: whether the function pointer on the stack is a fallible one. The call site
		// cannot see that for itself -- the callee is a value, not a name -- so the validator,
		// which resolves the pointer's signature, records it here for code generation.
		void setCalleeFallible(bool fallible) {
			mCalleeFallible = fallible;
		}

		bool calleeFallible() const {
			return mCalleeFallible;
		}

		// `nth` only: the type of the element it leaves on the stack, empty when the validator
		// could not work it out. The backend cannot work it out for itself -- by the time `nth`
		// runs, the array is two pushes back and which name put it there is not recoverable --
		// so what a following `<<field` or `-> name` needs is recorded here, the same way
		// `call` carries its callee's fallibility.
		const std::string& elementType() const {
			return mElementType;
		}

	private:
		// Semantic validation setters (only accessible by SemanticValidator)
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

		void setElementType(const std::string& type) {
			mElementType = type;
		}

		std::string mName;
		std::string mTypeParam;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
		bool mIsMethodCall = false;
		std::string mReceiverType;
		size_t mMethodInputParamCount = 0;
		size_t mMethodReceiverPositionFromTop = 0;
		bool mAbortOnError = false;
		bool mPropagateOnError = false;
		bool mCalleeFallible = false;
		std::string mElementType;
	};
}

#endif
