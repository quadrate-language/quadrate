// quaddoc - Generate HTML documentation from Quadrate source files
#include <quadrate/cli/cli.h>
#include <quadrate/cli/help.h>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_function_pointer.h>
#include <quadrate/qc/ast_node_global_var.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_import.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_struct_declaration.h>
#include <quadrate/qc/ast_node_struct_field.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
	// Identifiers the body referenced, collected off the AST during
	// parseModule. buildCallGraph resolves these into calls once every module
	// is known; qualified ones are `module::name`, plain ones a bare name.
	std::vector<std::string> qualifiedRefs, plainRefs;
	// Cross-module references, populated by buildCallGraph after all
	// modules are parsed. Each entry is "module::function". Sorted, deduped.
	std::vector<std::string> calls;
	std::vector<std::string> calledBy;
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
	// Anything the parser could not read. A module with errors is still
	// documented as far as it parsed, but the caller reports them so a broken
	// file is not silently documented as empty.
	std::vector<Qd::ErrorInfo> parseErrors;
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

// Expand `use foo.qd` includes.
//
// Textual on purpose, and the only place a Quadrate construct is still matched
// by hand: this runs *before* parsing, to build the single buffer that gets
// parsed and that the doc index is line-numbered against. A module split across
// files (`crypto.qd` pulling in `md5.qd`, `sha256.qd`, ...) is documented as the
// one module it presents itself as.
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
//
// `@param`, `@return`, `@error`, `@example` and `@field` are a small language of
// their own, living inside comments that the Quadrate lexer discards. There is
// no AST for them, so they are matched here. What these tags do *not* decide any
// more is what a declaration is: names, types and order come from the signature,
// and a tag only supplies prose.

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
//
// Structure comes from the compiler's own parser: which declarations a module
// has, their names, parameters, receivers, generics and visibility are read off
// the AST rather than matched out of the text. The previous line-based scanner
// silently dropped anything its patterns did not anticipate -- `pub inline fn`
// never matched `pub\s+fn`, which is why all 17 functions of the `sys` module,
// and so the whole module, were missing from the generated documentation.
//
// One thing still has to be read textually: doc comments. The lexer discards
// `///` along with every other comment, so the AST cannot say what a
// declaration's documentation is. collectDocs below scans the source once for
// runs of `///` lines and indexes each run by the line of the code that follows
// it; a declaration then looks up its own line, which the AST does give us.

// A doc block and where it landed.
struct DocIndex {
	// Doc block indexed by the source line of the declaration it precedes.
	std::unordered_map<size_t, DocInfo> byLine;
	// Those same lines in file order, so the first unclaimed one can become the
	// module description.
	std::vector<size_t> order;
	// Blocks separated from what follows by a blank line, so attached to
	// nothing. The first is the module description when there is one.
	std::vector<DocInfo> floating;
};

static DocIndex collectDocs(const std::string& source) {
	DocIndex docs;
	std::vector<std::string> buf;
	std::istringstream stream(source);
	std::string line;
	size_t lineNo = 0;

	while (std::getline(stream, line)) {
		lineNo++;
		std::string t = trim(line);

		if (t.rfind("///", 0) == 0) {
			// Drop the marker and one optional space, keeping further
			// indentation so that @example blocks survive intact.
			std::string text = t.substr(3);
			if (!text.empty() && text[0] == ' ') {
				text.erase(0, 1);
			}
			buf.push_back(text);
			continue;
		}

		if (buf.empty()) {
			continue;
		}

		if (t.empty()) {
			docs.floating.push_back(parseDocBuffer(buf));
			buf.clear();
			continue;
		}

		// An ordinary comment between a doc block and its declaration does not
		// break the association -- `/// ...` then `// TODO` then `pub fn` still
		// documents the function.
		if (t.rfind("//", 0) == 0 || t.rfind("/*", 0) == 0) {
			continue;
		}

		docs.byLine[lineNo] = parseDocBuffer(buf);
		docs.order.push_back(lineNo);
		buf.clear();
	}

	return docs;
}

