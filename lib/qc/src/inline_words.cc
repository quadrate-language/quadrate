// Splicing of `inline fn` bodies at their call sites. See inline_words.h.

#include <atomic>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_break.h>
#include <quadrate/qc/ast_node_case.h>
#include <quadrate/qc/ast_node_conditional_break.h>
#include <quadrate/qc/ast_node_continue.h>
#include <quadrate/qc/ast_node_field_access.h>
#include <quadrate/qc/ast_node_field_set.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_function_pointer.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_if.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_loop.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_switch.h>
#include <quadrate/qc/ast_node_test.h>
#include <quadrate/qc/inline_words.h>

#include "ast_node_block.h"

namespace Qd {

	namespace {
		using Names = std::unordered_set<std::string>;

		bool isHiddenName(const std::string& name) {
			return !name.empty() && name[0] == '$';
		}

		// Whether a body can be copied into a caller and mean the same there: built only of the
		// nodes Copier knows, with nothing in it about the function it is in
		bool splicable(const IAstNode* node, int loopDepth) {
			if (!node) {
				return true;
			}
			auto children = [&](int depth) {
				for (size_t i = 0; i < node->childCount(); i++) {
					if (!splicable(node->child(i), depth)) {
						return false;
					}
				}
				return true;
			};
			switch (node->type()) {
			case IAstNode::Type::BLOCK:
			case IAstNode::Type::IF_STATEMENT:
			case IAstNode::Type::SWITCH_STATEMENT:
			case IAstNode::Type::CASE_STATEMENT:
				return children(loopDepth);
			case IAstNode::Type::LOOP_STATEMENT:
				return children(loopDepth + 1);
			case IAstNode::Type::FOR_STATEMENT:
				return isHiddenName(static_cast<const AstNodeForStatement*>(node)->iteratorName()) &&
					   children(loopDepth + 1);
			case IAstNode::Type::LOCAL:
				for (const std::string& name : static_cast<const AstNodeLocal*>(node)->names()) {
					if (!isHiddenName(name)) {
						return false;
					}
				}
				return true;
			case IAstNode::Type::BREAK_STATEMENT:
			case IAstNode::Type::CONTINUE_STATEMENT:
			case IAstNode::Type::CONDITIONAL_BREAK:
				return loopDepth > 0;
			case IAstNode::Type::LITERAL:
			case IAstNode::Type::IDENTIFIER:
			case IAstNode::Type::INSTRUCTION:
			case IAstNode::Type::SCOPED_IDENTIFIER:
			case IAstNode::Type::FIELD_ACCESS:
			case IAstNode::Type::FIELD_SET:
			case IAstNode::Type::FUNCTION_POINTER_REFERENCE:
				return true;
			default:
				return false;
			}
		}

		// The names a body refers to, other than the hidden ones: in the caller these must mean
		// what they meant in the word
		void collectNames(const IAstNode* node, Names& names) {
			if (!node) {
				return;
			}
			if (node->type() == IAstNode::Type::IDENTIFIER) {
				const std::string& name = static_cast<const AstNodeIdentifier*>(node)->name();
				if (!isHiddenName(name)) {
					names.insert(name);
				}
			}
			for (size_t i = 0; i < node->childCount(); i++) {
				collectNames(node->child(i), names);
			}
		}

		// Every name a function declares: a local, an iterator, a parameter of a `def`
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

		// A copy of a splicable body. The leaves are copied whole, with what the validator left
		// on them; the hidden names lowering made get a suffix of the copy's own, so two copies
		// in one function do not share a local.
		class Copier {
		public:
			explicit Copier(size_t id) : mSuffix("_inl" + std::to_string(id)) {
			}

			IAstNode* copy(const IAstNode* node) {
				IAstNode* out = make(node);
				out->setPosition(node->line(), node->column());
				return out;
			}

		private:
			std::string mSuffix;

			std::string rename(const std::string& name) const {
				return isHiddenName(name) ? name + mSuffix : name;
			}

			IAstNode* owned(IAstNode* child, IAstNode* parent) {
				if (child) {
					child->setParent(parent);
				}
				return child;
			}

