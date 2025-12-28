#ifndef QUADLSP_FUNCTION_INFO_H
#define QUADLSP_FUNCTION_INFO_H

#include <string>
#include <vector>

// Structure to hold function information for completions
struct FunctionInfo {
	std::string name;
	std::vector<std::string> inputParams;  // "name:type" format
	std::vector<std::string> outputParams; // "name:type" format
	std::string signature;				   // Full signature string
	std::string snippet;				   // LSP snippet with placeholders

	// Generic type support
	std::vector<std::string> typeParams; // Type parameters: "T", "U", etc.
	bool isGeneric = false;				 // True if function has type params

	// Method support (receiver-first convention)
	bool isMethod = false;						   // True if function has receiver
	std::string receiverName;					   // Receiver parameter name (e.g., "v")
	std::string receiverType;					   // Receiver struct type (e.g., "Vec")
	std::vector<std::string> receiverTypeParams;   // Receiver type params if generic
};

#endif