// Render a parameter list the way it is written in source: `name:type`,
// separated by spaces, with ` -- ` between inputs and outputs.
static std::string formatParams(
		const std::vector<const Qd::AstNodeParameter*>& in, const std::vector<const Qd::AstNodeParameter*>& out) {
	// Both empty is spelled `()` rather than `( -- )`; the AST cannot tell the
	// two apart, and `()` is what the receiver methods in the corpus write.
	if (in.empty() && out.empty()) {
		return "()";
	}
	// A parameter may be written as a bare type -- `fn sq(f64 -- f64)` is a
	// legal stack effect -- in which case there is no `name:` to print.
	auto one = [](const Qd::AstNodeParameter* p) {
		return p->name().empty() ? p->typeString() : p->name() + ":" + p->typeString();
	};

	std::string s = "(";
	for (size_t i = 0; i < in.size(); i++) {
		if (i > 0) {
			s += " ";
		}
		s += one(in[i]);
	}
	s += " -- ";
	for (size_t i = 0; i < out.size(); i++) {
		if (i > 0) {
			s += " ";
		}
		s += one(out[i]);
	}
	s += ")";
	return s;
}

static std::string formatTypeParams(const std::vector<std::string>& typeParams) {
	if (typeParams.empty()) {
		return "";
	}
	std::string s = "<";
	for (size_t i = 0; i < typeParams.size(); i++) {
		if (i > 0) {
			s += ", ";
		}
		s += typeParams[i];
	}
	return s + ">";
}

// The parameters a function actually takes, described by its doc comment where
// the comment says something. The signature is the source of truth for name,
// type and order; `@param`/`@return` only supply prose. A tag naming something
// the function does not have is stale and is dropped rather than printed.
static std::vector<Param> mergeParams(
		const std::vector<const Qd::AstNodeParameter*>& actual, const std::vector<Param>& documented) {
	std::vector<Param> merged;
	merged.reserve(actual.size());
	std::vector<bool> used(documented.size(), false);

	for (const Qd::AstNodeParameter* p : actual) {
		Param out{p->name(), p->typeString(), ""};
		for (size_t i = 0; i < documented.size(); i++) {
			if (!used[i] && !out.name.empty() && documented[i].name == out.name) {
				out.desc = documented[i].desc;
				used[i] = true;
				break;
			}
		}
		merged.push_back(out);
	}

	// Tags are written in signature order, so an unmatched one still describes
	// the parameter in its position. This recovers two common cases: a tag that
	// names the parameter differently from the declaration (`@return rng` for
	// an output declared `rng2`), and a parameter written as a bare type, which
	// has no name for a tag to match but is invariably documented with one.
	for (size_t i = 0; i < merged.size(); i++) {
		if (!merged[i].desc.empty() || i >= documented.size() || used[i]) {
			continue;
		}
		merged[i].desc = documented[i].desc;
		if (merged[i].name.empty()) {
			merged[i].name = documented[i].name;
		}
		used[i] = true;
	}

	return merged;
}

// Identifiers referenced by a function body, for the cross-reference graph.
// Reading these off the AST rather than regexing the text means a name inside a
// string literal or a comment is no longer counted as a call.
static void collectRefs(
		const Qd::IAstNode* node, std::vector<std::string>& qualified, std::vector<std::string>& plain) {
	if (!node) {
		return;
	}
	switch (node->type()) {
	case Qd::IAstNode::Type::SCOPED_IDENTIFIER: {
		const auto* scoped = static_cast<const Qd::AstNodeScopedIdentifier*>(node);
		qualified.push_back(scoped->scope() + "::" + scoped->name());
		break;
	}
	case Qd::IAstNode::Type::IDENTIFIER: {
		const auto* ident = static_cast<const Qd::AstNodeIdentifier*>(node);
		plain.push_back(ident->name());
		break;
	}
	case Qd::IAstNode::Type::FUNCTION_POINTER_REFERENCE: {
		// `&name` passes a function without calling it, which is still a
		// reference worth showing in the cross-reference list.
		const auto* ref = static_cast<const Qd::AstNodeFunctionPointerReference*>(node);
		plain.push_back(ref->functionName());
		break;
	}
	default:
		break;
	}
	for (size_t i = 0; i < node->childCount(); i++) {
		collectRefs(node->child(i), qualified, plain);
	}
}

