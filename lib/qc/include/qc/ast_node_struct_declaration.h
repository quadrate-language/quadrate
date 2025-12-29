#ifndef QD_QC_AST_NODE_STRUCT_DECLARATION_H
#define QD_QC_AST_NODE_STRUCT_DECLARATION_H

#include "ast_node.h"
#include "ast_node_struct_field.h"
#include <string>
#include <vector>

namespace Qd {
	/**
	 * @brief AST node representing a struct declaration
	 *
	 * Example: pub struct Vec2 { x:f64 y:f64 }
	 * Generic: struct Pair<T, U> { first:T second:U }
	 */
	class AstNodeStructDeclaration : public IAstNode {
	public:
		AstNodeStructDeclaration(const std::string& name, bool isPublic = false)
			: mName(name), mIsPublic(isPublic), mParent(nullptr), mLine(0), mColumn(0) {
		}

		AstNodeStructDeclaration(const std::string& name, const std::vector<std::string>& typeParams,
				bool isPublic = false)
			: mName(name), mTypeParams(typeParams), mIsPublic(isPublic), mParent(nullptr),
			  mLine(0), mColumn(0) {
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

		const std::vector<std::string>& typeParams() const {
			return mTypeParams;
		}

		bool isGeneric() const {
			return !mTypeParams.empty();
		}

		void addTypeParam(const std::string& typeParam) {
			mTypeParams.push_back(typeParam);
		}

	private:
		std::string mName;
		std::vector<std::string> mTypeParams;
		bool mIsPublic;
		std::vector<AstNodeStructField*> mFields;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
