/**
 * @file test_file_utils.cc
 * @brief Unit tests for the qdcli file utilities
 */

#include <cstring>
#include <filesystem>
#include <fstream>
#include <quadrate/cli/file_utils.h>
#include <string>
#include <unit-check/uc.h>

namespace fs = std::filesystem;

// Create a temporary test directory
class TempDir {
public:
	TempDir() {
		mPath = fs::temp_directory_path() / ("qdcli_test_" + std::to_string(rand()));
		fs::create_directories(mPath);
	}

	~TempDir() {
		fs::remove_all(mPath);
	}

	fs::path path() const {
		return mPath;
	}

	std::string pathStr() const {
		return mPath.string();
	}

private:
	fs::path mPath;
};

// ============================================================================
// isValidUtf8 tests
// ============================================================================

TEST(ValidUtf8_EmptyString) {
	ASSERT(qdcli::isValidUtf8(""), "empty string should be valid UTF-8");
}

TEST(ValidUtf8_AsciiOnly) {
	ASSERT(qdcli::isValidUtf8("Hello, World!"), "ASCII string should be valid UTF-8");
	ASSERT(qdcli::isValidUtf8("0123456789"), "digits should be valid UTF-8");
	ASSERT(qdcli::isValidUtf8("~!@#$%^&*()"), "symbols should be valid UTF-8");
}

TEST(ValidUtf8_TwoByteChars) {
	ASSERT(qdcli::isValidUtf8("café"), "2-byte UTF-8 chars should be valid");
	ASSERT(qdcli::isValidUtf8("über"), "German umlauts should be valid");
	ASSERT(qdcli::isValidUtf8("résumé"), "French accents should be valid");
}

TEST(ValidUtf8_ThreeByteChars) {
	ASSERT(qdcli::isValidUtf8("日本語"), "Japanese should be valid UTF-8");
	ASSERT(qdcli::isValidUtf8("中文"), "Chinese should be valid UTF-8");
	ASSERT(qdcli::isValidUtf8("한글"), "Korean should be valid UTF-8");
}

TEST(ValidUtf8_FourByteChars) {
	ASSERT(qdcli::isValidUtf8("\xF0\x9F\x98\x80"), "4-byte emoji should be valid UTF-8");
	ASSERT(qdcli::isValidUtf8("Hello \xF0\x9F\x98\x80 World"), "mixed with emoji should be valid");
}

TEST(InvalidUtf8_NullByte) {
	std::string withNull = "Hello\0World";
	withNull.resize(11); // Include the null byte
	ASSERT(!qdcli::isValidUtf8(withNull), "string with embedded null should be invalid");
}

TEST(InvalidUtf8_TruncatedSequence) {
	ASSERT(!qdcli::isValidUtf8("\xC3"), "truncated 2-byte sequence should be invalid");
	ASSERT(!qdcli::isValidUtf8("\xE4\xB8"), "truncated 3-byte sequence should be invalid");
	ASSERT(!qdcli::isValidUtf8("\xF0\x9F\x98"), "truncated 4-byte sequence should be invalid");
}

TEST(InvalidUtf8_InvalidStartByte) {
	ASSERT(!qdcli::isValidUtf8("\x80"), "continuation byte alone should be invalid");
	ASSERT(!qdcli::isValidUtf8("\xFE\x80\x80\x80\x80\x80"), "invalid start byte 0xFE should be invalid");
}

TEST(InvalidUtf8_InvalidContinuation) {
	ASSERT(!qdcli::isValidUtf8("\xC3\x00"), "invalid continuation (null) should be invalid");
	ASSERT(!qdcli::isValidUtf8("\xC3\x7F"), "invalid continuation (ASCII) should be invalid");
}

// ============================================================================
// readFile / writeFile tests
// ============================================================================

TEST(ReadFile_Simple) {
	TempDir dir;
	std::string filepath = dir.pathStr() + "/test.txt";

	// Write test content
	std::ofstream out(filepath);
	out << "Hello, World!";
	out.close();

	// Read it back
	std::string content = qdcli::readFile(filepath);
	ASSERT_STR_EQ(content.c_str(), "Hello, World!", "content should match");
}

TEST(ReadFile_EmptyFile) {
	TempDir dir;
	std::string filepath = dir.pathStr() + "/empty.txt";

	// Create empty file
	std::ofstream out(filepath);
	out.close();

	// Read it back
	std::string content = qdcli::readFile(filepath);
	ASSERT(content.empty(), "empty file should have empty content");
}

