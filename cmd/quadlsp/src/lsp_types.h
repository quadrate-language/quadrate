#ifndef QUADLSP_TYPES_H
#define QUADLSP_TYPES_H

#include <string>
#include <utility>
#include <vector>

// Structure to hold constant information for completions
struct ConstantInfo {
	std::string name;
	std::string value;
};

// Structure to hold function information for completions
struct FunctionInfo {
	std::string name;
	std::vector<std::string> inputParams;  // "name:type" format
	std::vector<std::string> outputParams; // "name:type" format
	std::string signature;				   // Full signature string
	std::string snippet;				   // LSP snippet with placeholders
	size_t line = 0;					   // Line number where function is defined

	// Generic type support
	std::vector<std::string> typeParams; // Type parameters: "T", "U", etc.
	bool isGeneric = false;				 // True if function has type params

	// Method support (receiver-first convention)
	bool isMethod = false;						 // True if function has receiver
	std::string receiverName;					 // Receiver parameter name (e.g., "v")
	std::string receiverType;					 // Receiver struct type (e.g., "Vec")
	std::vector<std::string> receiverTypeParams; // Receiver type params if generic
};

// Structure to hold struct information for completions
struct StructInfo {
	std::string name;
	std::vector<std::pair<std::string, std::string>> fields; // field name -> type
	std::string signature;									 // Full struct declaration

	// Generic type support
	std::vector<std::string> typeParams; // Type parameters: "T", "U", etc.
	bool isGeneric = false;				 // True if struct has type params
};

#endif // QUADLSP_TYPES_H
