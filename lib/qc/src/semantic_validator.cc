#include <cstring>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/colors.h>
#include <qc/semantic_validator.h>

namespace Qd {

	// List of built-in instructions (must match ast.cc)
	static const char* BUILTIN_INSTRUCTIONS[] = {"*", "+", "-", ".", "/", "add", "div", "dup", "mul", "print", "prints",
			"printsv", "printv", "rot", "sq", "sub", "swap"};

	SemanticValidator::SemanticValidator() : filename_(nullptr), error_count_(0) {
	}

	bool SemanticValidator::isBuiltInInstruction(const char* name) const {
		static const size_t count = sizeof(BUILTIN_INSTRUCTIONS) / sizeof(BUILTIN_INSTRUCTIONS[0]);
		for (size_t i = 0; i < count; i++) {
			if (strcmp(name, BUILTIN_INSTRUCTIONS[i]) == 0) {
				return true;
			}
		}
		return false;
	}

	void SemanticValidator::reportError(const char* message) {
		// GCC/Clang style: quadc: filename: error: message
		std::cerr << Colors::bold() << "quadc: " << Colors::reset();
		if (filename_) {
			std::cerr << Colors::bold() << filename_ << ":" << Colors::reset() << " ";
		}
		std::cerr << Colors::bold() << Colors::red() << "error:" << Colors::reset() << " ";
		std::cerr << Colors::bold() << message << Colors::reset() << std::endl;
		error_count_++;
	}

	size_t SemanticValidator::validate(IAstNode* program, const char* filename) {
		error_count_ = 0;
		filename_ = filename;
		defined_functions_.clear();

		// Pass 1: Collect all function definitions
		collectDefinitions(program);

		// Pass 2: Validate all references
		validateReferences(program);

		return error_count_;
	}

	void SemanticValidator::collectDefinitions(IAstNode* node) {
		if (!node) {
			return;
		}

		// If this is a function declaration, add it to the symbol table
		if (node->type() == IAstNode::Type::FunctionDeclaration) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
			defined_functions_.insert(func->name());
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			collectDefinitions(node->child(i));
		}
	}

	void SemanticValidator::validateReferences(IAstNode* node) {
		if (!node) {
			return;
		}

		// Check if this is an identifier (function call)
		if (node->type() == IAstNode::Type::Identifier) {
			AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(node);
			const char* name = ident->name().c_str();

			// Check if it's a built-in instruction
			if (isBuiltInInstruction(name)) {
				// Valid built-in, no error
				return;
			}

			// Check if it's a defined function
			if (defined_functions_.find(name) == defined_functions_.end()) {
				// Not found - report error
				std::string error_msg = "Undefined function '";
				error_msg += name;
				error_msg += "'";
				reportError(error_msg.c_str());
			}
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			validateReferences(node->child(i));
		}
	}

} // namespace Qd
