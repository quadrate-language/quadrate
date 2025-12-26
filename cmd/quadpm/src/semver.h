#ifndef QUADPM_SEMVER_H
#define QUADPM_SEMVER_H

#include <string>
#include <vector>

// Semantic version (major.minor.patch[-prerelease][+build])
struct SemVer {
	int major = 0;
	int minor = 0;
	int patch = 0;
	std::string prerelease; // Optional prerelease label (e.g., "alpha", "beta.1")
	std::string build;		// Optional build metadata (ignored in comparisons)

	// Check if this is a valid parsed version
	bool isValid() const { return major >= 0 && minor >= 0 && patch >= 0; }

	// Compare two versions (-1: this < other, 0: equal, 1: this > other)
	int compare(const SemVer& other) const;

	// Comparison operators
	bool operator<(const SemVer& other) const { return compare(other) < 0; }
	bool operator>(const SemVer& other) const { return compare(other) > 0; }
	bool operator<=(const SemVer& other) const { return compare(other) <= 0; }
	bool operator>=(const SemVer& other) const { return compare(other) >= 0; }
	bool operator==(const SemVer& other) const { return compare(other) == 0; }
	bool operator!=(const SemVer& other) const { return compare(other) != 0; }

	// Convert to string
	std::string toString() const;
};

// Parse a semver string (with or without leading 'v')
// Returns invalid SemVer (major=-1) on parse error
SemVer parseSemVer(const std::string& version);

// Check if a string looks like a semver version
bool isSemVer(const std::string& version);

// Version constraint types
enum class ConstraintOp {
	EQ,	  // = or exact match (default)
	GT,	  // >
	GTE,  // >=
	LT,	  // <
	LTE,  // <=
	CARET, // ^  (compatible with version, allows minor/patch updates)
	TILDE, // ~  (approximately equivalent, allows patch updates)
	ANY	  // *  (any version)
};

// A single version constraint (e.g., ">=1.0.0" or "^2.1.0")
struct VersionConstraint {
	ConstraintOp op = ConstraintOp::EQ;
	SemVer version;

	// Check if a version satisfies this constraint
	bool satisfies(const SemVer& v) const;
};

// A version range (conjunction of constraints, or disjunction with ||)
// Supports: ^1.2.0, ~1.2.0, >=1.0.0 <2.0.0, 1.x, *, etc.
struct VersionRange {
	// Each inner vector is an AND of constraints; outer vector is OR
	// Example: ">=1.0.0 <2.0.0 || >=3.0.0" -> [[>=1.0.0, <2.0.0], [>=3.0.0]]
	std::vector<std::vector<VersionConstraint>> alternatives;

	// Check if a version satisfies this range
	bool satisfies(const SemVer& v) const;

	// Check if this is a valid range
	bool isValid() const { return !alternatives.empty(); }
};

// Parse a version range string
// Supports: ^1.2.0, ~1.2.0, >=1.0.0, <=1.0.0, >1.0.0, <1.0.0, =1.0.0
//           1.2.x, 1.x, *, 1.2.0 - 2.0.0, >=1.0.0 <2.0.0, 1.0.0 || 2.0.0
VersionRange parseVersionRange(const std::string& range);

// Find the best matching version from a list of versions
// Returns empty SemVer if no match found
SemVer findBestMatch(const VersionRange& range, const std::vector<SemVer>& versions);

// Sort versions in descending order (newest first)
void sortVersionsDesc(std::vector<SemVer>& versions);

#endif
