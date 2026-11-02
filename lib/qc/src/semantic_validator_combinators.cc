// Type checking for the combinators: dip, keep, bi, tri, bi_star, tri_star, both, times, each,
// map, filter, fold.
//
// A combinator is checked by doing what it does to the modelled stack. A function value it runs
// is either written in place -- a `fn { }` immediately before it, whose body is then walked
// right here on the real stack, with the real types -- or any other function value, which is
// applied by its type exactly as `call` applies it.

#include <string>
#include <unordered_map>
#include <vector>

#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_anonymous_function.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/instructions.h>
#include <quadrate/qc/semantic_validator.h>

namespace Qd {

	namespace {
		// A function value a combinator takes, and the literal that wrote it when that is the
		// node right before the combinator
		struct Quotation {
			StackValueType type;
			std::string typeName;
			AstNodeAnonymousFunction* literal;
		};

		// What a combinator needs, for its underflow diagnostic
		const char* combinatorOperands(const std::string& name) {
			if (name == "bi") {
				return "a value and two function values";
			}
			if (name == "tri") {
				return "a value and three function values";
			}
			if (name == "bi_star") {
				return "two values and two function values";
			}
			if (name == "tri_star") {
				return "three values and three function values";
			}
			if (name == "both" || name == "dip2" || name == "keep2") {
				return "two values and a function value";
			}
			if (name == "times") {
				return "a count and a function value";
			}
			if (name == "fold") {
				return "an array or a string, an initial value and a function value";
			}
			if (name == "each" || name == "any" || name == "all" || name == "find" || name == "index") {
				return "an array or a string and a function value";
			}
			if (name == "scan") {
				return "an array or a string, a start position and a function value";
			}
			if (name == "map" || name == "filter") {
				return "an array and a function value";
			}
			return "a value and a function value";
		}

		const char* stackTypeName(StackValueType type) {
			switch (type) {
			case StackValueType::INT:
				return "i64";
			case StackValueType::FLOAT:
				return "f64";
			case StackValueType::D128:
				return "d128";
			case StackValueType::STRING:
				return "str";
			case StackValueType::PTR:
				return "ptr";
			default:
				return "_";
			}
		}

		// The type a value has, spelled as a type: its struct, array or function type when it
		// has one, `_` when it is not known
		std::string typeNameOf(StackValueType type, const std::string& structType) {
			if (!structType.empty() && (type == StackValueType::PTR || type == StackValueType::TYPEVAR)) {
				return structType;
			}
			return stackTypeName(type);
		}

		bool isOpen(StackValueType type) {
			return type == StackValueType::UNKNOWN || type == StackValueType::ANY || type == StackValueType::TYPEVAR;
		}
	}