// The doc block written above the declaration on this line, if any.
static DocInfo docFor(const DocIndex& docs, size_t line) {
	auto it = docs.byLine.find(line);
	return it == docs.byLine.end() ? DocInfo{} : it->second;
}

// Walk the tree and record every public declaration. Function bodies are not
// descended into: a declaration cannot appear inside one, and the body is
// handled separately for the call graph.
static void collectDecls(const Qd::IAstNode* node, Module& mod, const DocIndex& docs, std::set<size_t>& claimed) {
	if (!node) {
		return;
	}

	switch (node->type()) {
	case Qd::IAstNode::Type::FUNCTION_DECLARATION: {
		const auto* fnNode = static_cast<const Qd::AstNodeFunctionDeclaration*>(node);
		if (!fnNode->isPublic()) {
			return;
		}
		DocInfo doc = docFor(docs, fnNode->line());
		claimed.insert(fnNode->line());

		std::vector<const Qd::AstNodeParameter*> in, out;
		for (const auto& p : fnNode->inputParameters()) {
			in.push_back(static_cast<const Qd::AstNodeParameter*>(p.get()));
		}
		for (const auto& p : fnNode->outputParameters()) {
			out.push_back(static_cast<const Qd::AstNodeParameter*>(p.get()));
		}

		Function fn;
		fn.name = fnNode->name();
		fn.fallible = fnNode->throws();
		fn.desc = doc.desc;
		fn.params = mergeParams(in, doc.params);
		fn.returns = mergeParams(out, doc.returns);
		fn.errors = doc.errors;
		fn.examples = doc.examples;

		if (fnNode->hasReceiver()) {
			const std::string receiverType = fnNode->receiverType() + formatTypeParams(fnNode->receiverTypeParams());
			fn.receiver = fnNode->receiverName() + ":" + receiverType;
			// Parenthesised, as the language writes it, so the rendered
			// signature can be pasted back into a source file.
			fn.signature = "(" + fn.receiver + ") ";

			// The receiver is an input like any other, and the corpus documents
			// it with an ordinary `@param`, so it leads the parameter table.
			// Its type comes from the signature: several modules describe the
			// receiver as `ptr` in the tag where the declaration says `Flag`.
			Param self{fnNode->receiverName(), receiverType, ""};
			for (const Param& documented : doc.params) {
				if (documented.name == self.name) {
					self.desc = documented.desc;
					break;
				}
			}
			fn.params.insert(fn.params.begin(), self);
		}
		fn.signature += fnNode->name() + formatTypeParams(fnNode->typeParams()) + formatParams(in, out);
		if (fn.fallible) {
			fn.signature += "!";
		}

		collectRefs(fnNode->body(), fn.qualifiedRefs, fn.plainRefs);
		mod.functions.push_back(std::move(fn));
		return; // do not descend into parameters or body
	}

	case Qd::IAstNode::Type::IMPORT_STATEMENT: {
		// `import "libio.a" as "io" { pub fn ... }` -- how every C-implemented
		// module declares its surface. These functions hang off the import node
		// rather than appearing as children, so they need handling here or the
		// seventeen native modules would document nothing.
		const auto* imp = static_cast<const Qd::AstNodeImport*>(node);
		for (const auto& imported : imp->functions()) {
			if (!imported->isPublic) {
				continue;
			}
			DocInfo doc = docFor(docs, imported->line);
			claimed.insert(imported->line);

			std::vector<const Qd::AstNodeParameter*> in, out;
			for (const auto& p : imported->inputParameters) {
				in.push_back(p.get());
			}
			for (const auto& p : imported->outputParameters) {
				out.push_back(p.get());
			}

			Function fn;
			fn.name = imported->name;
			fn.fallible = imported->throws;
			fn.desc = doc.desc;
			fn.params = mergeParams(in, doc.params);
			fn.returns = mergeParams(out, doc.returns);
			fn.errors = doc.errors;
			fn.examples = doc.examples;
			fn.signature = imported->name + formatParams(in, out);
			if (fn.fallible) {
				fn.signature += "!";
			}
			mod.functions.push_back(std::move(fn));
		}
		return;
	}

	case Qd::IAstNode::Type::STRUCT_DECLARATION: {
		const auto* st = static_cast<const Qd::AstNodeStructDeclaration*>(node);
		if (!st->isPublic()) {
			return;
		}
		DocInfo doc = docFor(docs, st->line());
		claimed.insert(st->line());

		Struct s;
		s.name = st->name() + formatTypeParams(st->typeParams());
		s.desc = doc.desc;
		for (const auto& field : st->fields()) {
			Field f{field->name(), field->typeName(), ""};
			// A field's description can come from a `///` on the field itself
			// or from an `@field` tag on the struct; prefer the closer one.
			auto own = docs.byLine.find(field->line());
			if (own != docs.byLine.end() && !own->second.desc.empty()) {
				f.desc = own->second.desc;
				claimed.insert(field->line());
			} else {
				for (const Field& documented : doc.fields) {
					if (documented.name == f.name) {
						f.desc = documented.desc;
						break;
					}
				}
			}
			s.fields.push_back(f);
		}
		// A struct documented only with @field tags and no parsed fields (an
		// opaque handle, say) still lists what the tags describe.
		if (s.fields.empty()) {
			s.fields = doc.fields;
		}
		mod.structs.push_back(std::move(s));
		return;
	}

	case Qd::IAstNode::Type::CONSTANT_DECLARATION: {
		const auto* c = static_cast<const Qd::AstNodeConstant*>(node);
		if (!c->isPublic()) {
			return;
		}
		DocInfo doc = docFor(docs, c->line());
		claimed.insert(c->line());
		mod.constants.push_back({c->name(), c->value() ? c->value() : "", doc.desc});
		return;
	}

	case Qd::IAstNode::Type::GLOBAL_VAR_DECLARATION: {
		const auto* v = static_cast<const Qd::AstNodeGlobalVar*>(node);
		if (!v->isPublic()) {
			return;
		}
		DocInfo doc = docFor(docs, v->line());
		claimed.insert(v->line());
		// Documented alongside constants; the initialiser as written is more
		// useful than the folded value.
		mod.constants.push_back({v->name(), v->sourceExpr(), doc.desc});
		return;
	}

	default:
		break;
	}

	for (size_t i = 0; i < node->childCount(); i++) {
		collectDecls(node->child(i), mod, docs, claimed);
	}
}

