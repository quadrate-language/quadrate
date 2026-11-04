#ifndef QUADFMT_USES_H
#define QUADFMT_USES_H

#include <string>

enum class UsesResult {
	Ok,
	ParseError, // the source does not parse
	Unsafe,		// the rewrite would not produce valid source
};

// Adds the use statements `source` needs and removes the ones it does not,
// leaving every other line as it was. `out` is set only on Ok.
UsesResult fixUses(const std::string& source, const std::string& filename, std::string& out);

#endif
