#!/usr/bin/env python3
"""
Generate mkdocs markdown from documented Quadrate module files.

Parses /// doc comments with @param, @return, @example, @error tags
and generates structured markdown documentation.

Usage:
    python scripts/gen_docs.py                    # Generate all stdlib docs
    python scripts/gen_docs.py lib/stdmathqd/qd/math/module.qd  # Single file
"""

import sys
import re
import os
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class DocItem:
    """A documented item (function, constant, or struct)."""
    name: str
    kind: str  # "fn", "const", "struct"
    description: list[str] = field(default_factory=list)
    params: list[tuple[str, str, str]] = field(default_factory=list)  # (name, type, desc)
    returns: list[tuple[str, str, str]] = field(default_factory=list)  # (name, type, desc)
    examples: list[str] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)
    signature: str = ""
    value: str = ""  # For constants
    is_public: bool = True
    is_failable: bool = False


@dataclass
class Module:
    """A documented module."""
    name: str
    description: list[str] = field(default_factory=list)
    items: list[DocItem] = field(default_factory=list)


def parse_doc_comment(lines: list[str]) -> tuple[list[str], list[tuple[str, str, str]],
                                                   list[tuple[str, str, str]], list[str], list[str]]:
    """Parse doc comment lines into description, params, returns, examples, errors."""
    description = []
    params = []
    returns = []
    examples = []
    errors = []

    for line in lines:
        line = line.strip()
        if not line:
            continue

        if line.startswith("@param "):
            # @param name type Description
            match = re.match(r"@param\s+(\w+)\s+(\w+)\s*(.*)", line)
            if match:
                params.append((match.group(1), match.group(2), match.group(3).strip()))
        elif line.startswith("@return "):
            # @return name type Description
            match = re.match(r"@return\s+(\w+)\s+(\w+)\s*(.*)", line)
            if match:
                returns.append((match.group(1), match.group(2), match.group(3).strip()))
        elif line.startswith("@example "):
            examples.append(line[9:].strip())
        elif line.startswith("@error "):
            errors.append(line[7:].strip())
        else:
            description.append(line)

    return description, params, returns, examples, errors


def parse_signature(line: str) -> tuple[str, str, bool]:
    """Parse function signature, return (name, signature, is_failable)."""
    # pub fn name(params -- returns)!
    match = re.match(r"(?:pub\s+)?fn\s+(\w+)\s*\(([^)]*)\)\s*(!?)", line)
    if match:
        name = match.group(1)
        sig = match.group(2).strip()
        is_failable = match.group(3) == "!"
        return name, f"( {sig} )" if sig else "( -- )", is_failable
    return "", "", False


def parse_const(line: str) -> tuple[str, str]:
    """Parse constant definition, return (name, value)."""
    # pub const Name = value
    match = re.match(r"(?:pub\s+)?const\s+(\w+)\s*=\s*(.+)", line)
    if match:
        return match.group(1), match.group(2).strip()
    return "", ""


def parse_struct(line: str) -> str:
    """Parse struct definition, return name."""
    # pub struct Name {
    match = re.match(r"(?:pub\s+)?struct\s+(\w+)\s*\{?", line)
    if match:
        return match.group(1)
    return ""


