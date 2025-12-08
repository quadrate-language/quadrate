#!/usr/bin/env python3
"""
Generate mkdocs markdown from documented Quadrate module files.

Parses /// doc comments with @param, @return, @example, @error tags
and generates structured markdown documentation.

Also generates the language reference from lib/qc/include/qc/reference.def.

Usage:
    python scripts/gen_docs.py                    # Generate all docs
    python scripts/gen_docs.py lib/qdmathqd/qd/math/module.qd  # Single file
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

    # Separate items by kind and sort alphabetically
    constants = sorted([i for i in module.items if i.kind == "const"], key=lambda x: x.name)
    structs = sorted([i for i in module.items if i.kind == "struct"], key=lambda x: x.name)
    functions = sorted([i for i in module.items if i.kind == "fn"], key=lambda x: x.name)

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

            # Outputs table
            if fn.returns:
                lines.append("| Output | Type | Description |")
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
                    # Transform "code -> output" format to clearer format
                    if " -> " in ex:
                        parts = ex.rsplit(" -> ", 1)
                        code = parts[0]
                        output = parts[1]
                        lines.append(f"{code}  // {output}")
                    else:
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

    for item in sorted(module.items, key=lambda x: x.name):
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
    "base64": "lib/qdbase64/qd/base64/module.qd",
    "bits": "lib/qdbits/qd/bits/module.qd",
    "bytes": "lib/qdbytes/qd/bytes/module.qd",
    "crc32": "lib/qdcrc32/qd/crc32/module.qd",
    "flag": "lib/qdflag/qd/flag/module.qd",
    "fmt": "lib/qdfmt/qd/fmt/module.qd",
    "hex": "lib/qdhex/qd/hex/module.qd",
    "io": "lib/qdio/qd/io/module.qd",
    "json": "lib/qdjson/qd/json/module.qd",
    "math": "lib/qdmath/qd/math/module.qd",
    "mem": "lib/qdmem/qd/mem/module.qd",
    "net": "lib/qdnet/qd/net/module.qd",
    "os": "lib/qdos/qd/os/module.qd",
    "path": "lib/qdpath/qd/path/module.qd",
    "rand": "lib/qdrand/qd/rand/module.qd",
    "regex": "lib/qdregex/qd/regex/module.qd",
    "sb": "lib/qdsb/qd/sb/module.qd",
    "sha256": "lib/qdsha256/qd/sha256/module.qd",
    "sort": "lib/qdsort/qd/sort/module.qd",
    "str": "lib/qdstr/qd/str/module.qd",
    "strconv": "lib/qdstrconv/qd/strconv/module.qd",
    "testing": "lib/qdtesting/qd/testing/module.qd",
    "time": "lib/qdtime/qd/time/module.qd",
    "unicode": "lib/qdunicode/qd/unicode/module.qd",
    "uri": "lib/qduri/qd/uri/module.qd",
    "uuid": "lib/qduuid/qd/uuid/module.qd",
}


@dataclass
class RefItem:
    """A reference item (keyword or builtin)."""
    name: str
    kind: str  # "keyword" or "builtin"
    description: str = ""
    signature: str = ""
    examples: list[str] = field(default_factory=list)
    category: str = ""


def parse_reference_def(filepath: str) -> list[RefItem]:
    """Parse the reference.def file and extract keywords and builtins."""
    items = []
    current_category = ""
    doc_buffer = []

    with open(filepath, "r") as f:
        lines = f.readlines()

    i = 0
    while i < len(lines):
        line = lines[i].rstrip()

        # Track category from section comments
        if line.startswith("// ===") and i + 1 < len(lines):
            next_line = lines[i + 1].strip()
            if next_line.startswith("// ") and not next_line.startswith("// ==="):
                current_category = next_line[3:].strip()
            i += 1
            continue

        # Collect doc comments
        if line.startswith("///"):
            doc_text = line[3:].strip()
            doc_buffer.append(doc_text)
            i += 1
            continue

        # Parse KEYWORD(name, "description")
        match = re.match(r'KEYWORD\((\S+),\s*"(.*)"\)', line)
        if match:
            name = match.group(1)
            desc = match.group(2)

            # Extract examples from doc buffer
            examples = []
            for doc in doc_buffer:
                if doc.startswith("@example "):
                    examples.append(doc[9:])

            items.append(RefItem(
                name=name,
                kind="keyword",
                description=desc,
                examples=examples,
                category=current_category
            ))
            doc_buffer = []
            i += 1
            continue

        # Parse BUILTIN(name, "signature", "description")
        match = re.match(r'BUILTIN\(([^,]+),\s*"([^"]*)",\s*"(.*)"\)', line)
        if match:
            name = match.group(1)
            sig = match.group(2)
            desc = match.group(3)

            # Extract examples from doc buffer
            examples = []
            for doc in doc_buffer:
                if doc.startswith("@example "):
                    examples.append(doc[9:])

            items.append(RefItem(
                name=name,
                kind="builtin",
                description=desc,
                signature=sig,
                examples=examples,
                category=current_category
            ))
            doc_buffer = []
            i += 1
            continue

        # Clear buffer on non-matching lines
        if not line.startswith("//"):
            doc_buffer = []

        i += 1

    return items


def generate_reference_markdown(items: list[RefItem]) -> str:
    """Generate markdown for the language reference."""
    lines = []

    lines.append("# Language Reference")
    lines.append("")
    lines.append("This page documents all Quadrate keywords and built-in instructions.")
    lines.append("")

    # Keywords section
    keywords = [i for i in items if i.kind == "keyword"]
    if keywords:
        lines.append("## Keywords")
        lines.append("")
        lines.append("| Keyword | Description |")
        lines.append("|---------|-------------|")
        for kw in keywords:
            lines.append(f"| [`{kw.name}`](#{kw.name.replace('->', 'arrow').replace('=>', 'case-arrow')}) | {kw.description} |")
        lines.append("")

        for kw in keywords:
            anchor = kw.name.replace("->", "arrow").replace("=>", "case-arrow")
            lines.append(f"### {kw.name}")
            lines.append("")
            lines.append(kw.description)
            lines.append("")
            if kw.examples:
                lines.append("**Example:**")
                lines.append("")
                lines.append("```qd")
                for ex in kw.examples:
                    lines.append(ex)
                lines.append("```")
                lines.append("")
            lines.append("---")
            lines.append("")

    # Builtins by category
    builtins = [i for i in items if i.kind == "builtin"]
    categories = {}
    for b in builtins:
        cat = b.category or "Other"
        if cat not in categories:
            categories[cat] = []
        categories[cat].append(b)

    lines.append("## Built-in Instructions")
    lines.append("")

    for cat in categories:
        lines.append(f"### {cat}")
        lines.append("")
        lines.append("| Instruction | Signature | Description |")
        lines.append("|-------------|-----------|-------------|")
        for b in categories[cat]:
            sig = f"`{b.signature}`" if b.signature else "-"
            lines.append(f"| [`{b.name}`](#{b.name.replace('+', 'plus').replace('-', 'minus').replace('*', 'star').replace('/', 'slash').replace('%', 'percent').replace('!', 'bang').replace('<', 'lt').replace('>', 'gt').replace('=', 'eq')}) | {sig} | {b.description} |")
        lines.append("")

        for b in categories[cat]:
            anchor = b.name.replace("+", "plus").replace("-", "minus").replace("*", "star").replace("/", "slash").replace("%", "percent").replace("!", "bang").replace("<", "lt").replace(">", "gt").replace("=", "eq")
            lines.append(f"#### {b.name}")
            lines.append("")
            lines.append(b.description)
            lines.append("")
            if b.signature:
                lines.append(f"**Signature:** `{b.signature}`")
                lines.append("")
            if b.examples:
                lines.append("**Example:**")
                lines.append("")
                lines.append("```qd")
                for ex in b.examples:
                    lines.append(ex)
                lines.append("```")
                lines.append("")
            lines.append("---")
            lines.append("")

    # Remove trailing separator
    if lines and lines[-2] == "---":
        lines = lines[:-2]

    return "\n".join(lines)


def main():
    # Find project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    # Output directories
    docs_dir = project_root / "docs" / "docs" / "docs" / "stdlib"
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

        for name, path in sorted(STDLIB_MODULES.items()):
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

        # Generate index - preserve custom header content
        index_path = docs_dir / "index.md"
        custom_header = """# Standard Library

