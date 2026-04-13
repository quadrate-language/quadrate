#ifndef QUADC_OPTIONS_H
#define QUADC_OPTIONS_H

#include <string>
#include <unordered_map>
#include <vector>

#define QUADC_VERSION "0.1.0"

struct Options {
	std::vector<std::string> files;
	std::vector<std::string> runArgs;	   // Arguments to pass to the program when using -r
	std::vector<std::string> includePaths; // Additional module search paths (-I)
	std::string outputName = "main";
	int optLevel = 0;		 // 0-3 for -O0 through -O3
	size_t stackSize = 1024; // Stack capacity
	bool help = false;
	bool version = false;
	bool saveTemps = false;
	bool verbose = false;
	bool dumpTokens = false;
	bool dumpAst = false;
	bool run = false;
	bool dumpIR = false;
	bool debugInfo = false;
	bool werror = false;
	bool readStdin = false;										 // Read source from stdin
	bool testMode = false;										 // Compile and run tests
	bool coverage = false;										 // Function coverage report (--coverage with --test)
	bool freestanding = false;									 // No hosted runtime (no libc, no auto-main)
	bool noJIT = false;											 // Disable JIT for -r mode
	std::string targetTriple;									 // Cross-compilation target (e.g., aarch64-linux-gnu)
	std::unordered_map<std::string, std::string> moduleVersions; // module name -> version
};

void printHelp();
void printVersion();
bool parseArgs(int argc, char* argv[], Options& opts);

#endif