def parse_module(filepath: str) -> Module:
    """Parse a .qd module file and extract documentation."""
    with open(filepath, "r") as f:
        content = f.read()

    # Get module name from path
    parts = Path(filepath).parts
    # Find 'qd' directory and get next part as module name
    try:
        qd_idx = parts.index("qd")
        module_name = parts[qd_idx + 1]
    except (ValueError, IndexError):
        module_name = Path(filepath).stem

    module = Module(name=module_name)
    lines = content.split("\n")

    i = 0
    doc_buffer = []
    module_doc_found = False

    while i < len(lines):
        line = lines[i].strip()

        # Collect doc comments
        if line.startswith("///"):
            doc_text = line[3:].strip()
            doc_buffer.append(doc_text)
            i += 1
            continue

        # Skip regular comments
        if line.startswith("//") or line.startswith("/*"):
            i += 1
            continue

        # Skip empty lines (but keep doc buffer)
        if not line:
            # If we have doc comments and haven't found module doc yet,
            # this might be the module description
            if doc_buffer and not module_doc_found and not module.description:
                # Check if next non-empty line is not a declaration
                j = i + 1
                while j < len(lines) and not lines[j].strip():
                    j += 1
                if j < len(lines):
                    next_line = lines[j].strip()
                    if next_line.startswith("///") or next_line.startswith("pub const") or \
                       next_line.startswith("const") or next_line.startswith("import"):
                        module.description = doc_buffer
                        module_doc_found = True
                        doc_buffer = []
            i += 1
            continue

        # Parse declarations
        is_public = line.startswith("pub ")

        # Function
        if "fn " in line and "(" in line:
            name, sig, is_failable = parse_signature(line)
            if name and is_public:
                desc, params, returns, examples, errors = parse_doc_comment(doc_buffer)
                item = DocItem(
                    name=name,
                    kind="fn",
                    description=desc,
                    params=params,
                    returns=returns,
                    examples=examples,
                    errors=errors,
                    signature=sig,
                    is_public=is_public,
                    is_failable=is_failable
                )
                module.items.append(item)
            doc_buffer = []

        # Constant
        elif "const " in line and "=" in line:
            name, value = parse_const(line)
            if name and is_public:
                desc, _, _, examples, _ = parse_doc_comment(doc_buffer)
                item = DocItem(
                    name=name,
                    kind="const",
                    description=desc,
                    value=value,
                    examples=examples,
                    is_public=is_public
                )
                module.items.append(item)
            doc_buffer = []

        # Struct
        elif "struct " in line:
            name = parse_struct(line)
            if name and is_public:
                desc, _, _, _, _ = parse_doc_comment(doc_buffer)
                item = DocItem(
                    name=name,
                    kind="struct",
                    description=desc,
                    is_public=is_public
                )
                module.items.append(item)
            doc_buffer = []

        # Import statement - module doc comes before this
        elif line.startswith("import "):
            if doc_buffer and not module.description:
                module.description = doc_buffer
                module_doc_found = True
            doc_buffer = []

        # Use statement
        elif line.startswith("use "):
            if doc_buffer and not module.description:
                module.description = doc_buffer
                module_doc_found = True
            doc_buffer = []

        # Other lines - clear buffer if not a declaration
        else:
            doc_buffer = []

        i += 1

    return module


def generate_markdown(module: Module) -> str:
    """Generate mkdocs markdown from a parsed module."""
    lines = []

    # Title
    lines.append(f"# {module.name}")
    lines.append("")

    # Module description
    if module.description:
        for desc in module.description:
            lines.append(desc)
        lines.append("")

    # Separate items by kind
    constants = [i for i in module.items if i.kind == "const"]
    structs = [i for i in module.items if i.kind == "struct"]
    functions = [i for i in module.items if i.kind == "fn"]

    # Constants table
    if constants:
        lines.append("## Constants")
        lines.append("")
        lines.append("| Name | Value | Description |")
        lines.append("|------|-------|-------------|")
        for c in constants:
            desc = " ".join(c.description) if c.description else ""
            lines.append(f"| `{c.name}` | `{c.value}` | {desc} |")
        lines.append("")

    # Structs
    if structs:
        lines.append("## Structs")
        lines.append("")
        for s in structs:
            lines.append(f"### {s.name}")
            lines.append("")
            if s.description:
                for desc in s.description:
                    lines.append(desc)
                lines.append("")

    # Functions
    if functions:
        lines.append("## Functions")
        lines.append("")

        for fn in functions:
            lines.append(f"### {fn.name}")
            lines.append("")

            # Description
            if fn.description:
                for desc in fn.description:
                    lines.append(desc)
                lines.append("")

            # Signature
            failable = "!" if fn.is_failable else ""
            lines.append(f"**Signature:** `{fn.signature}{failable}`")
            lines.append("")

            # Parameters table
            if fn.params:
                lines.append("| Parameter | Type | Description |")
                lines.append("|-----------|------|-------------|")
                for name, typ, desc in fn.params:
                    lines.append(f"| `{name}` | `{typ}` | {desc} |")
                lines.append("")

            # Returns table
            if fn.returns:
                lines.append("| Return | Type | Description |")
                lines.append("|--------|------|-------------|")
                for name, typ, desc in fn.returns:
                    lines.append(f"| `{name}` | `{typ}` | {desc} |")
                lines.append("")

            # Errors
            if fn.errors:
                lines.append("**Errors:**")
                lines.append("")
                for err in fn.errors:
                    lines.append(f"- {err}")
                lines.append("")

            # Examples
            if fn.examples:
                lines.append("**Example:**")
                lines.append("")
                lines.append("```qd")
                for ex in fn.examples:
                    lines.append(ex)
                lines.append("```")
                lines.append("")

            lines.append("---")
            lines.append("")

    # Remove trailing separator
    if lines and lines[-2] == "---":
        lines = lines[:-2]

    return "\n".join(lines)


