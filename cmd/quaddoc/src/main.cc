// quaddoc - Generate HTML documentation from Quadrate source files
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "version.h"

namespace fs = std::filesystem;

// --- Data types ---

struct Param {
	std::string name, type, desc;
};

struct Field {
	std::string name, type, desc;
};

struct Function {
	std::string name, signature, receiver, desc;
	bool fallible = false;
	std::vector<Param> params, returns, errors;
	std::vector<std::string> examples;
};

struct Struct {
	std::string name, desc;
	std::vector<Field> fields;
};

struct Constant {
	std::string name, value, desc;
};

struct Module {
	std::string name, desc, path;
	std::vector<Function> functions;
	std::vector<Struct> structs;
	std::vector<Constant> constants;
};

// --- Helpers ---

static std::string trim(const std::string& s) {
	auto start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) {
		return "";
	}
	auto end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}

static std::string htmlEscape(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	for (char c : s) {
		switch (c) {
		case '&':
			out += "&amp;";
			break;
		case '<':
			out += "&lt;";
			break;
		case '>':
			out += "&gt;";
			break;
		case '"':
			out += "&quot;";
			break;
		default:
			out += c;
		}
	}
	return out;
}

static std::string readFile(const std::string& path) {
	std::ifstream f(path);
	if (!f) {
		return "";
	}
	std::ostringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

// Expand `use foo.qd` includes
static std::string expandIncludes(const std::string& path) {
	std::ifstream f(path);
	if (!f) {
		return "";
	}
	std::string dir = fs::path(path).parent_path().string();
	std::ostringstream out;
	std::string line;
	std::regex useRe(R"(^\s*use\s+([a-zA-Z0-9_]+\.qd))");
	while (std::getline(f, line)) {
		std::smatch m;
		if (std::regex_search(line, m, useRe)) {
			std::string inc = dir + "/" + m[1].str();
			if (fs::exists(inc)) {
				out << expandIncludes(inc);
				continue;
			}
		}
		out << line << "\n";
	}
	return out.str();
}

// --- Doc comment parsing ---

struct DocInfo {
	std::string desc;
	std::vector<Param> params, returns, errors;
	std::vector<std::string> examples;
	std::vector<Field> fields;
};

static DocInfo parseDocBuffer(const std::vector<std::string>& buf) {
	DocInfo d;
	std::vector<std::string> descParts;
	std::regex paramRe(R"(^@param\s+(\w+)\s+(\S+)\s*(.*))");
	std::regex returnRe(R"(^@return\s+(\w+)\s+(\S+)\s*(.*))");
	std::regex errorRe(R"(^@error\s+(\S+)\s*(.*))");
	std::regex exampleRe(R"(^@example\s+(.+))");
	std::regex fieldRe(R"(^@field\s+(\w+)\s+(\S+)\s*(.*))");

	for (auto& line : buf) {
		std::smatch m;
		if (std::regex_search(line, m, paramRe)) {
			d.params.push_back({m[1], m[2], m[3]});
		} else if (std::regex_search(line, m, returnRe)) {
			d.returns.push_back({m[1], m[2], m[3]});
		} else if (std::regex_search(line, m, errorRe)) {
			d.errors.push_back({m[1], "", m[2]});
		} else if (std::regex_search(line, m, exampleRe)) {
			d.examples.push_back(m[1]);
		} else if (std::regex_search(line, m, fieldRe)) {
			d.fields.push_back({m[1], m[2], m[3]});
		} else if (line.empty() || line[0] != '@') {
			descParts.push_back(line);
		}
	}
	std::ostringstream ss;
	for (size_t i = 0; i < descParts.size(); i++) {
		if (i > 0) {
			ss << " ";
		}
		ss << descParts[i];
	}
	d.desc = trim(ss.str());
	return d;
}

// --- Module parsing ---

static Module parseModule(const std::string& path) {
	std::string content = expandIncludes(path);
	Module mod;
	mod.path = path;

	// Derive name
	auto stem = fs::path(path).stem().string();
	mod.name = (stem == "module") ? fs::path(path).parent_path().filename().string() : stem;

	std::regex docRe(R"(^\s*///\s?(.*))");
	std::regex pubFnRe(R"(^\s*pub\s+fn\s+(?:\(([^)]*)\)\s+)?(\w+)\s*\(([^)]*)\)\s*(!?))");
	std::regex pubConstRe(R"(^\s*pub\s+const\s+(\w+)\s*=\s*(.+))");
	std::regex structRe(R"(^\s*(?:pub\s+)?struct\s+(\w+(?:<[^>]+>)?))");
	std::regex pubStructRe(R"(^\s*pub\s+struct\s+)");
	std::regex fieldDefRe(R"(^\s*(\w+)\s*:\s*([a-zA-Z0-9_*<>]+))");

	std::vector<std::string> docBuf;
	bool moduleDocFound = false;
	bool inStruct = false;
	bool structIsPub = false;
	Struct curStruct;

	std::istringstream stream(content);
	std::string line;
	while (std::getline(stream, line)) {
		auto trimmed = trim(line);
		std::smatch m;

		// Doc comments
		if (std::regex_search(line, m, docRe)) {
			docBuf.push_back(m[1]);
			continue;
		}

		// Skip regular comments
		if (trimmed.substr(0, 2) == "//" || trimmed.substr(0, 2) == "/*") {
			continue;
		}

		// Empty line — capture module doc
		if (trimmed.empty()) {
			if (!docBuf.empty() && !moduleDocFound && mod.desc.empty()) {
				mod.desc = parseDocBuffer(docBuf).desc;
				moduleDocFound = true;
			}
			docBuf.clear();
			continue;
		}

		bool isPub = trimmed.substr(0, 4) == "pub ";

		// Struct fields
		if (inStruct) {
			if (trimmed == "}") {
				if (structIsPub) {
					mod.structs.push_back(curStruct);
				}
				inStruct = false;
			} else if (std::regex_search(trimmed, m, fieldDefRe)) {
				curStruct.fields.push_back({m[1], m[2], ""});
			}
			docBuf.clear();
			continue;
		}

		// Function
		if (std::regex_search(trimmed, m, pubFnRe) && isPub) {
			auto doc = parseDocBuffer(docBuf);
			Function fn;
			fn.name = m[2];
			fn.receiver = trim(m[1].str());
			fn.fallible = m[4].str() == "!";
			fn.desc = doc.desc;
			fn.params = doc.params;
			fn.returns = doc.returns;
			fn.errors = doc.errors;
			fn.examples = doc.examples;

			if (!fn.receiver.empty()) {
				fn.signature = fn.receiver + " " + fn.name + "(" + m[3].str() + ")";
			} else {
				fn.signature = fn.name + "(" + m[3].str() + ")";
			}
			if (fn.fallible) {
				fn.signature += "!";
			}

			mod.functions.push_back(fn);
			docBuf.clear();
			continue;
		}

		// Constant
		if (std::regex_search(trimmed, m, pubConstRe)) {
			auto doc = parseDocBuffer(docBuf);
			mod.constants.push_back({m[1], trim(m[2].str()), doc.desc});
			docBuf.clear();
			continue;
		}

		// Struct
		if (std::regex_search(trimmed, m, structRe)) {
			auto doc = parseDocBuffer(docBuf);
			structIsPub = isPub || std::regex_search(trimmed, pubStructRe);
			curStruct = Struct{m[1], doc.desc, {}};

			if (!doc.fields.empty()) {
				curStruct.fields = doc.fields;
				if (structIsPub) {
					mod.structs.push_back(curStruct);
				}
			} else if (trimmed.find('{') != std::string::npos && trimmed.find('}') == std::string::npos) {
				inStruct = true;
			}
			docBuf.clear();
			continue;
		}

		// Import/use — module doc
		if (trimmed.substr(0, 7) == "import " || trimmed.substr(0, 4) == "use ") {
			if (!docBuf.empty() && mod.desc.empty()) {
				mod.desc = parseDocBuffer(docBuf).desc;
				moduleDocFound = true;
			}
			docBuf.clear();
			continue;
		}

		docBuf.clear();
	}

	return mod;
}

static std::vector<Module> scanDirectory(const std::string& dir) {
	std::vector<Module> modules;
	for (auto& entry : fs::recursive_directory_iterator(dir)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".qd") {
			continue;
		}
		auto mod = parseModule(entry.path().string());
		if (!mod.functions.empty() || !mod.structs.empty() || !mod.constants.empty()) {
			modules.push_back(mod);
		}
	}
	std::sort(modules.begin(), modules.end(), [](auto& a, auto& b) { return a.name < b.name; });
	return modules;
}

