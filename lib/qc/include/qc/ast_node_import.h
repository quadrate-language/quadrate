#ifndef QD_QC_AST_NODE_IMPORT_H
#define QD_QC_AST_NODE_IMPORT_H

#include "ast_node.h"
#include "ast_node_parameter.h"
#include <string>
#include <vector>

namespace Qd {
	// Represents a function declaration within an import statement
	struct ImportedFunction {
		std::string name;								 // Function name in Quadrate (e.g., "printf")
		std::vector<AstNodeParameter*> inputParameters;	 // Input parameters
		std::vector<AstNodeParameter*> outputParameters; // Output parameters
		bool throws = false;							 // Whether the function can throw errors (marked with '!')
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
		AstNodeImport(const std::string& library, const std::string& namespaceName)
			: mLibrary(library), mNamespace(namespaceName), mParent(nullptr), mLine(0), mColumn(0) {
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

		void addFunction(ImportedFunction* func) {
			mFunctions.push_back(func);
		}

		const std::vector<ImportedFunction*>& functions() const {
			return mFunctions;
		}

	private:
		std::string mLibrary;					   // Library file (e.g., "libstdqd.so")
		std::string mNamespace;					   // Namespace (e.g., "std")
		std::vector<ImportedFunction*> mFunctions; // Declared functions
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