static Module parseModule(const std::string& path) {
	std::string content = expandIncludes(path);
	Module mod;
	mod.path = path;

	auto stem = fs::path(path).stem().string();
	mod.name = (stem == "module") ? fs::path(path).parent_path().filename().string() : stem;

	DocIndex docs = collectDocs(content);

	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(content.c_str(), false, path.c_str());
	if (!root) {
		return mod;
	}
	// Parse errors are reported by the caller; a partial tree still documents
	// everything the parser did understand, which beats documenting nothing.
	mod.parseErrors = ast.getErrors();

	std::set<size_t> claimed;
	collectDecls(root, mod, docs, claimed);

	// The module description: a block that was separated from the code by a
	// blank line, or failing that the first block attached to something that is
	// not a documented declaration -- a `use` line, typically.
	if (!docs.floating.empty()) {
		mod.desc = docs.floating.front().desc;
	} else {
		for (size_t line : docs.order) {
			if (claimed.count(line)) {
				continue;
			}
			mod.desc = docs.byLine.at(line).desc;
			break;
		}
	}

	return mod;
}

// Populate Function::calls and Function::calledBy across all modules from the
// references collected during parsing. A qualified `mod::name` is a call when
// that module documents such a function; a bare `name` is one only when this
// module defines it, which keeps locals and stack words out of the graph.
static void buildCallGraph(std::vector<Module>& modules) {
	std::unordered_map<std::string, std::pair<size_t, size_t>> index;
	std::unordered_map<std::string, std::unordered_set<std::string>> moduleSymbols;
	for (size_t mi = 0; mi < modules.size(); mi++) {
		auto& mod = modules[mi];
		for (size_t fi = 0; fi < mod.functions.size(); fi++) {
			std::string qn = mod.name + "::" + mod.functions[fi].name;
			index[qn] = {mi, fi};
			moduleSymbols[mod.name].insert(mod.functions[fi].name);
		}
	}

	for (auto& mod : modules) {
		for (auto& fn : mod.functions) {
			std::set<std::string> callees;
			std::string self = mod.name + "::" + fn.name;

			for (const std::string& qn : fn.qualifiedRefs) {
				if (qn != self && index.count(qn)) {
					callees.insert(qn);
				}
			}
			const auto& syms = moduleSymbols[mod.name];
			for (const std::string& name : fn.plainRefs) {
				if (name == fn.name || !syms.count(name)) {
					continue;
				}
				callees.insert(mod.name + "::" + name);
			}

			fn.calls.assign(callees.begin(), callees.end());
		}
	}

	for (auto& mod : modules) {
		for (auto& fn : mod.functions) {
			std::string self = mod.name + "::" + fn.name;
			for (const auto& callee : fn.calls) {
				auto it = index.find(callee);
				if (it == index.end()) {
					continue;
				}
				modules[it->second.first].functions[it->second.second].calledBy.push_back(self);
			}
		}
	}

	for (auto& mod : modules) {
		for (auto& fn : mod.functions) {
			std::sort(fn.calledBy.begin(), fn.calledBy.end());
			fn.calledBy.erase(std::unique(fn.calledBy.begin(), fn.calledBy.end()), fn.calledBy.end());
		}
	}
}

