// The main test program.

// It requires following env variables:
// * `VERBOSE` - 0 or 1, whether to enable verbose logging.
// * `COMPILER_COMMAND` - the compiler command that we should be using.
// * `LINKER_FLAGS` - those are added to COMPILER_COMMAND when linking.
// * `OUTPUT_DIR` - where to write the files.
// * `EXT_EXE` - the extension for executables.
// * `EXE_RUNNER` - the wrapper program used to run the executables, if any.

#include <taut/taut.hpp>

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
    // If true, the formatting library lacks the debug string formatter, so we're using our own fallback with a slightly different behavior.
    constexpr bool own_string_formatter = !HasNativeDebufFormat<std::string_view>;

    // Strings and chars.
    TA_CHECK( ta_test::string_conv::ToString("") == R"("")" );
    TA_CHECK( ta_test::string_conv::ToString((const char *)"ab\ncd ef") == R"("ab\ncd ef")" );
    TA_CHECK( ta_test::string_conv::ToString((char *)"ab\ncd ef") == R"("ab\ncd ef")" );
    TA_CHECK( ta_test::string_conv::ToString("ab\ncd ef") == R"("ab\ncd ef")" );
    TA_CHECK( ta_test::string_conv::ToString((char (&)[6])"ab\ncd ef") == R"("ab\ncd ef")" );
    TA_CHECK( ta_test::string_conv::ToString((std::string_view)"ab\ncd ef") == R"("ab\ncd ef")" );
    TA_CHECK( ta_test::string_conv::ToString((std::string)"ab\ncd ef") == R"("ab\ncd ef")" );
    TA_CHECK( ta_test::string_conv::ToString('a') == R"('a')" );
    TA_CHECK( ta_test::string_conv::ToString('\n') == R"('\n')" );

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
            else if (i == '\0' && own_string_formatter)
                escape = "\\0";
            else
            {
                if (own_string_formatter)
                    escape = CFG_TA_FMT_NAMESPACE::format("\\x{{{:02x}}}", i);
                else
                    escape = CFG_TA_FMT_NAMESPACE::format("\\u{{{:x}}}", i);
            }

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
        TA_CHECK( $[ta_test::string_conv::ToString("X\u061fY")] == $[own_string_formatter ? "\"X\u061fY\"" : R"("X\u{61f}Y")"] );

        // What about invalid unicode?
        TA_CHECK( $[ta_test::string_conv::ToString("X\xff\u061f\xef""Y")] == $[own_string_formatter ? "\"X\\x{ff}\u061f\\x{ef}Y\"" : R"("X\x{ff}\u{61f}\x{ef}Y")"] );

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
        TA_CHECK( $[ta_test::string_conv::ToString(char32_t(0x123f567e))] == R"(U'\U{123f567e}')" ); // Out-of-range character.
        TA_CHECK( $[ta_test::string_conv::ToString(std::u32string{char32_t(0x123f567e)})] == R"(U"\U{123f567e}")" ); // Out-of-range character.
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
            TA_CHECK( $[ta_test::string_conv::ToString(wchar_t(0x123f567e))] == R"(L'\U{123f567e}')" ); // Out-of-range character.
            TA_CHECK( $[ta_test::string_conv::ToString(std::wstring{wchar_t(0x123f567e)})] == R"(L"\U{123f567e}")" ); // Out-of-range character.
        }
        TA_CHECK( $[ta_test::string_conv::ToString(L'"')] == R"(L'"')" );
        TA_CHECK( $[ta_test::string_conv::ToString(L'\'')] == R"(L'\'')" );
        TA_CHECK( $[ta_test::string_conv::ToString(L"'")] == R"(L"'")" );
        TA_CHECK( $[ta_test::string_conv::ToString(L"\"")] == R"(L"\"")" );
    }


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
        TA_CHECK( $[ta_test::string_conv::ToString(T(1.23e-09L))] == R"(1.23e-09)" );

        TA_CHECK( $[ta_test::string_conv::ToString(std::numeric_limits<T>::infinity())] == "inf" );
        TA_CHECK( $[ta_test::string_conv::ToString(-std::numeric_limits<T>::infinity())] == "-inf" );
        TA_CHECK( $[ta_test::string_conv::ToString(std::numeric_limits<T>::quiet_NaN())] == "nan" );
        TA_CHECK( $[ta_test::string_conv::ToString(-std::numeric_limits<T>::quiet_NaN())] == "-nan" );
    };
    CheckFloat.operator()<float>();
    CheckFloat.operator()<double>();
    CheckFloat.operator()<long double>();

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


int main(int argc, char **argv)
{
    return ta_test::RunSimple(argc, argv);
}