The Quadrate standard library provides modules for common programming tasks.

## Using Modules

Import a module with `use`:

```qd
use str
use math

fn main( -- ) {
	"hello" str::upper prints nl  // HELLO
	16.0 math::sqrt print nl  // 4
}
```

## Fallible Functions

Functions marked with `!` can fail and require error handling:

```qd
use str

fn main( -- ) {
	"hello" 0 3 str::substring! prints nl  // "hel"
}
```

## Available Modules

"""
        with open(index_path, "w") as f:
            f.write(custom_header)
            f.write("| Module | Description |\n")
            f.write("|--------|-------------|\n")
            for name, path in sorted(STDLIB_MODULES.items()):
                filepath = project_root / path
                if filepath.exists():
                    module = parse_module(str(filepath))
                    desc = " ".join(module.description)[:60]
                    if len(" ".join(module.description)) > 60:
                        desc += "..."
                    f.write(f"| [{name}]({name}.md) | {desc} |\n")

        # Update mkdocs.yml nav with stdlib modules
        mkdocs_path = project_root / "docs" / "mkdocs.yml"
        if mkdocs_path.exists():
            with open(mkdocs_path, "r") as f:
                mkdocs_content = f.read()

            # Build the new Standard Library nav section
            stdlib_nav = "      - Standard Library:\n"
            stdlib_nav += "          - Overview: docs/stdlib/index.md\n"
            for name in sorted(STDLIB_MODULES.keys()):
                stdlib_nav += f"          - {name}: docs/stdlib/{name}.md\n"

            # Replace the Standard Library line (handles both single line and section)
            # Match either "- Standard Library: ..." or "- Standard Library:\n          - ..."
            pattern = r"      - Standard Library:.*?(?=\n      - |\n  - |\Z)"
            new_content = re.sub(pattern, stdlib_nav.rstrip(), mkdocs_content, flags=re.DOTALL)

            if new_content != mkdocs_content:
                with open(mkdocs_path, "w") as f:
                    f.write(new_content)
                print(f"Updated mkdocs.yml nav")

        print(f"\nGenerated {len(STDLIB_MODULES)} module docs + index")
        print(f"Markdown: {docs_dir}")
        print(f"JSON API: {json_dir}")

        # Generate language reference from reference.def
        ref_def_path = project_root / "lib" / "qc" / "include" / "qc" / "reference.def"
        if ref_def_path.exists():
            print(f"\nGenerating language reference...")
            ref_items = parse_reference_def(str(ref_def_path))
            ref_markdown = generate_reference_markdown(ref_items)

            ref_docs_dir = project_root / "docs" / "docs" / "docs"
            ref_path = ref_docs_dir / "reference.md"
            with open(ref_path, "w") as f:
                f.write(ref_markdown)

            keywords = len([i for i in ref_items if i.kind == "keyword"])
            builtins = len([i for i in ref_items if i.kind == "builtin"])
            print(f"  {keywords} keywords, {builtins} built-in instructions")
            print(f"  Written to: {ref_path}")
        else:
            print(f"\nWarning: {ref_def_path} not found, skipping reference generation")


if __name__ == "__main__":
    main()
