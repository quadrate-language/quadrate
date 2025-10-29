#include "cxxopts.hpp"
#include <cgen/compiler.h>
#include <cgen/linker.h>
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

std::string createTempDir() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 15);

	std::stringstream ss;
	ss << ".tmp_";
	for (int i = 0; i < 8; i++) {
		ss << std::hex << dis(gen);
	}

	std::string tmpDir = ss.str();
	std::filesystem::create_directory(tmpDir);
	return tmpDir;
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
	if (shared && statik) {
		std::cerr << "quadc: cannot build both shared and static library at the same time." << std::endl;
		return 1;
	}
	if (shared) {
		outputFilename = "lib" + outputFilename + ".so";
	} else if (statik) {
		outputFilename = "lib" + outputFilename + ".a";
	}

	const std::string outputDir = createTempDir();
	TempDirGuard tempGuard(outputDir);

	// Check if we should preserve temp files
	const bool saveTemps = result["save-temps"].as<bool>();
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
			if (!std::filesystem::create_directory(packageDir)) {
				std::cerr << "Directory already exists or failed to create: " << packageDir << std::endl;
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
								   "#include <quadrate/runtime/runtime.h>\n\n"
								   "extern qd_exec_result usr_main_main(qd_context* ctx);\n\n"
								   "int main(void) {\n"
								   "    qd_context ctx;\n"
								   "    qd_stack_init(&ctx.st, 1024);\n"
								   "    usr_main_main(&ctx);\n"
								   "    qd_stack_destroy(ctx.st);\n"
								   "    return 0;\n"
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
		bool linkSuccess = linker.link(translationUnits, outputFilename.c_str(), linkerFlags.c_str(), verbose);
		if (!linkSuccess) {
			std::cerr << "quadc: linking failed" << std::endl;
			return 1;
		}
	}

	return 0;
}