			IAstNode* make(const IAstNode* node) {
				switch (node->type()) {
				case IAstNode::Type::BLOCK: {
					auto* out = new AstNodeBlock();
					for (size_t i = 0; i < node->childCount(); i++) {
						out->addChild(owned(copy(node->child(i)), out));
					}
					return out;
				}
				case IAstNode::Type::IF_STATEMENT: {
					const auto* src = static_cast<const AstNodeIfStatement*>(node);
					auto* out = new AstNodeIfStatement();
					if (src->thenBody()) {
						out->setThenBody(owned(copy(src->thenBody()), out));
					}
					if (src->elseBody()) {
						out->setElseBody(owned(copy(src->elseBody()), out));
					}
					return out;
				}
				case IAstNode::Type::LOOP_STATEMENT: {
					const auto* src = static_cast<const AstNodeLoopStatement*>(node);
					auto* out = new AstNodeLoopStatement();
					if (src->body()) {
						out->setBody(owned(copy(src->body()), out));
					}
					return out;
				}
				case IAstNode::Type::FOR_STATEMENT: {
					const auto* src = static_cast<const AstNodeForStatement*>(node);
					auto* out = new AstNodeForStatement();
					out->setIteratorName(rename(src->iteratorName()));
					if (src->body()) {
						out->setBody(owned(copy(src->body()), out));
					}
					return out;
				}
				case IAstNode::Type::SWITCH_STATEMENT: {
					const auto* src = static_cast<const AstNodeSwitchStatement*>(node);
					auto* out = new AstNodeSwitchStatement();
					for (const AstNodeCase* c : src->cases()) {
						out->addCase(static_cast<AstNodeCase*>(owned(copy(c), out)));
					}
					return out;
				}
				case IAstNode::Type::CASE_STATEMENT: {
					const auto* src = static_cast<const AstNodeCase*>(node);
					auto* out = new AstNodeCase(src->value() ? copy(src->value()) : nullptr, src->isDefault());
					if (out->value()) {
						out->value()->setParent(out);
					}
					if (src->body()) {
						out->setBody(owned(copy(src->body()), out));
					}
					return out;
				}
				case IAstNode::Type::LOCAL: {
					std::vector<std::string> names;
					for (const std::string& name : static_cast<const AstNodeLocal*>(node)->names()) {
						names.push_back(rename(name));
					}
					return new AstNodeLocal(names);
				}
				case IAstNode::Type::IDENTIFIER: {
					const auto* src = static_cast<const AstNodeIdentifier*>(node);
					// A hidden name is a local, which carries nothing from the validator
					if (isHiddenName(src->name())) {
						return new AstNodeIdentifier(rename(src->name()));
					}
					return new AstNodeIdentifier(*src);
				}
				case IAstNode::Type::LITERAL:
					return new AstNodeLiteral(*static_cast<const AstNodeLiteral*>(node));
				case IAstNode::Type::INSTRUCTION:
					return new AstNodeInstruction(*static_cast<const AstNodeInstruction*>(node));
				case IAstNode::Type::SCOPED_IDENTIFIER:
					return new AstNodeScopedIdentifier(*static_cast<const AstNodeScopedIdentifier*>(node));
				case IAstNode::Type::FIELD_ACCESS:
					return new AstNodeFieldAccess(*static_cast<const AstNodeFieldAccess*>(node));
				case IAstNode::Type::FIELD_SET:
					return new AstNodeFieldSet(*static_cast<const AstNodeFieldSet*>(node));
				case IAstNode::Type::FUNCTION_POINTER_REFERENCE:
					return new AstNodeFunctionPointerReference(
							*static_cast<const AstNodeFunctionPointerReference*>(node));
				case IAstNode::Type::CONDITIONAL_BREAK:
					return new AstNodeConditionalBreak(*static_cast<const AstNodeConditionalBreak*>(node));
				case IAstNode::Type::BREAK_STATEMENT:
					return new AstNodeBreak();
				case IAstNode::Type::CONTINUE_STATEMENT:
					return new AstNodeContinue();
				default:
					// splicable() let nothing else through
					return new AstNodeBlock();
				}
			}
		};

		// Whether a call converts any of its arguments: the validator lists every parameter,
		// most with nothing to do
		bool castsArguments(const std::vector<CastDirection>& casts) {
			for (CastDirection cast : casts) {
				if (cast != CastDirection::NONE) {
					return true;
				}
			}
			return false;
		}

		class Inliner {
		public:
			// The words of every file: by bare name the program's and its merged files', which
			// share its names; by `module::name` a module's, whose bodies name their module's
			// declarations qualified by now (resolveNames) and so mean the same anywhere
			Inliner(IAstNode* root, const std::vector<InlineModule>& modules) {
				addWords(root, "");
				for (const InlineModule& m : modules) {
					if (m.root) {
						addWords(m.root, m.merged ? "" : m.name + "::");
					}
				}
			}

