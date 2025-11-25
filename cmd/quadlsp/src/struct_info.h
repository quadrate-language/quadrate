#ifndef QUADLSP_STRUCT_INFO_H
#define QUADLSP_STRUCT_INFO_H

#include <string>
#include <utility>
#include <vector>

// Structure to hold struct information for completions
struct StructInfo {
	std::string name;
	std::vector<std::pair<std::string, std::string>> fields; // field name -> type
	std::string signature;									 // Full struct declaration
};

#endif
