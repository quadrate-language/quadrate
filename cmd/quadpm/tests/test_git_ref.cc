/**
 * @file test_git_ref.cc
 * @brief Unit tests for git URL parsing
 */

#include "../src/git_ref.h"
#include <unit-check/uc.h>

TEST(GitRefScpNoRefTest) {
	GitRef r = parseGitUrl("git@github.com:user/repo");
	ASSERT_STR_EQ("git@github.com:user/repo", r.url.c_str(), "userinfo @ is not a ref separator");
	ASSERT_STR_EQ("", r.ref.c_str(), "no ref");
	ASSERT_STR_EQ("github.com/user/repo", r.hostPath.c_str(), "host path");
	ASSERT_STR_EQ("repo", r.moduleName.c_str(), "module name");
}

TEST(GitRefScpWithRefTest) {
	GitRef r = parseGitUrl("git@github.com:user/repo@v1.2.0");
	ASSERT_STR_EQ("git@github.com:user/repo", r.url.c_str(), "url");
	ASSERT_STR_EQ("v1.2.0", r.ref.c_str(), "ref");
}

TEST(GitRefScpTopLevelTest) {
	GitRef r = parseGitUrl("git@host:repo");
	ASSERT_STR_EQ("git@host:repo", r.url.c_str(), "url");
	ASSERT_STR_EQ("", r.ref.c_str(), "no ref");
	ASSERT_STR_EQ("host/repo", r.hostPath.c_str(), "host path");
}

TEST(GitRefSshUserinfoTest) {
	GitRef r = parseGitUrl("ssh://git@host.example/u/r");
	ASSERT_STR_EQ("ssh://git@host.example/u/r", r.url.c_str(), "userinfo @ is not a ref separator");
	ASSERT_STR_EQ("", r.ref.c_str(), "no ref");
	ASSERT_STR_EQ("host.example/u/r", r.hostPath.c_str(), "host path");

	GitRef withRef = parseGitUrl("ssh://git@host.example/u/r@main");
	ASSERT_STR_EQ("ssh://git@host.example/u/r", withRef.url.c_str(), "url");
	ASSERT_STR_EQ("main", withRef.ref.c_str(), "ref");
}

TEST(GitRefHttpsUserinfoTest) {
	GitRef r = parseGitUrl("https://user@github.com/u/r@feature/x");
	ASSERT_STR_EQ("https://user@github.com/u/r", r.url.c_str(), "url");
	ASSERT_STR_EQ("feature/x", r.ref.c_str(), "ref with slash");
	ASSERT_STR_EQ("github.com/u/r", r.hostPath.c_str(), "userinfo dropped from host path");
}

TEST(GitRefFileUrlTest) {
	GitRef r = parseGitUrl("file:///tmp/x/lib@v1");
	ASSERT_STR_EQ("file:///tmp/x/lib", r.url.c_str(), "url");
	ASSERT_STR_EQ("v1", r.ref.c_str(), "ref");
	ASSERT_STR_EQ("local/tmp/x/lib", r.hostPath.c_str(), "file url maps like a local path");

	GitRef up = parseGitUrl("file:///../../../tmp/x/lib");
	ASSERT_STR_EQ("local/tmp/x/lib", up.hostPath.c_str(), "leading .. cannot climb above /");
}

TEST(GitRefRemoteDotDotKeptTest) {
	GitRef r = parseGitUrl("https://evil/../../..");
	ASSERT_STR_EQ("evil/../../..", r.hostPath.c_str(), "remote .. is kept for the install check to reject");
}
