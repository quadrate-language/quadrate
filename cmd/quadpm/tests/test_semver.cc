/**
 * @file test_semver.cc
 * @brief Unit tests for semver parsing and matching
 */

#include "../src/semver.h"
#include <unit-check/uc.h>

TEST(SemVerParseBasicTest) {
	SemVer v = parseSemVer("1.2.3");
	ASSERT_EQ(1, v.major, "major version");
	ASSERT_EQ(2, v.minor, "minor version");
	ASSERT_EQ(3, v.patch, "patch version");
	ASSERT_TRUE(v.isValid(), "should be valid");
}

TEST(SemVerParseWithVPrefixTest) {
	SemVer v = parseSemVer("v1.2.3");
	ASSERT_EQ(1, v.major, "major version");
	ASSERT_EQ(2, v.minor, "minor version");
	ASSERT_EQ(3, v.patch, "patch version");
	ASSERT_TRUE(v.isValid(), "should be valid");
}

TEST(SemVerParsePrereleaseTest) {
	SemVer v = parseSemVer("1.2.3-alpha.1");
	ASSERT_EQ(1, v.major, "major version");
	ASSERT_EQ(2, v.minor, "minor version");
	ASSERT_EQ(3, v.patch, "patch version");
	ASSERT_STR_EQ("alpha.1", v.prerelease.c_str(), "prerelease");
	ASSERT_TRUE(v.isValid(), "should be valid");
}

TEST(SemVerParseBuildMetadataTest) {
	SemVer v = parseSemVer("1.2.3+build.456");
	ASSERT_EQ(1, v.major, "major version");
	ASSERT_EQ(2, v.minor, "minor version");
	ASSERT_EQ(3, v.patch, "patch version");
	ASSERT_STR_EQ("build.456", v.build.c_str(), "build");
	ASSERT_TRUE(v.isValid(), "should be valid");
}

TEST(SemVerParseFullTest) {
	SemVer v = parseSemVer("v2.0.0-rc.1+build.123");
	ASSERT_EQ(2, v.major, "major version");
	ASSERT_EQ(0, v.minor, "minor version");
	ASSERT_EQ(0, v.patch, "patch version");
	ASSERT_STR_EQ("rc.1", v.prerelease.c_str(), "prerelease");
	ASSERT_STR_EQ("build.123", v.build.c_str(), "build");
	ASSERT_TRUE(v.isValid(), "should be valid");
}

TEST(SemVerParseInvalidTest) {
	SemVer v = parseSemVer("invalid");
	ASSERT_FALSE(v.isValid(), "should be invalid");
}

TEST(SemVerParseEmptyTest) {
	SemVer v = parseSemVer("");
	ASSERT_FALSE(v.isValid(), "empty should be invalid");
}

TEST(SemVerParseMajorOnlyTest) {
	SemVer v = parseSemVer("1");
	ASSERT_EQ(1, v.major, "major version");
	ASSERT_EQ(0, v.minor, "minor should default to 0");
	ASSERT_EQ(0, v.patch, "patch should default to 0");
	ASSERT_TRUE(v.isValid(), "should be valid");
}

TEST(SemVerCompareMajorTest) {
	SemVer v1 = parseSemVer("1.0.0");
	SemVer v2 = parseSemVer("2.0.0");
	ASSERT_TRUE(v1 < v2, "1.0.0 < 2.0.0");
	ASSERT_TRUE(v2 > v1, "2.0.0 > 1.0.0");
}

TEST(SemVerCompareMinorTest) {
	SemVer v1 = parseSemVer("1.1.0");
	SemVer v2 = parseSemVer("1.2.0");
	ASSERT_TRUE(v1 < v2, "1.1.0 < 1.2.0");
}

TEST(SemVerComparePatchTest) {
	SemVer v1 = parseSemVer("1.0.1");
	SemVer v2 = parseSemVer("1.0.2");
	ASSERT_TRUE(v1 < v2, "1.0.1 < 1.0.2");
}

