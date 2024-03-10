// The main test program.

// It requires following env variables:
// * `VERBOSE` - 0 or 1, whether to enable verbose logging.
// * `COMPILER_COMMAND` - the compiler command that we should be using.
// * `LINKER_FLAGS` - those are added to COMPILER_COMMAND when linking.
// * `OUTPUT_DIR` - where to write the files.
// * `EXT_EXE` - the extension for executables.
// * `EXE_RUNNER` - the wrapper program used to run the executables, if any.

#include <taut/taut.hpp>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Whether `std::system` uses CMD.
#ifndef IS_WINDOWS_SHELL
#ifdef _WIN32
#define IS_WINDOWS_SHELL 1
#else
#define IS_WINDOWS_SHELL 0
#endif
#endif

#if IS_WINDOWS_SHELL
#define DEV_NULL "NUL"
#else
#define DEV_NULL "/dev/null"
#endif

// Reads an environment variable, throws if it doesn't exist.
[[nodiscard]] std::string_view ReadEnvVar(const char *varname)
{
    char *env = std::getenv(varname);
    if (!env)
        throw std::runtime_error(std::string(varname) + " env variable must be set!");
    return env;
}

// Whether we should be verbose.
[[nodiscard]] bool IsVerbose()
{
    static const bool ret = []{
        std::string_view var = ReadEnvVar("VERBOSE");
        if (var == "0")
            return false;
        if (var == "1")
            return true;
        throw std::runtime_error("VERBOSE must be 0 or 1");
    }();
    return ret;
}

// Reads the contents of `filename` and returns them.
[[nodiscard]] std::string ReadFile(std::string filename)
{
    std::ifstream input(filename);
    if (!input)
        throw std::runtime_error("Can't open file: " + filename);

    std::ostringstream ss;
    ss << input.rdbuf();

    if (input.fail()) // If any error other than EOF...
        throw std::runtime_error("Can't read file: " + filename);

    return std::move(ss).str();
}

// Check that the strings are equal. If not, print the diff and fail the test.
void CheckStringEquality(std::string_view a, std::string_view b)
{
    if (a != b)
    {
        static const std::string output_dir(ReadEnvVar("OUTPUT_DIR"));
        std::string path_a = output_dir + "/diff_a.txt";
        std::string path_b = output_dir + "/diff_b.txt";
        std::string path_result = output_dir + "/diff_result.txt";
        std::ofstream file(path_a);
        file << a;
        file.close();
        file.open(path_b);
        file << b;
        file.close();
        std::system(("diff --color=always " + path_a + " " + path_b + " >" + path_result).c_str());
        std::cout << ReadFile(path_result) << '\n';
    }

    TA_CHECK( a == b );
}

struct TryCompileParams
{
    // If set, link the executable and write the resulting filename to `*exe_filename`.
    // If null, only check the syntax.
    std::string *exe_filename = nullptr;
    // If set, capture the compiler output and write it to this string.
    std::string *compiler_output = nullptr;

    // If true, the compiler output isn't printed to the terminal.
    // Has no effect when `compiler_output` is set, because that also suppresses the output.
    bool discard_compiler_output = false;
};

// Tries to compile `code`, returns the compiler's exit status.
[[nodiscard]] int TryCompile(std::string_view code, const TryCompileParams &params = {})
{
    static const std::string base_command(ReadEnvVar("COMPILER_COMMAND"));
    static const std::string linker_flags(ReadEnvVar("LINKER_FLAGS"));
    static const std::string ext_exe(ReadEnvVar("EXT_EXE"));

    static const std::string output_dir(ReadEnvVar("OUTPUT_DIR"));

    std::string source_filename = output_dir + "/tmp.cpp";

    { // Write the source file.
        std::ofstream source_file(source_filename);
        if (!source_file)
            throw std::runtime_error("Can't create temporary source file: " + source_filename);
        source_file << code << '\n';
        if (!source_file)
            throw std::runtime_error("Can't write to the temporary source file: " + source_filename);
    }

    std::string compiler_command = base_command + " " + source_filename;

    if (!params.exe_filename)
    {
        compiler_command += " -fsyntax-only";
    }
    else
    {
        *params.exe_filename = output_dir + "/tmp" + ext_exe;
        compiler_command += " " + linker_flags + " -o " + *params.exe_filename;
    }

    std::string output_filename;
    if (params.compiler_output)
    {
        output_filename = output_dir + "/tmp.output";
        compiler_command += " >" + output_filename + " 2>" + output_filename;
    }
    else if (params.discard_compiler_output)
    {
        compiler_command += " >" DEV_NULL " 2>" DEV_NULL;
    }

    if (IsVerbose())
        std::cout << "Running compiler command: " << compiler_command << '\n';
    int status = std::system(compiler_command.c_str());

    if (params.compiler_output)
    {
        *params.compiler_output = ReadFile(output_filename);

        if (IsVerbose())
            std::cout << "Compiler says:\n" + *params.compiler_output;
    }

    return status;
}

// Check that `code` compiles.
void MustCompile(std::string_view code)
{
    TA_CHECK( TryCompile(code) == 0 );
}

// Check that `code` fails with a compilation error.
// If `regex` isn't empty, also validates the compiler output against the regex.
void MustNotCompile(std::string_view code, std::string_view regex = "")
{
    std::string output;

    TryCompileParams params;
    if (!regex.empty())
        params.compiler_output = &output;
    else
        params.discard_compiler_output = true;

    TA_CHECK( TryCompile(code, params) != 0 );

    if (!regex.empty())
    {
        std::regex regex_object(regex.begin(), regex.end());
        TA_CHECK( std::regex_search(output, regex_object) );
    }
}

// Check that `code` compiles, and then try running it with certain flags.
struct CodeRunner
{
    std::string exe_filename;

  private:
    // Runs the code with `flags`.
    // If `output` isn't null, it receives the executable output. Otherwise the output is discarded.
    // Returns the executable status.
    int RunLow(std::string_view flags, std::string *output = nullptr)
    {
        static const std::string exe_runner(ReadEnvVar("EXE_RUNNER"));

        std::string output_file;

        std::string command;
        if (!exe_runner.empty())
            command += exe_runner + " ";

        command += exe_filename;
        command += " ";
        command += flags;

        if (output)
        {
            static const std::string output_dir(ReadEnvVar("OUTPUT_DIR"));
            output_file = output_dir + "/tmp.output";
            command += " >" + output_file + " 2>" + output_file;
        }
        else
        {
            command += " >" DEV_NULL " 2>" DEV_NULL;
        }

        if (IsVerbose())
            std::cout << "Running executable: " << command << '\n';
        int status = std::system(command.c_str());

        if (output)
            *output = ReadFile(output_file);

        return status;
    }

  public:
    CodeRunner &Run(std::string_view flags = "")
    {
        TA_CHECK( RunLow(flags) == 0 );
        return *this;
    }
    CodeRunner &Fail(std::string_view flags = "")
    {
        TA_CHECK( RunLow(flags) != 0 );
        return *this;
    }
    CodeRunner &RunWithExactOutput(std::string_view flags, std::string_view expected_output)
    {
        std::string output;
        TA_CHECK( RunLow(flags, &output) == 0 );
        CheckStringEquality(output, expected_output);
        return *this;
    }
    CodeRunner &FailWithExactOutput(std::string_view flags, std::string_view expected_output)
    {
        std::string output;
        TA_CHECK( RunLow(flags, &output) != 0 );
        CheckStringEquality(output, expected_output);
        return *this;
    }
    CodeRunner &FailWithOutputMatching(std::string_view flags, std::regex regex)
    {
        std::string output;
        TA_CHECK( RunLow(flags, &output) != 0 );
        TA_CHECK( std::regex_search(output, regex) );
        return *this;
    }
};
// Compile the code and then run some checks on the exe.
[[nodiscard]] CodeRunner MustCompileAndThen(std::string_view code)
{
    CodeRunner runner;
    TryCompileParams params;
    params.exe_filename = &runner.exe_filename;
    TA_CHECK( TryCompile(code, params) == 0 );
    return runner;
}

// Whether `T` has a native `{:?}` debug format in this formatting library.
template <typename T>
concept HasNativeDebufFormat = requires(CFG_TA_FMT_NAMESPACE::formatter<T> f){f.set_debug_format();};

namespace TestTypes
{
    struct UserDefinedTupleLike
    {
        int x = 0;
        std::string y;
    };

    template <std::size_t I, typename T>
    requires std::is_same_v<std::remove_cvref_t<T>, UserDefinedTupleLike>
    auto &&get(T &&value)
    {
        if constexpr (I == 0)
            return std::forward<T>(value).x;
        else if constexpr (I == 1)
            return std::forward<T>(value).y;
        else
            static_assert(ta_test::meta::AlwaysFalse<ta_test::meta::ValueTag<I>, T>::value, "Bad tuple index!");
    }
}
template <> struct std::tuple_size<TestTypes::UserDefinedTupleLike> : std::integral_constant<std::size_t, 2> {};
template <> struct std::tuple_element<0, TestTypes::UserDefinedTupleLike> {using type = int;};
template <> struct std::tuple_element<1, TestTypes::UserDefinedTupleLike> {using type = std::string;};

