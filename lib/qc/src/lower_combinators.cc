// Lowering of the combinators into locals, `for` and `call`. See lower_combinators.h.
//
//   x q dip            -> $x  Q  $x
//   x q keep           -> $x  $x Q  $x
//   x y q dip2         -> $y -> $x  Q  $x $y
//   x y q keep2        -> $y -> $x  $x $y Q  $x $y
//   x p q bi           -> $x  $x P  $x Q                 (tri likewise)
//   x y p q bi_star    -> $y  P  $y Q
//   x y z p q r tri_star -> $z -> $y  P  $y Q  $z R
//   x y q both         -> $q -> $y -> $x  $x $q call  $y $q call
//   n q times          -> $n  0 $n 1 for $i { Q }
//   arr q each         -> $a  0 $a len 1 for $i { $a $i nth Q }
//   arr q map          -> $a  [] -> $o  0 $a len 1 for $i { $o $a $i nth Q append -> $o }  $o
//   arr q filter       -> $a  [] -> $o  0 $a len 1 for $i { $a $i nth -> $e  $e Q if { $o $e append -> $o } }  $o
//   arr acc q fold     -> $c -> $a  $c  0 $a len 1 for $i { $a $i nth Q }
//   arr q any          -> $a  0 -> $r  0 $a len 1 for $i { $a $i nth Q if { 1 -> $r break } }  $r
//   arr q all          -> $a  1 -> $r  0 $a len 1 for $i { $a $i nth Q not if { 0 -> $r break } }  $r
//   arr q find         -> $a  Z -> $e 0 -> $r  0 $a len 1 for $i { $a $i nth -> $x  $x Q if {
//                           $x -> $e 1 -> $r break } }  $e $r          (Z: the zero of the element type)
//   arr q index        -> $a  -1 -> $r  0 $a len 1 for $i { $a $i nth Q if { $i -> $r break } }  $r
//   arr n q scan       -> $s  $s 0 < if { 0 -> $s }  -> $a  -1 -> $r
//                           $s $a len 1 for $i { $a $i nth Q if { $i -> $r break } }  $r
//
// `each`, `fold` and the searching words also take a string, whose elements are its characters
// as codepoints; `len` and `nth` read one as they read an array.
//
// Q is the body of a function value written in place, when it can stand in the enclosing
// function unchanged, and `-> $q ... $q call` otherwise. The `$` names cannot be written in
// source, so they collide with nothing; each combinator gets its own.
//
// The derived shuffles are lowered here too, since each is two of the core ones:
//   drop2 -> drop drop     tuck -> swap over     rrot -> rot rot
// and the four-deep ones, which need a value held aside:
//   rot4  -> -> $t  rot  $t swap          rrot4 -> swap  -> $t  rot rot  $t

#include <atomic>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_anonymous_function.h>
#include <quadrate/qc/ast_node_array_literal.h>
#include <quadrate/qc/ast_node_break.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_if.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/instructions.h>
#include <quadrate/qc/lower_combinators.h>

#include "ast_node_block.h"

namespace Qd {

	namespace {
		using Names = std::unordered_set<std::string>;

		bool isHiddenName(const std::string& name) {
			return !name.empty() && name[0] == '$';
		}

		// Every name a function body declares, wherever: a local of that name shadows a builtin
		// from where it is declared, and a combinator so shadowed is not one
		void collectDeclaredNames(const IAstNode* node, Names& names) {
			if (!node) {
				return;
			}
			if (node->type() == IAstNode::Type::LOCAL) {
				for (const std::string& name : static_cast<const AstNodeLocal*>(node)->names()) {
					names.insert(name);
				}
			} else if (node->type() == IAstNode::Type::FOR_STATEMENT) {
				names.insert(static_cast<const AstNodeForStatement*>(node)->iteratorName());
			} else if (node->type() == IAstNode::Type::VARIABLE_DECLARATION) {
				names.insert(static_cast<const AstNodeParameter*>(node)->name());
			}
			for (size_t i = 0; i < node->childCount(); i++) {
				collectDeclaredNames(node->child(i), names);
			}
		}

