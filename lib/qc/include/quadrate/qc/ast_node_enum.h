#ifndef QD_QC_AST_NODE_ENUM_H
#define QD_QC_AST_NODE_ENUM_H

#include "ast_node.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Qd {
	class AstNodeEnumDeclaration : public IAstNode {
	public:
		struct Variant {
			std::string name;
			int64_t value;
		};

		AstNodeEnumDeclaration(const std::string& name, bool isPublic = false)
			: mName(name), mIsPublic(isPublic), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::ENUM_DECLARATION;
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

		bool isPublic() const {
			return mIsPublic;
		}

		void addVariant(const std::string& name, int64_t value) {
			mVariants.push_back({name, value});
		}

		const std::vector<Variant>& variants() const {
			return mVariants;
		}

	private:
		std::string mName;
		bool mIsPublic;
		std::vector<Variant> mVariants;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