	void SemanticValidator::typeCheckCombinator(AstNodeInstruction* instr, IAstNode* block, size_t index,
			std::vector<StackValueType>& typeStack, std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<std::string>& structTypeStack) {
		const std::string& name = instr->name();
		const Combinator* combinator = findCombinator(name);
		const std::string prefix = "Type error in '" + name + "': ";

		if (instr->abortOnError() || instr->propagateOnError()) {
			reportError(instr, ("Cannot use '" + std::string(instr->abortOnError() ? "!" : "?") + "' operator on '" +
									   name + "': it is not fallible")
									   .c_str());
		}
		if (typeStack.size() < combinator->functionValues + combinator->values) {
			reportError(instr, (prefix + "Stack underflow (requires " + combinatorOperands(name) + ")").c_str());
			mHasUnpredictableStack = true;
			return;
		}
		while (structTypeStack.size() < typeStack.size()) {
			structTypeStack.insert(structTypeStack.begin(), "");
		}

		// The function values, bottom first. Those written in place are the nodes right before
		// this one, the last function value first.
		std::vector<Quotation> quotations(combinator->functionValues);
		bool inPlace = true;
		for (size_t k = 0; k < combinator->functionValues; k++) {
			Quotation& q = quotations[combinator->functionValues - 1 - k];
			q.type = typeStack.back();
			q.typeName = structTypeStack.back();
			typeStack.pop_back();
			structTypeStack.pop_back();
			IAstNode* before = (block && index > k) ? block->child(index - 1 - k) : nullptr;
			inPlace = inPlace && before && before->type() == IAstNode::Type::ANONYMOUS_FUNCTION;
			q.literal = inPlace ? static_cast<AstNodeAnonymousFunction*>(before) : nullptr;
			if (q.type != StackValueType::PTR && !isOpen(q.type)) {
				reportError(instr, (prefix + "expects a function value, got " + stackTypeName(q.type)).c_str());
				mHasUnpredictableStack = true;
				return;
			}
			// A combinator runs its function value as a plain call, with nowhere for a failure
			// to go. The function value pushed last is the one a pending signature describes.
			const bool fallible = q.literal
										  ? q.literal->throws()
										  : (k == 0 && mPendingFnSignature.has_value() && mPendingFnSignature->throws);
			if (fallible) {
				reportErrorWithHint(instr,
						(prefix + "the function value is fallible, and a combinator has nowhere to "
								  "send its failure")
								.c_str(),
						"handle the failure inside the function value, or call it with 'call' and handle it there");
			}
		}
		mPendingFnSignature.reset();

		// Run one function value on `stack`
		auto run = [&](const Quotation& q, std::vector<StackValueType>& stack, std::vector<std::string>& structs) {
			if (q.literal && !q.literal->hasSignature()) {
				// Its body, in place: inside the enclosing function's stack, under the rules of
				// its own body -- not fallible, no loop to break out of, nothing to return.
				const bool savedFallible = mCurrentFunctionFallible;
				const size_t savedOutputCount = mCurrentFunctionOutputCount;
				const bool savedInLoopBody = mInLoopBody;
				auto savedLoopJumps = mLoopJumps;
				const IAstNode* savedFunctionBody = mCurrentFunctionBody;
				auto savedStructTypes = mLocalVariableStructTypes;
				auto savedFnSignatures = mLocalVariableFnSignatures;
				mCurrentFunctionFallible = false;
				mCurrentFunctionOutputCount = 0;
				mInLoopBody = false;
				mLoopJumps.clear();
				mCurrentFunctionBody = q.literal->body();
				auto bodyLocals = localVariables;
				typeCheckBlock(q.literal->body(), stack, bodyLocals, structs);
				mCurrentFunctionFallible = savedFallible;
				mCurrentFunctionOutputCount = savedOutputCount;
				mInLoopBody = savedInLoopBody;
				mLoopJumps = savedLoopJumps;
				mCurrentFunctionBody = savedFunctionBody;
				mLocalVariableStructTypes = savedStructTypes;
				mLocalVariableFnSignatures = savedFnSignatures;
				return;
			}
			stack.push_back(q.type);
			structs.push_back(q.typeName);
			if (!q.typeName.empty()) {
				mPendingFnSignature = parseFnTypeString(q.typeName);
			}
			checkBuiltinCall(instr, stack, structs, true, name.c_str());
			mPendingFnSignature.reset();
		};

		auto pop = [&](StackValueType& type, std::string& structType) {
			type = typeStack.back();
			structType = structTypeStack.back();
			typeStack.pop_back();
			structTypeStack.pop_back();
		};
		auto push = [&](StackValueType type, const std::string& structType) {
			typeStack.push_back(type);
			structTypeStack.push_back(structType);
		};

		// The element type of an array value, or unknown; with `chars`, a string's too, whose
		// elements are its characters
		auto elementOf = [&](StackValueType type, const std::string& structType, StackValueType& elemType,
								 std::string& elemStruct, bool chars = false) {
			elemType = StackValueType::UNKNOWN;
			elemStruct.clear();
			if (chars && type == StackValueType::STRING) {
				elemType = StackValueType::INT;
				return true;
			}
			if (structType.size() > 2 && structType.compare(0, 2, "[]") == 0) {
				const std::string elem = structType.substr(2);
				elemType = stringToStackValueType(elem);
				if (elemType == StackValueType::PTR) {
					elemStruct = elem;
				}
				return true;
			}
			if (type == StackValueType::PTR || isOpen(type)) {
				return true;
			}
			reportError(instr, (prefix + (chars ? "expects an array or a string, got " : "expects an array, got ") +
									   typeNameOf(type, structType))
									   .c_str());
			return false;
		};

		// A function value run on the stack it leaves, any number of times, has to leave it as it
		// found it
		auto checkUnchanged = [&](const std::vector<StackValueType>& before,
									  const std::vector<std::string>& beforeStructs) {
			if (mHasUnpredictableStack) {
				return;
			}
			// Whatever it did, the stack after is the one it found: that is what the combinator
			// promises, and an error about the depth after it would only repeat this one
			if (typeStack.size() != before.size()) {
				const bool deeper = typeStack.size() > before.size();
				const size_t by = deeper ? typeStack.size() - before.size() : before.size() - typeStack.size();
				reportError(instr, (prefix +
										   "the function value must leave the stack as it found it, since it runs "
										   "again on what it leaves, but it leaves it " +
										   std::to_string(by) + (deeper ? " deeper" : " shallower"))
										   .c_str());
				typeStack = before;
				structTypeStack = beforeStructs;
				return;
			}
			for (size_t j = 0; j < before.size(); j++) {
				if (isOpen(before[j]) || isOpen(typeStack[j]) || before[j] == typeStack[j]) {
					continue;
				}
				reportError(instr, (prefix +
										   "the function value must leave the stack as it found it, since it runs "
										   "again on what it leaves, but it turns a " +
										   typeNameOf(before[j], beforeStructs[j]) + " into a " +
										   typeNameOf(typeStack[j], structTypeStack[j]))
										   .c_str());
				typeStack = before;
				structTypeStack = beforeStructs;
				return;
			}
		};

		// Run a function value on a stack of its own, holding exactly `inputs`, where it must
		// leave exactly one value
		auto runAlone = [&](const Quotation& q, const std::vector<StackValueType>& inputs,
								const std::vector<std::string>& inputStructs, StackValueType& result,
								std::string& resultStruct) {
			std::vector<StackValueType> stack = inputs;
			std::vector<std::string> structs = inputStructs;
			const std::string savedFloor = mFloorStackVariable;
			const bool savedUnpredictable = mHasUnpredictableStack;
			mFloorStackVariable.clear();
			mHasUnpredictableStack = false;
			const size_t errorsBefore = mErrorCount;
			run(q, stack, structs);
			const bool unpredictable = mHasUnpredictableStack;
			mFloorStackVariable = savedFloor;
			mHasUnpredictableStack = savedUnpredictable;
			result = StackValueType::UNKNOWN;
			resultStruct.clear();
			// An error in the body has said what is wrong with it already
			if (unpredictable || mErrorCount != errorsBefore) {
				return;
			}
			if (stack.size() != 1) {
				const std::string takes = inputs.size() == 1 ? "one value" : std::to_string(inputs.size()) + " values";
				reportError(instr, (prefix + "the function value must take " + takes +
										   " and leave one, but it leaves " + std::to_string(stack.size()))
										   .c_str());
				return;
			}
			result = stack.back();
			resultStruct = structs.back();
		};

		StackValueType xType;
		std::string xStruct;
		if (name == "dip") {
			pop(xType, xStruct);
			run(quotations[0], typeStack, structTypeStack);
			push(xType, xStruct);
		} else if (name == "keep") {
			pop(xType, xStruct);
			push(xType, xStruct);
			run(quotations[0], typeStack, structTypeStack);
			push(xType, xStruct);
		} else if (name == "dip2" || name == "keep2") {
			StackValueType yType;
			std::string yStruct;
			pop(yType, yStruct);
			pop(xType, xStruct);
			if (name == "keep2") {
				push(xType, xStruct);
				push(yType, yStruct);
			}
			run(quotations[0], typeStack, structTypeStack);
			push(xType, xStruct);
			push(yType, yStruct);
		} else if (name == "bi" || name == "tri") {
			pop(xType, xStruct);
			for (const Quotation& q : quotations) {
				push(xType, xStruct);
				run(q, typeStack, structTypeStack);
			}
		} else if (name == "bi_star" || name == "tri_star") {
			// One function value for each value, the first under the rest: Factor's bi* and tri*
			std::vector<StackValueType> types(quotations.size());
			std::vector<std::string> structs(quotations.size());
			for (size_t k = quotations.size(); k > 0; k--) {
				pop(types[k - 1], structs[k - 1]);
			}
			for (size_t k = 0; k < quotations.size(); k++) {
				push(types[k], structs[k]);
				run(quotations[k], typeStack, structTypeStack);
			}
		} else if (name == "both") {
			StackValueType yType;
			std::string yStruct;
			pop(yType, yStruct);
			pop(xType, xStruct);
			push(xType, xStruct);
			run(quotations[0], typeStack, structTypeStack);
			push(yType, yStruct);
			run(quotations[0], typeStack, structTypeStack);
		} else if (name == "times") {
			pop(xType, xStruct);
			if (xType != StackValueType::INT && !isOpen(xType)) {
				reportError(instr, (prefix + "expects an i64 count, got " + typeNameOf(xType, xStruct)).c_str());
			}
			const auto before = typeStack;
			const auto beforeStructs = structTypeStack;
			run(quotations[0], typeStack, structTypeStack);
			checkUnchanged(before, beforeStructs);
		} else if (name == "each") {
			pop(xType, xStruct);
			StackValueType elemType;
			std::string elemStruct;
			if (!elementOf(xType, xStruct, elemType, elemStruct, true)) {
				return;
			}
			const auto before = typeStack;
			const auto beforeStructs = structTypeStack;
			push(elemType, elemStruct);
			run(quotations[0], typeStack, structTypeStack);
			checkUnchanged(before, beforeStructs);
		} else if (name == "map" || name == "filter") {
			pop(xType, xStruct);
			StackValueType elemType;
			std::string elemStruct;
			if (!elementOf(xType, xStruct, elemType, elemStruct)) {
				return;
			}
			StackValueType result;
			std::string resultStruct;
			runAlone(quotations[0], {elemType}, {elemStruct}, result, resultStruct);
			if (name == "map") {
				push(StackValueType::PTR, "[]" + typeNameOf(result, resultStruct));
			} else {
				if (result != StackValueType::INT && !isOpen(result)) {
					reportError(instr, (prefix + "the function value must leave a flag (i64), but it leaves a " +
											   typeNameOf(result, resultStruct))
											   .c_str());
				}
				push(StackValueType::PTR, xStruct.empty() ? "[]" + typeNameOf(elemType, elemStruct) : xStruct);
			}
		} else if (name == "any" || name == "all" || name == "find" || name == "index" || name == "scan") {
			// The searching words also walk a string, one character (codepoint, i64) at a time
			if (name == "scan") {
				StackValueType startType;
				std::string startStruct;
				pop(startType, startStruct);
				if (startType != StackValueType::INT && !isOpen(startType)) {
					reportError(instr,
							(prefix + "the start position must be an i64, got " + typeNameOf(startType, startStruct))
									.c_str());
					return;
				}
			}
			pop(xType, xStruct);
			StackValueType elemType = StackValueType::INT;
			std::string elemStruct;
			const bool chars = xType == StackValueType::STRING;
			if (!chars && xType != StackValueType::PTR && !isOpen(xType)) {
				reportError(
						instr, (prefix + "expects an array or a string, got " + typeNameOf(xType, xStruct)).c_str());
				return;
			}
			if (!chars) {
				elementOf(xType, xStruct, elemType, elemStruct);
			}
			StackValueType result;
			std::string resultStruct;
			runAlone(quotations[0], {elemType}, {elemStruct}, result, resultStruct);
			if (result != StackValueType::INT && !isOpen(result)) {
				reportError(instr, (prefix + "the function value must leave a flag (i64), but it leaves a " +
										   typeNameOf(result, resultStruct))
										   .c_str());
			}
			// Tells lowering, which has no types of its own, what it walks: the element type of an
			// array, which `find` also takes its not-found zero from, or nothing when the subject
			// is a string or not known to be an array, which is read through '__size' and '__at'.
			instr->setResolvedType(chars || isOpen(xType) ? "" : typeNameOf(elemType, elemStruct));
			if (name == "find") {
				push(elemType, elemStruct);
			}
			push(StackValueType::INT, "");
		} else if (name == "fold") {
			StackValueType accType;
			std::string accStruct;
			pop(accType, accStruct);
			pop(xType, xStruct);
			StackValueType elemType;
			std::string elemStruct;
			if (!elementOf(xType, xStruct, elemType, elemStruct, true)) {
				return;
			}
			StackValueType result;
			std::string resultStruct;
			runAlone(quotations[0], {accType, elemType}, {accStruct, elemStruct}, result, resultStruct);
			if (!isOpen(accType) && !isOpen(result) && result != accType) {
				reportError(instr, (prefix + "the function value must leave the accumulator's type, " +
										   typeNameOf(accType, accStruct) + ", but it leaves a " +
										   typeNameOf(result, resultStruct))
										   .c_str());
			}
			if (isOpen(accType)) {
				push(result, resultStruct);
			} else {
				push(accType, accStruct);
			}
		}
	}

} // namespace Qd
