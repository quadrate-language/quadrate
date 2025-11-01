#include "cxxopts.hpp"
#include <cerrno>
#include <cgen/compiler.h>
#include <cgen/linker.h>
#include <cgen/process.h>
#include <cgen/source_file.h>
#include <cgen/transpiler.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <qc/colors.h>
#include <random>
#include <sstream>

#define QUADC_VERSION "0.1.0"

std::string getCompilerFlags() {
	// Optimization flags for tiny executables
	std::string opts = "-Os -flto -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -fno-ident";

	if (std::filesystem::exists("./dist/include")) {
		return opts + " -I./dist/include";
	}
	const char* home = getenv("HOME");
	if (home) {
		std::filesystem::path localInclude = std::filesystem::path(home) / ".local" / "include";
		if (std::filesystem::exists(localInclude)) {
			return opts + " -I" + localInclude.string();
		}
	}
	return opts;
}

std::string getLinkerFlags() {
	// Optimization flags for tiny executables
	std::string opts = "-flto -Wl,--gc-sections -s";

	if (std::filesystem::exists("./dist/lib")) {
		return opts + " -L./dist/lib -Wl,-rpath,./dist/lib -lquadrate -lm -pthread";
	}
	const char* home = getenv("HOME");
	if (home) {
		std::filesystem::path localLib = std::filesystem::path(home) / ".local" / "lib";
		if (std::filesystem::exists(localLib)) {
			return opts + " -L" + localLib.string() + " -Wl,-rpath," + localLib.string() + " -lquadrate -lm -pthread";
		}
	}
	return opts + " -lquadrate -lm -pthread";
}

std::string createTempDir(bool useCwd) {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 15);

	// Determine base directory: use cwd if --save-temps, otherwise use system temp
	std::filesystem::path baseDir;
	if (useCwd) {
		baseDir = std::filesystem::current_path();
	} else {
		baseDir = std::filesystem::temp_directory_path();
	}

	// Try up to 10 times to create a unique directory
	for (int attempt = 0; attempt < 10; attempt++) {
		std::stringstream ss;
		ss << "qd_";
		for (int i = 0; i < 8; i++) {
			ss << std::hex << dis(gen);
		}

		std::filesystem::path tmpDir = baseDir / ss.str();

		// Check if directory already exists
		if (std::filesystem::exists(tmpDir)) {
			continue;
		}

		// Try to create the directory
		std::error_code ec;
		if (std::filesystem::create_directory(tmpDir, ec)) {
			return tmpDir.string();
		}

		// If creation failed for reasons other than "already exists", fail
		if (ec && ec.value() != EEXIST) {
			std::cerr << "quadc: failed to create temporary directory: " << ec.message() << std::endl;
			exit(1);
		}
	}

	// Failed to create directory after multiple attempts
	std::cerr << "quadc: failed to create temporary directory after 10 attempts" << std::endl;
	exit(1);
}

// RAII guard for automatic temp directory cleanup
class TempDirGuard {
public:
	explicit TempDirGuard(const std::string& path) : mPath(path), mShouldDelete(true) {
	}

	~TempDirGuard() {
		if (mShouldDelete && !mPath.empty()) {
			std::filesystem::remove_all(mPath);
		}
	}

	void release() {
		mShouldDelete = false;
	}

	// Prevent copying
	TempDirGuard(const TempDirGuard&) = delete;
	TempDirGuard& operator=(const TempDirGuard&) = delete;

private:
	std::string mPath;
	bool mShouldDelete;
};

