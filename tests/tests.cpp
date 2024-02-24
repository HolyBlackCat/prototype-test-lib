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
#include <sstream>
#include <stdexcept>
#include <string>

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

const std::string common_program_prefix = R"(
#include <taut/taut.hpp>
int main(int argc, char **argv) {return ta_test::RunSimple(argc, argv);}
)";


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
    // Strings and chars.
    TA_CHECK( ta_test::string_conv::ToString((const char *)"ab\ncd") == R"("ab\ncd")" );
    TA_CHECK( ta_test::string_conv::ToString((char *)"ab\ncd") == R"("ab\ncd")" );
    TA_CHECK( ta_test::string_conv::ToString("ab\ncd") == R"("ab\ncd")" );
    TA_CHECK( ta_test::string_conv::ToString((char (&)[6])"ab\ncd") == R"("ab\ncd")" );
    TA_CHECK( ta_test::string_conv::ToString((std::string_view)"ab\ncd") == R"("ab\ncd")" );
    TA_CHECK( ta_test::string_conv::ToString((std::string)"ab\ncd") == R"("ab\ncd")" );
    TA_CHECK( ta_test::string_conv::ToString('a') == R"('a')" );
    TA_CHECK( ta_test::string_conv::ToString('\n') == R"('\n')" );

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
    };
    CheckFloat.operator()<float>();
    CheckFloat.operator()<double>();
    CheckFloat.operator()<long double>();
}


int main(int argc, char **argv)
{
    return ta_test::RunSimple(argc, argv);
}