		// Whether this body can be spliced into the enclosing function as it is: nothing in it
		// may mean something different there. A local would be a local of the enclosing
		// function; `return` and `defer` would be about the enclosing function; a loop exit
		// outside a loop of its own would leave the enclosing loop; a `for` iterator would be
		// the enclosing function's.
		bool standsAlone(const IAstNode* node, int loopDepth) {
			if (!node) {
				return true;
			}
			switch (node->type()) {
			case IAstNode::Type::ANONYMOUS_FUNCTION:
				return true; // a function of its own
			case IAstNode::Type::LOCAL:
				for (const std::string& name : static_cast<const AstNodeLocal*>(node)->names()) {
					if (!isHiddenName(name)) {
						return false;
					}
				}
				return true;
			case IAstNode::Type::RETURN_STATEMENT:
			case IAstNode::Type::DEFER_STATEMENT:
				return false;
			case IAstNode::Type::BREAK_STATEMENT:
			case IAstNode::Type::CONTINUE_STATEMENT:
			case IAstNode::Type::CONDITIONAL_BREAK:
				return loopDepth > 0;
			case IAstNode::Type::FOR_STATEMENT:
				if (!isHiddenName(static_cast<const AstNodeForStatement*>(node)->iteratorName())) {
					return false;
				}
				return standsAlone(node->child(0), loopDepth + 1);
			case IAstNode::Type::LOOP_STATEMENT:
				for (size_t i = 0; i < node->childCount(); i++) {
					if (!standsAlone(node->child(i), loopDepth + 1)) {
						return false;
					}
				}
				return true;
			default:
				for (size_t i = 0; i < node->childCount(); i++) {
					if (!standsAlone(node->child(i), loopDepth)) {
						return false;
					}
				}
				return true;
			}
		}

		// The two core shuffles a derived one is, or nullptr
		const std::pair<const char*, const char*>* derivedShuffle(const std::string& name) {
			static const std::pair<const char*, const char*> DROP2 = {"drop", "drop"};
			static const std::pair<const char*, const char*> TUCK = {"swap", "over"};
			static const std::pair<const char*, const char*> RROT = {"rot", "rot"};
			if (name == "drop2") {
				return &DROP2;
			}
			if (name == "tuck") {
				return &TUCK;
			}
			if (name == "rrot") {
				return &RROT;
			}
			return nullptr;
		}

		bool inlinable(const AstNodeAnonymousFunction* literal) {
			return literal->isStack() && !literal->throws() && literal->body() && standsAlone(literal->body(), 0);
		}

		class Lowering {
		public:
			void lowerTree(IAstNode* node, const Names& declared) {
				if (!node) {
					return;
				}
				if (node->type() == IAstNode::Type::FUNCTION_DECLARATION ||
						node->type() == IAstNode::Type::TEST_DECLARATION ||
						node->type() == IAstNode::Type::ANONYMOUS_FUNCTION) {
					// A lambda sees the names of the function around it, which it captures
					Names inner = node->type() == IAstNode::Type::ANONYMOUS_FUNCTION ? declared : Names{};
					collectDeclaredNames(node, inner);
					for (size_t i = 0; i < node->childCount(); i++) {
						lowerTree(node->child(i), inner);
					}
					return;
				}
				for (size_t i = 0; i < node->childCount(); i++) {
					lowerTree(node->child(i), declared);
				}
				if (node->type() == IAstNode::Type::BLOCK) {
					lowerBlock(static_cast<AstNodeBlock*>(node), declared);
				}
			}

		private:
			static std::atomic<size_t> sNextId;

			size_t mLine = 0;
			size_t mColumn = 0;

			template <typename T>
			T* at(T* node) {
				node->setPosition(mLine, mColumn);
				return node;
			}

			IAstNode* local(const std::string& name) {
				return at(new AstNodeLocal(name));
			}

			IAstNode* ident(const std::string& name) {
				return at(new AstNodeIdentifier(name));
			}

			IAstNode* instr(const char* name) {
				return at(new AstNodeInstruction(name));
			}

			IAstNode* integer(const char* value) {
				return at(new AstNodeLiteral(value, AstNodeLiteral::LiteralType::INTEGER));
			}

