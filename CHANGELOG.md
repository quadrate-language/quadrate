# Changelog

## 0.2.0 (unreleased)

### Compiler
- Split `ast.cc` into 5 compilation units for faster parallel builds
- Unified block parsers — removed ~990 lines of duplicated code from `parseFunctionDeclaration`
- Fixed cross-module method call resolution (3 bugs: validator depth search, reachability analysis, codegen fallback)
- Fixed defer block stack effect check for cross-module method calls
- Fixed `gen_docs.sh` to handle generic type parameters on methods

### Standard Library
- **New functions**: regex `find`, `find_all`, `replace`, `match_count`; fmt `print`, `println`, `eprintln`, `sprintln`, `fprintf`; sort `floats`, `floats_desc`, `unique`, `is_sorted_floats`; crypto `hmac_sha256`, `hmac_sha512`; net `set_timeout`, `set_keepalive`, `lookup`, `get_peer_addr`
- **Sort upgrade**: replaced insertion sort with quicksort (median-of-three pivot, insertion sort fallback for small partitions)
- **Method refactor**: `sb` (StringBuilder), `rand` (Rng), `regex` (Regex), `flag` (Flag) now use method syntax instead of standalone functions taking `ptr`
- String interpolation (`$"..."`) works with method-based StringBuilder

### Documentation
- Generated docs for 16 previously undocumented modules (crypto, json, http, tls, regex, log, base64, uri, uuid, hex, fuzzy, ct, hof, sort, net, tty)
- Added `quaddoc` to toolchain guide
- Updated mkdocs.yml navigation with all stdlib modules
- Updated specification: `>>field!` documented in operator table

### Other
- Removed stale `examples/rest-api/` and `examples/web-server/`
- Removed duplicate `docs/docs/docs/` directory
- Added `.editorconfig`
- Updated shell completions with `doc`, `init`, `clean` subcommands and `quaddoc` completions
- Added `make dist` target for release tarballs with SHA256 checksums
- Added `make tag BUMP=major|minor|patch` for version bumping
- CI now produces release artifacts on all 3 platforms
- Version scheme changed from 2.0.0-alpha to 0.x semver

## 0.1.0 (previously 2.0.0-alpha.5)

- `>>field!` (non-returning field set)
- Playground updates: quick reference, share, format, auto-save
- `quaddoc` tool for HTML documentation generation
- REPL auto-display fix
- Editor support documentation
- Standardized stack instruction naming
- Merged error test directories
- All tools use shared `qdcli` for version and color handling
