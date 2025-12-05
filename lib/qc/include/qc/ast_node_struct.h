#ifndef QD_QC_AST_NODE_STRUCT_H
#define QD_QC_AST_NODE_STRUCT_H

#include "ast_node.h"
#include <string>
#include <vector>

namespace Qd {
	/**
	 * @brief AST node representing a struct field
	 */
	class AstNodeStructField : public IAstNode {
	public:
		AstNodeStructField(const std::string& name, const std::string& typeName)
			: mName(name), mTypeName(typeName), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::STRUCT_FIELD;
		}

		size_t childCount() const override {
			return 0;
		}

		IAstNode* child(size_t) const override {
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

	private:
		std::string mName;
		std::string mTypeName;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};

	/**
	 * @brief AST node representing a struct declaration
	 *
	 * Example: pub struct Vec2 { x:f64 y:f64 }
	 */
	class AstNodeStructDeclaration : public IAstNode {
	public:
		AstNodeStructDeclaration(const std::string& name, bool isPublic = false)
			: mName(name), mIsPublic(isPublic), mParent(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeStructDeclaration() {
			for (auto* field : mFields) {
				delete field;
			}
		}

		IAstNode::Type type() const override {
			return Type::STRUCT_DECLARATION;
		}

		size_t childCount() const override {
			return mFields.size();
		}

		IAstNode* child(size_t index) const override {
			if (index < mFields.size()) {
				return mFields[index];
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

		bool isPublic() const {
			return mIsPublic;
		}

		void addField(AstNodeStructField* field) {
			mFields.push_back(field);
		}

		const std::vector<AstNodeStructField*>& fields() const {
			return mFields;
		}

	private:
		std::string mName;
		bool mIsPublic;
		std::vector<AstNodeStructField*> mFields;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};

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
			: mStructName(structName), mParent(nullptr), mLine(0), mColumn(0) {
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

	private:
		std::string mStructName;
		std::vector<StructFieldInit> mFieldInits;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};

	/**
	 * @brief AST node representing field access
	 *
	 * Example: v @x
	 * Accesses field 'x' from struct in local variable 'v'
	 */
	class AstNodeFieldAccess : public IAstNode {
	public:
		AstNodeFieldAccess(const std::string& varName, const std::string& fieldName)
			: mVarName(varName), mFieldName(fieldName), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::FIELD_ACCESS;
		}

		size_t childCount() const override {
			return 0;
		}

		IAstNode* child(size_t) const override {
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

		const std::string& varName() const {
			return mVarName;
		}

		const std::string& fieldName() const {
			return mFieldName;
		}

	private:
		std::string mVarName;
		std::string mFieldName;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
