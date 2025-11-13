# Quadrate

## Project Overview

Quadrate is a stack-based programming language inspired by Forth. It compiles to C, with GCC as a backend, and aims to produce optimized native executables. The language features a clean syntax, strong type checking, a module system, and a comprehensive standard library.

The project is built using the Meson build system, with a Makefile providing convenient targets for common development tasks. The codebase is written in C and C++.

The project is structured into the following main directories:

*   `bin/`: Contains the source code for the Quadrate compiler (`quadc`), formatter (`quadfmt`), and Language Server Protocol (`quadlsp`) server.
*   `lib/`: Contains the core libraries of the Quadrate language, including the compiler frontend (`qc`), C code generator (`cgen`), runtime (`qdrt`), embedding API (`qd`), and the standard library (`stdqd`).
*   `tests/`: Contains the test suites for the project.
*   `examples/`: Contains example Quadrate programs.
*   `editors/`: Contains editor integrations, such as for Neovim.

## Building and Running

The project uses a `Makefile` that wraps Meson commands.

### Build Commands

*   **Debug build (default):**
    ```bash
    make
    ```
*   **Release build:**
    ```bash
    make release
    ```

### Running Tests

*   **Run all tests:**
    ```bash
    make tests
    ```
*   **Run tests with Valgrind:**
    ```bash
    make valgrind
    ```

### Running Examples

*   **Build examples:**
    ```bash
    make examples
    ```
*   **Run a specific example:**
    ```bash
    ./dist/examples/hello-world
    ```

### Installation

*   **Install the project:**
    ```bash
    sudo make install
    ```
*   **Uninstall the project:**
    ```bash
    sudo make uninstall
    ```

## Development Conventions

*   **Code Formatting:** The project uses `clang-format` for code formatting. To format the code, run:
    ```bash
    make format
    ```
*   **Compiler:** The project prefers to use `clang` and `clang++` as the C and C++ compilers, respectively. This is set in the `Makefile`.
*   **Contributing:** Contributions are welcome. Issues should be reported on the project's todo tracker, and patches should be sent to the mailing list. More details can be found in the `CONTRIBUTING.md` file.