def generate_json(module: Module) -> str:
    """Generate JSON from a parsed module (for MCP server)."""
    import json

    data = {
        "name": module.name,
        "description": " ".join(module.description),
        "constants": [],
        "structs": [],
        "functions": []
    }

    for item in module.items:
        if item.kind == "const":
            data["constants"].append({
                "name": item.name,
                "value": item.value,
                "description": " ".join(item.description)
            })
        elif item.kind == "struct":
            data["structs"].append({
                "name": item.name,
                "description": " ".join(item.description)
            })
        elif item.kind == "fn":
            data["functions"].append({
                "name": item.name,
                "signature": item.signature,
                "failable": item.is_failable,
                "description": " ".join(item.description),
                "params": [{"name": n, "type": t, "description": d} for n, t, d in item.params],
                "returns": [{"name": n, "type": t, "description": d} for n, t, d in item.returns],
                "errors": item.errors,
                "examples": item.examples
            })

    return json.dumps(data, indent=2)


# Standard library modules and their paths
STDLIB_MODULES = {
    "bits": "lib/stdbitsqd/qd/bits/module.qd",
    "math": "lib/stdmathqd/qd/math/module.qd",
    "str": "lib/stdstrqd/qd/str/module.qd",
    "io": "lib/stdioqd/qd/io/module.qd",
    "fmt": "lib/stdfmtqd/qd/fmt/module.qd",
    "mem": "lib/stdmemqd/qd/mem/module.qd",
    "os": "lib/stdosqd/qd/os/module.qd",
    "net": "lib/stdnetqd/qd/net/module.qd",
    "time": "lib/stdtimeqd/qd/time/module.qd",
    "json": "lib/stdjsonqd/qd/json/module.qd",
    "base64": "lib/stdbase64qd/qd/base64/module.qd",
    "strconv": "lib/stdstrconvqd/qd/strconv/module.qd",
    "unicode": "lib/stdunicodeqd/qd/unicode/module.qd",
    "flag": "lib/stdflagqd/qd/flag/module.qd",
}


def main():
    # Find project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    # Output directories
    docs_dir = project_root / "docs" / "stdlib"
    json_dir = project_root / "docs" / "api"

    # Create output directories
    docs_dir.mkdir(parents=True, exist_ok=True)
    json_dir.mkdir(parents=True, exist_ok=True)

    # Process files
    if len(sys.argv) > 1:
        # Single file mode
        filepath = sys.argv[1]
        module = parse_module(filepath)

        if "--json" in sys.argv:
            print(generate_json(module))
        else:
            print(generate_markdown(module))
    else:
        # Generate all stdlib docs
        print(f"Generating documentation to {docs_dir}")

        for name, path in STDLIB_MODULES.items():
            filepath = project_root / path
            if not filepath.exists():
                print(f"  SKIP {name}: {path} not found")
                continue

            module = parse_module(str(filepath))

            # Write markdown
            md_path = docs_dir / f"{name}.md"
            with open(md_path, "w") as f:
                f.write(generate_markdown(module))

            # Write JSON
            json_path = json_dir / f"{name}.json"
            with open(json_path, "w") as f:
                f.write(generate_json(module))

            fn_count = len([i for i in module.items if i.kind == "fn"])
            const_count = len([i for i in module.items if i.kind == "const"])
            print(f"  {name}: {fn_count} functions, {const_count} constants")

        # Generate index
        index_path = docs_dir / "index.md"
        with open(index_path, "w") as f:
            f.write("# Standard Library\n\n")
            f.write("Quadrate standard library modules.\n\n")
            f.write("| Module | Description |\n")
            f.write("|--------|-------------|\n")
            for name, path in STDLIB_MODULES.items():
                filepath = project_root / path
                if filepath.exists():
                    module = parse_module(str(filepath))
                    desc = " ".join(module.description)[:60]
                    if len(" ".join(module.description)) > 60:
                        desc += "..."
                    f.write(f"| [{name}]({name}.md) | {desc} |\n")

        print(f"\nGenerated {len(STDLIB_MODULES)} module docs + index")
        print(f"Markdown: {docs_dir}")
        print(f"JSON API: {json_dir}")


if __name__ == "__main__":
    main()