int main(int argc, char** argv) {
	cxxopts::Options options("quadc", "Quadrate compiler");
	options.add_options()("h,help", "Display help.")("v,version", "Display compiler version.")(
			"o", "Output filename", cxxopts::value<std::string>()->default_value("main"))("save-temps",
			"Save temporary files", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("no-colors",
			"Disable colored output", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("verbose",
			"Print compilation commands",
			cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("dump-tokens", "Print tokens",
			cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("r,run", "Run the compiled program",
			cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
			"shared", "Build a shared library", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
			"static", "Build a static library", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
			"files", "Input files", cxxopts::value<std::vector<std::string>>());

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

	// Configure colored output
	const bool noColors = result["no-colors"].as<bool>();
	Qd::Colors::setEnabled(!noColors);

	std::string outputFilename = result["o"].as<std::string>();
	const bool shared = result["shared"].as<bool>();
	const bool statik = result["static"].as<bool>();
	const bool run = result["run"].as<bool>();

	if (shared && statik) {
		std::cerr << "quadc: cannot build both shared and static library at the same time." << std::endl;
		return 1;
	}
	if (run && (shared || statik)) {
		std::cerr << "quadc: cannot run a library" << std::endl;
		return 1;
	}
	if (shared) {
		outputFilename = "lib" + outputFilename + ".so";
	} else if (statik) {
		outputFilename = "lib" + outputFilename + ".a";
	}

	// Check if we should preserve temp files (needed for temp dir location)
	const bool saveTemps = result["save-temps"].as<bool>();

	const std::string outputDir = createTempDir(saveTemps);
	TempDirGuard tempGuard(outputDir);

	// When running, place executable in temp directory's bin subdirectory
	std::string outputPath;
	if (run) {
		std::filesystem::path binDir = std::filesystem::path(outputDir) / "bin";
		std::filesystem::create_directory(binDir);
		outputPath = (binDir / outputFilename).string();
	} else {
		outputPath = outputFilename;
	}

	// Preserve temp files if requested
	if (saveTemps) {
		tempGuard.release();
		std::cout << "Temporary files saved in: " << outputDir << std::endl;
	}

	// Check if verbose output is enabled
	const bool verbose = result["verbose"].as<bool>();

	// Check if token dumping is enabled
	const bool dumpTokens = result["dump-tokens"].as<bool>();

	if (result.count("files")) {
		auto files = result["files"].as<std::vector<std::string>>();
		Qd::Transpiler transpiler;
		std::vector<Qd::SourceFile> transpiledSources;
		for (const auto& file : files) {
			std::ifstream qdFile(file);
			if (!qdFile.is_open()) {
				std::cerr << "quadc: cannot find " << file << ": No such file or directory" << std::endl;
				continue;
			}
			qdFile.seekg(0, std::ios::end);
			auto pos = qdFile.tellg();
			qdFile.seekg(0);
			if (pos < 0) {
				std::cerr << "quadc: error reading " << file << std::endl;
				continue;
			}
			size_t size = static_cast<size_t>(pos);
			std::string buffer(size, ' ');
			qdFile.read(&buffer[0], static_cast<std::streamsize>(size));
			std::string filename = std::filesystem::path(file).filename().string();
			if (auto ts = transpiler.emit(filename.c_str(), "main", buffer.c_str(), verbose, dumpTokens)) {
				transpiledSources.push_back(*ts);
			} else {
				return 1;
			}
		}

		for (auto& source : transpiledSources) {
			std::filesystem::path packageDir = std::filesystem::path(outputDir) / source.package;
			std::error_code ec;
			std::filesystem::create_directory(packageDir, ec);
			// Only fail if error is not "already exists"
			if (ec && ec.value() != EEXIST) {
				std::cerr << "quadc: failed to create directory " << packageDir << ": " << ec.message() << std::endl;
				return 1;
			}
			std::filesystem::path filePath = std::filesystem::path(outputDir) / source.filename;
			std::ofstream outFile(filePath);
			outFile << source.content;
			outFile.close();
		}

		// Generate main.c before compiling any sources
		if (!shared && !statik) {
			const char* mainFile = "// This file is automatically generated by the Quadrate compiler.\n"
								   "// Do not edit manually.\n\n"
								   "#include <quadrate/qd.h>\n\n"
								   "extern qd_exec_result usr_main_main(qd_context* ctx);\n\n"
								   "int main(void) {\n"
								   "    qd_context* ctx = qd_create_context(1024);\n"
								   "    qd_exec_result res = usr_main_main(ctx);\n"
								   "    qd_free_context(ctx);\n"
								   "    return res.code;\n"
								   "}\n";
			std::filesystem::path mainFilePath = std::filesystem::path(outputDir) / "main.c";
			std::ofstream mainFileStream(mainFilePath);
			mainFileStream << mainFile;
			mainFileStream.close();
		}

		std::vector<TranslationUnit> translationUnits;
		Qd::Compiler compiler;
		std::string compilerFlags = getCompilerFlags();
		for (auto& source : transpiledSources) {
			if (auto tu = compiler.compile(
						(std::filesystem::path(outputDir) / source.filename).c_str(), compilerFlags.c_str(), verbose)) {
				translationUnits.push_back(*tu);
			} else {
				std::cerr << "quadc: compilation failed for " << source.filename << std::endl;
				return 1;
			}
		}

		// Compile main.c only if it was generated (not building shared/static library)
		if (!shared && !statik) {
			std::filesystem::path mainCPath = std::filesystem::path(outputDir) / "main.c";
			if (auto tu = compiler.compile(mainCPath.string().c_str(), compilerFlags.c_str(), verbose)) {
				translationUnits.push_back(*tu);
			} else {
				std::cerr << "quadc: compilation failed for main.c" << std::endl;
				return 1;
			}
		}

		Qd::Linker linker;
		std::string linkerFlags = getLinkerFlags();
		bool linkSuccess = linker.link(translationUnits, outputPath.c_str(), linkerFlags.c_str(), verbose);
		if (!linkSuccess) {
			std::cerr << "quadc: linking failed" << std::endl;
			return 1;
		}

		// Run the program if requested
		if (run) {
			// Execute the compiled binary safely
			std::vector<std::string> args; // No arguments to the program
			int exitCode = Qd::executeProcess(outputPath, args);
			if (exitCode != 0) {
				std::cerr << "quadc: program exited with code " << exitCode << std::endl;
				return exitCode;
			}
		}
	}

	return 0;
}
