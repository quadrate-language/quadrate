// Walks over the AST that the rewriting passes share, so they agree on what a body declares.

#ifndef QD_QC_AST_WALK_H
#define QD_QC_AST_WALK_H

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_set>

#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_parameter.h>

namespace Qd {

	// The `$` names lowering makes up; they cannot be written in source
	inline bool isHiddenName(const std::string& name) {
		return !name.empty() && name[0] == '$';
	}

	// Every name a body declares, wherever: its locals, `for` iterators, a `def`'s parameters.
	// `keepLocal`, when given, decides which `-> name` locals count as declarations (a store to
	// a global is none); iterators and parameters always count.
	inline void collectDeclaredNames(const IAstNode* node, std::unordered_set<std::string>& names,
			const std::function<bool(const std::string&)>& keepLocal = {}) {
		if (!node) {
			return;
		}
		if (node->type() == IAstNode::Type::LOCAL) {
			for (const std::string& name : static_cast<const AstNodeLocal*>(node)->names()) {
				if (!keepLocal || keepLocal(name)) {
					names.insert(name);
				}
			}
		} else if (node->type() == IAstNode::Type::FOR_STATEMENT) {
			names.insert(static_cast<const AstNodeForStatement*>(node)->iteratorName());
		} else if (node->type() == IAstNode::Type::VARIABLE_DECLARATION) {
			names.insert(static_cast<const AstNodeParameter*>(node)->name());
		}
		for (size_t i = 0; i < node->childCount(); i++) {
			collectDeclaredNames(node->child(i), names, keepLocal);
		}
	}

} // namespace Qd

#endif // QD_QC_AST_WALK_H
