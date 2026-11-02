// Qualifying a module's own names. See resolve_names.h.

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_array_literal.h>
#include <quadrate/qc/ast_node_as_cast.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_enum.h>
#include <quadrate/qc/ast_node_field_access.h>
#include <quadrate/qc/ast_node_field_set.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_function_pointer.h>
#include <quadrate/qc/ast_node_global_var.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_struct_construction.h>
#include <quadrate/qc/ast_node_struct_declaration.h>
#include <quadrate/qc/resolve_names.h>

namespace Qd {

	namespace {
		using Names = std::unordered_set<std::string>;

		class Resolver {
		public:
			explicit Resolver(const std::string& module) : mModule(module) {
			}

			// A module may be several files, each its own tree; the names of all of them are the
			// module's
			void collect(IAstNode* root) {
				for (size_t i = 0; i < root->childCount(); i++) {
					IAstNode* child = root->child(i);
					if (!child) {
						continue;
					}
					switch (child->type()) {
					case IAstNode::Type::FUNCTION_DECLARATION: {
						auto* fn = static_cast<AstNodeFunctionDeclaration*>(child);
						if (!fn->hasReceiver()) {
							mFunctions.insert(fn->name());
						}
						break;
					}
					case IAstNode::Type::CONSTANT_DECLARATION:
						mValues.insert(static_cast<AstNodeConstant*>(child)->name());
						break;
					case IAstNode::Type::GLOBAL_VAR_DECLARATION:
						mValues.insert(static_cast<AstNodeGlobalVar*>(child)->name());
						mGlobals.insert(static_cast<AstNodeGlobalVar*>(child)->name());
						break;
					case IAstNode::Type::STRUCT_DECLARATION:
						mStructs.insert(static_cast<AstNodeStructDeclaration*>(child)->name());
						break;
					case IAstNode::Type::ENUM_DECLARATION:
						mEnums.insert(static_cast<AstNodeEnumDeclaration*>(child)->name());
						break;
					default:
						break;
					}
				}
			}

			void run(IAstNode* root) {
				for (size_t i = 0; i < root->childCount(); i++) {
					IAstNode* child = root->child(i);
					if (!child) {
						continue;
					}
					Names locals;
					if (child->type() == IAstNode::Type::FUNCTION_DECLARATION ||
							child->type() == IAstNode::Type::TEST_DECLARATION) {
						collectLocals(child, locals);
					}
					resolve(child, locals);
				}
			}

		private:
			std::string mModule;
			Names mFunctions;
			Names mValues; // constants and globals
			Names mGlobals;
			Names mStructs;
			Names mEnums;

			std::string qualified(const std::string& name) const {
				return mModule + "::" + name;
			}

			// The module's struct a type names, qualified; anything else as it is
			std::string structType(const std::string& type) const {
				return mStructs.count(type) ? qualified(type) : type;
			}

			// Every name a function declares: its locals, iterators, a `def`'s parameters. `-> name`
			// with a global's name stores to the global -- a local never shadows one -- and is no
			// declaration.
			void collectLocals(const IAstNode* node, Names& names) const {
				if (!node) {
					return;
				}
				if (node->type() == IAstNode::Type::LOCAL) {
					for (const std::string& name : static_cast<const AstNodeLocal*>(node)->names()) {
						if (!mGlobals.count(name)) {
							names.insert(name);
						}
					}
				} else if (node->type() == IAstNode::Type::FOR_STATEMENT) {
					names.insert(static_cast<const AstNodeForStatement*>(node)->iteratorName());
				} else if (node->type() == IAstNode::Type::VARIABLE_DECLARATION) {
					names.insert(static_cast<const AstNodeParameter*>(node)->name());
				}
				for (size_t i = 0; i < node->childCount(); i++) {
					collectLocals(node->child(i), names);
				}
			}

			void resolve(IAstNode* node, const Names& locals) {
				if (!node) {
					return;
				}
				switch (node->type()) {
				case IAstNode::Type::IDENTIFIER: {
					auto* ident = static_cast<AstNodeIdentifier*>(node);
					if (ident->isMethodCall()) {
						ident->setQualifiedReceiverType(structType(ident->receiverType()));
					} else if (!locals.count(ident->name()) &&
							   (mFunctions.count(ident->name()) || mValues.count(ident->name()) ||
									   mStructs.count(ident->name()))) {
						ident->setName(qualified(ident->name()));
					}
					break;
				}
				case IAstNode::Type::INSTRUCTION: {
					// A method named like a builtin, `p len`, is an instruction with a receiver
					auto* inst = static_cast<AstNodeInstruction*>(node);
					if (!inst->receiverType().empty()) {
						inst->setQualifiedReceiverType(structType(inst->receiverType()));
					}
					break;
				}
				case IAstNode::Type::SCOPED_IDENTIFIER: {
					auto* scoped = static_cast<AstNodeScopedIdentifier*>(node);
					if (mEnums.count(scoped->scope())) {
						scoped->setScope(qualified(scoped->scope()));
					}
					break;
				}
				case IAstNode::Type::LOCAL: {
					// `-> name` into one of the module's globals is a store to it
					auto* local = static_cast<AstNodeLocal*>(node);
					for (size_t i = 0; i < local->names().size(); i++) {
						const std::string& name = local->names()[i];
						if (!locals.count(name) && mValues.count(name)) {
							local->rename(i, qualified(name));
						}
					}
					break;
				}
				case IAstNode::Type::FUNCTION_POINTER_REFERENCE: {
					auto* ref = static_cast<AstNodeFunctionPointerReference*>(node);
					if (mFunctions.count(ref->functionName())) {
						ref->setFunctionName(qualified(ref->functionName()));
					}
					break;
				}
				case IAstNode::Type::STRUCT_CONSTRUCTION: {
					auto* construction = static_cast<AstNodeStructConstruction*>(node);
					construction->setStructName(structType(construction->structName()));
					break;
				}
				case IAstNode::Type::FIELD_ACCESS: {
					auto* access = static_cast<AstNodeFieldAccess*>(node);
					access->setStructType(structType(access->structType()));
					break;
				}
				case IAstNode::Type::FIELD_SET: {
					auto* set = static_cast<AstNodeFieldSet*>(node);
					set->setStructType(structType(set->structType()));
					break;
				}
				case IAstNode::Type::AS_CAST: {
					auto* cast = static_cast<AstNodeAsCast*>(node);
					cast->setTypeName(structType(cast->typeName()));
					break;
				}
				case IAstNode::Type::ARRAY_LITERAL: {
					auto* array = static_cast<AstNodeArrayLiteral*>(node);
					if (!array->elementType().empty()) {
						array->setElementType(structType(array->elementType()));
					}
					break;
				}
				default:
					break;
				}
				for (size_t i = 0; i < node->childCount(); i++) {
					resolve(node->child(i), locals);
				}
			}
		};
	}

	void resolveNames(const std::vector<std::pair<std::string, IAstNode*>>& files) {
		std::unordered_map<std::string, Resolver> modules;
		for (const auto& [module, root] : files) {
			if (root) {
				modules.try_emplace(module, module).first->second.collect(root);
			}
		}
		for (const auto& [module, root] : files) {
			if (root) {
				modules.at(module).run(root);
			}
		}
	}
}
