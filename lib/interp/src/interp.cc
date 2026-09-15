// interp - Interpreted execution of Quadrate source.
//
// Dispatch table from instruction name to runtime function, plus an AST walk.
// Interpreted and compiled code share the same runtime.

#include <quadrate/interp/interp.h>

#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_if.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/ast_node_loop.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/rt/array.h>
#include <quadrate/rt/qd_string.h>
#include <quadrate/rt/stack.h>

#include <cctype>
#include <cinttypes>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

	using OpFn = int (*)(qd_context*);

	// Runtime function plus the depth it needs: the runtime exits on underflow.
	struct BuiltinOp {
		OpFn fn;
		size_t arity;
	};

	const std::unordered_map<std::string, BuiltinOp>& opTable() {
		static const std::unordered_map<std::string, BuiltinOp> table = {
				// Arithmetic — symbol and word spellings reach the same op
				{"+", {qd_add, 2}},
				{"add", {qd_add, 2}},
				{"-", {qd_sub, 2}},
				{"sub", {qd_sub, 2}},
				{"*", {qd_mul, 2}},
				{"mul", {qd_mul, 2}},
				{"/", {qd_div, 2}},
				{"div", {qd_div, 2}},
				{"%", {qd_mod, 2}},
				{"mod", {qd_mod, 2}},
				{"neg", {qd_neg, 1}},
				{"++", {qd_inc, 1}},
				{"inc", {qd_inc, 1}},
				{"--", {qd_dec, 1}},
				{"dec", {qd_dec, 1}},

				// Comparison
				{"==", {qd_eq, 2}},
				{"eq", {qd_eq, 2}},
				{"!=", {qd_neq, 2}},
				{"neq", {qd_neq, 2}},
				{"<", {qd_lt, 2}},
				{"lt", {qd_lt, 2}},
				{">", {qd_gt, 2}},
				{"gt", {qd_gt, 2}},
				{"<=", {qd_lte, 2}},
				{"lte", {qd_lte, 2}},
				{">=", {qd_gte, 2}},
				{"gte", {qd_gte, 2}},
				{"within", {qd_within, 3}},

				// Bitwise and logical
				{"and", {qd_and, 2}},
				{"or", {qd_or, 2}},
				{"xor", {qd_xor, 2}},
				{"not", {qd_not, 1}},
				{"lnot", {qd_lnot, 1}},
				{"shl", {qd_shl, 2}},
				{"shr", {qd_shr, 2}},

				// Stack
				{"dup", {qd_dup, 1}},
				{"dup2", {qd_dup2, 2}},
				{"drop", {qd_drop, 1}},
				{"swap", {qd_swap, 2}},
				{"over", {qd_over, 2}},
				{"nip", {qd_nip, 2}},
				{"rot", {qd_rot, 3}},
				{"pick", {qd_pick, 1}},
				{"roll", {qd_roll, 1}},
				{"depth", {qd_depth, 0}},
				{"clear", {qd_clear, 0}},
				{"free", {qd_free, 1}},

				// Arrays
				{"len", {qd_len, 1}},
				{"nth", {qd_nth, 2}},
				{"append", {qd_append, 2}},
				{"set", {qd_set, 3}},
				{"makei", {qd_makei, 1}},
				{"makef", {qd_makef, 1}},
				{"makes", {qd_makes, 1}},
				{"makep", {qd_makep, 1}},

				// I/O
				{"print", {qd_print, 1}},
				{"printv", {qd_printv, 1}},
				{"prints", {qd_prints, 1}},
				{"printsv", {qd_printsv, 1}},
				{"nl", {qd_nl, 0}},

				// Errors
				{"err", {qd_err, 1}},
				{"panic", {qd_panic, 1}},
		};
		return table;
	}

} // namespace

enum class Flow {
	Normal,
	Break,
	Continue
};

// Nodes one eval may walk. Nothing can interrupt a running evaluation, so an
// unbounded loop would otherwise need a power cycle. About a second on an A53.
#define QD_INTERP_DEFAULT_STEP_LIMIT 2000000u

// Each level costs a C stack frame in the walk.
#define QD_INTERP_MAX_CALL_DEPTH 256

// Holding the Ast here releases it when the declaration is replaced or removed.
struct Declared {
	const Qd::IAstNode* node;
	std::shared_ptr<Qd::Ast> ast;
};

struct qd_interp {
	qd_context* ctx;   ///< Execution context holding the stack
	bool owns_ctx;	   ///< Whether destroy() should free the context
	std::string error; ///< Message from the most recent failure
	Flow flow;		   ///< Set by break/continue, cleared by the enclosing loop
	uint64_t steps;	   ///< Nodes walked in this call
	uint64_t step_limit;

