#include "semver.h"
#include <algorithm>
#include <cctype>
#include <sstream>

// Helper: trim whitespace from string
static std::string trim(const std::string& s) {
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) {
		return "";
	}
	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}

// Helper: split string by delimiter
static std::vector<std::string> split(const std::string& s, char delim) {
	std::vector<std::string> parts;
	std::istringstream iss(s);
	std::string part;
	while (std::getline(iss, part, delim)) {
		parts.push_back(part);
	}
	return parts;
}

// Helper: parse integer from string, returns -1 on error
static int parseNum(const std::string& s) {
	if (s.empty()) {
		return -1;
	}
	for (char c : s) {
		if (!std::isdigit(static_cast<unsigned char>(c))) {
			return -1;
		}
	}
	try {
		return std::stoi(s);
	} catch (...) {
		return -1;
	}
}

// Compare prerelease strings
// Empty prerelease is greater than any prerelease (1.0.0 > 1.0.0-alpha)
static int comparePrerelease(const std::string& a, const std::string& b) {
	if (a.empty() && b.empty()) {
		return 0;
	}
	if (a.empty()) {
		return 1; // No prerelease > prerelease
	}
	if (b.empty()) {
		return -1; // Prerelease < no prerelease
	}

	// Split by '.' and compare each identifier
	std::vector<std::string> partsA = split(a, '.');
	std::vector<std::string> partsB = split(b, '.');

	size_t minLen = std::min(partsA.size(), partsB.size());
	for (size_t i = 0; i < minLen; i++) {
		// Try numeric comparison first
		int numA = parseNum(partsA[i]);
		int numB = parseNum(partsB[i]);

		if (numA >= 0 && numB >= 0) {
			// Both numeric
			if (numA != numB) {
				return (numA < numB) ? -1 : 1;
			}
		} else if (numA >= 0) {
			// Numeric < non-numeric
			return -1;
		} else if (numB >= 0) {
			// Non-numeric > numeric
			return 1;
		} else {
			// Both alphanumeric - lexical comparison
			int cmp = partsA[i].compare(partsB[i]);
			if (cmp != 0) {
				return (cmp < 0) ? -1 : 1;
			}
		}
	}

	// Longer prerelease is greater if all previous parts equal
	if (partsA.size() != partsB.size()) {
		return (partsA.size() < partsB.size()) ? -1 : 1;
	}

	return 0;
}

int SemVer::compare(const SemVer& other) const {
	if (major != other.major) {
		return (major < other.major) ? -1 : 1;
	}
	if (minor != other.minor) {
		return (minor < other.minor) ? -1 : 1;
	}
	if (patch != other.patch) {
		return (patch < other.patch) ? -1 : 1;
	}
	return comparePrerelease(prerelease, other.prerelease);
}

std::string SemVer::toString() const {
	std::ostringstream oss;
	oss << major << "." << minor << "." << patch;
	if (!prerelease.empty()) {
		oss << "-" << prerelease;
	}
	if (!build.empty()) {
		oss << "+" << build;
	}
	return oss.str();
}

SemVer parseSemVer(const std::string& version) {
	SemVer result;
	result.major = -1; // Mark as invalid initially

	std::string v = trim(version);
	if (v.empty()) {
		return result;
	}

	// Strip leading 'v' or 'V'
	if (v[0] == 'v' || v[0] == 'V') {
		v = v.substr(1);
	}

	// Extract build metadata (+...)
	size_t plusPos = v.find('+');
	if (plusPos != std::string::npos) {
		result.build = v.substr(plusPos + 1);
		v = v.substr(0, plusPos);
	}

	// Extract prerelease (-...)
	size_t dashPos = v.find('-');
	if (dashPos != std::string::npos) {
		result.prerelease = v.substr(dashPos + 1);
		v = v.substr(0, dashPos);
	}

	// Parse major.minor.patch
	std::vector<std::string> parts = split(v, '.');
	if (parts.empty() || parts.size() > 3) {
		return result;
	}

	result.major = parseNum(parts[0]);
	if (result.major < 0) {
		return result;
	}

	if (parts.size() >= 2) {
		result.minor = parseNum(parts[1]);
		if (result.minor < 0) {
			result.major = -1;
			return result;
		}
	}

	if (parts.size() >= 3) {
		result.patch = parseNum(parts[2]);
		if (result.patch < 0) {
			result.major = -1;
			return result;
		}
	}

	return result;
}

bool isSemVer(const std::string& version) {
	std::string v = trim(version);
	if (v.empty()) {
		return false;
	}

	// Strip leading 'v'
	if (v[0] == 'v' || v[0] == 'V') {
		v = v.substr(1);
	}

	// Must start with a digit
	if (v.empty() || !std::isdigit(static_cast<unsigned char>(v[0]))) {
		return false;
	}

	SemVer parsed = parseSemVer(version);
	return parsed.isValid();
}