// Test our own testing functions.
TA_TEST(rig_selftest)
{
    MustCompile("#include <version>");
    MustNotCompile("blah");

    MustCompileAndThen("#include <iostream>\nint main(){std::cout << \"Hello, world!\\n\"; return 0;}")
        .Run()
        .RunWithExactOutput("", "Hello, world!\n");

    MustCompileAndThen("#include <iostream>\nint main(){std::cout << \"Hello, world!\\n\"; return 1;}")
        .Fail()
        .FailWithExactOutput("", "Hello, world!\n");
}

TA_TEST(string_conv/to_string)
{
    // Integers.
    auto CheckInt = [&]<typename T>
    {
        TA_CHECK( ta_test::string_conv::ToString(T(42)) == R"(42)" );
    };
    CheckInt.operator()<signed char>();
    CheckInt.operator()<unsigned char>();
    CheckInt.operator()<short>();
    CheckInt.operator()<unsigned short>();
    CheckInt.operator()<int>();
    CheckInt.operator()<unsigned int>();
    CheckInt.operator()<long>();
    CheckInt.operator()<unsigned long>();
    CheckInt.operator()<long long>();
    CheckInt.operator()<unsigned long long>();

    // Floating-point numbers.
    auto CheckFloat = [&]<typename T>
    {
        TA_CHECK( $[ta_test::string_conv::ToString(T(12.3L))] == R"(12.3)" );
        TA_CHECK( $[ta_test::string_conv::ToString(T(-12.3L))] == R"(-12.3)" );
        TA_CHECK( $[ta_test::string_conv::ToString(T(1.23e-09L))] == R"(1.23e-09)" );

        TA_CHECK( $[ta_test::string_conv::ToString(std::numeric_limits<T>::infinity())] == "inf" );
        TA_CHECK( $[ta_test::string_conv::ToString(-std::numeric_limits<T>::infinity())] == "-inf" );
        TA_CHECK( $[ta_test::string_conv::ToString(std::numeric_limits<T>::quiet_NaN())] == "nan" );
        TA_CHECK( $[ta_test::string_conv::ToString(-std::numeric_limits<T>::quiet_NaN())] == "-nan" );
    };
    CheckFloat.operator()<float>();
    CheckFloat.operator()<double>();
    CheckFloat.operator()<long double>();

    // Strings and chars.
    TA_CHECK( $[ta_test::string_conv::ToString("")] == R"("")" );
    TA_CHECK( $[ta_test::string_conv::ToString((const char *)"ab\ncd ef")] == R"("ab\ncd ef")" );
    TA_CHECK( $[ta_test::string_conv::ToString((char *)"ab\ncd ef")] == R"("ab\ncd ef")" );
    TA_CHECK( $[ta_test::string_conv::ToString("ab\ncd ef")] == R"("ab\ncd ef")" );
    TA_CHECK( $[ta_test::string_conv::ToString((char (&)[6])"ab\ncd ef")] == R"("ab\ncd ef")" );
    TA_CHECK( $[ta_test::string_conv::ToString((std::string_view)"ab\ncd ef")] == R"("ab\ncd ef")" );
    TA_CHECK( $[ta_test::string_conv::ToString((std::string)"ab\ncd ef")] == R"("ab\ncd ef")" );
    TA_CHECK( $[ta_test::string_conv::ToString('a')] == R"('a')" );
    TA_CHECK( $[ta_test::string_conv::ToString('\n')] == R"('\n')" );

    { // String escapes.
        // Control characters.
        for (char i = 0; i < 32; i++)
        {
            std::string escape;
            std::string alt_escape;

            if (i == '\a')
                escape = "\\a";
            else if (i == '\b')
                escape = "\\b";
            else if (i == '\f')
                escape = "\\f";
            else if (i == '\n')
                escape = "\\n";
            else if (i == '\r')
                escape = "\\r";
            else if (i == '\t')
                escape = "\\t";
            else if (i == '\v')
                escape = "\\v";
            else
                escape = CFG_TA_FMT_NAMESPACE::format("\\u{{{:x}}}", i);

            TA_CHECK( $[ta_test::string_conv::ToString(std::string{'X',i,'Y'})] == $["\"X" + escape + "Y\""] );
        }

        // Escaped quotes.
        TA_CHECK( $[ta_test::string_conv::ToString("X\"Y")] == R"("X\"Y")" );
        TA_CHECK( $[ta_test::string_conv::ToString("X'Y")] == R"("X'Y")" );
        TA_CHECK( $[ta_test::string_conv::ToString("X\\Y")] == R"("X\\Y")" );
        // Escaped quotes in single characters.
        TA_CHECK( $[ta_test::string_conv::ToString('"')] == R"('"')" );
        TA_CHECK( $[ta_test::string_conv::ToString('\'')] == R"('\'')" );
        TA_CHECK( $[ta_test::string_conv::ToString('\\')] == R"('\\')" );

        // Stuff that doesn't need escaping:
        TA_CHECK( $[ta_test::string_conv::ToString("X?Y")] == R"("X?Y")" );

        // Decoding unicode characters?!
        TA_CHECK( $[ta_test::string_conv::ToString("X\u061fY")] == "\"X\u061fY\"" );

        // What about invalid unicode?
        TA_CHECK( $[ta_test::string_conv::ToString("X\xff\u061f\xef""Y")] == "\"X\\x{ff}\u061f\\x{ef}Y\"");

        // Incomlete UTF-8 characters?
        // This is a prefix of e.g. `\xe2\x97\x8a` U+25CA LOZENGE.
        TA_CHECK( $[ta_test::string_conv::ToString("X\xe2\x97")] == R"("X\x{e2}\x{97}")" );
    }

    { // All character types.
        // char:
        TA_CHECK( $[ta_test::string_conv::ToString("blah")] == R"("blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString(+"blah")] == R"("blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((char (&)[5])"blah")] == R"("blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((char *)"blah")] == R"("blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((std::string_view)"blah")] == R"("blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((std::string)"blah")] == R"("blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString('A')] == R"('A')" );
        TA_CHECK( $[ta_test::string_conv::ToString("A")] == R"("A")" );
        TA_CHECK( $[ta_test::string_conv::ToString('\n')] == R"('\n')" );
        TA_CHECK( $[ta_test::string_conv::ToString("\n")] == R"("\n")" );
        TA_CHECK( $[ta_test::string_conv::ToString('\xff')] == R"('\x{ff}')" );
        TA_CHECK( $[ta_test::string_conv::ToString("\xff")] == R"("\x{ff}")" );
        TA_CHECK( $[ta_test::string_conv::ToString("\u061f")] == "\"\u061f\"" );
        TA_CHECK( $[ta_test::string_conv::ToString('"')] == R"('"')" );
        TA_CHECK( $[ta_test::string_conv::ToString('\'')] == R"('\'')" );
        TA_CHECK( $[ta_test::string_conv::ToString("'")] == R"("'")" );
        TA_CHECK( $[ta_test::string_conv::ToString("\"")] == R"("\"")" );

        // char8_t:
        TA_CHECK( $[ta_test::string_conv::ToString(u8"blah")] == R"(u8"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString(+u8"blah")] == R"(u8"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((char8_t (&)[5])u8"blah")] == R"(u8"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((char8_t *)u8"blah")] == R"(u8"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((std::u8string_view)u8"blah")] == R"(u8"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((std::u8string)u8"blah")] == R"(u8"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString(u8'A')] == R"(u8'A')" );
        TA_CHECK( $[ta_test::string_conv::ToString(u8"A")] == R"(u8"A")" );
        TA_CHECK( $[ta_test::string_conv::ToString(u8'\n')] == R"(u8'\n')" );
        TA_CHECK( $[ta_test::string_conv::ToString(u8"\n")] == R"(u8"\n")" );
        TA_CHECK( $[ta_test::string_conv::ToString(u8'\xff')] == R"(u8'\x{ff}')" );
        TA_CHECK( $[ta_test::string_conv::ToString(u8"\xff")] == R"(u8"\x{ff}")" );
        TA_CHECK( $[ta_test::string_conv::ToString(u8"\u061f")] == "u8\"\u061f\"" );
        TA_CHECK( $[ta_test::string_conv::ToString(u8'"')] == R"(u8'"')" );
        TA_CHECK( $[ta_test::string_conv::ToString(u8'\'')] == R"(u8'\'')" );
        TA_CHECK( $[ta_test::string_conv::ToString(u8"'")] == R"(u8"'")" );
        TA_CHECK( $[ta_test::string_conv::ToString(u8"\"")] == R"(u8"\"")" );
        // char16_t:
        TA_CHECK( $[ta_test::string_conv::ToString(u"blah")] == R"(u"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString(+u"blah")] == R"(u"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((char16_t (&)[5])u"blah")] == R"(u"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((char16_t *)u"blah")] == R"(u"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((std::u16string_view)u"blah")] == R"(u"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((std::u16string)u"blah")] == R"(u"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString(u'A')] == R"(u'A')" );
        TA_CHECK( $[ta_test::string_conv::ToString(u"A")] == R"(u"A")" );
        TA_CHECK( $[ta_test::string_conv::ToString(u'\n')] == R"(u'\n')" );
        TA_CHECK( $[ta_test::string_conv::ToString(u"\n")] == R"(u"\n")" );
        TA_CHECK( $[ta_test::string_conv::ToString(u'\xff')] == "u'\u00ff'" );
        TA_CHECK( $[ta_test::string_conv::ToString(u"\xff")] == "u\"\u00ff\"" );
        TA_CHECK( $[ta_test::string_conv::ToString(u'\u061f')] == "u'\u061f'" );
        TA_CHECK( $[ta_test::string_conv::ToString(u"\u061f")] == "u\"\u061f\"" );
        TA_CHECK( $[ta_test::string_conv::ToString(u'"')] == R"(u'"')" );
        TA_CHECK( $[ta_test::string_conv::ToString(u'\'')] == R"(u'\'')" );
        TA_CHECK( $[ta_test::string_conv::ToString(u"'")] == R"(u"'")" );
        TA_CHECK( $[ta_test::string_conv::ToString(u"\"")] == R"(u"\"")" );
        // char32_t:
        TA_CHECK( $[ta_test::string_conv::ToString(U"blah")] == R"(U"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString(+U"blah")] == R"(U"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((char32_t (&)[5])U"blah")] == R"(U"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((char32_t *)U"blah")] == R"(U"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((std::u32string_view)U"blah")] == R"(U"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((std::u32string)U"blah")] == R"(U"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString(U'A')] == R"(U'A')" );
        TA_CHECK( $[ta_test::string_conv::ToString(U"A")] == R"(U"A")" );
        TA_CHECK( $[ta_test::string_conv::ToString(U'\n')] == R"(U'\n')" );
        TA_CHECK( $[ta_test::string_conv::ToString(U"\n")] == R"(U"\n")" );
        TA_CHECK( $[ta_test::string_conv::ToString(U'\xff')] == "U'\u00ff'" );
        TA_CHECK( $[ta_test::string_conv::ToString(U"\xff")] == "U\"\u00ff\"" );
        TA_CHECK( $[ta_test::string_conv::ToString(U'\u061f')] == "U'\u061f'" );
        TA_CHECK( $[ta_test::string_conv::ToString(U"\u061f")] == "U\"\u061f\"" );
        TA_CHECK( $[ta_test::string_conv::ToString(U'\U0001FBCA')] == "U'\U0001FBCA'" ); // U+1FBCA WHITE UP-POINTING CHEVRON
        TA_CHECK( $[ta_test::string_conv::ToString(U"\U0001FBCA")] == "U\"\U0001FBCA\"" ); // U+1FBCA WHITE UP-POINTING CHEVRON
        TA_CHECK( $[ta_test::string_conv::ToString(char32_t(0x123f567e))] == R"(U'\x{123f567e}')" ); // Out-of-range character.
        TA_CHECK( $[ta_test::string_conv::ToString(std::u32string{char32_t(0x123f567e)})] == R"(U"\x{123f567e}")" ); // Out-of-range character.
        TA_CHECK( $[ta_test::string_conv::ToString(U'"')] == R"(U'"')" );
        TA_CHECK( $[ta_test::string_conv::ToString(U'\'')] == R"(U'\'')" );
        TA_CHECK( $[ta_test::string_conv::ToString(U"'")] == R"(U"'")" );
        TA_CHECK( $[ta_test::string_conv::ToString(U"\"")] == R"(U"\"")" );
        // wchar_t:
        TA_CHECK( $[ta_test::string_conv::ToString(L"blah")] == R"(L"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString(+L"blah")] == R"(L"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((wchar_t (&)[5])L"blah")] == R"(L"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((wchar_t *)L"blah")] == R"(L"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((std::wstring_view)L"blah")] == R"(L"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString((std::wstring)L"blah")] == R"(L"blah")" );
        TA_CHECK( $[ta_test::string_conv::ToString(L'A')] == R"(L'A')" );
        TA_CHECK( $[ta_test::string_conv::ToString(L"A")] == R"(L"A")" );
        TA_CHECK( $[ta_test::string_conv::ToString(L'\n')] == R"(L'\n')" );
        TA_CHECK( $[ta_test::string_conv::ToString(L"\n")] == R"(L"\n")" );
        TA_CHECK( $[ta_test::string_conv::ToString(L'\xff')] == "L'\u00ff'" );
        TA_CHECK( $[ta_test::string_conv::ToString(L"\xff")] == "L\"\u00ff\"" );
        TA_CHECK( $[ta_test::string_conv::ToString(L'\u061f')] == "L'\u061f'" );
        TA_CHECK( $[ta_test::string_conv::ToString(L"\u061f")] == "L\"\u061f\"" );
        if (sizeof(wchar_t) >= 4)
        {
            // Fat wchar8_t: (everywhere except Windows)
            TA_CHECK( $[ta_test::string_conv::ToString(L'\U0001FBCA')] == "L'\U0001FBCA'" ); // U+1FBCA WHITE UP-POINTING CHEVRON
            TA_CHECK( $[ta_test::string_conv::ToString(L"\U0001FBCA")] == "L\"\U0001FBCA\"" ); // U+1FBCA WHITE UP-POINTING CHEVRON
            TA_CHECK( $[ta_test::string_conv::ToString(wchar_t(0x123f567e))] == R"(L'\x{123f567e}')" ); // Out-of-range character.
            TA_CHECK( $[ta_test::string_conv::ToString(std::wstring{wchar_t(0x123f567e)})] == R"(L"\x{123f567e}")" ); // Out-of-range character.
        }
        TA_CHECK( $[ta_test::string_conv::ToString(L'"')] == R"(L'"')" );
        TA_CHECK( $[ta_test::string_conv::ToString(L'\'')] == R"(L'\'')" );
        TA_CHECK( $[ta_test::string_conv::ToString(L"'")] == R"(L"'")" );
        TA_CHECK( $[ta_test::string_conv::ToString(L"\"")] == R"(L"\"")" );
    }

    { // std::filesystem::path
        #ifdef _WIN32
        TA_CHECK( $[ta_test::string_conv::ToString(std::filesystem::path(L"foo/\u061f/bar"))] == "L\"foo/\u061f/bar\"" );
        #else
        TA_CHECK( $[ta_test::string_conv::ToString(std::filesystem::path("foo/\u061f/bar"))] == "\"foo/\u061f/bar\"" );
        #endif
    }

    // Ranges.
    TA_CHECK( $[ta_test::string_conv::ToString(std::vector<int>{1,2,3})] == "[1, 2, 3]" );
    TA_CHECK( $[ta_test::string_conv::ToString(std::vector<int>{})] == "[]" );

    TA_CHECK( $[ta_test::string_conv::ToString(std::set<int>{1,2,3})] == "{1, 2, 3}" );
    TA_CHECK( $[ta_test::string_conv::ToString(std::set<int>{})] == "{}" );

    TA_CHECK( $[ta_test::string_conv::ToString(std::map<int, std::string>{{1,"a"},{2,"b"},{3,"c"}})] == R"({1: "a", 2: "b", 3: "c"})" );
    TA_CHECK( $[ta_test::string_conv::ToString(std::map<int, std::string>{})] == "{}" );

    // `std::array` counts as a range.
    TA_CHECK( $[ta_test::string_conv::ToString(std::array<int, 3>{1,2,3})] == "[1, 2, 3]" );
    TA_CHECK( $[ta_test::string_conv::ToString(std::array<int, 0>{})] == "[]" );

    // Check that range element types use our formatter, if this is enabled.
    TA_CHECK( $[ta_test::string_conv::ToString(std::vector{nullptr, nullptr})] ==
        $[CFG_TA_FMT_ALLOW_NATIVE_RANGE_FORMATTING && CFG_TA_FMT_HAS_RANGE_FORMATTING ? "[0x0, 0x0]" : "[nullptr, nullptr]"]
    );

    // Tuple-like:
    TA_CHECK( $[ta_test::string_conv::ToString(std::tuple{1,"a",3.4})] == "(1, \"a\", 3.4)" );
    TA_CHECK( $[ta_test::string_conv::ToString(std::tuple{})] == "()" );
    TA_CHECK( $[ta_test::string_conv::ToString(std::tuple{10,20})] == "(10, 20)" ); // Duplicate element tuples.
    // ... pairs:
    TA_CHECK( $[ta_test::string_conv::ToString(std::pair{1,"a"})] == "(1, \"a\")" );
    // ... user-defined types with `get` in an ADL namespace.
    TA_CHECK( $[ta_test::string_conv::ToString(TestTypes::UserDefinedTupleLike{10,"blah"})] == "(10, \"blah\")" );

    // Nullptr.
    // Formatting libraries print it as `0x0`, but we override that for sanity.
    TA_CHECK( $[ta_test::string_conv::ToString(nullptr)] == "nullptr" );

    // Exact string.
    TA_CHECK( $[ta_test::string_conv::ToString(ta_test::string_conv::ExactString{"foo\nbar blah"})] == "foo\nbar blah" );
}