	std::unordered_map<std::string, Declared> functions; ///< `fn` declarations
	size_t call_depth;
	std::string last_declared; ///< Name the most recent eval declared, if any
};

namespace {

	bool evalNode(qd_interp* interp, const Qd::IAstNode* node);
	bool evalNative(qd_interp* interp, const std::string& name);

	bool evalChildren(qd_interp* interp, const Qd::IAstNode* node) {
		const size_t count = node->childCount();
		for (size_t i = 0; i < count; i++) {
			if (!evalNode(interp, node->child(i))) {
				return false;
			}
			// A break or continue abandons the rest of the block
			if (interp->flow != Flow::Normal) {
				break;
			}
		}
		return true;
	}

	// `1 if { ... }` takes its condition from the stack. Zero is false.
	bool evalIf(qd_interp* interp, const Qd::IAstNode* node) {
		int64_t condition = 0;
		if (qd_stack_size(interp->ctx->st) == 0) {
			interp->error = "'if' needs a condition on the stack";
			return false;
		}
		qd_clear_error(interp->ctx);
		if (qd_pop_i(interp->ctx, &condition) != 0) {
			const char* message = qd_error_message(interp->ctx);
			interp->error = (message != nullptr) ? message : "'if' condition must be an integer";
			return false;
		}

		if (condition != 0) {
			return evalNode(interp, node->child(0));
		}
		if (node->childCount() > 1) {
			return evalNode(interp, node->child(1));
		}
		return true;
	}

	bool evalLoop(qd_interp* interp, const Qd::IAstNode* node) {
		const Qd::IAstNode* body = node->child(0);

		for (;;) {
			if (!evalNode(interp, body)) {
				return false;
			}

			if (interp->flow == Flow::Break) {
				interp->flow = Flow::Normal;
				break;
			}
			if (interp->flow == Flow::Continue) {
				interp->flow = Flow::Normal;
			}

			// An empty body walks no node, so the step budget alone would never stop it.
			if (++interp->steps > interp->step_limit) {
				interp->error = "execution limit reached; is this loop missing a break?";
				return false;
			}
		}
		return true;
	}

	// Strip quotes and decode escapes as generator_nodes.cc does, so both tiers
	// agree what a literal means.
	std::string decodeString(const std::string& literal) {
		std::string body = literal;
		if (body.size() >= 2 && body.front() == '"' && body.back() == '"') {
			body = body.substr(1, body.size() - 2);
		}

		std::string out;
		out.reserve(body.size());

		for (size_t i = 0; i < body.size(); i++) {
			if (body[i] != '\\' || i + 1 >= body.size()) {
				out += body[i];
				continue;
			}

			switch (body[i + 1]) {
			case 'n':
				out += '\n';
				i++;
				break;
			case 't':
				out += '\t';
				i++;
				break;
			case 'r':
				out += '\r';
				i++;
				break;
			case '\\':
				out += '\\';
				i++;
				break;
			case '"':
				out += '"';
				i++;
				break;
			case '0':
				out += '\0';
				i++;
				break;
			case 'e':
				out += '\x1b';
				i++;
				break;
			case 'x':
				if (i + 3 < body.size() && isxdigit(static_cast<unsigned char>(body[i + 2])) != 0 &&
						isxdigit(static_cast<unsigned char>(body[i + 3])) != 0) {
					out += static_cast<char>(std::strtol(body.substr(i + 2, 2).c_str(), nullptr, 16));
					i += 3;
				} else {
					out += body[i];
				}
				break;
			default:
				out += body[i];
				break;
			}
		}
		return out;
	}

	bool pushLiteral(qd_interp* interp, const Qd::AstNodeLiteral* literal) {
		const std::string& text = literal->value();

		switch (literal->literalType()) {
		case Qd::AstNodeLiteral::LiteralType::INTEGER: {
			// Base 0 so hex, octal and binary prefixes parse as written
			const long long value = std::strtoll(text.c_str(), nullptr, 0);
			return qd_push_i(interp->ctx, static_cast<int64_t>(value)) == 0;
		}
		case Qd::AstNodeLiteral::LiteralType::FLOAT:
			return qd_push_f(interp->ctx, std::strtod(text.c_str(), nullptr)) == 0;
		case Qd::AstNodeLiteral::LiteralType::STRING:
			return qd_push_s(interp->ctx, decodeString(text).c_str()) == 0;
		case Qd::AstNodeLiteral::LiteralType::BOOL:
			// 'true' and 'Ok' are 1, 'false' and 'Err' are 0
			return qd_push_i(interp->ctx, (text == "true" || text == "Ok") ? 1 : 0) == 0;
		case Qd::AstNodeLiteral::LiteralType::NULL_PTR:
			return qd_push_p(interp->ctx, nullptr) == 0;
		}

		interp->error = "unknown literal kind";
		return false;
	}