bool VersionConstraint::satisfies(const SemVer& v) const {
	switch (op) {
	case ConstraintOp::ANY:
		return true;

	case ConstraintOp::EQ:
		return v == version;

	case ConstraintOp::GT:
		return v > version;

	case ConstraintOp::GTE:
		return v >= version;

	case ConstraintOp::LT:
		return v < version;

	case ConstraintOp::LTE:
		return v <= version;

	case ConstraintOp::CARET: {
		// ^X.Y.Z allows changes that do not modify the left-most non-zero digit
		// ^1.2.3 := >=1.2.3 <2.0.0
		// ^0.2.3 := >=0.2.3 <0.3.0
		// ^0.0.3 := >=0.0.3 <0.0.4
		if (v < version) {
			return false;
		}
		if (version.major != 0) {
			// ^1.x.x -> <2.0.0
			return v.major == version.major;
		} else if (version.minor != 0) {
			// ^0.1.x -> <0.2.0
			return v.major == 0 && v.minor == version.minor;
		} else {
			// ^0.0.x -> exact patch
			return v.major == 0 && v.minor == 0 && v.patch == version.patch;
		}
	}

	case ConstraintOp::TILDE: {
		// ~X.Y.Z allows patch-level changes
		// ~1.2.3 := >=1.2.3 <1.3.0
		if (v < version) {
			return false;
		}
		return v.major == version.major && v.minor == version.minor;
	}
	}

	return false;
}

bool VersionRange::satisfies(const SemVer& v) const {
	for (const auto& conjunction : alternatives) {
		bool allMatch = true;
		for (const auto& constraint : conjunction) {
			if (!constraint.satisfies(v)) {
				allMatch = false;
				break;
			}
		}
		if (allMatch) {
			return true;
		}
	}
	return false;
}

// Parse a single constraint (no ||)
static std::vector<VersionConstraint> parseConjunction(const std::string& input) {
	std::vector<VersionConstraint> constraints;
	std::string s = trim(input);

	if (s.empty() || s == "*") {
		// Any version
		VersionConstraint c;
		c.op = ConstraintOp::ANY;
		constraints.push_back(c);
		return constraints;
	}

	// Handle hyphen ranges: 1.2.3 - 2.0.0 (inclusive)
	size_t hyphenPos = s.find(" - ");
	if (hyphenPos != std::string::npos) {
		std::string left = trim(s.substr(0, hyphenPos));
		std::string right = trim(s.substr(hyphenPos + 3));

		SemVer leftVer = parseSemVer(left);
		SemVer rightVer = parseSemVer(right);

		if (leftVer.isValid() && rightVer.isValid()) {
			VersionConstraint c1;
			c1.op = ConstraintOp::GTE;
			c1.version = leftVer;
			constraints.push_back(c1);

			VersionConstraint c2;
			c2.op = ConstraintOp::LTE;
			c2.version = rightVer;
			constraints.push_back(c2);
		}
		return constraints;
	}

	// Split by whitespace for multiple constraints
	std::istringstream iss(s);
	std::string token;

	while (iss >> token) {
		VersionConstraint c;
		size_t versionStart = 0;

		// Detect operator
		if (token.substr(0, 2) == ">=") {
			c.op = ConstraintOp::GTE;
			versionStart = 2;
		} else if (token.substr(0, 2) == "<=") {
			c.op = ConstraintOp::LTE;
			versionStart = 2;
		} else if (token[0] == '>') {
			c.op = ConstraintOp::GT;
			versionStart = 1;
		} else if (token[0] == '<') {
			c.op = ConstraintOp::LT;
			versionStart = 1;
		} else if (token[0] == '=') {
			c.op = ConstraintOp::EQ;
			versionStart = 1;
		} else if (token[0] == '^') {
			c.op = ConstraintOp::CARET;
			versionStart = 1;
		} else if (token[0] == '~') {
			c.op = ConstraintOp::TILDE;
			versionStart = 1;
		} else {
			c.op = ConstraintOp::EQ;
		}

		std::string versionStr = token.substr(versionStart);

		// Handle x-ranges: 1.x, 1.2.x
		size_t xPos = versionStr.find('x');
		if (xPos == std::string::npos) {
			xPos = versionStr.find('X');
		}
		if (xPos == std::string::npos) {
			xPos = versionStr.find('*');
		}

		if (xPos != std::string::npos) {
			// X-range: convert to constraints
			std::string prefix = versionStr.substr(0, xPos);
			if (prefix.empty() || prefix == ".") {
				// Just * or x
				c.op = ConstraintOp::ANY;
				constraints.push_back(c);
			} else {
				// Parse partial version
				if (!prefix.empty() && prefix.back() == '.') {
					prefix = prefix.substr(0, prefix.size() - 1);
				}
				std::vector<std::string> parts = split(prefix, '.');
				if (parts.size() == 1) {
					// 1.x -> >=1.0.0 <2.0.0
					int maj = parseNum(parts[0]);
					if (maj >= 0) {
						VersionConstraint c1;
						c1.op = ConstraintOp::GTE;
						c1.version.major = maj;
						c1.version.minor = 0;
						c1.version.patch = 0;
						constraints.push_back(c1);

						VersionConstraint c2;
						c2.op = ConstraintOp::LT;
						c2.version.major = maj + 1;
						c2.version.minor = 0;
						c2.version.patch = 0;
						constraints.push_back(c2);
					}
				} else if (parts.size() == 2) {
					// 1.2.x -> >=1.2.0 <1.3.0
					int maj = parseNum(parts[0]);
					int min = parseNum(parts[1]);
					if (maj >= 0 && min >= 0) {
						VersionConstraint c1;
						c1.op = ConstraintOp::GTE;
						c1.version.major = maj;
						c1.version.minor = min;
						c1.version.patch = 0;
						constraints.push_back(c1);

						VersionConstraint c2;
						c2.op = ConstraintOp::LT;
						c2.version.major = maj;
						c2.version.minor = min + 1;
						c2.version.patch = 0;
						constraints.push_back(c2);
					}
				}
			}
		} else {
			// Regular version
			c.version = parseSemVer(versionStr);
			if (c.version.isValid()) {
				constraints.push_back(c);
			}
		}
	}

	return constraints;
}