static std::vector<Module> scanDirectory(const std::string& dir, bool quiet) {
	std::vector<Module> modules;
	for (auto& entry : fs::recursive_directory_iterator(dir)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".qd") {
			continue;
		}
		auto mod = parseModule(entry.path().string());

		// A file the parser chokes on still documents whatever came before the
		// error, so generation continues -- but say so rather than quietly
		// publishing a module with half its functions missing, which is the
		// failure mode the old text scanner had for every file it mis-read.
		if (!quiet) {
			for (const Qd::ErrorInfo& err : mod.parseErrors) {
				std::cerr << mod.path << ":" << err.line << ":" << err.column << ": warning: " << err.message
						  << " (documentation for this file may be incomplete)\n";
			}
		}

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
dl { margin: 0.5rem 0 1rem 1rem; }
dt { font-weight: 500; margin-top: 0.4rem; }
dd { margin-left: 1rem; color: #555; }
hr { border: none; border-top: 1px solid #eee; margin: 1.5rem 0; }
.xref { font-size: 0.9rem; color: #666; }
.xref a { text-decoration: none; }
.xref a:hover { text-decoration: underline; }
)";

static void writeParamList(std::ofstream& f, const char* header, const std::vector<Param>& params) {
	if (params.empty()) {
		return;
	}
	f << "<p><strong>" << header << ":</strong></p>\n<dl>\n";
	for (auto& p : params) {
		f << "<dt><code>" << htmlEscape(p.name) << "</code> <code>" << htmlEscape(p.type) << "</code></dt>\n";
		if (!p.desc.empty()) {
			f << "<dd>" << htmlEscape(p.desc) << "</dd>\n";
		}
	}
	f << "</dl>\n";
}

static void writeErrorList(std::ofstream& f, const std::vector<Param>& errors) {
	if (errors.empty()) {
		return;
	}
	f << "<p><strong>Errors:</strong></p>\n<dl>\n";
	for (auto& e : errors) {
		f << "<dt><code>" << htmlEscape(e.name) << "</code></dt>\n";
		if (!e.desc.empty()) {
			f << "<dd>" << htmlEscape(e.desc) << "</dd>\n";
		}
	}
	f << "</dl>\n";
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
		f << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><title>" << htmlEscape(title)
		  << "</title>\n<link rel=\"stylesheet\" href=\"style.css\"></head>\n<body>\n"
		  << "<nav><h1>" << htmlEscape(title) << " Documentation</h1></nav>\n"
		  << "<div class=\"container\">\n<h2>Modules</h2>\n<div class=\"module-list\">\n";
		for (auto& mod : modules) {
			f << "<div class=\"module-card\">\n"
			  << "<h3><a href=\"" << mod.name << ".html\">" << htmlEscape(mod.name) << "</a></h3>\n"
			  << "<p>" << htmlEscape(mod.desc) << "</p>\n"
			  << "<span class=\"count\">" << mod.functions.size() << " functions";
			if (!mod.structs.empty()) {
				f << ", " << mod.structs.size() << " structs";
			}
			if (!mod.constants.empty()) {
				f << ", " << mod.constants.size() << " constants";
			}
			f << "</span>\n</div>\n";
		}
		f << "</div>\n</div>\n</body></html>\n";
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
					f << "<p>" << htmlEscape(s.desc) << "</p>\n";
				}
				if (!s.fields.empty()) {
					f << "<dl>\n";
					for (auto& fld : s.fields) {
						f << "<dt><code>" << htmlEscape(fld.name) << "</code> <code>" << htmlEscape(fld.type)
						  << "</code></dt>\n";
						if (!fld.desc.empty()) {
							f << "<dd>" << htmlEscape(fld.desc) << "</dd>\n";
						}
					}
					f << "</dl>\n";
				}
			}
		}

		// Constants
		if (!mod.constants.empty()) {
			f << "<h2>Constants</h2>\n<dl>\n";
			for (auto& c : mod.constants) {
				f << "<dt><code>" << htmlEscape(c.name) << "</code> = <code>" << htmlEscape(c.value)
				  << "</code></dt>\n";
				if (!c.desc.empty()) {
					f << "<dd>" << htmlEscape(c.desc) << "</dd>\n";
				}
			}
			f << "</dl>\n";
		}

		// Functions
		if (!mod.functions.empty()) {
			f << "<h2>Functions</h2>\n";
			bool first = true;
			for (auto& fn : mod.functions) {
				if (!first) {
					f << "<hr>\n";
				}
				first = false;
				f << "<div class=\"fn" << (fn.fallible ? " fallible" : "") << "\" id=\"" << fn.name << "\">\n"
				  << "<h3>" << htmlEscape(fn.name);
				if (fn.fallible) {
					f << " [fallible]";
				}
				f << "</h3>\n<pre><code>fn " << htmlEscape(fn.signature) << "</code></pre>\n";
				if (!fn.desc.empty()) {
					f << "<p>" << htmlEscape(fn.desc) << "</p>\n";
				}
				writeParamList(f, "Parameters", fn.params);
				writeParamList(f, "Returns", fn.returns);
				writeErrorList(f, fn.errors);
				if (!fn.examples.empty()) {
					f << "<p><strong>Examples:</strong></p>\n";
					for (auto& ex : fn.examples) {
						f << "<pre><code>" << htmlEscape(ex) << "</code></pre>\n";
					}
				}
				auto renderRefs = [&f](const char* label, const std::vector<std::string>& refs) {
					if (refs.empty()) {
						return;
					}
					f << "<p class=\"xref\"><strong>" << label << ":</strong> ";
					bool firstRef = true;
					for (const auto& qn : refs) {
						if (!firstRef) {
							f << ", ";
						}
						firstRef = false;
						auto pos = qn.find("::");
						std::string targetMod = pos == std::string::npos ? "" : qn.substr(0, pos);
						std::string targetFn = pos == std::string::npos ? qn : qn.substr(pos + 2);
						f << "<a href=\"" << htmlEscape(targetMod) << ".html#" << htmlEscape(targetFn) << "\"><code>"
						  << htmlEscape(qn) << "</code></a>";
					}
					f << "</p>\n";
				};
				renderRefs("Calls", fn.calls);
				renderRefs("Called by", fn.calledBy);
				f << "</div>\n";
			}
		}

		f << "</div></body></html>\n";
	}

	return 0;
}

