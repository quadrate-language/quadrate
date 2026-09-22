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

static bool prereleaseAllowed(const std::vector<VersionConstraint>& conjunction, const SemVer& v) {
	if (v.prerelease.empty()) {
		return true;
	}
	for (const auto& c : conjunction) {
		if (c.op != ConstraintOp::ANY && !c.version.prerelease.empty() && c.version.major == v.major &&
				c.version.minor == v.minor && c.version.patch == v.patch) {
			return true;
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
		if (allMatch && prereleaseAllowed(conjunction, v)) {
			return true;
		}
	}
	return false;
}

namespace {
	struct PartialVersion {
		int parts[3] = {0, 0, 0};
		int count = 0;
		std::string prerelease;
		bool valid = false;
	};

	bool isWildcard(const std::string& s) {
		return s == "x" || s == "X" || s == "*";
	}

	PartialVersion parsePartialVersion(const std::string& input) {
		PartialVersion pv;
		std::string v = input;
		if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) {
			v = v.substr(1);
		}

		size_t plusPos = v.find('+');
		if (plusPos != std::string::npos) {
			v = v.substr(0, plusPos);
		}

		size_t dashPos = v.find('-');
		if (dashPos != std::string::npos) {
			pv.prerelease = v.substr(dashPos + 1);
			v = v.substr(0, dashPos);
			if (pv.prerelease.empty()) {
				return pv;
			}
			for (char c : pv.prerelease) {
				if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-') {
					return pv;
				}
			}
		}

		if (v.empty() || v.back() == '.') {
			return pv;
		}
		std::vector<std::string> parts = split(v, '.');
		if (parts.empty() || parts.size() > 3) {
			return pv;
		}

		bool wildcard = false;
		for (size_t i = 0; i < parts.size(); i++) {
			if (isWildcard(parts[i])) {
				wildcard = true;
				continue;
			}
			if (wildcard) {
				return pv;
			}
			int n = parseNum(parts[i]);
			if (n < 0) {
				return pv;
			}
			pv.parts[i] = n;
			pv.count++;
		}

		if (!pv.prerelease.empty() && pv.count != 3) {
			return pv;
		}
		pv.valid = true;
		return pv;
	}

	SemVer makeVersion(int major, int minor, int patch, const std::string& prerelease = "") {
		SemVer s;
		s.major = major;
		s.minor = minor;
		s.patch = patch;
		s.prerelease = prerelease;
		return s;
	}

	SemVer lowerBound(const PartialVersion& pv) {
		return makeVersion(pv.parts[0], pv.parts[1], pv.parts[2], pv.prerelease);
	}

	SemVer nextAfterPartial(const PartialVersion& pv) {
		if (pv.count == 1) {
			return makeVersion(pv.parts[0] + 1, 0, 0);
		}
		return makeVersion(pv.parts[0], pv.parts[1] + 1, 0);
	}

	void addConstraint(std::vector<VersionConstraint>& out, ConstraintOp op, const SemVer& v) {
		VersionConstraint c;
		c.op = op;
		c.version = v;
		out.push_back(c);
	}

	bool addComparator(std::vector<VersionConstraint>& out, ConstraintOp op, const PartialVersion& pv) {
		if (pv.count == 0) {
			if (op == ConstraintOp::GT || op == ConstraintOp::LT) {
				return false;
			}
			addConstraint(out, ConstraintOp::ANY, SemVer());
			return true;
		}

		bool full = pv.count == 3;
		switch (op) {
		case ConstraintOp::ANY:
		case ConstraintOp::EQ:
			if (full) {
				addConstraint(out, ConstraintOp::EQ, lowerBound(pv));
			} else {
				addConstraint(out, ConstraintOp::GTE, lowerBound(pv));
				addConstraint(out, ConstraintOp::LT, nextAfterPartial(pv));
			}
			return true;
		case ConstraintOp::GT:
			if (full) {
				addConstraint(out, ConstraintOp::GT, lowerBound(pv));
			} else {
				addConstraint(out, ConstraintOp::GTE, nextAfterPartial(pv));
			}
			return true;
		case ConstraintOp::GTE:
			addConstraint(out, ConstraintOp::GTE, lowerBound(pv));
			return true;
		case ConstraintOp::LT:
			addConstraint(out, ConstraintOp::LT, lowerBound(pv));
			return true;
		case ConstraintOp::LTE:
			if (full) {
				addConstraint(out, ConstraintOp::LTE, lowerBound(pv));
			} else {
				addConstraint(out, ConstraintOp::LT, nextAfterPartial(pv));
			}
			return true;
		case ConstraintOp::TILDE:
			addConstraint(out, ConstraintOp::GTE, lowerBound(pv));
			addConstraint(out, ConstraintOp::LT,
					pv.count == 1 ? makeVersion(pv.parts[0] + 1, 0, 0) : makeVersion(pv.parts[0], pv.parts[1] + 1, 0));
			return true;
		case ConstraintOp::CARET: {
			addConstraint(out, ConstraintOp::GTE, lowerBound(pv));
			SemVer upper;
			if (pv.parts[0] != 0 || pv.count == 1) {
				upper = makeVersion(pv.parts[0] + 1, 0, 0);
			} else if (pv.parts[1] != 0 || pv.count == 2) {
				upper = makeVersion(0, pv.parts[1] + 1, 0);
			} else {
				upper = makeVersion(0, 0, pv.parts[2] + 1);
			}
			addConstraint(out, ConstraintOp::LT, upper);
			return true;
		}
		}
		return false;
	}

	bool isOperatorOnly(const std::string& token) {
		return token == ">=" || token == "<=" || token == ">" || token == "<" || token == "=" || token == "^" ||
			   token == "~";
	}
} // namespace