	bool evalInstruction(qd_interp* interp, const Qd::AstNodeInstruction* instruction) {
		const std::unordered_map<std::string, BuiltinOp>& table = opTable();
		const auto found = table.find(instruction->name());
		if (found == table.end()) {
			interp->error = "'" + instruction->name() + "' cannot be interpreted";
			return false;
		}

		const BuiltinOp& op = found->second;

		// Refuse rather than let the runtime end the process
		const size_t depth = static_cast<size_t>(qd_stack_size(interp->ctx->st));
		if (depth < op.arity) {
			interp->error = "'" + instruction->name() + "' needs " + std::to_string(op.arity) +
							(op.arity == 1 ? " value, " : " values, ") + std::to_string(depth) + " on the stack";
			return false;
		}

		qd_clear_error(interp->ctx);
		if (op.fn(interp->ctx) != 0) {
			const char* message = qd_error_message(interp->ctx);
			interp->error = (message != nullptr) ? message : ("'" + instruction->name() + "' failed");
			return false;
		}
		return true;
	}

	// Whether the source declares rather than evaluates; a leading keyword decides.
	bool startsWithDeclaration(const char* source) {
		const char* p = source;
		// Skip comments too: a stored program usually opens with one
		for (;;) {
			while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
				p++;
			}
			if (p[0] == '/' && p[1] == '/') {
				while (*p != '\0' && *p != '\n') {
					p++;
				}
				continue;
			}
			if (p[0] == '/' && p[1] == '*') {
				p += 2;
				while (*p != '\0' && !(p[0] == '*' && p[1] == '/')) {
					p++;
				}
				if (*p != '\0') {
					p += 2;
				}
				continue;
			}
			break;
		}

