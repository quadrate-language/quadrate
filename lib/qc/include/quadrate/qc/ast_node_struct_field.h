#ifndef QD_QC_AST_NODE_STRUCT_FIELD_H
#define QD_QC_AST_NODE_STRUCT_FIELD_H

#include "ast_node.h"
#include <memory>
#include <string>
#include <vector>

namespace Qd {
	/**
	 * @brief AST node representing a struct field
	 *
	 * Example: x:i64 or x:i64 = 42
	 */
	class AstNodeStructField : public IAstNode {
	public:
		AstNodeStructField(const std::string& name, const std::string& typeName)
			: mName(name), mTypeName(typeName), mParent(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeStructField() = default;

		IAstNode::Type type() const override {
			return Type::STRUCT_FIELD;
		}

		size_t childCount() const override {
			return mDefaultValue.size();
		}

		IAstNode* child(size_t index) const override {
			if (index < mDefaultValue.size()) {
				return mDefaultValue[index].get();
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

		const std::string& name() const {
			return mName;
		}

		const std::string& typeName() const {
			return mTypeName;
		}

		bool hasDefaultValue() const {
			return !mDefaultValue.empty();
		}

		const std::vector<std::unique_ptr<IAstNode>>& defaultValue() const {
			return mDefaultValue;
		}

		void setDefaultValue(std::vector<IAstNode*> nodes) {
			mDefaultValue.clear();
			for (auto* node : nodes) {
				node->setParent(this);
				mDefaultValue.emplace_back(node);
			}
		}

	private:
		std::string mName;
		std::string mTypeName;
		std::vector<std::unique_ptr<IAstNode>> mDefaultValue;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
