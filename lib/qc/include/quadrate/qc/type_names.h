/**
 * @file type_names.h
 * @brief The built-in type names, in one place.
 *
 * The compiler, the language server, the REPL and quaduses all need to know which names
 * are types. Each used to keep its own list, and the lists disagreed: `bool` was a type in
 * a struct field and nowhere else, `f32` and `string` only inside `cast<...>`.
 */

#pragma once

#include <string_view>

namespace Qd {

	struct BuiltinType {
		std::string_view name;
		std::string_view description;
	};

	struct BuiltinTypeAlias {
		std::string_view name;
		std::string_view target;
		std::string_view description;
	};

	/// The types a value has on the stack, and `any`, which accepts each of them
	inline constexpr BuiltinType PRIMITIVE_TYPES[] = {
			{"i64", "64-bit signed integer"},
			{"f64", "64-bit floating point"},
			{"d128", "Decimal floating point, 34 digits"},
			{"str", "String"},
			{"ptr", "Pointer"},
			{"any", "Any type, carried with the value"},
	};

	/// Widths for struct fields and memory. On the stack each is an i64: a load extends, a
	/// store truncates.
	inline constexpr BuiltinType SIZED_INT_TYPES[] = {
			{"i32", "32-bit signed integer (field/mem only)"},
			{"i16", "16-bit signed integer (field/mem only)"},
			{"i8", "8-bit signed integer (field/mem only)"},
			{"u64", "64-bit unsigned integer (field/mem only)"},
			{"u32", "32-bit unsigned integer (field/mem only)"},
			{"u16", "16-bit unsigned integer (field/mem only)"},
			{"u8", "8-bit unsigned integer (field/mem only)"},
	};

	/// Names that mean another type wherever a type is written, as a `type` declaration would
	inline constexpr BuiltinTypeAlias BUILTIN_TYPE_ALIASES[] = {
			{"bool", "i64", "Alias for i64: 0 is false, any other value true"},
	};

	inline bool isPrimitiveTypeName(std::string_view name) {
		for (const auto& type : PRIMITIVE_TYPES) {
			if (type.name == name) {
				return true;
			}
		}
		return false;
	}

	inline bool isSizedIntTypeName(std::string_view name) {
		for (const auto& type : SIZED_INT_TYPES) {
			if (type.name == name) {
				return true;
			}
		}
		return false;
	}

	/// The type a built-in alias stands for, or empty when `name` is not one
	inline std::string_view builtinTypeAliasTarget(std::string_view name) {
		for (const auto& alias : BUILTIN_TYPE_ALIASES) {
			if (alias.name == name) {
				return alias.target;
			}
		}
		return {};
	}

	/// Whether `name` is a type with no declaration: a primitive, a sized integer or a built-in alias
	inline bool isBuiltinTypeName(std::string_view name) {
		return isPrimitiveTypeName(name) || isSizedIntTypeName(name) || !builtinTypeAliasTarget(name).empty();
	}

} // namespace Qd