TEST(SemVerCompareEqualTest) {
	SemVer v1 = parseSemVer("1.2.3");
	SemVer v2 = parseSemVer("1.2.3");
	ASSERT_TRUE(v1 == v2, "1.2.3 == 1.2.3");
}

TEST(SemVerComparePrereleaseTest) {
	// Prerelease versions have lower precedence
	SemVer v1 = parseSemVer("1.0.0-alpha");
	SemVer v2 = parseSemVer("1.0.0");
	ASSERT_TRUE(v1 < v2, "1.0.0-alpha < 1.0.0");
}

TEST(SemVerComparePrereleaseOrderTest) {
	SemVer v1 = parseSemVer("1.0.0-alpha");
	SemVer v2 = parseSemVer("1.0.0-beta");
	ASSERT_TRUE(v1 < v2, "1.0.0-alpha < 1.0.0-beta");
}

TEST(IsSemVerValidTest) {
	ASSERT_TRUE(isSemVer("1.2.3"), "1.2.3 is semver");
	ASSERT_TRUE(isSemVer("v1.2.3"), "v1.2.3 is semver");
	ASSERT_TRUE(isSemVer("0.0.1"), "0.0.1 is semver");
}

TEST(IsSemVerInvalidTest) {
	ASSERT_FALSE(isSemVer("main"), "main is not semver");
	ASSERT_FALSE(isSemVer("develop"), "develop is not semver");
	ASSERT_FALSE(isSemVer("feature/test"), "feature/test is not semver");
}

TEST(VersionRangeExactTest) {
	VersionRange r = parseVersionRange("1.2.3");
	ASSERT_TRUE(r.isValid(), "range should be valid");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.2.3")), "should satisfy 1.2.3");
	ASSERT_FALSE(r.satisfies(parseSemVer("1.2.4")), "should not satisfy 1.2.4");
}

TEST(VersionRangeCaretTest) {
	VersionRange r = parseVersionRange("^1.2.0");
	ASSERT_TRUE(r.isValid(), "range should be valid");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.2.0")), "should satisfy 1.2.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.2.5")), "should satisfy 1.2.5");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.9.0")), "should satisfy 1.9.0");
	ASSERT_FALSE(r.satisfies(parseSemVer("2.0.0")), "should not satisfy 2.0.0");
	ASSERT_FALSE(r.satisfies(parseSemVer("1.1.0")), "should not satisfy 1.1.0");
}

TEST(VersionRangeCaretZeroMinorTest) {
	// ^0.2.0 should allow 0.2.x
	VersionRange r = parseVersionRange("^0.2.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("0.2.0")), "should satisfy 0.2.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("0.2.5")), "should satisfy 0.2.5");
	ASSERT_FALSE(r.satisfies(parseSemVer("0.3.0")), "should not satisfy 0.3.0");
	ASSERT_FALSE(r.satisfies(parseSemVer("1.0.0")), "should not satisfy 1.0.0");
}

TEST(VersionRangeTildeTest) {
	VersionRange r = parseVersionRange("~1.2.0");
	ASSERT_TRUE(r.isValid(), "range should be valid");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.2.0")), "should satisfy 1.2.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.2.5")), "should satisfy 1.2.5");
	ASSERT_FALSE(r.satisfies(parseSemVer("1.3.0")), "should not satisfy 1.3.0");
}

TEST(VersionRangeGteTest) {
	VersionRange r = parseVersionRange(">=1.0.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.0.0")), "should satisfy 1.0.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("2.0.0")), "should satisfy 2.0.0");
	ASSERT_FALSE(r.satisfies(parseSemVer("0.9.0")), "should not satisfy 0.9.0");
}