			AstNodeBlock* makeBlock(std::vector<IAstNode*> nodes) {
				AstNodeBlock* b = at(new AstNodeBlock());
				for (IAstNode* n : nodes) {
					n->setParent(b);
					b->addChild(n);
				}
				return b;
			}

			// The zero of a type, for `find` to leave when nothing matches
			IAstNode* zeroOf(const std::string& type) {
				if (type == "f64") {
					return at(new AstNodeLiteral("0.0", AstNodeLiteral::LiteralType::FLOAT));
				}
				if (type == "d128") {
					return at(new AstNodeLiteral("0d", AstNodeLiteral::LiteralType::D128));
				}
				if (type == "str") {
					return at(new AstNodeLiteral("", AstNodeLiteral::LiteralType::STRING));
				}
				if (type == "i64" || type == "_" || type.empty()) {
					return integer("0");
				}
				return at(new AstNodeLiteral("null", AstNodeLiteral::LiteralType::NULL_PTR));
			}

			// `<start> <end> 1 for $i { body }`, with <end> what `endNodes` push and <start> what
			// `startNodes` push, or 0
			void forRange(std::vector<IAstNode*>& out, std::vector<IAstNode*> endNodes, const std::string& iterator,
					std::vector<IAstNode*> body, std::vector<IAstNode*> startNodes = {}) {
				if (startNodes.empty()) {
					out.push_back(integer("0"));
				}
				for (IAstNode* n : startNodes) {
					out.push_back(n);
				}
				for (IAstNode* n : endNodes) {
					out.push_back(n);
				}
				out.push_back(integer("1"));
				auto* loop = at(new AstNodeForStatement());
				loop->setIteratorName(iterator);
				AstNodeBlock* b = makeBlock(std::move(body));
				b->setParent(loop);
				loop->setBody(b);
				out.push_back(loop);
			}

			void lowerBlock(AstNodeBlock* block, const Names& declared) {
				for (size_t i = 0; i < block->childCount(); i++) {
					IAstNode* child = block->child(i);
					if (child->type() != IAstNode::Type::INSTRUCTION) {
						continue;
					}
					auto* inst = static_cast<AstNodeInstruction*>(child);
					if (inst->isMethodCall() || declared.count(inst->name())) {
						continue;
					}
					if (const std::pair<const char*, const char*>* parts = derivedShuffle(inst->name())) {
						mLine = inst->line();
						mColumn = inst->column();
						std::vector<IAstNode*> out = {instr(parts->first), instr(parts->second)};
						for (IAstNode* n : out) {
							n->setParent(block);
						}
						block->replaceChildWithMany(i, out);
						i++;
						continue;
					}
					if (inst->name() == "rot4" || inst->name() == "rrot4") {
						mLine = inst->line();
						mColumn = inst->column();
						const std::string held = "$t" + std::to_string(sNextId++);
						std::vector<IAstNode*> out;
						if (inst->name() == "rot4") {
							out = {local(held), instr("rot"), ident(held), instr("swap")};
						} else {
							out = {instr("swap"), local(held), instr("rot"), instr("rot"), ident(held)};
						}
						for (IAstNode* n : out) {
							n->setParent(block);
						}
						block->replaceChildWithMany(i, out);
						i += out.size() - 1;
						continue;
					}
					const Combinator* combinator = findCombinator(inst->name());
					if (!combinator) {
						continue;
					}
					i = lowerCombinator(block, i, *combinator);
				}
			}

