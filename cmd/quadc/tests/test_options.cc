/**
 * @file test_options.cc
 * @brief Unit tests for quadc command-line options parsing
 */

#include "../src/options.h"
#include <cstring>
#include <unit-check/uc.h>
#include <vector>

// Helper to create argc/argv from a vector of strings
class ArgBuilder {
public:
	ArgBuilder() {
		// argv[0] is always the program name
		args.push_back(strdup("quadc"));
	}

	~ArgBuilder() {
		for (char* arg : args) {
			free(arg);
		}
	}

	ArgBuilder& add(const char* arg) {
		args.push_back(strdup(arg));
		return *this;
	}

	int argc() const {
		return static_cast<int>(args.size());
	}

	char** argv() {
		return args.data();
	}

private:
	std::vector<char*> args;
};

// Basic option tests

TEST(ParseHelp) {
	ArgBuilder ab;
	ab.add("--help");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --help");
	ASSERT(opts.help, "help flag should be set");
}

TEST(ParseHelpShort) {
	ArgBuilder ab;
	ab.add("-h");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse -h");
	ASSERT(opts.help, "help flag should be set");
}

TEST(ParseVersion) {
	ArgBuilder ab;
	ab.add("--version");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --version");
	ASSERT(opts.version, "version flag should be set");
}

TEST(ParseVersionShort) {
	ArgBuilder ab;
	ab.add("-v");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse -v");
	ASSERT(opts.version, "version flag should be set");
}

TEST(ParseOutputName) {
	ArgBuilder ab;
	ab.add("-o").add("myprogram");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse -o");
	ASSERT(opts.outputName == "myprogram", "output name should be set");
}

TEST(ParseOutputNameMissingArg) {
	ArgBuilder ab;
	ab.add("-o");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(!result, "should fail when -o has no argument");
}

// Target triple tests

TEST(ParseTargetTriple) {
	ArgBuilder ab;
	ab.add("--target").add("aarch64-linux-gnu");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --target");
	ASSERT(opts.targetTriple == "aarch64-linux-gnu", "target triple should be set");
}

TEST(ParseTargetTripleX86) {
	ArgBuilder ab;
	ab.add("--target").add("x86_64-linux-gnu");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --target x86_64");
	ASSERT(opts.targetTriple == "x86_64-linux-gnu", "target triple should be x86_64-linux-gnu");
}

TEST(ParseTargetTripleMacOS) {
	ArgBuilder ab;
	ab.add("--target").add("aarch64-apple-darwin");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --target for macOS");
	ASSERT(opts.targetTriple == "aarch64-apple-darwin", "target triple should be aarch64-apple-darwin");
}

TEST(ParseTargetTripleMissingArg) {
	ArgBuilder ab;
	ab.add("--target");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(!result, "should fail when --target has no argument");
}

TEST(ParseTargetTripleWithOtherFlags) {
	ArgBuilder ab;
	ab.add("-O2");
	ab.add("--target").add("aarch64-linux-gnu");
	ab.add("-g");
	ab.add("-o").add("output");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --target with other flags");
	ASSERT(opts.targetTriple == "aarch64-linux-gnu", "target triple should be set");
	ASSERT(opts.optLevel == 2, "optimization level should be 2");
	ASSERT(opts.debugInfo, "debug info should be enabled");
	ASSERT(opts.outputName == "output", "output name should be set");
}

TEST(ParseTargetTripleEmpty) {
	ArgBuilder ab;
	ab.add("--target").add("");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --target with empty string");
	ASSERT(opts.targetTriple == "", "target triple should be empty string");
}

TEST(DefaultTargetTripleEmpty) {
	ArgBuilder ab;
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse without --target");
	ASSERT(opts.targetTriple.empty(), "target triple should be empty by default");
}

// Optimization level tests

TEST(ParseOptimizationO0) {
	ArgBuilder ab;
	ab.add("-O0");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse -O0");
	ASSERT(opts.optLevel == 0, "opt level should be 0");
}

TEST(ParseOptimizationO1) {
	ArgBuilder ab;
	ab.add("-O1");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse -O1");
	ASSERT(opts.optLevel == 1, "opt level should be 1");
}

TEST(ParseOptimizationO2) {
	ArgBuilder ab;
	ab.add("-O2");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse -O2");
	ASSERT(opts.optLevel == 2, "opt level should be 2");
}

TEST(ParseOptimizationO3) {
	ArgBuilder ab;
	ab.add("-O3");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse -O3");
	ASSERT(opts.optLevel == 3, "opt level should be 3");
}

// Debug info tests

TEST(ParseDebugInfo) {
	ArgBuilder ab;
	ab.add("-g");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse -g");
	ASSERT(opts.debugInfo, "debug info should be enabled");
}

// Run mode tests

TEST(ParseRun) {
	ArgBuilder ab;
	ab.add("-r");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse -r");
	ASSERT(opts.run, "run flag should be set");
}

TEST(ParseRunLong) {
	ArgBuilder ab;
	ab.add("--run");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --run");
	ASSERT(opts.run, "run flag should be set");
}

// Test mode tests

TEST(ParseTestMode) {
	ArgBuilder ab;
	ab.add("--test");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --test");
	ASSERT(opts.testMode, "test mode should be enabled");
	ASSERT(opts.run, "run should be auto-enabled in test mode");
}

