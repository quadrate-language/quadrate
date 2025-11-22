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
	 * @brief AST node representing struct construction
	 *
	 * Example: 1.0 2.0 Vec2
	 * Stack-based construction: values are on the stack, struct name consumes them
	 */
	class AstNodeStructConstruction : public IAstNode {
	public:
		AstNodeStructConstruction(const std::string& structName)
			: mStructName(structName), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::STRUCT_CONSTRUCTION;
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

		const std::string& structName() const {
			return mStructName;
		}

	private:
		std::string mStructName;
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