// Parse a single conjunction (no ||). Returns false if any token is not a
// valid comparator.
static bool parseConjunction(const std::string& input, std::vector<VersionConstraint>& constraints) {
	std::string s = input;
	std::replace(s.begin(), s.end(), ',', ' ');

	std::vector<std::string> tokens;
	std::istringstream iss(s);
	std::string token;
	while (iss >> token) {
		if (!tokens.empty() && isOperatorOnly(tokens.back())) {
			tokens.back() += token;
		} else {
			tokens.push_back(token);
		}
	}

	if (tokens.empty()) {
		addConstraint(constraints, ConstraintOp::ANY, SemVer());
		return true;
	}

	if (tokens.size() == 3 && tokens[1] == "-") {
		PartialVersion left = parsePartialVersion(tokens[0]);
		PartialVersion right = parsePartialVersion(tokens[2]);
		if (!left.valid || !right.valid) {
			return false;
		}
		if (left.count > 0) {
			addConstraint(constraints, ConstraintOp::GTE, lowerBound(left));
		}
		if (right.count == 3) {
			addConstraint(constraints, ConstraintOp::LTE, lowerBound(right));
		} else if (right.count > 0) {
			addConstraint(constraints, ConstraintOp::LT, nextAfterPartial(right));
		}
		if (constraints.empty()) {
			addConstraint(constraints, ConstraintOp::ANY, SemVer());
		}
		return true;
	}

	for (const auto& tok : tokens) {
		ConstraintOp op = ConstraintOp::EQ;
		size_t versionStart = 0;
		if (tok.compare(0, 2, ">=") == 0) {
			op = ConstraintOp::GTE;
			versionStart = 2;
		} else if (tok.compare(0, 2, "<=") == 0) {
			op = ConstraintOp::LTE;
			versionStart = 2;
		} else if (tok[0] == '>') {
			op = ConstraintOp::GT;
			versionStart = 1;
		} else if (tok[0] == '<') {
			op = ConstraintOp::LT;
			versionStart = 1;
		} else if (tok[0] == '=') {
			op = ConstraintOp::EQ;
			versionStart = 1;
		} else if (tok[0] == '^') {
			op = ConstraintOp::CARET;
			versionStart = 1;
		} else if (tok[0] == '~') {
			op = ConstraintOp::TILDE;
			versionStart = 1;
		}

		PartialVersion pv = parsePartialVersion(tok.substr(versionStart));
		if (!pv.valid || !addComparator(constraints, op, pv)) {
			return false;
		}
	}

	return true;
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

		std::vector<VersionConstraint> conjunction;
		if (!parseConjunction(part, conjunction)) {
			result.alternatives.clear();
			return result;
		}
		result.alternatives.push_back(conjunction);
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
