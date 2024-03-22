#include "UnitTests/unitTestCommon.h"

#include "Core/Headers/StringHelper.h"
#include "Platform/File/Headers/FileManagement.h"

namespace Divide
{

//TEST_FAILURE_INDENT(4);
// We are using third party string libraries (STL, Boost, EASTL) that went through proper testing
// This list of tests only verifies utility functions

template<CTRE_REGEX_INPUT_TYPE T>
vector<string> getFiles(const string& input)
{
    istringstream inputStream(input);
    string line;
    vector<string> include_file;
    while (std::getline(inputStream, line))
    {
        if ( auto m = ctre::match<T>(line) )
        {
            include_file.emplace_back(Util::Trim(m.get<1>().str()));
        }
    }

    return include_file;
}

TEST_CASE("Regex Test", "[string_tests]")
{
    platformInitRunListener::PlatformInit();

    SECTION("Success")
    {
        {
            const string& inputInclude1("#include \"blaBla.h\"");
            const string& inputInclude2("#include <blaBla.h>");
            const string& inputInclude3("# include \"blaBla.h\"");
            const string& inputInclude4("   #include <  blaBla.h>");
            const string& resultInclude("blaBla.h");

            vector<string> temp1 = getFiles<Paths::g_includePattern>(inputInclude1);
            CHECK_TRUE(temp1.size() == 1);
            if (!temp1.empty()) {
                CHECK_EQUAL(resultInclude, temp1.front());
            }

            vector<string> temp2 = getFiles<Paths::g_includePattern>(inputInclude2);
            CHECK_TRUE(temp2.size() == 1);
            if (!temp2.empty()) {
                CHECK_EQUAL(resultInclude, temp2.front());
            }

            vector<string> temp3 = getFiles<Paths::g_includePattern>(inputInclude3);
            CHECK_TRUE(temp3.size() == 1);
            if (!temp3.empty()) {
                CHECK_EQUAL(resultInclude, temp3.front());
            }

            vector<string> temp4 = getFiles<Paths::g_includePattern>(inputInclude4);
            CHECK_TRUE(temp4.size() == 1);
            if (!temp4.empty()) {
                CHECK_EQUAL(resultInclude, temp4.front());
            }
        }
        {
            const string& inputUse1("use(\"blaBla.h\")");
            const string& inputUse2("use( \"blaBla.h\")");
            const string& inputUse3("      use         (\"blaBla.h\")");
            const string& inputUse4("use(\"blaBla.h\"         )");
            const string& resultUse("blaBla.h");

            vector<string> temp1 = getFiles<Paths::g_usePattern>(inputUse1);
            CHECK_TRUE(temp1.size() == 1);
            if (!temp1.empty()) {
                CHECK_EQUAL(resultUse, temp1.front());
            }

            vector<string> temp2 = getFiles<Paths::g_usePattern>(inputUse2);
            CHECK_TRUE(temp2.size() == 1);
            if (!temp2.empty()) {
                CHECK_EQUAL(resultUse, temp2.front());
            }

            vector<string> temp3 = getFiles<Paths::g_usePattern>(inputUse3);
            CHECK_TRUE(temp3.size() == 1);
            if (!temp3.empty()) {
                CHECK_EQUAL(resultUse, temp3.front());
            }

            vector<string> temp4 = getFiles<Paths::g_usePattern>(inputUse4);
            CHECK_TRUE(temp4.size() == 1);
            if (!temp4.empty()) {
                CHECK_EQUAL(resultUse, temp4.front());
            }
        }
    }

    SECTION( "Fail" )
    {
        {
            const string& inputInclude1("#include\"blaBla.h\"");
            const string& inputInclude2("#include<blaBla.h>");
            const string& inputInclude3("# include \"blaBla.h");
            const string& inputInclude4("   include <  blaBla.h>");

            const vector<string> temp1 = getFiles<Paths::g_includePattern>(inputInclude1);
            CHECK_FALSE(temp1.size() == 1);
            const vector<string> temp2 = getFiles<Paths::g_includePattern>(inputInclude2);
            CHECK_FALSE(temp2.size() == 1);
            const vector<string> temp3 = getFiles<Paths::g_includePattern>(inputInclude3);
            CHECK_FALSE(temp3.size() == 1);
            const vector<string> temp4 = getFiles<Paths::g_includePattern>(inputInclude4);
            CHECK_FALSE(temp4.size() == 1);
        }
        {
            const string& inputUse1("use(\"blaBla.h)");
            const string& inputUse2("usadfse( \"blaBla.h\")");
            const string& inputUse3("      use    ---   (\"blaBla.h\")");

            const vector<string> temp1 = getFiles<Paths::g_usePattern>(inputUse1);
            CHECK_FALSE(temp1.size() == 1);
            const vector<string> temp2 = getFiles<Paths::g_usePattern>(inputUse2);
            CHECK_FALSE(temp2.size() == 1);
            const vector<string> temp3 = getFiles<Paths::g_usePattern>(inputUse3);
            CHECK_FALSE(temp3.size() == 1);
        }
    }
}

TEST_CASE( "Begins With Test", "[string_tests]" )
{
    const string input1("STRING TO BE TESTED");
    const string input2("    STRING TO BE TESTED");

    CHECK_TRUE(Util::BeginsWith(input1, "STRING", true));
    CHECK_TRUE(Util::BeginsWith(input2, "STRING", true));
    CHECK_TRUE(Util::BeginsWith(input2, "    STRING", false));
    CHECK_FALSE(Util::BeginsWith(input2, "STRING", false));
}

TEST_CASE( "Replace In Place Test", "[string_tests]" )
{
    string input("STRING TO BE TESTED");

    const string match("TO BE");
    const string replacement("HAS BEEN");
    const string output("STRING HAS BEEN TESTED");

    Util::ReplaceStringInPlace(input, match, replacement);
    CHECK_EQUAL(input, output);
}

TEST_CASE( "Get Permutations Test", "[string_tests]" )
{
    const string input("ABC");
    vector<string> permutations;
    Util::GetPermutations(input, permutations);
    CHECK_TRUE(permutations.size() == 6);
}

TEST_CASE( "Parse Numbers Test", "[string_tests]" )
{
    const string input1("2");
    const string input2("b");
    CHECK_TRUE(Util::IsNumber(input1));
    CHECK_FALSE(Util::IsNumber(input2));
}

TEST_CASE( "Trailing Characters Test", "[string_tests]" )
{
    const string input("abcdefg");
    const string extension("efg");
    CHECK_TRUE(Util::GetTrailingCharacters(input, 3) == extension);
    CHECK_TRUE(Util::GetTrailingCharacters(input, 20) == input);

    constexpr size_t length = 4;
    CHECK_TRUE(Util::GetTrailingCharacters(input, length).size() == length);
}

TEST_CASE( "Compare (case-insensitive) Test", "[string_tests]" )
{
    const string inputA("aBcdEf");
    const string inputB("ABCdef");
    const string inputC("abcdefg");
    CHECK_TRUE(Util::CompareIgnoreCase(inputA, inputA));
    CHECK_TRUE(Util::CompareIgnoreCase(inputA, inputB));
    CHECK_FALSE(Util::CompareIgnoreCase(inputB, inputC));
}

TEST_CASE( "Has Extension Test", "[string_tests]" )
{
    const char* input = "something.ext";
    const char* ext1 = "ext";
    const char* ext2 = "bak";
    CHECK_TRUE(hasExtension(input, ext1));
    CHECK_FALSE(hasExtension(input, ext2));
}

TEST_CASE( "Split Test", "[string_tests]" )
{
    const string input1("a b c d");
    const vector<string> result = {"a", "b", "c", "d"};

    CHECK_EQUAL((Util::Split<vector<string>, string>(input1.c_str(), ' ')), result);
    CHECK_TRUE((Util::Split<vector<string>, string>(input1.c_str(), ',').size()) == 1);
    CHECK_TRUE((Util::Split<vector<string>, string>(input1.c_str(), ',')[0]) == input1);

    const string input2("a,b,c,d");
    CHECK_EQUAL((Util::Split<vector<string>, string>(input2.c_str(), ',')), result);
}

TEST_CASE( "Path Split Test", "[string_tests]" )
{
    const char* input = "/path/path2/path4/file.test";
    const string result1("file.test");
    const string result2("/path/path2/path4");

    const auto[name, path] = splitPathToNameAndLocation(input);
    CHECK_EQUAL(path, result2);
    CHECK_EQUAL(name, result1);
}

TEST_CASE( "Line Count Test", "[string_tests]" )
{

    const string input1("bla");
    const string input2("bla\nbla");
    const string input3("bla\nbla\nbla");

    CHECK_EQUAL(Util::LineCount(input1), 1u);
    CHECK_EQUAL(Util::LineCount(input2), 2u);
    CHECK_EQUAL(Util::LineCount(input3), 3u);
}

TEST_CASE( "Trim Test", "[string_tests]" )
{
    const string input1("  abc");
    const string input2("abc  ");
    const string input3("  abc  ");
    const string result("abc");

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

TEST_CASE( "Format Test", "[string_tests]" )
{
    const char* input1("A %s b is %d %s");
    const char* input2("%2.2f");
    const string result1("A is ok, b is 2 \n");
    const string result2("12.21");

    CHECK_EQUAL(Util::StringFormat(input1, "is ok,", 2, "\n"), result1);
    CHECK_EQUAL(Util::StringFormat(input2, 12.2111f), result2);
}

TEST_CASE( "Remove Char Test", "[string_tests]" )
{
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

TEST_CASE( "Constexpr Hash Test", "[string_tests]" )
{
    constexpr const char* const str = "TEST test TEST";
    constexpr std::string_view str2 = str;

    constexpr U64 value = _ID(str);
    CHECK_EQUAL(value, _ID(str));

    constexpr U64 value2 = _ID_VIEW(str2.data(), str2.length());
    CHECK_EQUAL(value2, value);

    CHECK_EQUAL(value, "TEST test TEST"_id);
}

TEST_CASE( "Runtime Hash Test", "[string_tests]" )
{
    const char* str = "TEST String garbagegarbagegarbage";
    const std::string_view str2 = str;

    const U64 input1 = _ID(str);
    const U64 input2 = _ID_VIEW(str2.data(), str2.length());

    CHECK_EQUAL(input1, _ID(str));
    CHECK_EQUAL(_ID(str), _ID(string(str).c_str()));
    CHECK_EQUAL(input1, _ID(string(str).c_str()));
    CHECK_EQUAL(input1, input2);
}

TEST_CASE( "Allocator Test", "[string_tests]" )
{
    const char* input = "TEST test TEST";
    string input1(input);
    string_fast input2(input);
    for( size_t i = 0; i < input1.size(); ++i) {
        CHECK_EQUAL(input1[i], input2[i]);
    }
}

TEST_CASE( "Stringstream Test", "[string_tests]" )
{
    string result1;
    string_fast result2;
    stringstream_fast s;
    const char* input = "TEST-test-TEST";
    s << input;
    s >> result1;
    CHECK_EQUAL(result1, string(input));
    s.clear();
    s << input;
    s >> result2;
    CHECK_EQUAL(result2, string_fast(input));
}

} //namespace Divide