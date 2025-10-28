#include <cstring>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_instruction.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_parameter.h>
#include <qc/colors.h>
#include <qc/semantic_validator.h>

namespace Qd {

	// List of built-in instructions (must match ast.cc)
	static const char* BUILTIN_INSTRUCTIONS[] = {"*", "+", "-", ".", "/", "abs", "acos", "add", "asin", "atan", "cb",
			"cbrt", "ceil", "clear", "cos", "dec", "depth", "div", "dup", "floor", "inc", "mul", "nip", "over", "print",
			"prints", "printsv", "printv", "rot", "sin", "sq", "sqrt", "sub", "swap", "tan"};

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

		// Pass 3: Type check
		typeCheckFunction(program);

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

	void SemanticValidator::typeCheckFunction(IAstNode* node) {
		if (!node) {
			return;
		}

		// Type check each function definition
		if (node->type() == IAstNode::Type::FunctionDeclaration) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
			std::vector<StackValueType> type_stack;

			// Initialize type stack with input parameters
			// Input parameters are on the stack when the function starts
			for (size_t i = 0; i < func->inputParameters().size(); i++) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[i]);
				const std::string& typeStr = param->typeString();

				if (typeStr == "i") {
					type_stack.push_back(StackValueType::INT);
				} else if (typeStr == "f") {
					type_stack.push_back(StackValueType::FLOAT);
				} else if (typeStr == "s") {
					type_stack.push_back(StackValueType::STRING);
				} else {
					// Untyped or unknown - treat as ANY
					type_stack.push_back(StackValueType::ANY);
				}
			}

			// Type check the function body
			if (func->body()) {
				typeCheckBlock(func->body(), type_stack);
			}
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			typeCheckFunction(node->child(i));
		}
	}

	void SemanticValidator::typeCheckBlock(IAstNode* node, std::vector<StackValueType>& type_stack) {
		if (!node) {
			return;
		}

		// Process each child in the block
		for (size_t i = 0; i < node->childCount(); i++) {
			IAstNode* child = node->child(i);
			if (!child) {
				continue;
			}

			switch (child->type()) {
			case IAstNode::Type::Literal: {
				AstNodeLiteral* lit = static_cast<AstNodeLiteral*>(child);
				switch (lit->literalType()) {
				case AstNodeLiteral::LiteralType::Integer:
					type_stack.push_back(StackValueType::INT);
					break;
				case AstNodeLiteral::LiteralType::Float:
					type_stack.push_back(StackValueType::FLOAT);
					break;
				case AstNodeLiteral::LiteralType::String:
					type_stack.push_back(StackValueType::STRING);
					break;
				}
				break;
			}

			case IAstNode::Type::Instruction: {
				AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(child);
				typeCheckInstruction(instr->name().c_str(), type_stack);
				break;
			}

			case IAstNode::Type::Block: {
				// Recursively check nested blocks
				typeCheckBlock(child, type_stack);
				break;
			}

			case IAstNode::Type::IfStatement: {
				// For now, skip control flow type checking
				// (more complex - would need to merge type states from branches)
				break;
			}

			case IAstNode::Type::ForStatement: {
				// For now, skip loop type checking
				break;
			}

			default:
				// Other node types don't affect the type stack
				break;
			}
		}
	}

	void SemanticValidator::typeCheckInstruction(const char* name, std::vector<StackValueType>& type_stack) {
		// Handle instruction aliases
		if (strcmp(name, ".") == 0) {
			name = "print";
		} else if (strcmp(name, "/") == 0) {
			name = "div";
		} else if (strcmp(name, "*") == 0) {
			name = "mul";
		} else if (strcmp(name, "+") == 0) {
			name = "add";
		} else if (strcmp(name, "-") == 0) {
			name = "sub";
		}

		// Arithmetic operations: abs, sq (preserve type)
		if (strcmp(name, "abs") == 0 || strcmp(name, "sq") == 0) {
			if (type_stack.empty()) {
				std::string error_msg = "Type error in '";
				error_msg += name;
				error_msg += "': Stack underflow (requires 1 numeric value)";
				reportError(error_msg.c_str());
				return;
			}

			StackValueType top = type_stack.back();
			if (!isNumericType(top)) {
				std::string error_msg = "Type error in '";
				error_msg += name;
				error_msg += "': Expected numeric type, got ";
				error_msg += typeToString(top);
				reportError(error_msg.c_str());
				return;
			}
			// Type remains the same (already on stack)
		}
		// Trigonometric functions: sin, cos, tan, asin, acos, atan (always return float)
		else if (strcmp(name, "sin") == 0 || strcmp(name, "cos") == 0 || strcmp(name, "tan") == 0 ||
				 strcmp(name, "asin") == 0 || strcmp(name, "acos") == 0 || strcmp(name, "atan") == 0) {
			if (type_stack.empty()) {
				std::string error_msg = "Type error in '";
				error_msg += name;
				error_msg += "': Stack underflow (requires 1 numeric value)";
				reportError(error_msg.c_str());
				return;
			}

			StackValueType top = type_stack.back();
			if (!isNumericType(top)) {
				std::string error_msg = "Type error in '";
				error_msg += name;
				error_msg += "': Expected numeric type, got ";
				error_msg += typeToString(top);
				reportError(error_msg.c_str());
				return;
			}
			// Pop and push float (trig functions always return float)
			type_stack.pop_back();
			type_stack.push_back(StackValueType::FLOAT);
		}
		// Math functions: sqrt, cb, cbrt, ceil, floor (always return float)
		else if (strcmp(name, "sqrt") == 0 || strcmp(name, "cb") == 0 || strcmp(name, "cbrt") == 0 ||
				 strcmp(name, "ceil") == 0 || strcmp(name, "floor") == 0) {
			if (type_stack.empty()) {
				std::string error_msg = "Type error in '";
				error_msg += name;
				error_msg += "': Stack underflow (requires 1 numeric value)";
				reportError(error_msg.c_str());
				return;
			}

			StackValueType top = type_stack.back();
			if (!isNumericType(top)) {
				std::string error_msg = "Type error in '";
				error_msg += name;
				error_msg += "': Expected numeric type, got ";
				error_msg += typeToString(top);
				reportError(error_msg.c_str());
				return;
			}
			// Pop and push float (math functions always return float)
			type_stack.pop_back();
			type_stack.push_back(StackValueType::FLOAT);
		}
		// Increment/Decrement functions: inc, dec (preserve type)
		else if (strcmp(name, "inc") == 0 || strcmp(name, "dec") == 0) {
			if (type_stack.empty()) {
				std::string error_msg = "Type error in '";
				error_msg += name;
				error_msg += "': Stack underflow (requires 1 numeric value)";
				reportError(error_msg.c_str());
				return;
			}

			StackValueType top = type_stack.back();
			if (!isNumericType(top)) {
				std::string error_msg = "Type error in '";
				error_msg += name;
				error_msg += "': Expected numeric type, got ";
				error_msg += typeToString(top);
				reportError(error_msg.c_str());
				return;
			}
			// Type remains the same (already on stack)
		}
		// Binary arithmetic operations: add, sub, mul, div
		else if (strcmp(name, "add") == 0 || strcmp(name, "sub") == 0 || strcmp(name, "mul") == 0 ||
				 strcmp(name, "div") == 0) {
			if (type_stack.size() < 2) {
				std::string error_msg = "Type error in '";
				error_msg += name;
				error_msg += "': Stack underflow (requires 2 numeric values)";
				reportError(error_msg.c_str());
				return;
			}

			StackValueType b = type_stack.back();
			type_stack.pop_back();
			StackValueType a = type_stack.back();
			type_stack.pop_back();

			if (!isNumericType(a) || !isNumericType(b)) {
				std::string error_msg = "Type error in '";
				error_msg += name;
				error_msg += "': Expected numeric types, got ";
				error_msg += typeToString(a);
				error_msg += " and ";
				error_msg += typeToString(b);
				reportError(error_msg.c_str());
				return;
			}

			// Result is float if either operand is float, otherwise int
			StackValueType result = (a == StackValueType::FLOAT || b == StackValueType::FLOAT) ? StackValueType::FLOAT
																							   : StackValueType::INT;
			type_stack.push_back(result);
		}
		// Print operations: print, printv
		else if (strcmp(name, "print") == 0 || strcmp(name, "printv") == 0) {
			if (type_stack.empty()) {
				std::string error_msg = "Type error in '";
				error_msg += name;
				error_msg += "': Stack underflow (requires 1 value)";
				reportError(error_msg.c_str());
				return;
			}
			type_stack.pop_back(); // Pop the value
		}
		// Non-destructive print: prints, printsv
		else if (strcmp(name, "prints") == 0 || strcmp(name, "printsv") == 0) {
			// These don't modify the stack
		}
		// Stack operations: dup
		else if (strcmp(name, "dup") == 0) {
			if (type_stack.empty()) {
				reportError("Type error in 'dup': Stack underflow (requires 1 value)");
				return;
			}
			StackValueType top = type_stack.back();
			type_stack.push_back(top); // Duplicate
		}
		// Stack operations: swap
		else if (strcmp(name, "swap") == 0) {
			if (type_stack.size() < 2) {
				reportError("Type error in 'swap': Stack underflow (requires 2 values)");
				return;
			}
			StackValueType a = type_stack.back();
			type_stack.pop_back();
			StackValueType b = type_stack.back();
			type_stack.pop_back();
			type_stack.push_back(a);
			type_stack.push_back(b);
		}
		// Stack operations: over ( a b -- a b a )
		else if (strcmp(name, "over") == 0) {
			if (type_stack.size() < 2) {
				reportError("Type error in 'over': Stack underflow (requires 2 values)");
				return;
			}
			// Get the second element
			StackValueType second = type_stack[type_stack.size() - 2];
			// Push a copy of it to the top
			type_stack.push_back(second);
		}
		// Stack operations: nip ( a b -- b )
		else if (strcmp(name, "nip") == 0) {
			if (type_stack.size() < 2) {
				reportError("Type error in 'nip': Stack underflow (requires 2 values)");
				return;
			}
			StackValueType top = type_stack.back();
			type_stack.pop_back();
			type_stack.pop_back();	   // Remove second element
			type_stack.push_back(top); // Push top back
		}
		// Stack operations: clear (empties the entire stack)
		else if (strcmp(name, "clear") == 0) {
			// Clear all elements from the type stack
			type_stack.clear();
		}
		// Stack operations: depth (pushes the current stack depth as an integer)
		else if (strcmp(name, "depth") == 0) {
			// Push an int type onto the stack (depth is always an integer)
			type_stack.push_back(StackValueType::INT);
		}
	}

	bool SemanticValidator::isNumericType(StackValueType type) const {
		return type == StackValueType::INT || type == StackValueType::FLOAT;
	}

	const char* SemanticValidator::typeToString(StackValueType type) const {
		switch (type) {
		case StackValueType::INT:
			return "int";
		case StackValueType::FLOAT:
			return "float";
		case StackValueType::STRING:
			return "string";
		case StackValueType::ANY:
			return "any";
		case StackValueType::UNKNOWN:
			return "unknown";
		default:
			return "unknown";
		}
	}

} // namespace Qd
