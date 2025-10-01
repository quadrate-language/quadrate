#include "cxxopts.hpp"
#include <iostream>
#include <fstream>
#include "compiler.h"

#define QUADC_VERSION "0.1.0"

int main(int argc, char** argv) {
	cxxopts::Options options("quadc", "Quadrate compiler");
	options.add_options()
		("h,help", "Display help.")
		("v,version", "Display compiler version.")
		("files", "Input files", cxxopts::value<std::vector<std::string>>())
		;

	options.parse_positional({"files"});
	auto result = options.parse(argc, argv);

	if (result.count("help") || argc == 1) {
		std::cout << options.help() << std::endl;
		return 0;
	}

	if (result.count("version")) {
		std::cout << QUADC_VERSION << std::endl;
		return 0;
	}

	Compiler compiler;

	auto args = result.arguments();
	if (args.size() > 0) {
		for (const auto& arg : args) {
			std::ifstream file(arg.value());
			if (!file.is_open()) {
				std::cerr << "quadc: cannot find " << arg.value() << ": No such file or directory" << std::endl;
				continue;
			}
			file.seekg(0, std::ios::end);
			size_t size = file.tellg();
			std::string buffer(size, ' ');
			file.seekg(0);
			file.read(&buffer[0], size);
			compiler.compile(buffer.c_str());
		}
	}

	return 0;
}