// --- Main ---

int main(int argc, char* argv[]) {
	std::string outDir = "docs";
	std::string title = "Quadrate API";
	std::string customCSS;
	bool quiet = false;

	qdcli::BaseOptions base;
	bool parsed =
			qdcli::parseArgs(argc, argv, base, "quaddoc", [&](const char* arg, int& i, int ac, char* av[]) -> bool {
				std::string a(arg);
				// Options taking a value are matched before the value is fetched, so a
				// missing one reports "requires an argument" rather than falling through
				// to the unknown-option path and blaming the flag itself.
				if (a == "-o" || a == "--output" || a == "--title" || a == "--css") {
					if (i + 1 >= ac) {
						base.optionError = "option '" + a + "' requires an argument";
						return false;
					}
					const char* value = av[++i];
					if (a == "-o" || a == "--output") {
						outDir = value;
					} else if (a == "--title") {
						title = value;
					} else {
						customCSS = value;
					}
					return true;
				}
				if (a == "-q" || a == "--quiet") {
					quiet = true;
					return true;
				}
				return false;
			});

	if (!parsed) {
		return 1;
	}
	if (base.help) {
		qdcli::Help help("quaddoc", "Quadrate documentation generator");
		help.description("Generates an HTML API reference from /// doc comments, reading the\n"
						 "@param, @return, @error, @example and @field tags.")
				.usage("[options] [directory]", "default: the current directory")
				.section("Options")
				.standardOptions()
				.option('o', "--output", "<dir>", "Output directory (default: docs)")
				.option('q', "--quiet", "Print nothing but errors")
				.option("--title", "<str>", "Project title (default: \"Quadrate API\")")
				.option("--css", "<file>", "Append a custom CSS file after the default styles")
				.section("Examples")
				.item("quaddoc", "Scan the current directory, write to docs/")
				.item("quaddoc -o api lib/", "Scan lib/, write to api/")
				.item("quaddoc --title \"My Project\" .", "Set the project title")
				.item("quaddoc --css custom.css lib/", "Append custom styling");
		help.print();
		return 0;
	}
	if (base.version) {
		qdcli::printVersion("quaddoc");
		return 0;
	}

	std::string dir = base.paths.empty() ? "." : base.paths[0];

	// scanDirectory builds a recursive_directory_iterator, which throws
	// filesystem_error on anything that is not an openable directory. Left
	// unchecked that escapes main and aborts with a core dump rather than a
	// diagnostic, so validate the path here.
	std::error_code dirEc;
	if (!fs::exists(dir, dirEc)) {
		std::cerr << "quaddoc: " << dir << ": No such file or directory\n";
		return 1;
	}
	if (!fs::is_directory(dir, dirEc)) {
		std::cerr << "quaddoc: " << dir << ": Not a directory\n";
		return 1;
	}

	if (!quiet) {
		std::cout << "Scanning " << dir << "...\n";
	}

	auto modules = scanDirectory(dir, quiet);
	if (modules.empty()) {
		std::cout << "No documented modules found.\n";
		return 0;
	}

	buildCallGraph(modules);

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