		static const char* const KEYWORDS[] = {"fn ", "fn(", "const ", "struct ", "enum ", "use ", "type ", "test "};
		for (const char* keyword : KEYWORDS) {
			if (std::strncmp(p, keyword, std::strlen(keyword)) == 0) {
				return true;
			}
		}
		return false;
	}

	// Parameters arrive on the stack, so the body just runs.
	bool callFunction(qd_interp* interp, const std::string& name, const Qd::IAstNode* decl) {
		if (interp->call_depth >= QD_INTERP_MAX_CALL_DEPTH) {
			interp->error = "'" + name + "' recursed too deeply";
			return false;
		}

		// The body is the function's Block; the other children are parameters
		const Qd::IAstNode* body = nullptr;
		for (size_t i = 0; i < decl->childCount(); i++) {
			if (decl->child(i)->type() == Qd::IAstNode::Type::BLOCK) {
				body = decl->child(i);
			}
		}
		if (body == nullptr) {
			interp->error = "'" + name + "' has no body";
			return false;
		}

		interp->call_depth++;
		const bool ok = evalNode(interp, body);
		interp->call_depth--;

		// A break inside a function does not escape it
		interp->flow = Flow::Normal;
		return ok;
	}

	// Call a registered native function by the name written in source.
	bool evalName(qd_interp* interp, const std::string& name) {
		// A declared word shadows a registered native.
		const auto declared = interp->functions.find(name);
		if (declared != interp->functions.end()) {
			return callFunction(interp, name, declared->second.node);
		}
		return evalNative(interp, name);
	}

	bool evalNative(qd_interp* interp, const std::string& name) {
		qd_native_callback fn = nullptr;
		void* userdata = nullptr;
		size_t arity = 0;
		if (!qd_native_lookup(interp->ctx, name.c_str(), &fn, &userdata, &arity)) {
			interp->error = "'" + name + "' is not defined";
			return false;
		}

		// Same guard the builtins get
		const size_t depth = static_cast<size_t>(qd_stack_size(interp->ctx->st));
		if (depth < arity) {
			interp->error = "'" + name + "' needs " + std::to_string(arity) + (arity == 1 ? " value, " : " values, ") +
							std::to_string(depth) + " on the stack";
			return false;
		}

		qd_clear_error(interp->ctx);
		if (fn(interp->ctx, userdata) != 0) {
			const char* message = qd_error_message(interp->ctx);
			interp->error = (message != nullptr) ? message : ("'" + name + "' failed");
			return false;
		}
		return true;
	}

	bool evalNode(qd_interp* interp, const Qd::IAstNode* node) {
		using Type = Qd::IAstNode::Type;

		if (++interp->steps > interp->step_limit) {
			interp->error = "execution limit reached; is this loop missing a break?";
			return false;
		}

		switch (node->type()) {
		// Wrappers the parser needs; transparent to execution.
		case Type::PROGRAM:
		case Type::BLOCK:
		case Type::FUNCTION_DECLARATION:
			return evalChildren(interp, node);

		case Type::COMMENT:
		case Type::VARIABLE_DECLARATION: // a parameter, recorded by the declaration
			return true;

		case Type::LITERAL:
			if (!pushLiteral(interp, static_cast<const Qd::AstNodeLiteral*>(node))) {
				if (interp->error.empty()) {
					interp->error = "stack overflow";
				}
				return false;
			}
			return true;

		case Type::INSTRUCTION:
			return evalInstruction(interp, static_cast<const Qd::AstNodeInstruction*>(node));

		case Type::IF_STATEMENT:
			return evalIf(interp, node);

		case Type::LOOP_STATEMENT:
			return evalLoop(interp, node);

		case Type::BREAK_STATEMENT:
			interp->flow = Flow::Break;
			return true;

		case Type::CONTINUE_STATEMENT:
			interp->flow = Flow::Continue;
			return true;

		case Type::IDENTIFIER:
			return evalName(interp, static_cast<const Qd::AstNodeIdentifier*>(node)->name());

		case Type::SCOPED_IDENTIFIER: {
			const auto* scoped = static_cast<const Qd::AstNodeScopedIdentifier*>(node);
			return evalName(interp, scoped->scope() + "::" + scoped->name());
		}

		default:
			interp->error = "this construct cannot be interpreted yet";
			return false;
		}
	}

	// Walk with recovery armed: type mismatch, division by zero and out-of-range
	// pick unwind here.
	//
	// longjmp skips destructors, so nothing between this frame and a runtime call
	// may hold an object needing one. Keep it that way.
	bool runGuarded(qd_interp* interp, const Qd::IAstNode* root) {
		if (setjmp(*qd_recovery_buf(interp->ctx)) == 0) {
			qd_recovery_arm(interp->ctx);
			const bool ok = evalNode(interp, root);
			qd_recovery_disarm(interp->ctx);
			return ok;
		}

		// Unwound from a fatal runtime error; recovery disarmed itself
		const char* message = qd_error_message(interp->ctx);
		interp->error = (message != nullptr) ? message : "runtime error";
		return false;
	}

	qd_interp* makeInterp(qd_context* ctx, bool owns) {
		qd_interp* interp = new (std::nothrow) qd_interp{
				ctx, owns, std::string(), Flow::Normal, 0, QD_INTERP_DEFAULT_STEP_LIMIT, {}, 0, std::string()};
		if (interp == nullptr && owns) {
			qd_free_context(ctx);
		}
		return interp;
	}

} // namespace