TA_TEST(string_conv/from_string)
{
    constexpr auto FromStringPasses = [&]<typename T>(const char *source, const T &expected, int unused_trailing_characters = 0)
    {
        const char *orig_source = source;

        T value;

        // Initialize trivial types to junk, to make sure we overwrite them.
        if constexpr (std::is_trivially_constructible_v<T>)
            std::memset(&value, 0xab, sizeof(T));

        std::string error = ta_test::string_conv::FromStringTraits<T>{}(value, source);
        TA_CHECK( $[error] == "" );
        TA_CHECK( $[orig_source + std::strlen(orig_source) - $[source]] == $[unused_trailing_characters] );

        if constexpr (std::is_floating_point_v<T>)
        {
            if (std::isnan(expected))
            {
                TA_CHECK( std::isnan( $[value] ) );
                return;
            }
        }

        TA_CHECK( $[value] == $[expected] );
    };

    constexpr auto FromStringFails = [&]<typename T>(const char *source, int pos, std::string_view error_regex)
    {
        const char *orig_source = source;

        T value;

        // Initialize trivial types to junk, just in case.
        if constexpr (std::is_trivially_constructible_v<T>)
            std::memset(&value, 0xab, sizeof(T));

        std::string error = ta_test::string_conv::FromStringTraits<T>{}(value, source);
        TA_CHECK( error != "" );
        TA_CHECK( std::regex_match($[error], std::regex($[error_regex].begin(), error_regex.end())) );
        TA_CHECK( $[source - orig_source] == $[pos] );
    };

    // Integers.
    auto CheckInt = [&]<typename T>
    {
        FromStringPasses("42", T(42));
        FromStringPasses("42 ", T(42), 1);
        FromStringPasses("0x2a", T(42));
        FromStringPasses("0x2A", T(42));
        FromStringPasses("0X2a", T(42));
        FromStringPasses("0X2A", T(42));
        FromStringPasses("052", T(42));
        FromStringPasses("0b00101010", T(42));
        FromStringPasses("0B00101010", T(42));

        FromStringPasses("42e", T(42), 1);
        FromStringPasses("42e3", T(42), 2);
        FromStringPasses("42E", T(42), 1);
        FromStringPasses("42E3", T(42), 2);

        // Sign.
        FromStringPasses("+42", T(42));
        FromStringPasses("+42 ", T(42), 1);
        FromStringPasses("+0x2a", T(42));
        FromStringPasses("+0x2A", T(42));
        FromStringPasses("+0X2a", T(42));
        FromStringPasses("+0X2A", T(42));
        FromStringPasses("+0b00101010", T(42));
        FromStringPasses("+0B00101010", T(42));
        if constexpr (std::is_signed_v<T>)
        {
            FromStringPasses("-42", T(-42));
            FromStringPasses("-42 ", T(-42), 1);
            FromStringPasses("-0x2a", T(-42));
            FromStringPasses("-0x2A", T(-42));
            FromStringPasses("-0X2a", T(-42));
            FromStringPasses("-0X2A", T(-42));
            FromStringPasses("-0b00101010", T(-42));
            FromStringPasses("-0B00101010", T(-42));
        }

        std::string common_error = CFG_TA_FMT_NAMESPACE::format("Expected {}\\.", ta_test::text::TypeName<T>());

        FromStringFails.operator()<T>("", 0, common_error);
        FromStringFails.operator()<T>(" 42", 0, common_error);
        FromStringFails.operator()<T>(" -42", 0, common_error);
        FromStringFails.operator()<T>("- 42", 0, common_error);
        FromStringFails.operator()<T>(" +42", 0, common_error);
        FromStringFails.operator()<T>("+ 42", 0, common_error);

        FromStringPasses("0x0", T(0));
        FromStringPasses("0X0", T(0));
        FromStringPasses("0x", T(0), 1);
        FromStringPasses("0X", T(0), 1);
        FromStringPasses("0b0", T(0));
        FromStringPasses("0B0", T(0));
        FromStringPasses("0b", T(0), 1);
        FromStringPasses("0B", T(0), 1);

        if constexpr (std::is_unsigned_v<T>)
        {
            FromStringPasses("255", T(255));
            if constexpr (sizeof(T) == 1)
            {
                FromStringFails.operator()<T>("256", 0, common_error);
            }
            else
            {
                FromStringPasses("65535", T(65535));
                if constexpr (sizeof(T) == 2)
                {
                    FromStringFails.operator()<T>("65536", 0, common_error);
                }
                else
                {
                    FromStringPasses("4294967295", T(4294967295));
                    if constexpr (sizeof(T) == 4)
                    {
                        FromStringFails.operator()<T>("4294967296", 0, common_error);
                    }
                    else
                    {
                        FromStringPasses("18446744073709551615", T(18446744073709551615ULL));
                        if constexpr (sizeof(T) == 8)
                            FromStringFails.operator()<T>("18446744073709551616", 0, common_error);
                    }
                }
            }
        }
        else
        {
            FromStringPasses("127", T(127));
            FromStringPasses("-128", T(-128));
            if constexpr (sizeof(T) == 1)
            {
                FromStringFails.operator()<T>("128", 0, common_error);
                FromStringFails.operator()<T>("-129", 0, common_error);
            }
            else
            {
                FromStringPasses("32767", T(32767));
                FromStringPasses("-32768", T(-32768));
                if constexpr (sizeof(T) == 2)
                {
                    FromStringFails.operator()<T>("32768", 0, common_error);
                    FromStringFails.operator()<T>("-32769", 0, common_error);
                }
                else
                {
                    FromStringPasses("2147483647", T(2147483647));
                    FromStringPasses("-2147483648", T(-2147483648));
                    if constexpr (sizeof(T) == 4)
                    {
                        FromStringFails.operator()<T>("2147483648", 0, common_error);
                        FromStringFails.operator()<T>("-2147483649", 0, common_error);
                    }
                    else
                    {
                        FromStringPasses("9223372036854775807", T(9223372036854775807LL));
                        FromStringPasses("-9223372036854775808", T(-9223372036854775807LL - 1LL));
                        if constexpr (sizeof(T) == 8)
                        {
                            FromStringFails.operator()<T>("9223372036854775808", 0, common_error);
                            FromStringFails.operator()<T>("-9223372036854775809", 0, common_error);
                        }
                    }
                }
            }
        }
    };
    CheckInt.operator()<signed char>();
    CheckInt.operator()<unsigned char>();
    CheckInt.operator()<short>();
    CheckInt.operator()<unsigned short>();
    CheckInt.operator()<int>();
    CheckInt.operator()<unsigned int>();
    CheckInt.operator()<long>();
    CheckInt.operator()<unsigned long>();
    CheckInt.operator()<long long>();
    CheckInt.operator()<unsigned long long>();

    // Floating-point numbers.
    auto CheckFloat = [&]<typename T>
    {
        std::string common_error = CFG_TA_FMT_NAMESPACE::format("Expected {}.", ta_test::text::TypeName<T>());

        FromStringPasses("12.3", T(12.3L));
        FromStringPasses("12.3 ", T(12.3L), 1);
        FromStringPasses("+12.3", T(12.3L));
        FromStringPasses("-12.3", T(-12.3L));

        FromStringFails.operator()<T>(" 12.3", 0, common_error);
        FromStringFails.operator()<T>(" +12.3", 0, common_error);
        FromStringFails.operator()<T>("+ 12.3", 0, common_error);
        FromStringFails.operator()<T>(" -12.3", 0, common_error);
        FromStringFails.operator()<T>("- 12.3", 0, common_error);

        FromStringPasses("12.3e1", T(12.3e1L));
        FromStringPasses("12.3e+1", T(12.3e+1L));
        FromStringPasses("12.3e-1", T(12.3e-1L));
        FromStringPasses("+12.3e1", T(12.3e1L));
        FromStringPasses("+12.3e+1", T(12.3e+1L));
        FromStringPasses("+12.3e-1", T(12.3e-1L));
        FromStringPasses("-12.3e1", T(-12.3e1L));
        FromStringPasses("-12.3e+1", T(-12.3e+1L));
        FromStringPasses("-12.3e-1", T(-12.3e-1L));

        FromStringPasses("12.3e", T(12.3L), 1);
        FromStringPasses("12.3e+", T(12.3L), 2);
        FromStringPasses("12.3e-", T(12.3L), 2);

        FromStringPasses("inf", std::numeric_limits<T>::infinity());
        FromStringPasses("+inf", std::numeric_limits<T>::infinity());
        FromStringPasses("-inf", -std::numeric_limits<T>::infinity());
        FromStringPasses("INF", std::numeric_limits<T>::infinity());
        FromStringPasses("+INF", std::numeric_limits<T>::infinity());
        FromStringPasses("-INF", -std::numeric_limits<T>::infinity());
        FromStringPasses("Inf", std::numeric_limits<T>::infinity());
        FromStringPasses("+Inf", std::numeric_limits<T>::infinity());
        FromStringPasses("-Inf", -std::numeric_limits<T>::infinity());
        FromStringPasses("iNf", std::numeric_limits<T>::infinity());
        FromStringPasses("+iNf", std::numeric_limits<T>::infinity());
        FromStringPasses("-iNf", -std::numeric_limits<T>::infinity());
        FromStringPasses("infinity", std::numeric_limits<T>::infinity());
        FromStringPasses("+infinity", std::numeric_limits<T>::infinity());
        FromStringPasses("-infinity", -std::numeric_limits<T>::infinity());
        FromStringPasses("iNfIniTy", std::numeric_limits<T>::infinity());
        FromStringPasses("iNfIniTy", std::numeric_limits<T>::infinity());
        FromStringPasses("-iNfIniTy", -std::numeric_limits<T>::infinity());

        FromStringPasses("nan", std::numeric_limits<T>::quiet_NaN());
        FromStringPasses("+nan", std::numeric_limits<T>::quiet_NaN());
        FromStringPasses("-nan", -std::numeric_limits<T>::quiet_NaN());
        FromStringPasses("NAN", std::numeric_limits<T>::quiet_NaN());
        FromStringPasses("+NAN", std::numeric_limits<T>::quiet_NaN());
        FromStringPasses("-NAN", -std::numeric_limits<T>::quiet_NaN());
        FromStringPasses("Nan", std::numeric_limits<T>::quiet_NaN());
        FromStringPasses("+Nan", std::numeric_limits<T>::quiet_NaN());
        FromStringPasses("-Nan", -std::numeric_limits<T>::quiet_NaN());
        FromStringPasses("NaN", std::numeric_limits<T>::quiet_NaN());
        FromStringPasses("+NaN", std::numeric_limits<T>::quiet_NaN());
        FromStringPasses("-NaN", -std::numeric_limits<T>::quiet_NaN());
        FromStringPasses("nAn", std::numeric_limits<T>::quiet_NaN()); // Any combination of case.
        FromStringPasses("+nAn", std::numeric_limits<T>::quiet_NaN());
        FromStringPasses("-nAn", -std::numeric_limits<T>::quiet_NaN());

        FromStringPasses("inf ", std::numeric_limits<T>::infinity(), 1);
        FromStringPasses("infi", std::numeric_limits<T>::infinity(), 1);
        FromStringPasses("infinity", std::numeric_limits<T>::infinity());
        FromStringPasses("infinity ", std::numeric_limits<T>::infinity(), 1);
        FromStringPasses("infinitys", std::numeric_limits<T>::infinity(), 1);
        FromStringPasses("nan ", std::numeric_limits<T>::quiet_NaN(), 1);
        FromStringPasses("nani", std::numeric_limits<T>::quiet_NaN(), 1);

        FromStringFails.operator()<T>(" inf", 0, common_error);
        FromStringFails.operator()<T>(" +inf", 0, common_error);
        FromStringFails.operator()<T>("+ inf", 0, common_error);
        FromStringFails.operator()<T>(" -inf", 0, common_error);
        FromStringFails.operator()<T>("- inf", 0, common_error);

        FromStringFails.operator()<T>(" nan", 0, common_error);
        FromStringFails.operator()<T>(" +nan", 0, common_error);
        FromStringFails.operator()<T>("+ nan", 0, common_error);
        FromStringFails.operator()<T>(" -nan", 0, common_error);
        FromStringFails.operator()<T>("- nan", 0, common_error);
    };
    CheckFloat.operator()<float>();
    CheckFloat.operator()<double>();
    CheckFloat.operator()<long double>();

    { // Strings.
        // Basic sanity, with and without prefixes.
        FromStringPasses(R"("abc")", std::string("abc"));
        FromStringPasses(R"("abc")", std::wstring(L"abc"));
        FromStringPasses(R"(L"abc")", std::wstring(L"abc"));
        FromStringPasses(R"("abc")", std::u8string(u8"abc"));
        FromStringPasses(R"(u8"abc")", std::u8string(u8"abc"));
        FromStringPasses(R"("abc")", std::u16string(u"abc"));
        FromStringPasses(R"(u"abc")", std::u16string(u"abc"));
        FromStringPasses(R"("abc")", std::u32string(U"abc"));
        FromStringPasses(R"(U"abc")", std::u32string(U"abc"));
        // Reject mismatching prefix:
        FromStringFails.operator()<std::string>(R"(u8"a")", 0, "Expected opening `\"`\\.");
        FromStringFails.operator()<std::wstring>(R"(u8"a")", 0, "Expected opening `\"`\\.");

        // Empty strings.
        FromStringPasses(R"("")", std::string{});
        FromStringPasses(R"("")", std::wstring{});
        FromStringPasses(R"("")", std::u8string{});
        FromStringPasses(R"("")", std::u16string{});
        FromStringPasses(R"("")", std::u32string{});

        FromStringFails.operator()<std::string>(R"( "")", 0, "Expected opening `\"`\\.");
        FromStringFails.operator()<std::string>(R"(")", 1, "Expected closing `\"`\\.");
        FromStringFails.operator()<std::string>(R"("x)", 2, "Expected closing `\"`\\.");

        FromStringPasses(R"("abc"x)", std::string("abc"), 1);

        { // Escape sequences.
            // Invalid.
            FromStringFails.operator()<std::string>(R"("\y")", 2, "Invalid escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\A")", 2, "Invalid escape sequence\\."); // Make sure we're case-sensitive.
            FromStringFails.operator()<std::string>(R"("\-1")", 2, "Invalid escape sequence\\."); // Reject numbers with signs.
            FromStringFails.operator()<std::string>(R"("\+1")", 2, "Invalid escape sequence\\."); // Reject numbers with signs.
            FromStringFails.operator()<std::string>(R"("\N")", 2, "Named character escapes are not supported\\."); // Reject named character escapes.

            // Quotes.
            FromStringPasses(R"("X\"Y")", std::string("X\"Y"));
            FromStringPasses(R"("X\'Y")", std::string("X'Y"));
            FromStringPasses(R"("X"Y")", std::string("X"), 2);
            FromStringPasses(R"("X'Y")", std::string("X'Y"));

            // Question mark - meaningless and not supporetd.
            FromStringFails.operator()<std::string>(R"("\?")", 2, "Invalid escape sequence\\.");

            // Common escapes.
            FromStringPasses(R"("X\aY")", std::string("X\aY"));
            FromStringPasses(R"("X\bY")", std::string("X\bY"));
            FromStringPasses(R"("X\fY")", std::string("X\fY"));
            FromStringPasses(R"("X\nY")", std::string("X\nY"));
            FromStringPasses(R"("X\rY")", std::string("X\rY"));
            FromStringPasses(R"("X\tY")", std::string("X\tY"));
            FromStringPasses(R"("X\vY")", std::string("X\vY"));

            // Octal.
            FromStringPasses(R"("X\0Y")", std::string("X\0Y", 3));
            FromStringPasses(R"("X\1Y")", std::string("X\1Y"));
            FromStringPasses(R"("X\2Y")", std::string("X\2Y"));
            FromStringPasses(R"("X\3Y")", std::string("X\3Y"));
            FromStringPasses(R"("X\4Y")", std::string("X\4Y"));
            FromStringPasses(R"("X\5Y")", std::string("X\5Y"));
            FromStringPasses(R"("X\6Y")", std::string("X\6Y"));
            FromStringPasses(R"("X\7Y")", std::string("X\7Y"));
            FromStringFails.operator()<std::string>(R"("\8")", 2, "Invalid escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\9")", 2, "Invalid escape sequence\\.");

            FromStringPasses(R"("X\11Y")", std::string("X\11Y"));
            FromStringPasses(R"("X\111Y")", std::string("X\111Y"));
            FromStringPasses(R"("X\1111Y")", std::string("X\1111Y")); // Consume 3 digits max.
            FromStringPasses(R"("X\377Y")", std::string("X\377Y")); // 255
            FromStringFails.operator()<std::string>(R"("\400")", 1, "This value is not representable in the target character type\\.");
            FromStringFails.operator()<std::string>(R"("\777")", 1, "This value is not representable in the target character type\\.");

            FromStringPasses(R"("X\377Y")", std::u16string(u"X\377Y")); // 255
            FromStringPasses(R"("X\400Y")", std::u16string(u"X\400Y")); // 256
            FromStringPasses(R"("X\777Y")", std::u16string(u"X\777Y")); // 511
            FromStringPasses(R"("X\1111Y")", std::u16string(u"X\1111Y")); // Consume 3 digits max.

            // Octal braced.
            FromStringFails.operator()<std::string>(R"("\o1")", 3, "Expected opening `\\{` in the escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\o{}")", 4, "Expected octal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\o{")", 4, "Expected octal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\o{8")", 4, "Expected octal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\o{x")", 4, "Expected octal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\o{-1}")", 4, "Expected octal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\o{+1}")", 4, "Expected octal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\o{1")", 5, "Expected closing `\\}` in the escape sequence\\.");
            FromStringPasses(R"("X\o{0}Y")", std::string("X\0Y", 3));
            FromStringPasses(R"("X\o{1}Y")", std::string("X\1Y"));
            FromStringPasses(R"("X\o{1}1Y")", std::string("X\1""1Y"));
            FromStringPasses(R"("X\o{11}Y")", std::string("X\11Y"));
            FromStringPasses(R"("X\o{377}Y")", std::string("X\377Y"));
            FromStringPasses(R"("X\o{000000000377}Y")", std::string("X\377Y"));
            FromStringFails.operator()<std::string>(R"("\o{400}")", 1, "This value is not representable in the target character type\\.");
            FromStringFails.operator()<std::string>(R"("\o{1234}")", 1, "This value is not representable in the target character type\\.");
            FromStringFails.operator()<std::string>(R"("\o{37777777777}")", 1, "This value is not representable in the target character type\\."); // 2^32 - 1
            FromStringFails.operator()<std::string>(R"("\o{40000000000}")", 14, "Overflow in escape sequence\\."); // 2^32

            FromStringPasses(R"("X\o{0}Y")", std::u8string(u8"X\0Y", 3));
            FromStringPasses(R"("X\o{1}Y")", std::u8string(u8"X\1Y"));
            FromStringPasses(R"("X\o{11}Y")", std::u8string(u8"X\11Y"));
            FromStringPasses(R"("X\o{377}Y")", std::u8string(u8"X\377Y"));
            FromStringFails.operator()<std::u8string>(R"("\o{400}")", 1, "This value is not representable in the target character type\\.");

            FromStringPasses(R"("X\o{377}Y")", std::u16string(u"X\377Y")); // 255
            FromStringPasses(R"("X\o{177777}Y")", std::u16string(u"X\xffffY")); // 2^16 - 1
            FromStringFails.operator()<std::u16string>(R"("\o{200000}")", 1, "This value is not representable in the target character type\\.");
            FromStringFails.operator()<std::u16string>(R"("\o{40000000000}")", 14, "Overflow in escape sequence\\."); // 2^32

            FromStringPasses(R"("X\o{153777}Y")", std::u16string(u"X\xd7ffY"));
            FromStringPasses(R"("X\o{154000}Y")", std::u16string(u"X\xd800Y")); // Surrogate.
            FromStringPasses(R"("X\o{157777}Y")", std::u16string(u"X\xdfffY")); // Surrogate.
            FromStringPasses(R"("X\o{160000}Y")", std::u16string(u"X\xe000Y"));

            FromStringPasses(R"("X\o{377}Y")", std::u32string(U"X\377Y")); // 255
            FromStringPasses(R"("X\o{177777}Y")", std::u32string(U"X\xffffY")); // 2^16 - 1
            FromStringPasses(R"("X\o{37777777777}Y")", std::u32string(U"X\xffffffffY")); // 2^32 - 1
            FromStringFails.operator()<std::u32string>(R"("\o{40000000000}")", 14, "Overflow in escape sequence\\."); // 2^32

            FromStringPasses(R"("X\o{153777}Y")", std::u32string(U"X\xd7ffY"));
            FromStringPasses(R"("X\o{154000}Y")", std::u32string(U"X\xd800Y")); // Surrogate.
            FromStringPasses(R"("X\o{157777}Y")", std::u32string(U"X\xdfffY")); // Surrogate.
            FromStringPasses(R"("X\o{160000}Y")", std::u32string(U"X\xe000Y"));
            FromStringPasses(R"("X\o{4177777}Y")", std::u32string(U"X\x10ffffY"));
            FromStringPasses(R"("X\o{4200000}Y")", std::u32string(U"X\x110000Y")); // Out-of-range character.

            if constexpr (sizeof(wchar_t) == 2)
            {
                FromStringPasses(R"("X\o{377}Y")", std::wstring(L"X\377Y")); // 255
                FromStringPasses(R"("X\o{177777}Y")", std::wstring(L"X\xffffY")); // 2^16 - 1
                FromStringFails.operator()<std::wstring>(R"("\o{200000}")", 1, "This value is not representable in the target character type\\.");
                FromStringFails.operator()<std::wstring>(R"("\o{40000000000}")", 14, "Overflow in escape sequence\\."); // 2^32
            }
            else
            {
                FromStringPasses(R"("X\o{377}Y")", std::wstring(L"X\377Y")); // 255
                FromStringPasses(R"("X\o{177777}Y")", std::wstring(L"X\xffffY")); // 2^16 - 1
                #ifndef _WIN32 // This doesn't compile for 2-byte `wchar_t`s.
                FromStringPasses(R"("X\o{37777777777}Y")", std::wstring(L"X\xffffffffY")); // 2^32 - 1
                #endif
                FromStringFails.operator()<std::wstring>(R"("\o{40000000000}")", 14, "Overflow in escape sequence\\."); // 2^32
            }

            // Hexadecimal.
            FromStringPasses(R"("X\x1Y")", std::string("X\x1Y"));
            FromStringPasses(R"("X\x1fY")", std::string("X\x1fY"));
            FromStringPasses(R"("X\x1FY")", std::string("X\x1FY"));
            FromStringPasses(R"("X\xfFY")", std::string("X\xffY"));
            FromStringPasses(R"("X\x00000000000fFY")", std::string("X\xffY"));
            FromStringFails.operator()<std::string>(R"("X\x100Y")", 2, "This value is not representable in the target character type\\.");

            // --- u16
            FromStringPasses(R"("X\x1Y")", std::u16string(u"X\x1Y"));
            FromStringPasses(R"("X\x1fY")", std::u16string(u"X\x1fY"));
            FromStringPasses(R"("X\x1f2Y")", std::u16string(u"X\x1f2Y"));
            FromStringPasses(R"("X\x1f2eY")", std::u16string(u"X\x1f2eY"));
            FromStringFails.operator()<std::u16string>(R"("X\x10000Y")", 2, "This value is not representable in the target character type\\.");

            // --- --- Invalid codepints.
            FromStringPasses(R"("X\xd7ffY")", std::u16string(u"X\xd7ffY"));
            FromStringPasses(R"("X\xd800Y")", std::u16string(u"X\xd800Y")); // Surrogate.
            FromStringPasses(R"("X\xdfffY")", std::u16string(u"X\xdfffY")); // Surrogate.
            FromStringPasses(R"("X\xe000Y")", std::u16string(u"X\xe000Y"));

            // --- u32
            FromStringPasses(R"("X\x1Y")", std::u32string(U"X\x1Y"));
            FromStringPasses(R"("X\x1fY")", std::u32string(U"X\x1fY"));
            FromStringPasses(R"("X\x1f2Y")", std::u32string(U"X\x1f2Y"));
            FromStringPasses(R"("X\x1f2eY")", std::u32string(U"X\x1f2eY"));
            FromStringPasses(R"("X\x1f2e3Y")", std::u32string(U"X\x1f2e3Y"));
            FromStringPasses(R"("X\x1f2e3dY")", std::u32string(U"X\x1f2e3dY"));
            FromStringPasses(R"("X\x1f2e3d4Y")", std::u32string(U"X\x1f2e3d4Y"));
            FromStringPasses(R"("X\x1f2e3d4cY")", std::u32string(U"X\x1f2e3d4cY"));
            FromStringFails.operator()<std::u32string>(R"("X\x100000000Y")", 12, "Overflow in escape sequence\\.");

            // --- --- Invalid codepoints.
            FromStringPasses(R"("X\xd7ffY")", std::u32string(U"X\xd7ffY"));
            FromStringPasses(R"("X\xd800Y")", std::u32string(U"X\xd800Y")); // Surrogate.
            FromStringPasses(R"("X\xdfffY")", std::u32string(U"X\xdfffY")); // Surrogate.
            FromStringPasses(R"("X\xe000Y")", std::u32string(U"X\xe000Y"));
            FromStringPasses(R"("X\x10ffffY")", std::u32string(U"X\x10ffffY"));
            FromStringPasses(R"("X\x110000Y")", std::u32string(U"X\x110000Y")); // Out-of-range character.

            // Hexadecimal braced.
            FromStringFails.operator()<std::string>(R"("\x{}")", 4, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\x{")", 4, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\x{x")", 4, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\x{-1}")", 4, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\x{+1}")", 4, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("\x{1")", 5, "Expected closing `\\}` in the escape sequence\\.");
            FromStringPasses(R"("X\x{0}Y")", std::string("X\x0Y", 3));
            FromStringPasses(R"("X\x{1}Y")", std::string("X\x1Y"));
            FromStringPasses(R"("X\x{1}1Y")", std::string("X\x1""1Y"));
            FromStringPasses(R"("X\x{1f}Y")", std::string("X\x1fY"));
            FromStringPasses(R"("X\x{fF}Y")", std::string("X\xffY"));
            FromStringPasses(R"("X\x{0000000000000fF}Y")", std::string("X\xffY"));
            FromStringFails.operator()<std::string>(R"("\x{100}")", 1, "This value is not representable in the target character type\\.");

            // --- u16
            FromStringPasses(R"("X\x{1}Y")", std::u16string(u"X\x1Y"));
            FromStringPasses(R"("X\x{11}Y")", std::u16string(u"X\x11Y"));
            FromStringPasses(R"("X\x{111}Y")", std::u16string(u"X\x111Y"));
            FromStringPasses(R"("X\x{1111}Y")", std::u16string(u"X\x1111Y"));
            FromStringFails.operator()<std::u16string>(R"("\x{10000}")", 1, "This value is not representable in the target character type\\.");

            // --- u32
            FromStringPasses(R"("X\x{1}Y")", std::u32string(U"X\x1Y"));
            FromStringPasses(R"("X\x{1f}Y")", std::u32string(U"X\x1fY"));
            FromStringPasses(R"("X\x{1f1}Y")", std::u32string(U"X\x1f1Y"));
            FromStringPasses(R"("X\x{1f1e}Y")", std::u32string(U"X\x1f1eY"));
            FromStringPasses(R"("X\x{1f1e1}Y")", std::u32string(U"X\x1f1e1Y"));
            FromStringPasses(R"("X\x{1f1e1d}Y")", std::u32string(U"X\x1f1e1dY"));
            FromStringPasses(R"("X\x{1f1e1d1}Y")", std::u32string(U"X\x1f1e1d1Y"));
            FromStringPasses(R"("X\x{1f1e1d1c}Y")", std::u32string(U"X\x1f1e1d1cY"));
            FromStringFails.operator()<std::u32string>(R"("\x{100000000}")", 12, "Overflow in escape sequence\\.");

            // Unicode, 4 digits.
            FromStringFails.operator()<std::string>(R"("X\uY")", 4, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("X\ufY")", 5, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("X\ufFY")", 6, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("X\ufF1Y")", 7, "Expected hexadecimal digit in escape sequence\\.");
            FromStringPasses(R"("X\ufF12Y")", std::string("X\ufF12Y"));
            FromStringPasses(R"("X\ufF123Y")", std::string("X\ufF123Y"));
            FromStringPasses(R"("X\u0000Y")", std::string("X\0Y", 3));

            FromStringPasses(R"("X\ufF123Y")", std::u16string(u"X\ufF123Y"));
            FromStringPasses(R"("X\ufF123Y")", std::u32string(U"X\ufF123Y"));

            // --- Invalid codepoints.
            FromStringPasses(R"("X\ud7ffY")", std::string("X\ud7ffY"));
            FromStringFails.operator()<std::string>(R"("X\ud800Y")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringFails.operator()<std::string>(R"("X\udfffY")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringPasses(R"("X\ue000Y")", std::string("X\ue000Y"));
            // --- --- u16
            FromStringPasses(R"("X\ud7ffY")", std::u16string(u"X\ud7ffY"));
            FromStringFails.operator()<std::u16string>(R"("X\ud800Y")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringFails.operator()<std::u16string>(R"("X\udfffY")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringPasses(R"("X\ue000Y")", std::u16string(u"X\ue000Y"));
            // --- --- u32
            FromStringPasses(R"("X\ud7ffY")", std::u32string(U"X\ud7ffY"));
            FromStringFails.operator()<std::u32string>(R"("X\ud800Y")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringFails.operator()<std::u32string>(R"("X\udfffY")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringPasses(R"("X\ue000Y")", std::u32string(U"X\ue000Y"));

            // Unicode, 8 digits.
            FromStringFails.operator()<std::string>(R"("X\UY")", 4, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("X\UfY")", 5, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("X\UfFY")", 6, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("X\UfF1Y")", 7, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("X\UfF12Y")", 8, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("X\UfF123Y")", 9, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("X\UfF1234Y")", 10, "Expected hexadecimal digit in escape sequence\\.");
            FromStringFails.operator()<std::string>(R"("X\UfF12345Y")", 11, "Expected hexadecimal digit in escape sequence\\.");
            FromStringPasses(R"("X\U0010ffffY")", std::string("X\U0010ffffY"));
            FromStringPasses(R"("X\U0010ffff1Y")", std::string("X\U0010ffff1Y"));
            FromStringPasses(R"("X\U00000000Y")", std::string("X\0Y", 3));

            // --- Invalid codepoints.
            FromStringPasses(R"("X\U0000d7ffY")", std::string("X\U0000d7ffY"));
            FromStringFails.operator()<std::string>(R"("X\U0000d800Y")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringFails.operator()<std::string>(R"("X\U0000dfffY")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringPasses(R"("X\U0000e000Y")", std::string("X\U0000e000Y"));
            FromStringPasses(R"("X\U0010ffffY")", std::string("X\U0010ffffY"));
            FromStringFails.operator()<std::string>(R"("X\U00110000Y")", 2, "Invalid codepoint, larger than 0x10ffff\\."); // Out-of-range character.
            // --- --- u16
            FromStringPasses(R"("X\U0000d7ffY")", std::u16string(u"X\U0000d7ffY"));
            FromStringFails.operator()<std::u16string>(R"("X\U0000d800Y")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringFails.operator()<std::u16string>(R"("X\U0000dfffY")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringPasses(R"("X\U0000e000Y")", std::u16string(u"X\U0000e000Y"));
            FromStringPasses(R"("X\U0010ffffY")", std::u16string(u"X\U0010ffffY"));
            FromStringFails.operator()<std::u16string>(R"("X\U00110000Y")", 2, "Invalid codepoint, larger than 0x10ffff\\."); // Out-of-range character.
            // --- --- u32
            FromStringPasses(R"("X\U0000d7ffY")", std::u32string(U"X\U0000d7ffY"));
            FromStringFails.operator()<std::u32string>(R"("X\U0000d800Y")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringFails.operator()<std::u32string>(R"("X\U0000dfffY")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringPasses(R"("X\U0000e000Y")", std::u32string(U"X\U0000e000Y"));
            FromStringPasses(R"("X\U0010ffffY")", std::u32string(U"X\U0010ffffY"));
            FromStringFails.operator()<std::u32string>(R"("X\U00110000Y")", 2, "Invalid codepoint, larger than 0x10ffff\\."); // Out-of-range character.

            // Unicode, braced.
            FromStringFails.operator()<std::string>(R"("\U{1}")", 3, "Expected hexadecimal digit in escape sequence\\."); // Only lowercase `u` allows braces.
            FromStringPasses(R"("X\u{1}Y")", std::string("X\x01Y"));
            FromStringPasses(R"("X\u{000000000000001036}Y")", std::string("X\u1036Y"));
            FromStringPasses(R"("X\u{0010ffff}Y")", std::string("X\U0010ffffY"));
            FromStringFails.operator()<std::string>(R"("\u{100000000}")", 12, "Overflow in escape sequence\\.");

            // --- Invalid codepoints.
            FromStringPasses(R"("X\u{d7ff}Y")", std::string("X\U0000d7ffY"));
            FromStringFails.operator()<std::string>(R"("X\u{d800}Y")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringFails.operator()<std::string>(R"("X\u{dfff}Y")", 2, "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs\\."); // Surrogate.
            FromStringPasses(R"("X\u{e000}Y")", std::string("X\U0000e000Y"));
            FromStringPasses(R"("X\u{10ffff}Y")", std::string("X\U0010ffffY"));
            FromStringFails.operator()<std::string>(R"("X\u{110000}Y")", 2, "Invalid codepoint, larger than 0x10ffff\\."); // Out-of-range character.
        }

        { // Encoding primitives.
            // Here we test that all the primitives correctly stop at the end-of-string pointer.
            // This isn't used anywhere for now (isn't exposed outside of the primitives), but I still want it to work correctly, in case I need it later.

            { // Decoding.
                // Decoding an empty string shouldn't access any memory.
                auto CheckDecodeEmpty = [&]<typename T>()
                {
                    const T *cur = (const T *)0x100;
                    const T *const end = cur;
                    char32_t ch = 0;
                    const char *error = ta_test::text::encoding::low::DecodeOne(cur, end, ch);
                    TA_CHECK( $[std::string_view(error)] == "Unexpected end of string." );
                    TA_CHECK( cur == end );
                };
                CheckDecodeEmpty.operator()<char>();
                CheckDecodeEmpty.operator()<wchar_t>();
                CheckDecodeEmpty.operator()<char8_t>();
                CheckDecodeEmpty.operator()<char16_t>();
                CheckDecodeEmpty.operator()<char32_t>();

                { // Decoding a cutoff surrogate.
                    const char16_t *cur = u"\U0001FBCA"; // WHITE UP-POINTING CHEVRON
                    const char16_t *const old_cur = cur;
                    const char16_t *const end = cur + 1;
                    char32_t ch = 0;
                    const char *error = ta_test::text::encoding::low::DecodeOne(cur, end, ch);
                    TA_CHECK( $[std::string_view(error)] == "A lone high surrogate not followed by a low surrogate." );
                    TA_CHECK( cur == end ); // Advance by one character, intentionally!
                    TA_CHECK( $[ch] == $[old_cur[0]] ); // The first element of the array.
                }

                { // Decoding an incomplete UTF-8 character.
                    for (int i = 1; i <= 3; i++)
                    {
                        const char8_t *cur = u8"\U0001FBCA"; // WHITE UP-POINTING CHEVRON
                        const char8_t *const old_cur = cur;
                        const char8_t *const end = cur + i;
                        char32_t ch = 0;
                        const char *error = ta_test::text::encoding::low::DecodeOne(cur, end, ch);
                        TA_CHECK( $[std::string_view(error)] == "Incomplete multibyte UTF-8 character." );
                        TA_CHECK( cur == old_cur + 1 ); // Advance by one byte, intentionally!
                        TA_CHECK( $[ch] == $[old_cur[0]] ); // The first byte of the array.
                    }
                }
            }

            { // Unescaping.
                auto ExpectFailure = [&](std::string_view source, std::string_view expected_error)
                {
                    char32_t ch = 0; // This will have an indeterminate value after the error.
                    bool encode = false; // Same.
                    const char *cur = source.data();
                    const char *error = ta_test::text::encoding::low::DecodeAndUnescapeOne(cur, source.data() + source.size(), ch, encode);
                    TA_CHECK( $[error] == $[expected_error] );
                    TA_CHECK( $[std::size_t(cur - source.data())] == $[source.size()] );
                };
                auto ExpectSuccess = [&](std::string_view source, char32_t expected_char)
                {
                    char32_t ch = 0;
                    bool encode = false;
                    const char *cur = source.data();
                    const char *error = ta_test::text::encoding::low::DecodeAndUnescapeOne(cur, source.data() + source.size(), ch, encode);
                    TA_CHECK( $[error] == nullptr );
                    TA_CHECK( $[ch] == $[expected_char] );
                    TA_CHECK( $[std::size_t(cur - source.data())] == $[source.size()] );
                };

                ExpectFailure({"a", 0}, "Unexpected end of string.");

                ExpectFailure({"\\a", 1}, "Incomplete escape sequence at the end of string.");

                ExpectSuccess({"\\123", 2}, 01);
                ExpectSuccess({"\\123", 3}, 012);

                ExpectFailure({"\\x12", 2}, "Expected hexadecimal digit in escape sequence.");
                ExpectSuccess({"\\x12", 3}, 0x1);

                ExpectFailure({"\\u12345", 2}, "Expected hexadecimal digit in escape sequence.");
                ExpectFailure({"\\u12345", 3}, "Expected hexadecimal digit in escape sequence.");
                ExpectFailure({"\\u12345", 4}, "Expected hexadecimal digit in escape sequence.");
                ExpectFailure({"\\u12345", 5}, "Expected hexadecimal digit in escape sequence.");
                ExpectSuccess({"\\u12345", 6}, 0x1234);

                ExpectFailure({"\\U001012345", 2}, "Expected hexadecimal digit in escape sequence.");
                ExpectFailure({"\\U001012345", 3}, "Expected hexadecimal digit in escape sequence.");
                ExpectFailure({"\\U001012345", 4}, "Expected hexadecimal digit in escape sequence.");
                ExpectFailure({"\\U001012345", 5}, "Expected hexadecimal digit in escape sequence.");
                ExpectFailure({"\\U001012345", 6}, "Expected hexadecimal digit in escape sequence.");
                ExpectFailure({"\\U001012345", 7}, "Expected hexadecimal digit in escape sequence.");
                ExpectFailure({"\\U001012345", 8}, "Expected hexadecimal digit in escape sequence.");
                ExpectFailure({"\\U001012345", 9}, "Expected hexadecimal digit in escape sequence.");
                ExpectSuccess({"\\U001012345", 10}, 0x00101234);

                ExpectFailure({"\\o{123}a", 2}, "Expected opening `{` in the escape sequence.");
                ExpectFailure({"\\o{123}a", 3}, "Expected octal digit in escape sequence.");
                ExpectFailure({"\\o{123}a", 4}, "Expected closing `}` in the escape sequence.");
                ExpectFailure({"\\o{123}a", 5}, "Expected closing `}` in the escape sequence.");
                ExpectFailure({"\\o{123}a", 6}, "Expected closing `}` in the escape sequence.");
                ExpectSuccess({"\\o{123}a", 7}, 0123);

                ExpectFailure({"\\x{123}a", 2}, "Expected hexadecimal digit in escape sequence."); // Opening brace isn't mandatory here, hence this message.
                ExpectFailure({"\\x{123}a", 3}, "Expected hexadecimal digit in escape sequence.");
                ExpectFailure({"\\x{123}a", 4}, "Expected closing `}` in the escape sequence.");
                ExpectFailure({"\\x{123}a", 5}, "Expected closing `}` in the escape sequence.");
                ExpectFailure({"\\x{123}a", 6}, "Expected closing `}` in the escape sequence.");
                ExpectSuccess({"\\x{123}a", 7}, 0x123);

                ExpectFailure({"\\u{123}a", 2}, "Expected hexadecimal digit in escape sequence.");
                ExpectFailure({"\\u{123}a", 3}, "Expected hexadecimal digit in escape sequence.");
                ExpectFailure({"\\u{123}a", 4}, "Expected closing `}` in the escape sequence.");
                ExpectFailure({"\\u{123}a", 5}, "Expected closing `}` in the escape sequence.");
                ExpectFailure({"\\u{123}a", 6}, "Expected closing `}` in the escape sequence.");
                ExpectSuccess({"\\u{123}a", 7}, U'\u0123');
            }
        }

        // std::filesystem::path
        #ifdef _WIN32
        FromStringPasses(R"("foo/\u061f/bar")", std::filesystem::path(L"foo/\u061f/bar"));
        FromStringPasses(R"(L"foo/\u061f/bar")", std::filesystem::path(L"foo/\u061f/bar"));
        #else
        FromStringPasses(R"("foo/\u061f/bar")", std::filesystem::path("foo/\u061f/bar"));
        #endif
        FromStringFails.operator()<std::filesystem::path>("x", 0, "Expected opening `\"`.");
    }

    { // Characters.
        FromStringPasses("'a'", 'a');
        FromStringPasses("'\\n'", '\n');
        FromStringPasses("'\\0'", '\0');
        FromStringPasses("'\\xff'", '\xff');
        FromStringPasses("'\\u{12}'", '\u0012');
        FromStringFails.operator()<char>("'\\u{ff}'", 1, "This codepoint doesn't fit into a single character\\.");

        FromStringPasses("'a' ", 'a', 1);
        FromStringFails.operator()<char>(" 'a'", 0, "Expected opening `'`\\.");
        FromStringFails.operator()<char>("''", 1, "Expected a character before the closing `'`\\.");
        FromStringFails.operator()<char>("'aa'", 2, "Expected closing `'`.");

        // char8_t
        FromStringPasses("'a'", u8'a');
        FromStringPasses("u8'a'", u8'a');
        FromStringPasses("'\\n'", u8'\n');
        FromStringPasses("'\\0'", u8'\0');
        FromStringPasses("'\\xff'", u8'\xff');
        FromStringPasses("'\\u{12}'", u8'\u0012');
        FromStringFails.operator()<char8_t>("'\\u{ff}'", 1, "This codepoint doesn't fit into a single character\\.");

        // char16_t
        FromStringPasses("'a'", u'a');
        FromStringPasses("u'a'", u'a');
        FromStringPasses("'\\n'", u'\n');
        FromStringPasses("'\\0'", u'\0');
        FromStringPasses("'\\xff'", u'\xff');
        FromStringPasses("'\\u{12}'", u'\u0012');
        FromStringPasses("'\\u{ff}'", u'\u00ff');
        FromStringPasses("'\\xffff'", u'\xffff');
        FromStringFails.operator()<char16_t>("'\\u{1fbca}'", 1, "This codepoint doesn't fit into a single character\\.");

        // char32_t
        FromStringPasses("'a'", U'a');
        FromStringPasses("U'a'", U'a');
        FromStringPasses("'\\n'", U'\n');
        FromStringPasses("'\\0'", U'\0');
        FromStringPasses("'\\xff'", U'\xff');
        FromStringPasses("'\\u{12}'", U'\u0012');
        FromStringPasses("'\\u{ff}'", U'\u00ff');
        FromStringPasses("'\\xffff'", U'\xffff');
        FromStringPasses("'\\uffff'", U'\uffff');
        FromStringPasses("'\\U0010ffff'", U'\U0010ffff');
        FromStringFails.operator()<char32_t>("'\\u{00110000}'", 1, "Invalid codepoint, larger than 0x10ffff\\.");

        // wchar_t
        FromStringPasses("'a'", L'a');
        FromStringPasses("L'a'", L'a');
        FromStringPasses("'\\n'", L'\n');
        FromStringPasses("'\\0'", L'\0');
        FromStringPasses("'\\xff'", L'\xff');
        FromStringPasses("'\\u{12}'", L'\u0012');
        FromStringPasses("'\\u{ff}'", L'\u00ff');
        FromStringPasses("'\\xffff'", L'\xffff');
        if constexpr (sizeof(wchar_t) == 2)
        {
            FromStringFails.operator()<wchar_t>("'\\u{1fbca}'", 1, "This codepoint doesn't fit into a single character\\.");
        }
        else
        {
            #ifndef _WIN32 // This doesn't compile at all with 2-byte `wchar_t`.
            FromStringPasses("'\\uffff'", L'\uffff');
            FromStringPasses("'\\U0010ffff'", L'\U0010ffff');
            FromStringFails.operator()<wchar_t>("'\\u{00110000}'", 1, "Invalid codepoint, larger than 0x10ffff\\.");
            #endif
        }
    }
}


int main(int argc, char **argv)
{
    return ta_test::RunSimple(argc, argv);
}
