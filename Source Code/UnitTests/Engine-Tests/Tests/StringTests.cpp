#include "stdafx.h"

#include "Headers/Defines.h"

#include "Core/Headers/StringHelper.h"
#include "Platform/File/Headers/FileManagement.h"


namespace Divide {

//TEST_FAILURE_INDENT(4);
// We are using third party string libraries (STL, Boost, EASTL) that went through proper testing
// This list of tests only verifies utility functions

namespace {
    template <U64 NUM>
    struct test_const
    {
        static const U64 value = NUM;
    };
};

vectorImpl<stringImpl> getFiles(const stringImpl& input, const std::regex& pattern) {
    std::smatch matches;
    istringstreamImpl inputStream(input);
    stringImpl line;
    vectorImpl<stringImpl> include_file;
    while (std::getline(inputStream, line)) {
        if (std::regex_search(line, matches, pattern)) {
            include_file.emplace_back(Util::Trim(matches[1].str().c_str()));
        }
    }

    return include_file;
}

TEST(RegexSuccessTest)
{
    PreparePlatform();

    {
        const stringImpl& inputInclude1("#include \"blaBla.h\"");
        const stringImpl& inputInclude2("#include <blaBla.h>");
        const stringImpl& inputInclude3("# include \"blaBla.h\"");
        const stringImpl& inputInclude4("   #include <  blaBla.h>");
        const stringImpl& resultInclude("blaBla.h");

        vectorImpl<stringImpl> temp1 = getFiles(inputInclude1, Paths::g_includePattern);
        CHECK_TRUE(temp1.size() == 1);
        if (!temp1.empty()) {
            CHECK_EQUAL(resultInclude, temp1.front());
        }

        vectorImpl<stringImpl> temp2 = getFiles(inputInclude2, Paths::g_includePattern);
        CHECK_TRUE(temp2.size() == 1);
        if (!temp2.empty()) {
            CHECK_EQUAL(resultInclude, temp2.front());
        }

        vectorImpl<stringImpl> temp3 = getFiles(inputInclude3, Paths::g_includePattern);
        CHECK_TRUE(temp3.size() == 1);
        if (!temp3.empty()) {
            CHECK_EQUAL(resultInclude, temp3.front());
        }
        vectorImpl<stringImpl> temp4 = getFiles(inputInclude4, Paths::g_includePattern);
        CHECK_TRUE(temp4.size() == 1);
        if (!temp4.empty()) {
            CHECK_EQUAL(resultInclude, temp4.front());
        }

    }
    {
        const stringImpl& inputUse1("use(\"blaBla.h\")");
        const stringImpl& inputUse2("use( \"blaBla.h\")");
        const stringImpl& inputUse3("      use         (\"blaBla.h\")");
        const stringImpl& inputUse4("use(\"blaBla.h\"         )");
        const stringImpl& resultUse("blaBla.h");

        vectorImpl<stringImpl> temp1 = getFiles(inputUse1, Paths::g_usePattern);
        CHECK_TRUE(temp1.size() == 1);
        if (!temp1.empty()) {
            CHECK_EQUAL(resultUse, temp1.front());
        }

        vectorImpl<stringImpl> temp2 = getFiles(inputUse2, Paths::g_usePattern);
        CHECK_TRUE(temp2.size() == 1);
        if (!temp2.empty()) {
            CHECK_EQUAL(resultUse, temp2.front());
        }

        vectorImpl<stringImpl> temp3 = getFiles(inputUse3, Paths::g_usePattern);
        CHECK_TRUE(temp3.size() == 1);
        if (!temp3.empty()) {
            CHECK_EQUAL(resultUse, temp3.front());
        }
        vectorImpl<stringImpl> temp4 = getFiles(inputUse4, Paths::g_usePattern);
        CHECK_TRUE(temp4.size() == 1);
        if (!temp4.empty()) {
            CHECK_EQUAL(resultUse, temp4.front());
        }
    }
}

TEST(RegexFailTest)
{
    {
        const stringImpl& inputInclude1("#include\"blaBla.h\"");
        const stringImpl& inputInclude2("#include<blaBla.h>");
        const stringImpl& inputInclude3("# include \"blaBla.h");
        const stringImpl& inputInclude4("   include <  blaBla.h>");

        vectorImpl<stringImpl> temp1 = getFiles(inputInclude1, Paths::g_includePattern);
        CHECK_FALSE(temp1.size() == 1);
        vectorImpl<stringImpl> temp2 = getFiles(inputInclude2, Paths::g_includePattern);
        CHECK_FALSE(temp2.size() == 1);
        vectorImpl<stringImpl> temp3 = getFiles(inputInclude3, Paths::g_includePattern);
        CHECK_FALSE(temp3.size() == 1);
        vectorImpl<stringImpl> temp4 = getFiles(inputInclude4, Paths::g_includePattern);
        CHECK_FALSE(temp4.size() == 1);
    }
    {
        const stringImpl& inputUse1("use(\"blaBla.h)");
        const stringImpl& inputUse2("usadfse( \"blaBla.h\")");
        const stringImpl& inputUse3("      use    ---   (\"blaBla.h\")");

        vectorImpl<stringImpl> temp1 = getFiles(inputUse1, Paths::g_usePattern);
        CHECK_FALSE(temp1.size() == 1);
        vectorImpl<stringImpl> temp2 = getFiles(inputUse2, Paths::g_usePattern);
        CHECK_FALSE(temp2.size() == 1);
        vectorImpl<stringImpl> temp3 = getFiles(inputUse3, Paths::g_usePattern);
        CHECK_FALSE(temp3.size() == 1);
    }
}

TEST(TestReplaceInPlace)
{
    stringImpl input("STRING TO BE TESTED");

    const stringImpl match("TO BE");
    const stringImpl replacement("HAS BEEN");
    const stringImpl output("STRING HAS BEEN TESTED");

    Util::ReplaceStringInPlace(input, match, replacement);
    CHECK_EQUAL(input, output);
}

TEST(TestPermutations)
{
    const stringImpl input("ABC");
    vectorImpl<stringImpl> permutations;
    Util::GetPermutations(input, permutations);
    CHECK_TRUE(permutations.size() == 6);
}

TEST(TestNumberParse)
{
    const stringImpl input1("2");
    const stringImpl input2("b");
    CHECK_TRUE(Util::IsNumber(input1));
    CHECK_FALSE(Util::IsNumber(input2));
}

TEST(TestCharTrail)
{
    const stringImpl input("abcdefg");
    const stringImpl extension("efg");
    CHECK_TRUE(Util::GetTrailingCharacters(input, 3) == extension);
    CHECK_TRUE(Util::GetTrailingCharacters(input, 20) == input);

    size_t length = 4;
    CHECK_TRUE(Util::GetTrailingCharacters(input, length).size() == length);
}

TEST(TestCmpNoCase)
{
    const stringImpl inputA("aBcdEf");
    const stringImpl inputB("ABCdef");
    const stringImpl inputC("abcdefg");
    CHECK_TRUE(Util::CompareIgnoreCase(inputA, inputA));
    CHECK_TRUE(Util::CompareIgnoreCase(inputA, inputB));
    CHECK_FALSE(Util::CompareIgnoreCase(inputB, inputC));
}

TEST(TestExtension) {
    const stringImpl input("something.ext");
    const stringImpl ext1("ext");
    const stringImpl ext2("bak");
    CHECK_TRUE(hasExtension(input, ext1));
    CHECK_FALSE(hasExtension(input, ext2));
}

TEST(TestStringSplit) {
    const stringImpl input1("a b c d");
    const vectorImpl<stringImpl> result = {"a", "b", "c", "d"};

    CHECK_EQUAL(Util::Split(input1, ' '), result);
    CHECK_TRUE(Util::Split(input1, ',').size() == 1);
    CHECK_TRUE(Util::Split(input1, ',')[0] == input1);

    const stringImpl input2("a,b,c,d");
    CHECK_EQUAL(Util::Split(input2, ','), result);
}

TEST(TestFilePathSplit) {
    const stringImpl input("/path/path2/path4/file.test");
    const stringImpl result1("file.test");
    const stringImpl result2("/path/path2/path4/");

    FileWithPath fileResult = splitPathToNameAndLocation(input);
    CHECK_EQUAL(fileResult._path, result2);
    CHECK_EQUAL(fileResult._fileName, result1);
}

TEST(TestLineCount) {

    const stringImpl input1("bla");
    const stringImpl input2("bla\nbla");
    const stringImpl input3("bla\nbla\nbla");

    CHECK_EQUAL(Util::LineCount(input1), 1u);
    CHECK_EQUAL(Util::LineCount(input2), 2u);
    CHECK_EQUAL(Util::LineCount(input3), 3u);
}

TEST(TestStringTrim) {
    const stringImpl input1("  abc");
    const stringImpl input2("abc  ");
    const stringImpl input3("  abc  ");
    const stringImpl result("abc");

    CHECK_EQUAL(Util::Ltrim(input1), result);
    CHECK_EQUAL(Util::Ltrim(input2), input2);
    CHECK_EQUAL(Util::Ltrim(input3), input2);
    CHECK_EQUAL(Util::Ltrim(result), result);

    CHECK_EQUAL(Util::Rtrim(input1), input1);
    CHECK_EQUAL(Util::Rtrim(input2), result);
    CHECK_EQUAL(Util::Rtrim(input3), input1);
    CHECK_EQUAL(Util::Rtrim(result), result);

    CHECK_EQUAL(Util::Trim(input1), result);
    CHECK_EQUAL(Util::Trim(input2), result);
    CHECK_EQUAL(Util::Trim(input3), result);
    CHECK_EQUAL(Util::Trim(result), result);
}

TEST(TestStringFormat) {
    const char* input1("A %s b is %d %s");
    const char* input2("%2.2f");
    const stringImpl result1("A is ok, b is 2 \n");
    const stringImpl result2("12.21");

    CHECK_EQUAL(Util::StringFormat(input1, "is ok,", 2, "\n"), result1);
    CHECK_EQUAL(Util::StringFormat(input2, 12.2111f), result2);
}

TEST(TestCharRemove) {
    char input[] = {'a', 'b', 'c', 'b', 'd', '7', 'b', '\0' };
    char result[] = { 'a', 'c', 'd', '7', '\0'};

    Util::CStringRemoveChar(&input[0], 'b');

    CHECK_EQUAL(strlen(input), strlen(result));

    char* in = input;
    char* res = result;
    while(*in != '\0') {
        CHECK_EQUAL(*in, *res);

        in++; res++;
    }
}

TEST(HashIsConstantExpr)
{
    constexpr char* str = "TEST test TEST";

    constexpr U64 value = test_const<_ID(str)>::value;
    CHECK_EQUAL(value, _ID_RT(str));
}

TEST(TestRuntimeID)
{
    const char* str = "TEST String garbagegarbagegarbage";
    U64 input1 = _ID(str);
    CHECK_EQUAL(input1, _ID_RT(str));
    CHECK_EQUAL(_ID_RT(str), _ID_RT(stringImpl(str)));
    CHECK_EQUAL(input1, _ID_RT(stringImpl(str)));
}

TEST(TestStringAllocator)
{
    const char* input = "TEST test TEST";
    stringImpl input1(input);
    stringImplFast input2(input);
    for( U8 i = 0; i < input1.size(); ++i) {
        CHECK_EQUAL(input1[i], input2[i]);
    }
}

TEST(TestStringStream)
{
    stringImpl result1;
    stringImplFast result2;
    stringstreamImplFast s;
    const char* input = "TEST-test-TEST";
    s << input;
    s >> result1;
    CHECK_EQUAL(result1, stringImpl(input));
    s.clear();
    s << input;
    s >> result2;
    CHECK_EQUAL(result2, stringImplFast(input));
}

}; //namespace Divide