extern "C" {

qd_interp* qd_interp_create(size_t stack_size) {
	qd_context* ctx = qd_create_context(stack_size);
	if (ctx == nullptr) {
		return nullptr;
	}
	return makeInterp(ctx, true);
}

qd_interp* qd_interp_attach(qd_context* ctx) {
	if (ctx == nullptr) {
		return nullptr;
	}
	return makeInterp(ctx, false);
}

void qd_interp_destroy(qd_interp* interp) {
	if (interp == nullptr) {
		return;
	}
	if (interp->owns_ctx) {
		qd_free_context(interp->ctx);
	}
	delete interp;
}

bool qd_interp_register(
		qd_interp* interp, const char* name, const char* signature, qd_interp_native_fn fn, void* userdata) {
	if (interp == nullptr) {
		return false;
	}
	return qd_native_register(interp->ctx, name, signature, fn, userdata);
}

size_t qd_interp_registered_count(const qd_interp* interp) {
	return (interp != nullptr) ? qd_native_count(interp->ctx) : 0;
}

void qd_interp_set_step_limit(qd_interp* interp, uint64_t limit) {
	if (interp != nullptr) {
		interp->step_limit = (limit == 0) ? UINT64_MAX : limit;
	}
}

uint64_t qd_interp_step_limit(const qd_interp* interp) {
	return (interp != nullptr) ? interp->step_limit : 0;
}

const char* qd_interp_last_declared(const qd_interp* interp) {
	if (interp == nullptr || interp->last_declared.empty()) {
		return nullptr;
	}
	return interp->last_declared.c_str();
}

bool qd_interp_undeclare(qd_interp* interp, const char* name) {
	if (interp == nullptr || name == nullptr) {
		return false;
	}
	return interp->functions.erase(name) > 0;
}

void qd_interp_visit_words(const qd_interp* interp, qd_interp_word_visitor visit, void* userdata) {
	if (interp == nullptr || visit == nullptr) {
		return;
	}

	for (const auto& entry : opTable()) {
		if (!visit(entry.first.c_str(), userdata)) {
			return;
		}
	}

	if (!qd_native_visit(interp->ctx, visit, userdata)) {
		return;
	}

	for (const auto& entry : interp->functions) {
		if (!visit(entry.first.c_str(), userdata)) {
			return;
		}
	}
}

size_t qd_interp_declared_count(const qd_interp* interp) {
	return (interp != nullptr) ? interp->functions.size() : 0;
}

bool qd_interp_eval(qd_interp* interp, const char* source) {
	if (interp == nullptr || source == nullptr) {
		return false;
	}

	interp->error.clear();
	interp->last_declared.clear();
	interp->flow = Flow::Normal;
	interp->steps = 0;
	interp->call_depth = 0;

	// A declaration parses as a program; anything else as a function body.
	const bool declares = startsWithDeclaration(source);
	const std::string text = declares ? std::string(source) : ("fn main() {\n" + std::string(source) + "\n}");

	auto ast = std::make_shared<Qd::Ast>();
	Qd::IAstNode* root = ast->generate(text.c_str(), false, "<interp>");
	if (root == nullptr || ast->hasErrors()) {
		const std::vector<Qd::ErrorInfo>& errors = ast->getErrors();
		interp->error = errors.empty() ? "syntax error" : errors.front().message;
		return false;
	}

	if (declares) {
		// The nodes belong to this Ast, so the declaration keeps it alive.
		bool recorded = false;
		for (size_t i = 0; i < root->childCount(); i++) {
			const Qd::IAstNode* child = root->child(i);
			if (child->type() != Qd::IAstNode::Type::FUNCTION_DECLARATION) {
				continue;
			}
			const auto* fn = static_cast<const Qd::AstNodeFunctionDeclaration*>(child);
			interp->functions[fn->name()] = Declared{child, ast};
			if (interp->last_declared.empty()) {
				interp->last_declared = fn->name();
			}
			recorded = true;
		}
		if (!recorded) {
			interp->error = "nothing declared here can be interpreted yet";
			return false;
		}
		return true;
	}

	return runGuarded(interp, root);
}

const char* qd_interp_error(const qd_interp* interp) {
	if (interp == nullptr) {
		return "no interpreter";
	}
	return interp->error.c_str();
}

size_t qd_interp_depth(const qd_interp* interp) {
	if (interp == nullptr || interp->ctx == nullptr || interp->ctx->st == nullptr) {
		return 0;
	}
	return static_cast<size_t>(qd_stack_size(interp->ctx->st));
}

bool qd_interp_peek(const qd_interp* interp, size_t index, qd_interp_value* out) {
	if (interp == nullptr || out == nullptr || interp->ctx == nullptr || interp->ctx->st == nullptr) {
		return false;
	}

	const qd_stack* stack = interp->ctx->st;
	const size_t depth = static_cast<size_t>(qd_stack_size(stack));
	if (index >= depth) {
		return false;
	}

	const qd_stack_element_t& element = stack->data[depth - 1 - index];
	std::memset(out, 0, sizeof(*out));

	switch (element.type) {
	case QD_STACK_TYPE_INT:
		out->type = QD_INTERP_VALUE_INT;
		out->i = element.value.i;
		std::snprintf(out->text, sizeof(out->text), "%" PRId64, element.value.i);
		break;

	case QD_STACK_TYPE_FLOAT:
		out->type = QD_INTERP_VALUE_FLOAT;
		out->f = element.value.f;
		// %.15g keeps an integral result integral without losing precision.
		std::snprintf(out->text, sizeof(out->text), "%.15g", element.value.f);
		break;

	case QD_STACK_TYPE_STR: {
		out->type = QD_INTERP_VALUE_STR;
		const char* data = (element.value.s != nullptr) ? qd_string_data(element.value.s) : nullptr;
		std::snprintf(out->text, sizeof(out->text), "\"%s\"", (data != nullptr) ? data : "");
		break;
	}

	case QD_STACK_TYPE_PTR:
		out->type = QD_INTERP_VALUE_PTR;
		std::snprintf(out->text, sizeof(out->text), "<ptr %p>", element.value.p);
		break;
	}
	return true;
}

qd_context* qd_interp_context(const qd_interp* interp) {
	return (interp != nullptr) ? interp->ctx : nullptr;
}

} // extern "C"
