#ifndef QD_QC_AST_NODE_STRUCT_CONSTRUCTION_H
#define QD_QC_AST_NODE_STRUCT_CONSTRUCTION_H

#include "ast_node.h"
#include <string>
#include <vector>

namespace Qd {
	/**
	 * @brief Represents a field initializer in struct construction
	 *
	 * Example: x: 1.0 2.0 +
	 * The field name and an expression (block of AST nodes) that produces the value
	 */
	struct StructFieldInit {
		std::string fieldName;
		std::vector<IAstNode*> valueNodes; // Expression nodes that produce the field value
	};

	/**
	 * @brief AST node representing struct construction
	 *
	 * Example: Point { x: 1.0 y: 2.0 }
	 * Named field construction with explicit field initializers
	 */
	class AstNodeStructConstruction : public IAstNode {
	public:
		AstNodeStructConstruction(const std::string& structName)
			: mStructName(structName), mTypeArgs(), mParent(nullptr), mLine(0), mColumn(0) {
		}

		AstNodeStructConstruction(const std::string& structName, const std::vector<std::string>& typeArgs)
			: mStructName(structName), mTypeArgs(typeArgs), mParent(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeStructConstruction() {
			for (auto& init : mFieldInits) {
				for (auto* node : init.valueNodes) {
					delete node;
				}
			}
		}

		IAstNode::Type type() const override {
			return Type::STRUCT_CONSTRUCTION;
		}

		size_t childCount() const override {
			size_t count = 0;
			for (const auto& init : mFieldInits) {
				count += init.valueNodes.size();
			}
			return count;
		}

		IAstNode* child(size_t index) const override {
			size_t current = 0;
			for (const auto& init : mFieldInits) {
				if (index < current + init.valueNodes.size()) {
					return init.valueNodes[index - current];
				}
				current += init.valueNodes.size();
			}
			return nullptr;
		}

		IAstNode* parent() const override {
			return mParent;
		}

		void setParent(IAstNode* parent) override {
			mParent = parent;
		}

		size_t line() const override {
			return mLine;
		}

		size_t column() const override {
			return mColumn;
		}

		void setPosition(size_t line, size_t column) override {
			mLine = line;
			mColumn = column;
		}

		const std::string& structName() const {
			return mStructName;
		}

		void addFieldInit(const std::string& fieldName, std::vector<IAstNode*> valueNodes) {
			StructFieldInit init;
			init.fieldName = fieldName;
			init.valueNodes = std::move(valueNodes);
			for (auto* node : init.valueNodes) {
				node->setParent(this);
			}
			mFieldInits.push_back(std::move(init));
		}

		const std::vector<StructFieldInit>& fieldInits() const {
			return mFieldInits;
		}

		const std::vector<std::string>& typeArgs() const {
			return mTypeArgs;
		}

		bool hasTypeArgs() const {
			return !mTypeArgs.empty();
		}

		void addTypeArg(const std::string& typeArg) {
			mTypeArgs.push_back(typeArg);
		}

	private:
		std::string mStructName;
		std::vector<std::string> mTypeArgs;
		std::vector<StructFieldInit> mFieldInits;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
