// interp - Interpreted execution of Quadrate source
//
// Every builtin instruction already exists as a C function in the runtime
// (qd_add, qd_dup, qd_print), because that is what generated code calls into.
// So interpreting is a dispatch table from instruction name to runtime function
// plus a walk over the AST. There is no bytecode and no second implementation
// of the language's semantics: interpreted and compiled code execute through
// the same runtime.

#include <quadrate/interp/interp.h>

#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/rt/array.h>
#include <quadrate/rt/qd_string.h>
#include <quadrate/rt/stack.h>

#include <cinttypes>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <unordered_map>

namespace {

	using OpFn = int (*)(qd_context*);

	// A builtin instruction: the runtime function that performs it, and the
	// stack depth required before it may be called. The runtime treats
	// underflow as a compiler bug and ends the process, which is right for
	// generated code and wrong for a typed line, so the arity is checked here
	// rather than trusted.
	//
	// The names duplicate BUILTIN_INSTRUCTIONS in qc/instructions.h. Carrying
	// the arity there would give one source of truth, at the cost of touching
	// the compiler's instruction table.
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

// Interpreter state
struct qd_interp {
	qd_context* ctx;   ///< Execution context holding the stack
	bool owns_ctx;	   ///< Whether destroy() should free the context
	std::string error; ///< Message from the most recent failure
};

namespace {

	bool evalNode(qd_interp* interp, const Qd::IAstNode* node);

	bool evalChildren(qd_interp* interp, const Qd::IAstNode* node) {
		const size_t count = node->childCount();
		for (size_t i = 0; i < count; i++) {
			if (!evalNode(interp, node->child(i))) {
				return false;
			}
		}
		return true;
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
			return qd_push_s(interp->ctx, text.c_str()) == 0;
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

	bool evalNode(qd_interp* interp, const Qd::IAstNode* node) {
		using Type = Qd::IAstNode::Type;

		switch (node->type()) {
		// Containers: source is wrapped in a function to satisfy the
		// parser, and both wrappers are transparent to execution
		case Type::PROGRAM:
		case Type::BLOCK:
		case Type::FUNCTION_DECLARATION:
			return evalChildren(interp, node);

		case Type::COMMENT:
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

		default:
			interp->error = "this construct cannot be interpreted yet";
			return false;
		}
	}

	// Walk the tree with runtime recovery armed. The arity check stops the
	// common underflow, but a type mismatch, a division by zero and an
	// out-of-range pick or roll depend on values only visible at run time;
	// those unwind to here and become messages.
	//
	// longjmp does not run destructors, so nothing between this frame and a
	// runtime call may hold an object that needs one. The walk above is written
	// to that constraint (references and trivially destructible locals only)
	// and changes to it must keep to it.
	bool runGuarded(qd_interp* interp, const Qd::IAstNode* root) {
		if (setjmp(*qd_recovery_buf()) == 0) {
			qd_recovery_arm();
			const bool ok = evalNode(interp, root);
			qd_recovery_disarm();
			return ok;
		}

		// Unwound from a fatal runtime error; recovery disarmed itself
		const char* message = qd_error_message(interp->ctx);
		interp->error = (message != nullptr) ? message : "runtime error";
		return false;
	}

	qd_interp* makeInterp(qd_context* ctx, bool owns) {
		qd_interp* interp = new (std::nothrow) qd_interp{ctx, owns, std::string()};
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

bool qd_interp_eval(qd_interp* interp, const char* source) {
	if (interp == nullptr || source == nullptr) {
		return false;
	}

	interp->error.clear();

	// The parser accepts only declarations at top level, so the source is given
	// a function to live in. The walk treats that function as transparent,
	// which leaves the code executing against the persistent stack rather than
	// a fresh one.
	const std::string wrapped = "fn main() {\n" + std::string(source) + "\n}";

	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(wrapped.c_str(), false, "<interp>");
	if (root == nullptr || ast.hasErrors()) {
		const std::vector<Qd::ErrorInfo>& errors = ast.getErrors();
		interp->error = errors.empty() ? "syntax error" : errors.front().message;
		return false;
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
		// %.15g keeps an integral result looking integral while still
		// showing enough digits to be trusted
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