VersionRange parseVersionRange(const std::string& range) {
	VersionRange result;
	std::string s = trim(range);

	if (s.empty()) {
		return result;
	}

	// Split by || for OR
	size_t pos = 0;
	while (pos < s.size()) {
		size_t orPos = s.find("||", pos);
		std::string part;
		if (orPos == std::string::npos) {
			part = s.substr(pos);
			pos = s.size();
		} else {
			part = s.substr(pos, orPos - pos);
			pos = orPos + 2;
		}

		std::vector<VersionConstraint> conjunction = parseConjunction(part);
		if (!conjunction.empty()) {
			result.alternatives.push_back(conjunction);
		}
	}

	return result;
}

void sortVersionsDesc(std::vector<SemVer>& versions) {
	std::sort(versions.begin(), versions.end(), [](const SemVer& a, const SemVer& b) { return a > b; });
}

bool rangesHaveCommonVersion(const std::vector<VersionRange>& ranges) {
	if (ranges.empty()) {
		return true;
	}
	if (ranges.size() == 1) {
		return ranges[0].isValid();
	}

	auto mk = [](int M, int m, int p) {
		SemVer s;
		s.major = M;
		s.minor = m;
		s.patch = p;
		return s;
	};

	std::vector<SemVer> candidates;
	auto addCand = [&](const SemVer& v) {
		if (v.major < 0 || v.minor < 0 || v.patch < 0) {
			return;
		}
		candidates.push_back(v);
	};

	// Always probe the lowest possible version — useful when ranges only
	// have upper bounds.
	addCand(mk(0, 0, 0));

	for (const auto& r : ranges) {
		for (const auto& alt : r.alternatives) {
			for (const auto& c : alt) {
				if (c.op == ConstraintOp::ANY) {
					addCand(mk(1, 0, 0));
					continue;
				}
				const SemVer& v = c.version;
				addCand(v);
				addCand(mk(v.major, v.minor, v.patch + 1));
				addCand(mk(v.major, v.minor + 1, 0));
				addCand(mk(v.major + 1, 0, 0));
				if (v.patch > 0) {
					addCand(mk(v.major, v.minor, v.patch - 1));
				}
				if (v.minor > 0) {
					addCand(mk(v.major, v.minor - 1, 0));
				}
				if (v.major > 0) {
					addCand(mk(v.major - 1, 0, 0));
				}
			}
		}
	}

	for (const auto& v : candidates) {
		bool allSat = true;
		for (const auto& r : ranges) {
			if (!r.satisfies(v)) {
				allSat = false;
				break;
			}
		}
		if (allSat) {
			return true;
		}
	}
	return false;
}

SemVer findBestMatch(const VersionRange& range, const std::vector<SemVer>& versions) {
	// Make a sorted copy (newest first)
	std::vector<SemVer> sorted = versions;
	sortVersionsDesc(sorted);

	// Return first matching version (highest that satisfies)
	for (const auto& v : sorted) {
		if (range.satisfies(v)) {
			return v;
		}
	}

	// No match
	SemVer invalid;
	invalid.major = -1;
	return invalid;
}
