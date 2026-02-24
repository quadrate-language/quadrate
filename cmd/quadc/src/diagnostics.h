#ifndef QUADC_DIAGNOSTICS_H
#define QUADC_DIAGNOSTICS_H

#include <fstream>
#include <iostream>
#include <optional>
#include <quadrate/qc/colors.h>
#include <string>

// Helper functions for printing compiler diagnostics with consistent formatting

inline void printError(const std::string& message) {
	std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold() << Qd::Colors::red()
			  << "error: " << Qd::Colors::reset() << message << std::endl;
}

inline void printError(const std::string& file, size_t line, size_t column, const std::string& message) {
	std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset();
	std::cerr << Qd::Colors::bold() << file << ":" << line << ":" << column << ":" << Qd::Colors::reset() << " ";
	std::cerr << Qd::Colors::bold() << Qd::Colors::red() << "error:" << Qd::Colors::reset() << " ";
	std::cerr << Qd::Colors::bold() << message << Qd::Colors::reset() << std::endl;
}

inline void printNote(const std::string& message) {
	std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold() << Qd::Colors::cyan()
			  << "note: " << Qd::Colors::reset() << message << std::endl;
}

inline void printParseFailure(const std::string& file, size_t errorCount) {
	std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << "parsing failed for " << file << " with "
			  << errorCount << " errors" << std::endl;
}

// Read entire file contents into a string
// Returns std::nullopt on failure (file not found or read error)
inline std::optional<std::string> readFileContents(const std::string& filePath) {
	std::ifstream file(filePath);
	if (!file.is_open()) {
		return std::nullopt;
	}
	file.seekg(0, std::ios::end);
	auto pos = file.tellg();
	file.seekg(0);
	if (pos < 0) {
		return std::nullopt;
	}
	size_t size = static_cast<size_t>(pos);
	std::string buffer(size, ' ');
	file.read(&buffer[0], static_cast<std::streamsize>(size));
	return buffer;
}

#endif
