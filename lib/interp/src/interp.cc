// interp - Interpreted execution of Quadrate source.
//
// Dispatch table from instruction name to runtime function, plus an AST walk.
// Interpreted and compiled code share the same runtime.

#include <quadrate/interp/interp.h>

#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node_array_literal.h>
#include <quadrate/qc/ast_node_case.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_enum.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_if.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_loop.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_switch.h>
#include <quadrate/qc/numeric_literal.h>
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
#include <utility>
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

				// Logical
				{"and", {qd_land, 2}},
				{"or", {qd_lor, 2}},
				{"not", {qd_lnot, 1}},

				// Bitwise. This tier has no module system, so the qualified spellings are
				// registered as plain names: bits::and is how the rest of the language
				// reaches these, and a device with no package resolution still needs them.
				{"bits::and", {qd_and, 2}},
				{"bits::or", {qd_or, 2}},
				{"bits::xor", {qd_xor, 2}},
				{"bits::not", {qd_not, 1}},
				{"__and", {qd_and, 2}},
				{"__or", {qd_or, 2}},
				{"__xor", {qd_xor, 2}},
				{"__not", {qd_not, 1}},
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
				{"pick", {qd_pick, 3}},
				{"depth", {qd_depth, 0}},
				{"clear", {qd_clear, 0}},
				{"free", {qd_free, 1}},

				// Arrays
				{"len", {qd_len, 1}},
				{"nth", {qd_nth, 2}},
				{"append", {qd_append, 2}},
				{"set", {qd_set, 3}},

				// I/O
				{"print", {qd_print, 1}},
				{"printv", {qd_printv, 1}},
				{"prints", {qd_prints, 1}},
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
	Continue,
	Return
};

// A value bound to a name by '-> x', by a named parameter, or by a 'for'
// iterator.
//
// Strings are copied rather than reference-counted. A frame has to survive the
// longjmp a fatal runtime error performs, and a copy needs nothing released on
// that path; the stack's own reference is dropped when the value is popped.
struct Local {
	qd_stack_type type = QD_STACK_TYPE_INT;
	int64_t i = 0;
	double f = 0.0;
	void* p = nullptr;
	std::string s;
};

