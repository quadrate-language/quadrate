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

		/**
		 * @brief A comment written inside the enum body.
		 *
		 * Enum variants are plain values rather than AST nodes and childCount() is 0, so these
		 * are kept in their own list for the formatter. Without them a `// note` on a variant
		 * line was parsed and dropped, and `quadfmt -w` silently deleted it.
		 */
		struct BodyComment {
			std::string text;		///< comment body, without the // or /* */ delimiters
			bool isBlock;			///< true for a block comment
			size_t afterVariantIdx; ///< number of variants parsed before this comment
			bool trailing;			///< true if it sits on the same line as variant afterVariantIdx-1
		};

		void addBodyComment(const std::string& text, bool isBlock, size_t afterVariantIdx, bool trailing) {
			mBodyComments.push_back(BodyComment{text, isBlock, afterVariantIdx, trailing});
		}

		const std::vector<BodyComment>& bodyComments() const {
			return mBodyComments;
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
		std::vector<BodyComment> mBodyComments;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