			// Replaces the combinator at `index`, and the function values written in place
			// before it that are inlined, with its lowering. Returns the index of the last node
			// of the lowering.
			size_t lowerCombinator(AstNodeBlock* block, size_t index, const Combinator& combinator) {
				IAstNode* site = block->child(index);
				mLine = site->line();
				mColumn = site->column();
				const std::string name = static_cast<AstNodeInstruction*>(site)->name();
				const std::string id = std::to_string(sNextId++);
				auto hidden = [&](const char* what) { return std::string("$") + what + id; };

				// Each function value, bottom first: the body to splice in, or the local it is
				// bound to. The ones written in place are the nodes right before the combinator.
				// `both` runs its function value twice, so it is never spliced in.
				const size_t count = combinator.functionValues;
				std::vector<std::vector<IAstNode*>> bodies(count);
				std::vector<bool> spliced(count, false);
				size_t first = index;
				bool inPlace = true;
				for (size_t k = 0; k < count; k++) {
					const size_t q = count - 1 - k;
					IAstNode* before = index > k ? block->child(index - 1 - k) : nullptr;
					inPlace = inPlace && before && before->type() == IAstNode::Type::ANONYMOUS_FUNCTION;
					if (!inPlace) {
						break;
					}
					auto* literal = static_cast<AstNodeAnonymousFunction*>(before);
					if (name != "both" && inlinable(literal)) {
						bodies[q] = static_cast<AstNodeBlock*>(literal->body())->releaseChildren();
						spliced[q] = true;
					}
					first = index - 1 - k;
				}

				std::vector<IAstNode*> out;
				// The function values not spliced in are on the stack -- in place, they are still
				// pushed -- and are bound, top first
				for (size_t k = 0; k < count; k++) {
					const size_t q = count - 1 - k;
					if (!spliced[q]) {
						out.push_back(local(hidden("q") + "_" + std::to_string(q)));
					}
				}
				auto run = [&](std::vector<IAstNode*>& into, size_t q) {
					if (spliced[q]) {
						for (IAstNode* n : bodies[q]) {
							into.push_back(n);
						}
						bodies[q].clear();
						return;
					}
					into.push_back(ident(hidden("q") + "_" + std::to_string(q)));
					into.push_back(instr("call"));
				};

				const std::string x = hidden("x");
				const std::string a = hidden("a");
				const std::string o = hidden("o");
				const std::string it = hidden("i");
				if (name == "dip") {
					out.push_back(local(x));
					run(out, 0);
					out.push_back(ident(x));
				} else if (name == "keep") {
					out.push_back(local(x));
					out.push_back(ident(x));
					run(out, 0);
					out.push_back(ident(x));
				} else if (name == "dip2" || name == "keep2") {
					const std::string y = hidden("y");
					out.push_back(local(y));
					out.push_back(local(x));
					if (name == "keep2") {
						out.push_back(ident(x));
						out.push_back(ident(y));
					}
					run(out, 0);
					out.push_back(ident(x));
					out.push_back(ident(y));
				} else if (name == "bi" || name == "tri") {
					out.push_back(local(x));
					for (size_t q = 0; q < count; q++) {
						out.push_back(ident(x));
						run(out, q);
					}
				} else if (name == "bi_star" || name == "tri_star") {
					// The first value stays where it is, under the rest: P runs on it there
					std::vector<std::string> above;
					for (size_t q = 1; q < count; q++) {
						above.push_back(hidden("v") + "_" + std::to_string(q));
					}
					for (size_t k = above.size(); k > 0; k--) {
						out.push_back(local(above[k - 1]));
					}
					run(out, 0);
					for (size_t q = 1; q < count; q++) {
						out.push_back(ident(above[q - 1]));
						run(out, q);
					}
				} else if (name == "both") {
					const std::string y = hidden("y");
					out.push_back(local(y));
					out.push_back(local(x));
					out.push_back(ident(x));
					run(out, 0);
					out.push_back(ident(y));
					run(out, 0);
				} else if (name == "times") {
					const std::string n = hidden("n");
					out.push_back(local(n));
					std::vector<IAstNode*> body;
					run(body, 0);
					forRange(out, {ident(n)}, it, std::move(body));
				} else if (name == "each") {
					out.push_back(local(a));
					std::vector<IAstNode*> body = {ident(a), ident(it), instr("nth")};
					run(body, 0);
					forRange(out, {ident(a), instr("len")}, it, std::move(body));
				} else if (name == "map") {
					out.push_back(local(a));
					out.push_back(at(new AstNodeArrayLiteral()));
					out.push_back(local(o));
					std::vector<IAstNode*> body = {ident(o), ident(a), ident(it), instr("nth")};
					run(body, 0);
					body.push_back(instr("append"));
					body.push_back(local(o));
					forRange(out, {ident(a), instr("len")}, it, std::move(body));
					out.push_back(ident(o));
				} else if (name == "filter") {
					const std::string e = hidden("e");
					out.push_back(local(a));
					out.push_back(at(new AstNodeArrayLiteral()));
					out.push_back(local(o));
					std::vector<IAstNode*> body = {ident(a), ident(it), instr("nth"), local(e), ident(e)};
					run(body, 0);
					auto* test = at(new AstNodeIfStatement());
					AstNodeBlock* then = makeBlock({ident(o), ident(e), instr("append"), local(o)});
					then->setParent(test);
					test->setThenBody(then);
					body.push_back(test);
					forRange(out, {ident(a), instr("len")}, it, std::move(body));
					out.push_back(ident(o));
				} else if (name == "any" || name == "all" || name == "find" || name == "index" || name == "scan") {
					// Stop at the first element that decides the answer
					const std::string r = hidden("r");
					const std::string e = hidden("e");
					const std::string el = hidden("x");
					const std::string from = hidden("s");
					const bool isFind = name == "find";
					const bool isFrom = name == "scan";
					const bool isIndex = name == "index" || isFrom;
					// The validator leaves the element type here, which `find` takes its not-found
					// zero from; nothing, and so 0, for a string or with no validator at all.
					const std::string& walked = static_cast<AstNodeInstruction*>(site)->resolvedType();
					if (isFrom) {
						// A start before the beginning is the beginning
						out.push_back(local(from));
						out.push_back(ident(from));
						out.push_back(integer("0"));
						out.push_back(instr("<"));
						auto* clamp = at(new AstNodeIfStatement());
						AstNodeBlock* zero = makeBlock({integer("0"), local(from)});
						zero->setParent(clamp);
						clamp->setThenBody(zero);
						out.push_back(clamp);
					}
					out.push_back(local(a));
					if (isFind) {
						out.push_back(zeroOf(walked));
						out.push_back(local(e));
					}
					out.push_back(integer(isIndex ? "-1" : name == "all" ? "1" : "0"));
					out.push_back(local(r));
					std::vector<IAstNode*> body = {ident(a), ident(it), instr("nth")};
					if (isFind) {
						body.push_back(local(el));
						body.push_back(ident(el));
					}
					run(body, 0);
					if (name == "all") {
						body.push_back(instr("not"));
					}
					std::vector<IAstNode*> decided;
					if (isFind) {
						decided.push_back(ident(el));
						decided.push_back(local(e));
					}
					if (isIndex) {
						decided.push_back(ident(it));
					} else {
						decided.push_back(integer(name == "all" ? "0" : "1"));
					}
					decided.push_back(local(r));
					decided.push_back(at(new AstNodeBreak()));
					auto* test = at(new AstNodeIfStatement());
					AstNodeBlock* then = makeBlock(std::move(decided));
					then->setParent(test);
					test->setThenBody(then);
					body.push_back(test);
					std::vector<IAstNode*> start;
					if (isFrom) {
						start.push_back(ident(from));
					}
					forRange(out, {ident(a), instr("len")}, it, std::move(body), std::move(start));
					if (isFind) {
						out.push_back(ident(e));
					}
					out.push_back(ident(r));
				} else if (name == "fold") {
					const std::string c = hidden("c");
					out.push_back(local(c));
					out.push_back(local(a));
					out.push_back(ident(c));
					std::vector<IAstNode*> body = {ident(a), ident(it), instr("nth")};
					run(body, 0);
					forRange(out, {ident(a), instr("len")}, it, std::move(body));
				}

				for (IAstNode* n : out) {
					n->setParent(block);
				}
				// The spliced literals are emptied; drop them with the combinator. Those still
				// pushed stay where they are.
				size_t removed = 0;
				for (size_t k = first; k < index; k++) {
					const size_t pos = k - removed;
					if (spliced[count - (index - k)]) {
						delete block->releaseChild(pos);
						removed++;
					}
				}
				const size_t position = index - removed;
				block->replaceChildWithMany(position, out);
				return position + out.size() - 1;
			}
		};

		std::atomic<size_t> Lowering::sNextId{0};
	}

	void lowerCombinators(IAstNode* root) {
		Lowering lowering;
		lowering.lowerTree(root, {});
	}

} // namespace Qd
