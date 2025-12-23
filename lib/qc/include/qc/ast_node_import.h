#ifndef QD_QC_AST_NODE_IMPORT_H
#define QD_QC_AST_NODE_IMPORT_H

#include "ast_node.h"
#include "ast_node_parameter.h"
#include <string>
#include <vector>

namespace Qd {
	// ABI type for imported native functions
	enum class ImportABI {
		DIRECT, // Direct C signatures (register-based, fast)
		STACK	// Stack-based via qd_context* (compatible with embedding)
	};

	// Represents a function declaration within an import statement
	struct ImportedFunction {
		std::string name;								 // Function name in Quadrate (e.g., "printf")
		std::vector<AstNodeParameter*> inputParameters;	 // Input parameters
		std::vector<AstNodeParameter*> outputParameters; // Output parameters
		bool throws = false;							 // Whether the function can throw errors (marked with '!')
		bool isPublic = false;							 // Whether the function is public (marked with 'pub')
		size_t line;
		size_t column;

		~ImportedFunction() {
			for (auto* param : inputParameters) {
				delete param;
			}
			for (auto* param : outputParameters) {
				delete param;
			}
		}
	};

	class AstNodeImport : public IAstNode {
	public:
		AstNodeImport(const std::string& library, const std::string& namespaceName, ImportABI abi = ImportABI::DIRECT)
			: mLibrary(library), mNamespace(namespaceName), mABI(abi), mParent(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeImport() {
			for (auto* func : mFunctions) {
				delete func;
			}
		}

		IAstNode::Type type() const override {
			return Type::IMPORT_STATEMENT;
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

		const std::string& library() const {
			return mLibrary;
		}

		const std::string& namespaceName() const {
			return mNamespace;
		}

		ImportABI abi() const {
			return mABI;
		}

		bool isStackBased() const {
			return mABI == ImportABI::STACK;
		}

		void addFunction(ImportedFunction* func) {
			mFunctions.push_back(func);
		}

		const std::vector<ImportedFunction*>& functions() const {
			return mFunctions;
		}

	private:
		std::string mLibrary;					   // Library file (e.g., "libstdqd.so")
		std::string mNamespace;					   // Namespace (e.g., "native" or "stack")
		ImportABI mABI;							   // Calling convention (direct or stack-based)
		std::vector<ImportedFunction*> mFunctions; // Declared functions
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