// Names visible to one function body. Locals are function-scoped, as they are
// in the compiled tier -- a block does not open a scope of its own.
using Frame = std::unordered_map<std::string, Local>;

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
	qd_context* ctx = nullptr; ///< Execution context holding the stack
	bool owns_ctx = false;	   ///< Whether destroy() should free the context
	std::string error;		   ///< Message from the most recent failure
	Flow flow = Flow::Normal;  ///< Set by break/continue/return, cleared by what encloses it
	uint64_t steps = 0;		   ///< Nodes walked in this call
	uint64_t step_limit = QD_INTERP_DEFAULT_STEP_LIMIT;

	std::unordered_map<std::string, Declared> functions;			 ///< `fn` declarations
	std::unordered_map<std::string, std::string> constants;			 ///< `const` values and enum variants, as written
	std::unordered_map<std::string, std::vector<std::string>> enums; ///< enum name -> its keys in `constants`

	/// One frame per active call. frames.front() belongs to the top level and
	/// persists across evaluations, as the stack does.
	std::vector<Frame> frames;

	size_t call_depth = 0;
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

	// One iteration of a loop body. Returns false to stop the walk entirely;
	// sets `stop` when the loop itself is over but the walk continues.
	bool runLoopBody(qd_interp* interp, const Qd::IAstNode* body, bool& stop) {
		stop = false;
		if (!evalNode(interp, body)) {
			return false;
		}

		if (interp->flow == Flow::Break) {
			interp->flow = Flow::Normal;
			stop = true;
			return true;
		}
		if (interp->flow == Flow::Continue) {
			interp->flow = Flow::Normal;
			return true;
		}
		// A return leaves the loop and the function with it, so the flow stands
		stop = (interp->flow == Flow::Return);
		return true;
	}

	bool evalLoop(qd_interp* interp, const Qd::IAstNode* node) {
		const Qd::IAstNode* body = node->child(0);

		for (;;) {
			bool stop = false;
			if (!runLoopBody(interp, body, stop)) {
				return false;
			}
			if (stop) {
				break;
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
		case Qd::AstNodeLiteral::LiteralType::INTEGER:
			// Every literal that reaches here has been through the validator, which rejects
			// the ones that will not read, so the fallback stands for a case that cannot
			// arrive rather than for a value the program might see.
			return qd_push_i(interp->ctx, Qd::integerLiteralOr(text, 0)) == 0;
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

	void recordConstant(qd_interp* interp, const Qd::AstNodeConstant* constant) {
		interp->constants[constant->name()] = constant->value();
		if (interp->last_declared.empty()) {
			interp->last_declared = constant->name();
		}
	}

	// Variants are reachable as 'Enum::Variant', the spelling the compiled tier
	// resolves through the same constant table.
	void recordEnum(qd_interp* interp, const Qd::AstNodeEnumDeclaration* declaration) {
		std::vector<std::string>& keys = interp->enums[declaration->name()];
		for (const std::string& key : keys) {
			interp->constants.erase(key);
		}
		keys.clear();

		for (const auto& variant : declaration->variants()) {
			const std::string key = declaration->name() + "::" + variant.name;
			interp->constants[key] = std::to_string(variant.value);
			keys.push_back(key);
		}
		if (interp->last_declared.empty()) {
			interp->last_declared = declaration->name();
		}
	}

	// A const is stored as written, so its spelling decides its type -- the same
	// rule the compiled tier applies in generateIdentifier.
	bool pushConstantValue(qd_interp* interp, const std::string& text) {
		if (text.size() >= 2 && text.front() == '"') {
			return qd_push_s(interp->ctx, decodeString(text).c_str()) == 0;
		}
		if (text.find('.') != std::string::npos) {
			return qd_push_f(interp->ctx, std::strtod(text.c_str(), nullptr)) == 0;
		}
		return qd_push_i(interp->ctx, Qd::integerLiteralOr(text, 0)) == 0;
	}

	// The stack owns a reference to every string on it; a popped element that
	// nothing takes over has to drop it.
	void releaseElement(qd_stack_element_t& element) {
		if (element.type == QD_STACK_TYPE_STR && element.value.s != nullptr) {
			qd_string_release(element.value.s);
			element.value.s = nullptr;
		}
	}

	// Every binding a frame holds owns a reference, so dropping one gives it up.
	// The runtime's generic release knows an array from a struct from a raw
	// pointer, which is why a frame need not track which it has.
	void releaseLocal(Local& value) {
		if (value.type == QD_STACK_TYPE_PTR && value.p != nullptr) {
			qd_ptr_release(value.p);
			value.p = nullptr;
		}
	}

	void dropFrame(qd_interp* interp) {
		for (auto& entry : interp->frames.back()) {
			releaseLocal(entry.second);
		}
		interp->frames.pop_back();
	}

	// Bind a popped value to a name, consuming the element. Storing does not
	// retain -- the reference the stack held moves into the binding, as it does
	// in generateLocalOne.
	void bindLocal(qd_interp* interp, const std::string& name, qd_stack_element_t& element) {
		Local& slot = interp->frames.back()[name];
		releaseLocal(slot); // whatever the name held before
		slot.type = element.type;
		slot.i = 0;
		slot.f = 0.0;
		slot.p = nullptr;
		slot.s.clear();

		switch (element.type) {
		case QD_STACK_TYPE_INT:
			slot.i = element.value.i;
			break;
		case QD_STACK_TYPE_FLOAT:
			slot.f = element.value.f;
			break;
		case QD_STACK_TYPE_PTR:
			slot.p = element.value.p;
			break;
		case QD_STACK_TYPE_STR: {
			const char* data = (element.value.s != nullptr) ? qd_string_data(element.value.s) : nullptr;
			if (data != nullptr) {
				slot.s = data;
			}
			break;
		}
		}
		releaseElement(element);
	}

	// Reading a name pushes a copy, leaving the binding in place. A pointer is
	// retained first: 'len' and 'nth' consume the reference the stack carries,
	// so without this an array read twice would be freed under the second read.
	bool pushLocal(qd_interp* interp, const Local& value) {
		switch (value.type) {
		case QD_STACK_TYPE_INT:
			return qd_push_i(interp->ctx, value.i) == 0;
		case QD_STACK_TYPE_FLOAT:
			return qd_push_f(interp->ctx, value.f) == 0;
		case QD_STACK_TYPE_PTR:
			if (value.p != nullptr) {
				qd_ptr_retain(value.p);
			}
			return qd_push_p(interp->ctx, value.p) == 0;
		case QD_STACK_TYPE_STR:
			return qd_push_s(interp->ctx, value.s.c_str()) == 0;
		}
		return false;
	}

	// '-> a b c' pops three values, the top into a. '_' names nothing and drops.
	bool evalLocalBinding(qd_interp* interp, const Qd::AstNodeLocal* local) {
		const std::vector<std::string>& names = local->names();
		const size_t depth = static_cast<size_t>(qd_stack_size(interp->ctx->st));
		if (depth < names.size()) {
			interp->error = "'->' needs " + std::to_string(names.size()) +
							(names.size() == 1 ? " value, " : " values, ") + std::to_string(depth) + " on the stack";
			return false;
		}

		for (const std::string& name : names) {
			qd_stack_element_t element;
			if (qd_stack_pop(interp->ctx->st, &element) != QD_STACK_OK) {
				interp->error = "'-> " + name + "' could not take its value";
				return false;
			}
			if (name == "_") {
				releaseElement(element);
				continue;
			}
			bindLocal(interp, name, element);
		}
		return true;
	}

	// 'start end step for i { ... }'. The bounds come off the stack, step last,
	// and the end is exclusive -- the rule generateFor applies.
	bool evalFor(qd_interp* interp, const Qd::AstNodeForStatement* statement) {
		if (static_cast<size_t>(qd_stack_size(interp->ctx->st)) < 3) {
			interp->error = "'for' needs a start, an end and a step on the stack";
			return false;
		}

		qd_stack_element_t bounds[3]; // step, end, start
		for (qd_stack_element_t& element : bounds) {
			if (qd_stack_pop(interp->ctx->st, &element) != QD_STACK_OK) {
				interp->error = "'for' could not take its bounds";
				return false;
			}
		}

		double numbers[3] = {0.0, 0.0, 0.0};
		for (size_t i = 0; i < 3; i++) {
			if (bounds[i].type == QD_STACK_TYPE_INT) {
				numbers[i] = static_cast<double>(bounds[i].value.i);
			} else if (bounds[i].type == QD_STACK_TYPE_FLOAT) {
				numbers[i] = bounds[i].value.f;
			} else {
				releaseElement(bounds[0]);
				releaseElement(bounds[1]);
				releaseElement(bounds[2]);
				interp->error = "'for' bounds must be numbers";
				return false;
			}
		}

		// The start decides the iterator's type, as it does in the compiled tier
		const bool isFloat = bounds[2].type == QD_STACK_TYPE_FLOAT;
		const double step = numbers[0];
		const double end = numbers[1];
		double iterator = numbers[2];

		const std::string& name = statement->iteratorName();
		const Qd::IAstNode* body = statement->body();

		// An outer binding of the same name is set aside and restored afterwards.
		// Moved rather than copied: whatever reference it owns has to be held in
		// one place only, or restoring it would release what it still points at.
		bool hadShadowed = false;
		Local previous;
		{
			Frame& frame = interp->frames.back();
			const auto shadowed = frame.find(name);
			hadShadowed = shadowed != frame.end();
			if (hadShadowed) {
				previous = std::move(shadowed->second);
				shadowed->second = Local();
			}
		}

		bool ok = true;
		for (; (step < 0.0) ? (iterator > end) : (iterator < end); iterator += step) {
			Local& slot = interp->frames.back()[name];
			releaseLocal(slot); // in case the body rebound the name
			slot.type = isFloat ? QD_STACK_TYPE_FLOAT : QD_STACK_TYPE_INT;
			slot.i = static_cast<int64_t>(iterator);
			slot.f = iterator;
			slot.s.clear();

			bool stop = false;
			if (body != nullptr) {
				if (!runLoopBody(interp, body, stop)) {
					ok = false;
					break;
				}
			}
			if (stop) {
				break;
			}

			// A body that walks nothing would otherwise never meet the budget
			if (++interp->steps > interp->step_limit) {
				interp->error = "execution limit reached; is this loop missing a break?";
				ok = false;
				break;
			}
		}

		Local& restored = interp->frames.back()[name];
		releaseLocal(restored);
		if (hadShadowed) {
			restored = std::move(previous);
		} else {
			interp->frames.back().erase(name);
		}
		return ok;
	}

	// '[1, 2, 3]' builds a Quadrate array, whose element type the first element
	// decides -- what generateArrayLiteral does. The caller frees it.
	// `[size]T` -- the nodes are the size expression, and the array comes back zero-filled.
	// This is the form `make<T>` used to cover, which the interpreter never had: its builtin
	// table carried makei/makef/makes/makep and no generic `make` at all.
	bool evalSizedArrayLiteral(qd_interp* interp, const Qd::AstNodeArrayLiteral* literal) {
		const std::string& elemName = literal->elementType();
		qd_array_type elementType = QD_ARRAY_TYPE_INT;
		if (elemName == "f64" || elemName == "f32") {
			elementType = QD_ARRAY_TYPE_FLOAT;
		} else if (elemName == "str" || elemName == "string") {
			elementType = QD_ARRAY_TYPE_STR;
		} else if (elemName == "ptr") {
			elementType = QD_ARRAY_TYPE_PTR;
		} else if (elemName != "i64" && elemName != "i32" && elemName != "i16" && elemName != "i8" &&
				   elemName != "u64" && elemName != "u32" && elemName != "u16" && elemName != "u8") {
			// Structs are not a thing in this tier, so `[n]Point` has nothing to build.
			interp->error = "this tier has no structs, so an array of them cannot be built";
			return false;
		}

		int64_t size = 0;
		if (!literal->elements().empty()) {
			for (const auto& node : literal->elements()) {
				if (!evalNode(interp, node.get())) {
					return false;
				}
			}
			qd_stack_element_t element;
			if (qd_stack_pop(interp->ctx->st, &element) != QD_STACK_OK) {
				interp->error = "the size of an array literal left nothing on the stack";
				return false;
			}
			if (element.type != QD_STACK_TYPE_INT) {
				interp->error = "the size of an array literal has to be an integer";
				return false;
			}
			size = element.value.i;
		}
		if (size < 0) {
			interp->error = "an array literal cannot have a negative size";
			return false;
		}

		qd_array_t* array = qd_array_create(static_cast<size_t>(size), elementType);
		if (array == nullptr) {
			interp->error = "could not allocate the array";
			return false;
		}
		for (int64_t i = 0; i < size; i++) {
			int pushed = 0;
			switch (elementType) {
			case QD_ARRAY_TYPE_FLOAT:
				pushed = qd_array_push_float(array, 0.0);
				break;
			case QD_ARRAY_TYPE_STR:
				pushed = qd_array_push_ptr(array, qd_string_create(""));
				break;
			case QD_ARRAY_TYPE_PTR:
				pushed = qd_array_push_ptr(array, nullptr);
				break;
			default:
				pushed = qd_array_push_int(array, 0);
				break;
			}
			if (pushed != 0) {
				qd_array_release(array);
				interp->error = "could not grow the array";
				return false;
			}
		}

		if (qd_push_p(interp->ctx, array) != 0) {
			qd_array_release(array);
			interp->error = "stack overflow";
			return false;
		}
		return true;
	}

	bool evalArrayLiteral(qd_interp* interp, const Qd::AstNodeArrayLiteral* literal) {
		if (literal->hasElementType()) {
			return evalSizedArrayLiteral(interp, literal);
		}
		const auto& elements = literal->elements();

		qd_array_type elementType = QD_ARRAY_TYPE_INT;
		if (!elements.empty() && elements[0]->type() == Qd::IAstNode::Type::LITERAL) {
			switch (static_cast<const Qd::AstNodeLiteral*>(elements[0].get())->literalType()) {
			case Qd::AstNodeLiteral::LiteralType::FLOAT:
				elementType = QD_ARRAY_TYPE_FLOAT;
				break;
			case Qd::AstNodeLiteral::LiteralType::STRING:
				elementType = QD_ARRAY_TYPE_STR;
				break;
			default:
				break;
			}
		}

		qd_array_t* array = qd_array_create(elements.empty() ? 8 : elements.size(), elementType);
		if (array == nullptr) {
			interp->error = "could not allocate the array";
			return false;
		}

		for (const auto& element : elements) {
			if (element->type() != Qd::IAstNode::Type::LITERAL) {
				qd_array_release(array);
				interp->error = "an array literal takes literal elements only";
				return false;
			}

			const auto* value = static_cast<const Qd::AstNodeLiteral*>(element.get());
			const std::string& text = value->value();

			double number = 0.0;
			switch (value->literalType()) {
			case Qd::AstNodeLiteral::LiteralType::INTEGER:
				number = static_cast<double>(Qd::integerLiteralOr(text, 0));
				break;
			case Qd::AstNodeLiteral::LiteralType::BOOL:
				number = (text == "true" || text == "Ok") ? 1.0 : 0.0;
				break;
			case Qd::AstNodeLiteral::LiteralType::FLOAT:
				number = std::strtod(text.c_str(), nullptr);
				break;
			case Qd::AstNodeLiteral::LiteralType::STRING: {
				if (elementType != QD_ARRAY_TYPE_STR) {
					qd_array_release(array);
					interp->error = "an array literal holds one type; this element is a string";
					return false;
				}
				qd_string_t* held = qd_string_create(decodeString(text).c_str());
				if (held == nullptr) {
					qd_array_release(array);
					interp->error = "could not allocate the array element";
					return false;
				}
				const int pushed = qd_array_push_ptr(array, held);
				qd_string_release(held); // the array retained its own
				if (pushed != 0) {
					qd_array_release(array);
					interp->error = "could not grow the array";
					return false;
				}
				continue;
			}
			default:
				qd_array_release(array);
				interp->error = "this array element cannot be interpreted";
				return false;
			}

			if (elementType == QD_ARRAY_TYPE_STR) {
				qd_array_release(array);
				interp->error = "an array literal holds one type; this element is a number";
				return false;
			}

			const int pushed = (elementType == QD_ARRAY_TYPE_FLOAT)
									   ? qd_array_push_float(array, number)
									   : qd_array_push_int(array, static_cast<int64_t>(number));
			if (pushed != 0) {
				qd_array_release(array);
				interp->error = "could not grow the array";
				return false;
			}
		}

		if (qd_push_p(interp->ctx, array) != 0) {
			qd_array_release(array);
			interp->error = "stack overflow";
			return false;
		}
		return true;
	}

	// 'cast<T>' is one instruction name covering four runtime ops, picked by its
	// type parameter -- the mapping generator_nodes_instructions.cc uses.
	OpFn castOp(const std::string& typeParam) {
		if (typeParam == "f64" || typeParam == "f32" || typeParam == "f" || typeParam == "float" ||
				typeParam == "float64") {
			return qd_castf;
		}
		if (typeParam == "str" || typeParam == "string" || typeParam == "s") {
			return qd_casts;
		}
		if (typeParam == "ptr" || typeParam == "p" || typeParam == "pointer") {
			return qd_castp;
		}
		if (typeParam == "i64" || typeParam == "i32" || typeParam == "i16" || typeParam == "i8" || typeParam == "u64" ||
				typeParam == "u32" || typeParam == "u16" || typeParam == "u8" || typeParam == "i" ||
				typeParam == "int" || typeParam == "int64") {
			return qd_casti;
		}
		return nullptr;
	}

	bool evalInstruction(qd_interp* interp, const Qd::AstNodeInstruction* instruction) {
		// 'cast' carries its type parameter rather than spelling it in the name,
		// so it resolves before the table lookup.
		BuiltinOp cast{nullptr, 1};
		if (instruction->name() == "cast") {
			if (!instruction->hasTypeParam()) {
				interp->error = "'cast' needs a type, as in 'cast<i64>'";
				return false;
			}
			cast.fn = castOp(instruction->typeParam());
			if (cast.fn == nullptr) {
				interp->error = "'cast<" + instruction->typeParam() + ">' is not a conversion this tier knows";
				return false;
			}
			// 'cast' is total, so a string operand is refused for the numeric targets the
			// same way the compiler refuses it: parsing can fail and there is nowhere to say
			// so, and the old answer for a failure was the value 0. This tier can be exact
			// about it -- the operand's type is right there on the stack.
			if (cast.fn == qd_casti || cast.fn == qd_castf) {
				qd_stack_element_t top;
				if (qd_stack_size(interp->ctx->st) > 0 && qd_stack_peek(interp->ctx->st, &top) == QD_STACK_OK &&
						top.type == QD_STACK_TYPE_STR) {
					interp->error = std::string("a string cannot be cast to ") +
									((cast.fn == qd_casti) ? "an integer" : "a float") +
									"; use strconv (atoi, parse_int, parse_float), which can report a parse failure";
					return false;
				}
			}
		}

		const std::unordered_map<std::string, BuiltinOp>& table = opTable();
		const auto found = table.find(instruction->name());
		if (cast.fn == nullptr && found == table.end()) {
			interp->error = "'" + instruction->name() + "' cannot be interpreted";
			return false;
		}

		const BuiltinOp& op = (cast.fn != nullptr) ? cast : found->second;

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

		// The modifiers that may precede `fn` start a declaration just as `fn` does;
		// without them `stack fn twice(i64 -- r:i64) { 2 * }` was wrapped in a main
		// and parsed as an expression.
		static const char* const KEYWORDS[] = {
				"fn ", "fn(", "pub ", "inline ", "stack ", "const ", "struct ", "enum ", "use ", "type ", "test "};
		for (const char* keyword : KEYWORDS) {
			if (std::strncmp(p, keyword, std::strlen(keyword)) == 0) {
				return true;
			}
		}
		return false;
	}

	// Arguments arrive on the stack. A signature binds them into the frame and takes
	// them off the stack -- the rule the type checker states in
	// semantic_validator_typecheck.cc -- unless the function is `stack fn`, whose
	// inputs stay on the stack where the body reads them positionally.
	bool bindParameters(qd_interp* interp, const std::string& name, const Qd::AstNodeFunctionDeclaration* function) {
		const auto& inputs = function->inputParameters();
		if (inputs.empty() || function->hasReceiver() || function->isStack()) {
			return true;
		}

		const size_t depth = static_cast<size_t>(qd_stack_size(interp->ctx->st));
		if (depth < inputs.size()) {
			interp->error = "'" + name + "' needs " + std::to_string(inputs.size()) +
							(inputs.size() == 1 ? " value, " : " values, ") + std::to_string(depth) + " on the stack";
			return false;
		}

		// The last parameter is on top, so binding runs backwards
		for (size_t i = inputs.size(); i > 0; i--) {
			const auto* parameter = static_cast<const Qd::AstNodeParameter*>(inputs[i - 1].get());
			qd_stack_element_t element;
			if (qd_stack_pop(interp->ctx->st, &element) != QD_STACK_OK) {
				interp->error = "'" + name + "' could not take its arguments";
				return false;
			}
			bindLocal(interp, parameter->name(), element);
		}
		return true;
	}

	bool callFunction(qd_interp* interp, const std::string& name, const Qd::IAstNode* decl) {
		if (interp->call_depth >= QD_INTERP_MAX_CALL_DEPTH) {
			interp->error = "'" + name + "' recursed too deeply";
			return false;
		}

		const auto* function = static_cast<const Qd::AstNodeFunctionDeclaration*>(decl);
		const Qd::IAstNode* body = function->body();
		if (body == nullptr) {
			interp->error = "'" + name + "' has no body";
			return false;
		}

		interp->frames.emplace_back();
		interp->call_depth++;
		bool ok = bindParameters(interp, name, function);
		if (ok) {
			ok = evalNode(interp, body);
		}
		interp->call_depth--;
		dropFrame(interp);

		// A break or a return inside a function does not escape it
		interp->flow = Flow::Normal;
		return ok;
	}

	// Resolve a name written in source: a local, then a constant, then a
	// declared function, then a registered native.
	bool evalName(qd_interp* interp, const std::string& name) {
		// Scoped: a call below pushes a frame, which may move this one
		{
			const Frame& frame = interp->frames.back();
			const auto local = frame.find(name);
			if (local != frame.end()) {
				if (!pushLocal(interp, local->second)) {
					interp->error = "stack overflow";
					return false;
				}
				return true;
			}
		}

		const auto constant = interp->constants.find(name);
		if (constant != interp->constants.end()) {
			if (!pushConstantValue(interp, constant->second)) {
				interp->error = "stack overflow";
				return false;
			}
			return true;
		}

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

	// A case label naming a constant or an enum variant compares by the value's
	// written form, which is all a constant is -- the rule pushConstantValue
	// applies when the same name is read as a value.
	bool constantMatches(const std::string& text, const qd_stack_element_t& subject) {
		if (text.size() >= 2 && text.front() == '"') {
			if (subject.type != QD_STACK_TYPE_STR || subject.value.s == nullptr) {
				return false;
			}
			const char* held = qd_string_data(subject.value.s);
			return held != nullptr && decodeString(text) == held;
		}
		if (text.find('.') != std::string::npos) {
			return subject.type == QD_STACK_TYPE_FLOAT && subject.value.f == std::strtod(text.c_str(), nullptr);
		}
		int64_t value = 0;
		return subject.type == QD_STACK_TYPE_INT && Qd::parseIntegerLiteral(text, value) && subject.value.i == value;
	}

	// One case value against what switch took off the stack. `resolved` reports
	// whether the label meant anything at all: a name that resolves to nothing
	// is a mistake, not a case that failed to match, and the compiled tier
	// rejects it rather than silently never matching.
	bool caseMatches(qd_interp* interp, const Qd::IAstNode* value, const qd_stack_element_t& subject, bool& resolved) {
		resolved = true;

		if (value != nullptr && value->type() == Qd::IAstNode::Type::IDENTIFIER) {
			const auto* named = static_cast<const Qd::AstNodeIdentifier*>(value);
			const auto constant = interp->constants.find(named->name());
			resolved = constant != interp->constants.end();
			return resolved && constantMatches(constant->second, subject);
		}

		if (value != nullptr && value->type() == Qd::IAstNode::Type::SCOPED_IDENTIFIER) {
			const auto* scoped = static_cast<const Qd::AstNodeScopedIdentifier*>(value);
			const auto constant = interp->constants.find(scoped->scope() + "::" + scoped->name());
			resolved = constant != interp->constants.end();
			return resolved && constantMatches(constant->second, subject);
		}

		if (value == nullptr || value->type() != Qd::IAstNode::Type::LITERAL) {
			resolved = false;
			return false;
		}

		const auto* literal = static_cast<const Qd::AstNodeLiteral*>(value);
		const std::string& text = literal->value();

		switch (literal->literalType()) {
		case Qd::AstNodeLiteral::LiteralType::INTEGER:
			return subject.type == QD_STACK_TYPE_INT && subject.value.i == Qd::integerLiteralOr(text, 0);

		case Qd::AstNodeLiteral::LiteralType::BOOL:
			return subject.type == QD_STACK_TYPE_INT && subject.value.i == ((text == "true" || text == "Ok") ? 1 : 0);

		case Qd::AstNodeLiteral::LiteralType::FLOAT:
			return subject.type == QD_STACK_TYPE_FLOAT && subject.value.f == std::strtod(text.c_str(), nullptr);

		case Qd::AstNodeLiteral::LiteralType::STRING: {
			if (subject.type != QD_STACK_TYPE_STR || subject.value.s == nullptr) {
				return false;
			}
			const char* held = qd_string_data(subject.value.s);
			return held != nullptr && decodeString(text) == held;
		}

		default:
			resolved = false;
			return false;
		}
	}

	// switch takes its value off the stack, the way if takes its condition.
	//
	// The compiled tier has one more rule: where the value is the status left by
	// a user-defined fallible call, `Ok` means "no error" rather than "equals 1".
	// Nothing here can produce such a status -- this tier has no fallible calls --
	// so the two cannot disagree.
	bool evalSwitch(qd_interp* interp, const Qd::IAstNode* node) {
		if (qd_stack_size(interp->ctx->st) == 0) {
			interp->error = "'switch' needs a value on the stack";
			return false;
		}

		qd_stack_element_t subject;
		if (qd_stack_pop(interp->ctx->st, &subject) != QD_STACK_OK) {
			interp->error = "'switch' could not take its value";
			return false;
		}

		const auto* statement = static_cast<const Qd::AstNodeSwitchStatement*>(node);
		const Qd::AstNodeCase* fallback = nullptr;

		for (const Qd::AstNodeCase* branch : statement->cases()) {
			if (branch->isDefault()) {
				fallback = branch;
				continue;
			}
			bool resolved = false;
			const bool matched = caseMatches(interp, branch->value(), subject, resolved);
			if (!resolved) {
				releaseElement(subject);
				interp->error = "a case label must be a literal, a constant or an enum variant";
				return false;
			}
			if (matched) {
				releaseElement(subject);
				return branch->body() == nullptr || evalNode(interp, branch->body());
			}
		}

		// Nothing matched and no '_': the value is spent and nothing runs
		releaseElement(subject);
		if (fallback != nullptr && fallback->body() != nullptr) {
			return evalNode(interp, fallback->body());
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

		case Type::SWITCH_STATEMENT:
			return evalSwitch(interp, node);

		case Type::LOOP_STATEMENT:
			return evalLoop(interp, node);

		case Type::FOR_STATEMENT:
			return evalFor(interp, static_cast<const Qd::AstNodeForStatement*>(node));

		case Type::BREAK_STATEMENT:
			interp->flow = Flow::Break;
			return true;

		case Type::CONTINUE_STATEMENT:
			interp->flow = Flow::Continue;
			return true;

		case Type::RETURN_STATEMENT:
			interp->flow = Flow::Return;
			return true;

		case Type::LOCAL:
			return evalLocalBinding(interp, static_cast<const Qd::AstNodeLocal*>(node));

		case Type::ARRAY_LITERAL:
			return evalArrayLiteral(interp, static_cast<const Qd::AstNodeArrayLiteral*>(node));

		// A declaration inside an evaluated body records itself and runs nothing
		case Type::CONSTANT_DECLARATION:
			recordConstant(interp, static_cast<const Qd::AstNodeConstant*>(node));
			return true;

		case Type::ENUM_DECLARATION:
			recordEnum(interp, static_cast<const Qd::AstNodeEnumDeclaration*>(node));
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

		// Unwound from a fatal runtime error; recovery disarmed itself. The
		// frames a call in progress pushed are still there, since longjmp ran no
		// destructor -- they live in the interpreter, so dropping them here is
		// all the cleanup needed.
		while (interp->frames.size() > 1) {
			dropFrame(interp);
		}
		interp->call_depth = 0;

		const char* message = qd_error_message(interp->ctx);
		interp->error = (message != nullptr) ? message : "runtime error";
		return false;
	}

	qd_interp* makeInterp(qd_context* ctx, bool owns) {
		qd_interp* interp = new (std::nothrow) qd_interp();
		if (interp == nullptr) {
			if (owns) {
				qd_free_context(ctx);
			}
			return nullptr;
		}
		interp->ctx = ctx;
		interp->owns_ctx = owns;
		interp->frames.resize(1); // the top-level frame
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
	// Give up what the bindings own; the top-level frame lives as long as this
	while (!interp->frames.empty()) {
		dropFrame(interp);
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

	// Removing an enum removes the variants it put in the constant table
	const auto declared = interp->enums.find(name);
	if (declared != interp->enums.end()) {
		for (const std::string& key : declared->second) {
			interp->constants.erase(key);
		}
		interp->enums.erase(declared);
		return true;
	}

	return interp->functions.erase(name) > 0 || interp->constants.erase(name) > 0;
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

	for (const auto& entry : interp->constants) {
		if (!visit(entry.first.c_str(), userdata)) {
			return;
		}
	}
}

size_t qd_interp_declared_count(const qd_interp* interp) {
	if (interp == nullptr) {
		return 0;
	}
	// An enum counts once, not once per variant, so its variants are discounted
	// from the constant table they share
	size_t variants = 0;
	for (const auto& entry : interp->enums) {
		variants += entry.second.size();
	}
	return interp->functions.size() + interp->constants.size() - variants + interp->enums.size();
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
	while (interp->frames.size() > 1) {
		dropFrame(interp);
	}

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
			switch (child->type()) {
			case Qd::IAstNode::Type::FUNCTION_DECLARATION: {
				const auto* fn = static_cast<const Qd::AstNodeFunctionDeclaration*>(child);
				interp->functions[fn->name()] = Declared{child, ast};
				if (interp->last_declared.empty()) {
					interp->last_declared = fn->name();
				}
				recorded = true;
				break;
			}

			// A constant's value is copied out, so these keep no reference to
			// the parse they came from.
			case Qd::IAstNode::Type::CONSTANT_DECLARATION:
				recordConstant(interp, static_cast<const Qd::AstNodeConstant*>(child));
				recorded = true;
				break;

			case Qd::IAstNode::Type::ENUM_DECLARATION:
				recordEnum(interp, static_cast<const Qd::AstNodeEnumDeclaration*>(child));
				recorded = true;
				break;

			default:
				break;
			}
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