			void run(IAstNode* root) {
				for (size_t i = 0; i < root->childCount(); i++) {
					IAstNode* child = root->child(i);
					if (!child || (child->type() != IAstNode::Type::FUNCTION_DECLARATION &&
										  child->type() != IAstNode::Type::TEST_DECLARATION)) {
						continue;
					}
					Names declared;
					collectDeclaredNames(child, declared);
					std::vector<std::string> active;
					if (child->type() == IAstNode::Type::FUNCTION_DECLARATION) {
						active.push_back(static_cast<AstNodeFunctionDeclaration*>(child)->name());
					}
					walk(child, declared, active);
				}
			}

		private:
			struct Word {
				AstNodeFunctionDeclaration* decl;
				Names uses;
			};

			static std::atomic<size_t> sNextCopy;
			std::unordered_map<std::string, Word> mWords;

			void addWords(IAstNode* root, const std::string& prefix) {
				for (size_t i = 0; i < root->childCount(); i++) {
					IAstNode* child = root->child(i);
					if (!child || child->type() != IAstNode::Type::FUNCTION_DECLARATION) {
						continue;
					}
					auto* word = static_cast<AstNodeFunctionDeclaration*>(child);
					if (word->isInline() && word->isStack() && !word->throws() && !word->hasReceiver() &&
							word->typeParams().empty() && word->stackVariables().empty() && word->body() &&
							splicable(word->body(), 0)) {
						Names uses;
						collectNames(word->body(), uses);
						mWords[prefix + word->name()] = {word, std::move(uses)};
					}
				}
			}

			// The word a node calls, when it is one to splice here, and the name it is called by
			const Word* spliceable(const IAstNode* node, const Names& declared, const std::vector<std::string>& active,
					std::string& key) {
				if (node->type() == IAstNode::Type::IDENTIFIER) {
					const auto* ident = static_cast<const AstNodeIdentifier*>(node);
					// A local of the name is what the name means here; a call that needs its
					// arguments cast is left as a call, which casts them
					if (declared.count(ident->name()) || ident->isMethodCall() ||
							castsArguments(ident->parameterCasts())) {
						return nullptr;
					}
					key = ident->name();
				} else if (node->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
					const auto* scoped = static_cast<const AstNodeScopedIdentifier*>(node);
					if (scoped->isMethodCall() || castsArguments(scoped->parameterCasts())) {
						return nullptr;
					}
					key = scoped->scope() + "::" + scoped->name();
				} else {
					return nullptr;
				}
				auto it = mWords.find(key);
				if (it == mWords.end()) {
					return nullptr;
				}
				for (const std::string& name : active) {
					if (name == key) {
						return nullptr;
					}
				}
				for (const std::string& name : it->second.uses) {
					if (declared.count(name)) {
						return nullptr;
					}
				}
				return &it->second;
			}

			void walk(IAstNode* node, const Names& declared, std::vector<std::string>& active) {
				if (!node || node->type() == IAstNode::Type::ANONYMOUS_FUNCTION) {
					return; // a function value is a function of its own
				}
				if (node->type() == IAstNode::Type::BLOCK) {
					expand(static_cast<AstNodeBlock*>(node), declared, active);
					return;
				}
				for (size_t i = 0; i < node->childCount(); i++) {
					walk(node->child(i), declared, active);
				}
			}

			void expand(AstNodeBlock* block, const Names& declared, std::vector<std::string>& active) {
				for (size_t i = 0; i < block->childCount(); i++) {
					IAstNode* site = block->child(i);
					std::string key;
					const Word* word = spliceable(site, declared, active, key);
					if (!word) {
						walk(site, declared, active);
						continue;
					}
					// The copy is expanded with the word active, so a word that calls itself
					// keeps the call
					Copier copier(sNextCopy++);
					auto* copy = static_cast<AstNodeBlock*>(copier.copy(word->decl->body()));
					active.push_back(key);
					expand(copy, declared, active);
					active.pop_back();
					std::vector<IAstNode*> nodes = copy->releaseChildren();
					delete copy;
					for (IAstNode* n : nodes) {
						n->setParent(block);
					}
					block->replaceChildWithMany(i, nodes);
					if (nodes.empty()) {
						i--;
					} else {
						i += nodes.size() - 1;
					}
				}
			}
		};

		std::atomic<size_t> Inliner::sNextCopy{0};
	}

	void inlineWords(IAstNode* root, const std::vector<InlineModule>& modules) {
		if (!root) {
			return;
		}
		Inliner inliner(root, modules);
		inliner.run(root);
		for (const InlineModule& m : modules) {
			if (m.root) {
				inliner.run(m.root);
			}
		}
	}
}
