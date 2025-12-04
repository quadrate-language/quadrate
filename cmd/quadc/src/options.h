#ifndef QUADC_OPTIONS_H
#define QUADC_OPTIONS_H

#include <string>
#include <unordered_map>
#include <vector>

#define QUADC_VERSION "0.1.0"

struct Options {
	std::vector<std::string> files;
	std::string outputName = "main";
	int optLevel = 0; // 0-3 for -O0 through -O3
	size_t stackSize = 1024; // Stack capacity
	bool help = false;
	bool version = false;
	bool saveTemps = false;
	bool verbose = false;
	bool dumpTokens = false;
	bool run = false;
	bool dumpIR = false;
	bool debugInfo = false;
	bool werror = false;
	bool readStdin = false; // Read source from stdin
	bool testMode = false;  // Compile and run tests
	std::unordered_map<std::string, std::string> moduleVersions; // module name -> version
};

void printHelp();
void printVersion();
bool parseArgs(int argc, char* argv[], Options& opts);

#endif