TEST(ReadFile_Multiline) {
	TempDir dir;
	std::string filepath = dir.pathStr() + "/multi.txt";

	// Write multiline content
	std::ofstream out(filepath);
	out << "Line 1\n";
	out << "Line 2\n";
	out << "Line 3";
	out.close();

	// Read it back
	std::string content = qdcli::readFile(filepath);
	ASSERT(content.find("Line 1") != std::string::npos, "should contain Line 1");
	ASSERT(content.find("Line 2") != std::string::npos, "should contain Line 2");
	ASSERT(content.find("Line 3") != std::string::npos, "should contain Line 3");
}

TEST(ReadFile_NonExistent) {
	try {
		qdcli::readFile("/nonexistent/path/to/file.txt");
		ASSERT(false, "should throw for non-existent file");
	} catch (const std::runtime_error& e) {
		ASSERT(true, "should throw runtime_error");
	}
}

TEST(WriteFile_Simple) {
	TempDir dir;
	std::string filepath = dir.pathStr() + "/write_test.txt";

	// Write content
	qdcli::writeFile(filepath, "Test content");

	// Read it back
	std::ifstream in(filepath);
	std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	ASSERT_STR_EQ(content.c_str(), "Test content", "content should match");
}

TEST(WriteFile_Overwrite) {
	TempDir dir;
	std::string filepath = dir.pathStr() + "/overwrite.txt";

	// Write first content
	qdcli::writeFile(filepath, "First");

	// Overwrite
	qdcli::writeFile(filepath, "Second");

	// Read it back
	std::string content = qdcli::readFile(filepath);
	ASSERT_STR_EQ(content.c_str(), "Second", "content should be overwritten");
}

// ============================================================================
// collectFiles tests
// ============================================================================

TEST(CollectFiles_SingleFile) {
	TempDir dir;
	std::string filepath = dir.pathStr() + "/test.qd";

	// Create a .qd file
	qdcli::writeFile(filepath, "fn main() { }");

	// Collect from file path
	auto files = qdcli::collectFiles(filepath);
	ASSERT_EQ(1, static_cast<int>(files.size()), "should have 1 file");
	ASSERT_STR_EQ(files[0].c_str(), filepath.c_str(), "filepath should match");
}

TEST(CollectFiles_Directory) {
	TempDir dir;

	// Create multiple .qd files
	qdcli::writeFile(dir.pathStr() + "/a.qd", "fn a() { }");
	qdcli::writeFile(dir.pathStr() + "/b.qd", "fn b() { }");
	qdcli::writeFile(dir.pathStr() + "/c.txt", "not a qd file"); // Should be ignored

	// Collect from directory
	auto files = qdcli::collectFiles(dir.pathStr());
	ASSERT_EQ(2, static_cast<int>(files.size()), "should have 2 files");
}

TEST(CollectFiles_Recursive) {
	TempDir dir;

	// Create nested directories
	fs::create_directories(dir.path() / "sub1");
	fs::create_directories(dir.path() / "sub2");

	// Create .qd files in various places
	qdcli::writeFile(dir.pathStr() + "/root.qd", "fn root() { }");
	qdcli::writeFile(dir.pathStr() + "/sub1/a.qd", "fn a() { }");
	qdcli::writeFile(dir.pathStr() + "/sub2/b.qd", "fn b() { }");

	// Collect from directory
	auto files = qdcli::collectFiles(dir.pathStr());
	ASSERT_EQ(3, static_cast<int>(files.size()), "should have 3 files");
}

TEST(CollectFiles_Sorted) {
	TempDir dir;

	// Create files in reverse alphabetical order
	qdcli::writeFile(dir.pathStr() + "/z.qd", "fn z() { }");
	qdcli::writeFile(dir.pathStr() + "/m.qd", "fn m() { }");
	qdcli::writeFile(dir.pathStr() + "/a.qd", "fn a() { }");

	// Collect from directory
	auto files = qdcli::collectFiles(dir.pathStr());
	ASSERT_EQ(3, static_cast<int>(files.size()), "should have 3 files");

	// Should be sorted
	ASSERT(files[0].find("a.qd") != std::string::npos, "first should be a.qd");
	ASSERT(files[1].find("m.qd") != std::string::npos, "second should be m.qd");
	ASSERT(files[2].find("z.qd") != std::string::npos, "third should be z.qd");
}

// Main - required for test executable
int main() {
	return UC_PrintResults();
}