// Include path tests

TEST(ParseIncludePath) {
	ArgBuilder ab;
	ab.add("-I").add("/path/to/modules");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse -I");
	ASSERT(opts.includePaths.size() == 1, "should have one include path");
	ASSERT(opts.includePaths[0] == "/path/to/modules", "include path should be set");
}

TEST(ParseMultipleIncludePaths) {
	ArgBuilder ab;
	ab.add("-I").add("/path1");
	ab.add("-I").add("/path2");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse multiple -I");
	ASSERT(opts.includePaths.size() == 2, "should have two include paths");
}

// Stack size tests

TEST(ParseStackSize) {
	ArgBuilder ab;
	ab.add("-s").add("4096");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse -s");
	ASSERT(opts.stackSize == 4096, "stack size should be 4096");
}

TEST(ParseStackSizeInvalid) {
	ArgBuilder ab;
	ab.add("-s").add("invalid");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(!result, "should fail with invalid stack size");
}

TEST(ParseStackSizeZero) {
	ArgBuilder ab;
	ab.add("-s").add("0");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(!result, "should fail with zero stack size");
}

// Werror tests

TEST(ParseWerror) {
	ArgBuilder ab;
	ab.add("--werror");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --werror");
	ASSERT(opts.werror, "werror should be enabled");
}

// Dump options tests

TEST(ParseDumpTokens) {
	ArgBuilder ab;
	ab.add("--dump-tokens");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --dump-tokens");
	ASSERT(opts.dumpTokens, "dump tokens should be enabled");
}

TEST(ParseDumpAst) {
	ArgBuilder ab;
	ab.add("--dump-ast");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --dump-ast");
	ASSERT(opts.dumpAst, "dump AST should be enabled");
}

TEST(ParseDumpIR) {
	ArgBuilder ab;
	ab.add("--dump-ir");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --dump-ir");
	ASSERT(opts.dumpIR, "dump IR should be enabled");
}

// Save temps tests

TEST(ParseSaveTemps) {
	ArgBuilder ab;
	ab.add("--save-temps");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --save-temps");
	ASSERT(opts.saveTemps, "save temps should be enabled");
}

// Verbose tests

TEST(ParseVerbose) {
	ArgBuilder ab;
	ab.add("--verbose");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse --verbose");
	ASSERT(opts.verbose, "verbose should be enabled");
}

// Unknown option tests

TEST(ParseUnknownOption) {
	ArgBuilder ab;
	ab.add("--unknown-option");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(!result, "should fail with unknown option");
}

// File argument tests

TEST(ParseSingleFile) {
	ArgBuilder ab;
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse single file");
	ASSERT(opts.files.size() == 1, "should have one file");
	ASSERT(opts.files[0] == "test.qd", "file should be test.qd");
}

TEST(ParseMultipleFiles) {
	ArgBuilder ab;
	ab.add("file1.qd");
	ab.add("file2.qd");
	ab.add("file3.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse multiple files");
	ASSERT(opts.files.size() == 3, "should have three files");
}

// Run arguments (after --) tests

TEST(ParseRunArgs) {
	ArgBuilder ab;
	ab.add("-r");
	ab.add("test.qd");
	ab.add("--");
	ab.add("arg1");
	ab.add("arg2");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse run arguments");
	ASSERT(opts.runArgs.size() == 2, "should have two run arguments");
	ASSERT(opts.runArgs[0] == "arg1", "first run arg should be arg1");
	ASSERT(opts.runArgs[1] == "arg2", "second run arg should be arg2");
}

// Module version pinning tests

TEST(ParseModuleVersion) {
	ArgBuilder ab;
	ab.add("-l").add("mymodule@1.0.0");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse -l");
	ASSERT(opts.moduleVersions.count("mymodule") == 1, "should have module version");
	ASSERT(opts.moduleVersions["mymodule"] == "1.0.0", "version should be 1.0.0");
}

TEST(ParseModuleVersionInvalidFormat) {
	ArgBuilder ab;
	ab.add("-l").add("invalid");
	ab.add("test.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(!result, "should fail with invalid module version format");
}

// Complex command line tests

TEST(ComplexCommandLine) {
	ArgBuilder ab;
	ab.add("-O2");
	ab.add("-g");
	ab.add("--target").add("aarch64-linux-gnu");
	ab.add("-o").add("myapp");
	ab.add("-I").add("/usr/local/lib/quadrate");
	ab.add("-s").add("8192");
	ab.add("--werror");
	ab.add("main.qd");
	ab.add("utils.qd");
	Options opts;
	bool result = parseArgs(ab.argc(), ab.argv(), opts);
	ASSERT(result, "should parse complex command line");
	ASSERT(opts.optLevel == 2, "opt level should be 2");
	ASSERT(opts.debugInfo, "debug info should be enabled");
	ASSERT(opts.targetTriple == "aarch64-linux-gnu", "target should be aarch64-linux-gnu");
	ASSERT(opts.outputName == "myapp", "output should be myapp");
	ASSERT(opts.includePaths.size() == 1, "should have one include path");
	ASSERT(opts.stackSize == 8192, "stack size should be 8192");
	ASSERT(opts.werror, "werror should be enabled");
	ASSERT(opts.files.size() == 2, "should have two files");
}

// Main - required for test executable
int main(void) {
	return UC_PrintResults();
}
