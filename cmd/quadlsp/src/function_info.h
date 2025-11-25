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
};

#endif