// --- HTML generation ---

static const char* defaultCSS = R"(
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: system-ui, -apple-system, sans-serif; background: #fafafa; color: #222; line-height: 1.6; }
a { color: #5b5ea6; text-decoration: none; }
a:hover { text-decoration: underline; }
nav { background: #2d2d3f; color: #eee; padding: 1rem 2rem; }
nav h1 { font-size: 1.2rem; display: inline; }
nav a { color: #b8b8ff; margin-left: 1.5rem; }
.container { max-width: 900px; margin: 0 auto; padding: 2rem; }
h2 { border-bottom: 2px solid #ddd; padding-bottom: 0.3rem; margin: 2rem 0 1rem; color: #333; }
h3 { margin: 1.5rem 0 0.5rem; color: #444; }
.desc { color: #555; margin-bottom: 1rem; }
.sig { font-family: 'SF Mono', Consolas, monospace; background: #f0f0f5; padding: 0.5rem 0.8rem; border-radius: 4px; display: block; margin: 0.3rem 0; font-size: 0.9rem; border-left: 3px solid #5b5ea6; }
.fallible .sig { border-left-color: #d9534f; }
table { border-collapse: collapse; width: 100%; margin: 0.5rem 0; }
th, td { border: 1px solid #ddd; padding: 0.4rem 0.8rem; text-align: left; font-size: 0.9rem; }
th { background: #f5f5f5; font-weight: 500; }
code { font-family: 'SF Mono', Consolas, monospace; background: #f0f0f5; padding: 0.1rem 0.3rem; border-radius: 3px; font-size: 0.85rem; }
pre { background: #2d2d3f; color: #e8e8e8; padding: 0.8rem 1rem; border-radius: 4px; overflow-x: auto; margin: 0.5rem 0; }
pre code { background: none; padding: 0; color: inherit; }
.tag { font-size: 0.75rem; padding: 0.1rem 0.4rem; border-radius: 3px; font-weight: 500; }
.tag-fallible { background: #fce4e4; color: #c0392b; }
.module-list { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 1rem; }
.module-card { background: white; border: 1px solid #ddd; border-radius: 6px; padding: 1rem; }
.module-card h3 { margin: 0 0 0.3rem; }
.module-card p { font-size: 0.85rem; color: #666; }
.module-card .count { font-size: 0.8rem; color: #999; }
)";

static void writeParamTable(std::ofstream& f, const char* header, const std::vector<Param>& params) {
	if (params.empty()) {
		return;
	}
	f << "<table><tr><th>" << header << "</th><th>Type</th><th>Description</th></tr>\n";
	for (auto& p : params) {
		f << "<tr><td><code>" << htmlEscape(p.name) << "</code></td><td><code>" << htmlEscape(p.type)
		  << "</code></td><td>" << htmlEscape(p.desc) << "</td></tr>\n";
	}
	f << "</table>\n";
}

static void writeErrorTable(std::ofstream& f, const std::vector<Param>& errors) {
	if (errors.empty()) {
		return;
	}
	f << "<table><tr><th>Error</th><th>Description</th></tr>\n";
	for (auto& e : errors) {
		f << "<tr><td><code>" << htmlEscape(e.name) << "</code></td><td>" << htmlEscape(e.desc) << "</td></tr>\n";
	}
	f << "</table>\n";
}

static int generate(const std::vector<Module>& modules, const std::string& outDir, const std::string& title,
		const std::string& customCSSPath) {
	fs::create_directories(outDir);

	// Write CSS
	{
		std::ofstream f(outDir + "/style.css");
		f << defaultCSS;
		if (!customCSSPath.empty()) {
			std::string custom = readFile(customCSSPath);
			if (custom.empty()) {
				std::cerr << "Error: Cannot read CSS file: " << customCSSPath << "\n";
				return 1;
			}
			f << "\n/* Custom styles */\n" << custom;
		}
	}

	// Write index
	{
		std::ofstream f(outDir + "/index.html");
		f << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>" << htmlEscape(title)
		  << "</title><link rel=\"stylesheet\" href=\"style.css\"></head><body>\n"
		  << "<nav><h1>" << htmlEscape(title) << " Documentation</h1></nav>\n"
		  << "<div class=\"container\"><h2>Modules</h2><div class=\"module-list\">\n";
		for (auto& mod : modules) {
			f << "<div class=\"module-card\"><h3><a href=\"" << mod.name << ".html\">" << htmlEscape(mod.name)
			  << "</a></h3><p>" << htmlEscape(mod.desc) << "</p><span class=\"count\">" << mod.functions.size()
			  << " functions";
			if (!mod.structs.empty()) {
				f << ", " << mod.structs.size() << " structs";
			}
			if (!mod.constants.empty()) {
				f << ", " << mod.constants.size() << " constants";
			}
			f << "</span></div>\n";
		}
		f << "</div></div></body></html>\n";
	}

	// Write module pages
	for (auto& mod : modules) {
		std::ofstream f(outDir + "/" + mod.name + ".html");
		f << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>" << htmlEscape(mod.name) << " - "
		  << htmlEscape(title) << "</title><link rel=\"stylesheet\" href=\"style.css\"></head><body>\n"
		  << "<nav><h1>" << htmlEscape(title) << "</h1><a href=\"index.html\">All Modules</a></nav>\n"
		  << "<div class=\"container\"><h2>" << htmlEscape(mod.name) << "</h2>\n";
		if (!mod.desc.empty()) {
			f << "<p class=\"desc\">" << htmlEscape(mod.desc) << "</p>\n";
		}

		// Structs
		if (!mod.structs.empty()) {
			f << "<h2>Structs</h2>\n";
			for (auto& s : mod.structs) {
				f << "<h3 id=\"" << s.name << "\"><code>" << htmlEscape(s.name) << "</code></h3>\n";
				if (!s.desc.empty()) {
					f << "<p class=\"desc\">" << htmlEscape(s.desc) << "</p>\n";
				}
				if (!s.fields.empty()) {
					f << "<table><tr><th>Field</th><th>Type</th><th>Description</th></tr>\n";
					for (auto& fld : s.fields) {
						f << "<tr><td><code>" << htmlEscape(fld.name) << "</code></td><td><code>"
						  << htmlEscape(fld.type) << "</code></td><td>" << htmlEscape(fld.desc) << "</td></tr>\n";
					}
					f << "</table>\n";
				}
			}
		}

		// Constants
		if (!mod.constants.empty()) {
			f << "<h2>Constants</h2>\n<table><tr><th>Name</th><th>Value</th><th>Description</th></tr>\n";
			for (auto& c : mod.constants) {
				f << "<tr><td><code>" << htmlEscape(c.name) << "</code></td><td><code>" << htmlEscape(c.value)
				  << "</code></td><td>" << htmlEscape(c.desc) << "</td></tr>\n";
			}
			f << "</table>\n";
		}

		// Functions
		if (!mod.functions.empty()) {
			f << "<h2>Functions</h2>\n";
			for (auto& fn : mod.functions) {
				f << "<div class=\"fn" << (fn.fallible ? " fallible" : "") << "\" id=\"" << fn.name << "\">\n"
				  << "<h3>" << htmlEscape(fn.name);
				if (fn.fallible) {
					f << " <span class=\"tag tag-fallible\">fallible</span>";
				}
				f << "</h3>\n<code class=\"sig\">fn " << htmlEscape(fn.signature) << "</code>\n";
				if (!fn.desc.empty()) {
					f << "<p class=\"desc\">" << htmlEscape(fn.desc) << "</p>\n";
				}
				writeParamTable(f, "Param", fn.params);
				writeParamTable(f, "Return", fn.returns);
				writeErrorTable(f, fn.errors);
				if (!fn.examples.empty()) {
					f << "<strong>Examples:</strong>\n";
					for (auto& ex : fn.examples) {
						f << "<pre><code>" << htmlEscape(ex) << "</code></pre>\n";
					}
				}
				f << "</div>\n";
			}
		}

		f << "</div></body></html>\n";
	}

	return 0;
}

// --- Main ---

static void printHelp() {
	std::cout << "quaddoc - Generate HTML documentation from Quadrate source files\n\n"
			  << "Usage: quaddoc [options] <directory>\n\n"
			  << "Options:\n"
			  << "  -h, --help         Show this help message\n"
			  << "  -v, --version      Show version information\n"
			  << "  -o <dir>           Output directory (default: docs)\n"
			  << "  -q, --quiet        Quiet mode — no output except errors\n"
			  << "  --title <str>      Project title (default: \"Quadrate API\")\n"
			  << "  --css <file>       Append custom CSS file after default styles\n\n"
			  << "Examples:\n"
			  << "  quaddoc                          # Scan current dir, output to docs/\n"
			  << "  quaddoc -o api lib/              # Scan lib/, output to api/\n"
			  << "  quaddoc --title \"My Project\" .   # Custom title\n"
			  << "  quaddoc --css custom.css lib/    # Custom styling\n";
}

int main(int argc, char* argv[]) {
	std::string outDir = "docs";
	std::string title = "Quadrate API";
	std::string customCSS;
	std::string dir = ".";
	bool quiet = false;

	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "-h" || arg == "--help") {
			printHelp();
			return 0;
		} else if (arg == "-v" || arg == "--version") {
			std::cout << "quaddoc " << QUADRATE_VERSION << "\n";
			return 0;
		} else if (arg == "-o" && i + 1 < argc) {
			outDir = argv[++i];
		} else if (arg == "-q" || arg == "--quiet") {
			quiet = true;
		} else if (arg == "--title" && i + 1 < argc) {
			title = argv[++i];
		} else if (arg == "--css" && i + 1 < argc) {
			customCSS = argv[++i];
		} else if (arg[0] != '-') {
			dir = arg;
		} else {
			std::cerr << "quaddoc: unknown option: " << arg << "\n";
			return 1;
		}
	}

	if (!quiet) {
		std::cout << "Scanning " << dir << "...\n";
	}

	auto modules = scanDirectory(dir);
	if (modules.empty()) {
		std::cout << "No documented modules found.\n";
		return 0;
	}

	int err = generate(modules, outDir, title, customCSS);
	if (err) {
		return err;
	}

	if (!quiet) {
		std::cout << "Generated " << modules.size() << " module pages in " << outDir << "/\n";
		for (auto& mod : modules) {
			std::cout << "  " << mod.name << " — " << mod.functions.size() << " functions, " << mod.structs.size()
					  << " structs, " << mod.constants.size() << " constants\n";
		}
	}

	return 0;
}
