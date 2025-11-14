/**
 * @file generator.h
 * @brief LLVM code generation for Quadrate
 *
 * Provides code generation from Quadrate AST to LLVM IR, object files, and executables.
 */

#ifndef LLVMGEN_GENERATOR_H
#define LLVMGEN_GENERATOR_H

#include <memory>
#include <string>

// Forward declarations for LLVM types
namespace llvm {
	class LLVMContext;
	class Module;
	template <typename T, typename Inserter>
	class IRBuilder;
	class Function;
	class Value;
	class Type;
} // namespace llvm

namespace Qd {

	class IAstNode;

	/**
	 * @brief LLVM code generator for Quadrate
	 *
	 * The LlvmGenerator class translates Quadrate Abstract Syntax Trees (AST)
	 * into LLVM Intermediate Representation (IR), and subsequently into object
	 * files or executables.
	 *
	 * @par Compilation Pipeline:
	 * 1. Parse Quadrate source to AST (using Qd::Ast)
	 * 2. Generate LLVM IR from AST (using generate())
	 * 3. Optionally add additional modules (using addModuleAST())
	 * 4. Output IR, object file, or executable
	 *
	 * @par Example Usage:
	 * @code
	 * Qd::Ast ast;
	 * Qd::IAstNode* root = ast.generate(source, false, "main.qd");
	 *
	 * Qd::LlvmGenerator gen;
	 * if (gen.generate(root, "main")) {
	 *     gen.writeExecutable("program");
	 * }
	 * @endcode
	 *
	 * @note Uses the Pimpl idiom to hide LLVM implementation details
	 */
	class LlvmGenerator {
	public:
		/**
		 * @brief Construct a new LLVM generator
		 *
		 * Initializes LLVM context and prepares for code generation.
		 */
		LlvmGenerator();

		/**
		 * @brief Destructor - cleans up LLVM resources
		 */
		~LlvmGenerator();

		/**
		 * @brief Generate LLVM IR from a Quadrate AST
		 *
		 * Translates the provided AST into LLVM Intermediate Representation.
		 * This is the main compilation step that converts Quadrate code into
		 * LLVM IR.
		 *
		 * @param root Root node of the Quadrate AST (must not be NULL)
		 * @param moduleName Name for the generated LLVM module
		 * @return true if generation succeeded, false on error
		 *
		 * @note This must be called before any write operations
		 * @note Errors are reported to stderr
		 */
		bool generate(IAstNode* root, const std::string& moduleName);

		/**
		 * @brief Add an additional module to be compiled
		 *
		 * Adds another Quadrate module's AST to be included in the compilation.
		 * This allows linking multiple Quadrate modules into a single program.
		 *
		 * @param moduleName Name of the module
		 * @param moduleRoot Root node of the module's AST
		 *
		 * @note Must be called after generate() but before write operations
		 */
		void addModuleAST(const std::string& moduleName, IAstNode* moduleRoot);

		/**
		 * @brief Write LLVM IR to a text file
		 *
		 * Outputs the generated LLVM IR in human-readable text format (.ll file).
		 * Useful for debugging and inspection.
		 *
		 * @param filename Output filename (typically with .ll extension)
		 * @return true if write succeeded, false on error
		 *
		 * @note Must call generate() first
		 */
		bool writeIR(const std::string& filename);

		/**
		 * @brief Write object file
		 *
		 * Compiles the LLVM IR to a native object file (.o) for the target
		 * platform. The object file can be linked with other object files.
		 *
		 * @param filename Output filename (typically with .o extension)
		 * @return true if write succeeded, false on error
		 *
		 * @note Must call generate() first
		 */
		bool writeObject(const std::string& filename);

		/**
		 * @brief Write executable file
		 *
		 * Compiles and links the LLVM IR into a standalone executable.
		 * Automatically links with the Quadrate runtime library.
		 *
		 * @param filename Output filename (executable name)
		 * @return true if write succeeded, false on error
		 *
		 * @note Must call generate() first
		 * @note Requires libqdrt to be available for linking
		 */
		bool writeExecutable(const std::string& filename);

		/**
		 * @brief Get generated IR as a string
		 *
		 * Returns the LLVM IR as a string for debugging or inspection purposes.
		 *
		 * @return String containing the LLVM IR in text format
		 *
		 * @note Must call generate() first, otherwise returns empty string
		 */
		std::string getIRString() const;

	private:
		/**
		 * @brief Private implementation (Pimpl idiom)
		 *
		 * Hides LLVM implementation details from the public interface.
		 */
		class Impl;
		std::unique_ptr<Impl> impl;  ///< Pointer to implementation
	};

} // namespace Qd

#endif // LLVMGEN_GENERATOR_H
