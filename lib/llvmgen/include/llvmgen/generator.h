#ifndef LLVMGEN_GENERATOR_H
#define LLVMGEN_GENERATOR_H

#include <memory>
#include <string>

namespace llvm {
class LLVMContext;
class Module;
template<typename T, typename Inserter> class IRBuilder;
class Function;
class Value;
class Type;
} // namespace llvm

namespace Qd {

class IAstNode;

class LlvmGenerator {
  public:
	LlvmGenerator();
	~LlvmGenerator();

	// Generate LLVM IR from AST
	bool generate(IAstNode* root, const std::string& moduleName);

	// Add a module AST to be compiled with the main program
	void addModuleAST(const std::string& moduleName, IAstNode* moduleRoot);

	// Write LLVM IR to file (.ll)
	bool writeIR(const std::string& filename);

	// Write object file (.o)
	bool writeObject(const std::string& filename);

	// Write executable
	bool writeExecutable(const std::string& filename);

	// Get generated IR as string (for debugging)
	std::string getIRString() const;

  private:
	class Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace Qd

#endif