TEST(VersionRangeLtTest) {
	VersionRange r = parseVersionRange("<2.0.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.9.9")), "should satisfy 1.9.9");
	ASSERT_FALSE(r.satisfies(parseSemVer("2.0.0")), "should not satisfy 2.0.0");
}

TEST(VersionRangeCompoundTest) {
	VersionRange r = parseVersionRange(">=1.0.0 <2.0.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.0.0")), "should satisfy 1.0.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.5.0")), "should satisfy 1.5.0");
	ASSERT_FALSE(r.satisfies(parseSemVer("0.9.0")), "should not satisfy 0.9.0");
	ASSERT_FALSE(r.satisfies(parseSemVer("2.0.0")), "should not satisfy 2.0.0");
}

TEST(VersionRangeOrTest) {
	VersionRange r = parseVersionRange("1.0.0 || 2.0.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.0.0")), "should satisfy 1.0.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("2.0.0")), "should satisfy 2.0.0");
	ASSERT_FALSE(r.satisfies(parseSemVer("1.5.0")), "should not satisfy 1.5.0");
}

TEST(VersionRangeXRangeTest) {
	VersionRange r = parseVersionRange("1.x");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.0.0")), "should satisfy 1.0.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.9.9")), "should satisfy 1.9.9");
	ASSERT_FALSE(r.satisfies(parseSemVer("2.0.0")), "should not satisfy 2.0.0");
}

TEST(VersionRangeWildcardTest) {
	VersionRange r = parseVersionRange("*");
	ASSERT_TRUE(r.satisfies(parseSemVer("0.0.1")), "should satisfy any");
	ASSERT_TRUE(r.satisfies(parseSemVer("99.99.99")), "should satisfy any");
}

TEST(VersionRangeHyphenTest) {
	VersionRange r = parseVersionRange("1.0.0 - 2.0.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.0.0")), "should satisfy 1.0.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("1.5.0")), "should satisfy 1.5.0");
	ASSERT_TRUE(r.satisfies(parseSemVer("2.0.0")), "should satisfy 2.0.0");
	ASSERT_FALSE(r.satisfies(parseSemVer("0.9.0")), "should not satisfy 0.9.0");
	ASSERT_FALSE(r.satisfies(parseSemVer("2.0.1")), "should not satisfy 2.0.1");
}

TEST(FindBestMatchTest) {
	std::vector<SemVer> versions = {
			parseSemVer("1.0.0"),
			parseSemVer("1.1.0"),
			parseSemVer("1.2.0"),
			parseSemVer("2.0.0"),
	};

	VersionRange r = parseVersionRange("^1.0.0");
	SemVer best = findBestMatch(r, versions);
	ASSERT_TRUE(best.isValid(), "should find match");
	ASSERT_EQ(1, best.major, "major");
	ASSERT_EQ(2, best.minor, "minor (should be highest in ^1.x)");
	ASSERT_EQ(0, best.patch, "patch");
}

TEST(FindBestMatchNoMatchTest) {
	std::vector<SemVer> versions = {
			parseSemVer("1.0.0"),
			parseSemVer("1.1.0"),
	};

	VersionRange r = parseVersionRange("^2.0.0");
	SemVer best = findBestMatch(r, versions);
	ASSERT_FALSE(best.isValid(), "should not find match");
}

TEST(SemVerToStringBasicTest) {
	SemVer v = parseSemVer("1.2.3");
	ASSERT_STR_EQ("1.2.3", v.toString().c_str(), "toString");
}

TEST(SemVerToStringPrereleaseTest) {
	SemVer v = parseSemVer("1.2.3-alpha");
	ASSERT_STR_EQ("1.2.3-alpha", v.toString().c_str(), "toString with prerelease");
}

TEST(SemVerToStringFullTest) {
	SemVer v = parseSemVer("1.2.3-beta.1+build.456");
	ASSERT_STR_EQ("1.2.3-beta.1+build.456", v.toString().c_str(), "toString full");
}

int main(void) {
	return UC_PrintResults();
}
