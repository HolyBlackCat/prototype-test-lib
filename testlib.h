#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <version>

#ifndef CFG_TA_API
#if defined(_WIN32) && CFG_TA_SHARED
#define CFG_TA_API __declspec(dllimport)
#else
#define CFG_TA_API
#endif
#endif

// C++ standard release date.
#ifndef CFG_TA_CXX_STANDARD_DATE
#ifdef _MSC_VER
#define CFG_TA_CXX_STANDARD_DATE _MSVC_LANG // D:<
#else
#define CFG_TA_CXX_STANDARD_DATE __cplusplus
#endif
#endif
// C++ version number.
#ifndef CFG_TA_CXX_STANDARD
#if CFG_TA_CXX_STANDARD_DATE >= 202302
#define CFG_TA_CXX_STANDARD 20
#elif CFG_TA_CXX_STANDARD_DATE >= 202002
#define CFG_TA_CXX_STANDARD 20
#else
#error Need C++20 or newer.
#endif
#endif

// Override to change what we call to terminate the application.
// The logic is mostly copied from `SDL_TriggerBreakpoint()`.
// This can be empty, we follow up with a forced termination anyway.
// Note, this failed for me at least once when resuming from being paused on an exception (didn't show the line number; on Linux Clang).
#ifndef CFG_TA_BREAKPOINT
#if defined(_MSC_VER) // MSVC.
#define CFG_TA_BREAKPOINT() __debugbreak()
#elif __has_builtin(__builtin_debugtrap) // Clang?
#define CFG_TA_BREAKPOINT() __builtin_debugtrap()
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) // x86-64 GCC?
#define CFG_TA_BREAKPOINT() __asm__ __volatile__ ( "int $3\n\t" )
#elif defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__)) // Apple stuff?
#define CFG_TA_BREAKPOINT() __asm__ __volatile__ ( "brk #22\n\t" )
#elif defined(__APPLE__) && defined(__arm__) // More Apple stuff?
#define CFG_TA_BREAKPOINT() __asm__ __volatile__ ( "bkpt #22\n\t" )
#else
#define CFG_TA_BREAKPOINT() // Shrug.
#endif
#endif

// Whether to define `$(...)` as an alias for `TA_ARG(...)`.
#ifndef CFG_TA_USE_DOLLAR
#define CFG_TA_USE_DOLLAR 1
#endif
#if CFG_TA_USE_DOLLAR
#ifdef __clang__
// We can't `push` and `pop` this, since it has to extend to the user code. And inline `_Pragma` in the macro doesn't work too.
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#endif
#endif

// Whether to print file paths in errors in MSVC style.
#ifndef CFG_TA_MSVC_STYLE_ERRORS
#ifdef _MSC_VER
#define CFG_TA_MSVC_STYLE_ERRORS 1
#else
#define CFG_TA_MSVC_STYLE_ERRORS 0
#endif
#endif

// Whether to use `<cxxabi.h>` to demangle names from `typeid(...).name()`.
// Otherwise the names are used as is.
#ifndef CFG_TA_CXXABI_DEMANGLE
#ifndef _MSC_VER
#define CFG_TA_CXXABI_DEMANGLE 1
#else
#define CFG_TA_CXXABI_DEMANGLE 0
#endif
#endif

// A comma-separated list of macro names defined to be equivalent to `TA_ARG`.
// `TA_ARG` itself and optionally `$` are added automatically.
#ifndef CFG_TA_EXTRA_ARG_MACROS
#define CFG_TA_EXTRA_ARG_MACROS
#endif

// Whether to use libfmt instead of `std::format`.
// If you manually override the formatting function using other macros below, this will be ignored.
#ifndef CFG_TA_USE_LIBFMT
#ifdef __cpp_lib_format
#define CFG_TA_USE_LIBFMT 0
#elif __has_include(<fmt/format.h>)
#define CFG_TA_USE_LIBFMT 1
#else
#error ta_test needs a compiler supporting `#include <format>`, or installed libfmt, or a custom formatting function to be configured.
#endif
#endif

// The formatting function, `::std::format` or something similar.
#ifndef CFG_TA_FORMAT
#if CFG_TA_USE_LIBFMT
#include <fmt/format.h>
#define CFG_TA_FORMAT ::fmt::format
#else
#include <format>
#define CFG_TA_FORMAT ::std::format
#endif
#endif

// Whether `{:?}` is a valid format for strings and characters.
#ifndef CFG_TA_FORMAT_SUPPORTS_DEBUG_STRINGS
#if CFG_TA_CXX_STANDARD >= 23 || CFG_TA_USE_LIBFMT
#define CFG_TA_FORMAT_SUPPORTS_DEBUG_STRINGS 1
#else
#define CFG_TA_FORMAT_SUPPORTS_DEBUG_STRINGS 0
#endif
#endif

// Whether we should try to detect the debugger and break on failed assertions, on platforms where we know how to do so.
// NOTE: Only touch this if including the debugger detection code in the binary is somehow problematic.
// If you just want to temporarily disable automatic breakpoints, configure `modules::DebuggerDetector` in `ta_test::Runner` instead.
#ifndef CFG_TA_DETECT_DEBUGGER
#define CFG_TA_DETECT_DEBUGGER 1
#endif

// Whether we should try to detect stdout being attached to an interactive terminal,
#ifndef CFG_TA_DETECT_TERMINAL
#define CFG_TA_DETECT_TERMINAL 1
#endif

// Check condition, immediately fail the test if false.
#define TA_CHECK(...) DETAIL_TA_CHECK(false, throw ::ta_test::InterruptTestException(::ta_test::detail::ConstructInterruptTestException{});, #__VA_ARGS__, __VA_ARGS__)
// Check condition, later fail the test if false.
#define TA_SOFT_CHECK(...) DETAIL_TA_CHECK(true,, #__VA_ARGS__, __VA_ARGS__)

#define DETAIL_TA_CHECK(is_soft_, on_fail_, str_, ...) \
    /* Using `_ta_test_name_tag` to force this to be called only in tests. */\
    /* Using `? :` to force the contextual conversion to `bool`. */\
    if (::ta_test::detail::AssertWrapper<is_soft_, str_, #__VA_ARGS__, __FILE__, __LINE__> _ta_assertion{}; _ta_assertion((__VA_ARGS__) ? true : false)) {} else {\
        if (_ta_assertion.should_break) {CFG_TA_BREAKPOINT(); ::std::terminate();} \
        on_fail_ \
    }

// The expansion is enclosed in `(...)`, which lets you use it e.g. as a sole function argument: `func $(var)`.
#define TA_ARG(...) DETAIL_TA_ARG(__COUNTER__, __VA_ARGS__)
#if CFG_TA_USE_DOLLAR
#define $(...) TA_ARG(__VA_ARGS__)
#endif
#define DETAIL_TA_ARG(counter, ...) \
    /* Note the parentheses, they allow this to be transparently used e.g. as a single function parameter. */\
    /* Passing `counter` the second time is redundant, but helps with our parsing. */\
    (::ta_test::detail::ArgWrapper(__FILE__, __LINE__, counter)._ta_handle_arg_(counter, __VA_ARGS__))

#define TA_TEST(name) DETAIL_TA_TEST(name)

#define DETAIL_TA_TEST(name) \
    inline void _ta_test_func(::ta_test::ConstStringTag<#name>); \
    constexpr auto _ta_registration_helper(::ta_test::ConstStringTag<#name>) -> decltype(void(::std::integral_constant<\
        const std::nullptr_t *, &::ta_test::detail::register_test_helper<\
            ::ta_test::detail::SpecificTest<static_cast<void(*)(\
                ::ta_test::ConstStringTag<#name>\
            )>(_ta_test_func), #name, __FILE__, __LINE__>\
        >\
    >{})) {} \
    inline void _ta_test_func(::ta_test::ConstStringTag<#name>)

namespace ta_test
{
    struct Runner;
    struct BasicModule;

    // Parsing command line arguments.
    namespace flags
    {
        // The common base for command line flags.
        struct BasicFlag
        {
            // The description of this flag in the help menu.
            std::string help_desc;

            BasicFlag(std::string help_desc) : help_desc(std::move(help_desc)) {}

            // The spelling of this flag in the help menu, such as `--foo`, possibly with extra decorations around.
            virtual std::string HelpFlagSpelling() const = 0;

            // Given a string, try matching it against this flag. Return true if matched.
            // Call `request_arg` if you need an extra argument. You can call it any number of times to request extra arguments.
            // If `request_arg` runs out of arguments and returns null, you can immediately return false too, this will be reported as an error regardless.
            virtual bool ProcessFlag(Runner &runner, BasicModule &this_module, std::string_view input, std::function<std::optional<std::string_view>()> request_arg) = 0;
        };

        // A command line flag taking no arguments.
        struct SimpleFlag : BasicFlag
        {
            std::string flag;

            using Callback = std::function<void(Runner &runner, BasicModule &this_module)>;
            Callback callback;

            SimpleFlag(std::string flag, std::string help_desc, Callback callback)
                : BasicFlag(std::move(help_desc)), flag(std::move(flag)), callback(std::move(callback))
            {}

            std::string HelpFlagSpelling() const override
            {
                return "--" + flag;
            }

            bool ProcessFlag(Runner &runner, BasicModule &this_module, std::string_view input, std::function<std::optional<std::string_view>()> request_arg) override
            {
                (void)request_arg;

                if (!input.starts_with("--"))
                    return false;
                input.remove_prefix(2);

                if (input != flag)
                    return false;

                callback(runner, this_module);
                return true;
            }
        };

        // A command line flag for a boolean.
        struct BoolFlag : BasicFlag
        {
            std::string flag;

            using Callback = std::function<void(Runner &runner, BasicModule &this_module, bool value)>;
            Callback callback;

            BoolFlag(std::string flag, std::string help_desc, Callback callback)
                : BasicFlag(std::move(help_desc)), flag(std::move(flag)), callback(std::move(callback))
            {}

            std::string HelpFlagSpelling() const override
            {
                return "--[no-]" + flag;
            }

            bool ProcessFlag(Runner &runner, BasicModule &this_module, std::string_view input, std::function<std::optional<std::string_view>()> request_arg) override
            {
                (void)request_arg;

                if (!input.starts_with("--"))
                    return false;
                input.remove_prefix(2);

                bool value = true;
                if (input.starts_with("no-"))
                {
                    value = false;
                    input.remove_prefix(3);
                }

                if (input != flag)
                    return false;

                callback(runner, this_module, value);
                return true;
            }
        };

        // A command line flag that takes a string.
        struct StringFlag : BasicFlag
        {
            std::string flag;

            using Callback = std::function<void(Runner &runner, BasicModule &this_module, std::string_view value)>;
            Callback callback;

            StringFlag(std::string flag, std::string help_desc, Callback callback)
                : BasicFlag(std::move(help_desc)), flag(std::move(flag)), callback(std::move(callback))
            {}

            std::string HelpFlagSpelling() const override
            {
                return "--" + flag + " ...";
            }

            bool ProcessFlag(Runner &runner, BasicModule &this_module, std::string_view input, std::function<std::optional<std::string_view>()> request_arg) override
            {
                (void)request_arg;

                if (!input.starts_with("--"))
                    return false;
                input.remove_prefix(2);

                if (input != flag)
                    return false;

                auto arg = request_arg();
                if (!arg)
                    return false;

                callback(runner, this_module, *arg);
                return true;
            }
        };
    }

    // The common base of all modules.
    struct BasicModule
    {
        virtual ~BasicModule() = default;

        BasicModule() = default;
        BasicModule(const BasicModule &) = default;
        BasicModule(BasicModule &&) = default;
        BasicModule &operator=(const BasicModule &) = default;
        BasicModule &operator=(BasicModule &&) = default;

        // --- PARSING COMMAND LINE ARGUMENTS ---


        // Should return a list of the supported command line flags.
        // Store the flags permanently in your class, those pointers are obviously non-owning.
        [[nodiscard]] virtual std::vector<flags::BasicFlag *> GetFlags() {return {};}
        // This is called when an unknown flag is passed to the command line.
        // `abort` defaults to true. If it remains true after this is called on all modules, the application is terminated.
        virtual void OnUnknownFlag(std::string_view flag, bool &abort) {(void)flag; (void)abort;}
        // Same, but for when a flag lacks an argument.
        virtual void OnMissingFlagArgument(std::string_view flag, const flags::BasicFlag &flag_obj, bool &abort) {(void)flag; (void)flag_obj; (void)abort;}

        // --- RUNNING TESTS ---

        // A simple source location.
        struct SourceLoc
        {
            std::string_view file;
            int line = 0;
            friend auto operator<=>(const SourceLoc &, const SourceLoc &) = default;
        };
        // A source location augumented with the value of `__COUNTER__`.
        struct SourceLocCounter
        {
            std::string_view file;
            int line = 0;
            int counter = 0;
            friend auto operator<=>(const SourceLocCounter &, const SourceLocCounter &) = default;
        };

        struct BasicTestInfo
        {
          protected:
            ~BasicTestInfo() = default;

          public:
            // The name passed to the test macro.
            [[nodiscard]] virtual std::string_view Name() const = 0;

            // Where the test was declared.
            [[nodiscard]] virtual SourceLoc Location() const = 0;
        };
        // Whether the test should run.
        // This is called once for every test, with `enable` initially set to true. If it ends up false, the test is skipped.
        virtual void OnFilterTest(const BasicTestInfo &test, bool &enable) {(void)test; (void)enable;}

        struct RunTestsInfo
        {
            Runner *runner = nullptr;

            std::size_t num_tests = 0;
        };
        struct RunTestsResults : RunTestsInfo
        {
            std::size_t num_failed_tests = 0;
        };
        // This is called first, before any tests run.
        virtual void OnPreRunTests(const RunTestsInfo &data) {(void)data;}
        // This is called after all tests run.
        virtual void OnPostRunTests(const RunTestsResults &data) {(void)data;}

        struct SingleTestInfo
        {
            const RunTestsResults *all_tests = nullptr;
            const BasicTestInfo *test = nullptr;
        };
        struct SingleTestResults : SingleTestInfo
        {
            bool failed = false;
        };
        // This is called before every single test runs.
        virtual void OnPreRunSingleTest(const SingleTestInfo &data) {(void)data;}
        // This is called after every single test runs.
        virtual void OnPostRunSingleTest(const SingleTestResults &data) {(void)data;}

        // --- FAILING TESTS ---

        struct BasicAssertionInfo
        {
          protected:
            ~BasicAssertionInfo() = default;

          public:
            // You can set this to true to trigger a breakpoint.
            mutable bool should_break = false;

            // The enclosing assertion, if any.
            const BasicAssertionInfo *enclosing_assertion = nullptr;

            // Where the assertion is located in the source.
            [[nodiscard]] virtual const SourceLoc &SourceLocation() const = 0;

            // Whether this is a 'soft' assertion, i.e. doesn't immediately fail the test when false.
            [[nodiscard]] virtual bool IsSoft() const = 0;

            // How many assertions are currently running.
            [[nodiscard]] int AssertionStackDepth() const
            {
                int ret = 0;
                for (auto cur = this; cur; cur = cur->enclosing_assertion)
                    ret++;
                return ret;
            }

            // The exact code passed to the assertion macro, as a string. Before macro expansion.
            [[nodiscard]] virtual std::string_view Expr() const = 0;

            // How many `$(...)` arguments are in this assertion.
            [[nodiscard]] std::size_t NumArgs() const {return ArgsInfo().size();}

            // Misc information about an argument.
            struct ArgInfo
            {
                // The value of `__COUNTER__`.
                int counter = 0;
                // Parentheses nesting depth.
                std::size_t depth = 0;

                // Where this argument is located in the expression string.
                std::size_t expr_offset = 0;
                std::size_t expr_size = 0;
                // This is where the argument macro name is located in the expression string.
                std::size_t ident_offset = 0;
                std::size_t ident_size = 0;

                // Whether this argument has a complex enough spelling to require drawing a horizontal bracket.
                // This should be automatically true for all arguments with nested arguments.
                bool need_bracket = false;
            };

            // A stored argument representation as a string.
            struct StoredArg
            {
                enum class State
                {
                    not_started, // No value yet.
                    in_progress, // Started calculating, but no value yet.
                    done, // Has value.
                };
                State state = State::not_started;

                std::string value;
            };

            // All those functions return spans with size `NumArgs()`.
            // Information about each argument.
            [[nodiscard]] virtual std::span<const ArgInfo> ArgsInfo() const = 0;
            // Indices of the arguments (0..N-1), sorted in the preferred draw order.
            [[nodiscard]] virtual std::span<const std::size_t> ArgsInDrawOrder() const = 0;
            // The current values of the arguments.
            [[nodiscard]] virtual std::span<const StoredArg> StoredArgs() const = 0;

            // Gets the `StoredArg` from its location in the source. Returns null on failure.
            // Mostly for internal use.
            [[nodiscard]] virtual const StoredArg *FindStoredArgumentByLocation(const SourceLocCounter &loc) const = 0;
        };
        virtual void OnAssertionFailed(const BasicAssertionInfo &data) {(void)data;}

        // An exception falls out of a test. This is called right before the test fails.
        virtual void OnUncaughtException(const std::exception_ptr &e) {(void)e;}

        // --- MISC ---

        struct ExceptionInfo
        {
            // This is usually obtained by calling `text::Demangler{}(typeid(e).name())`.
            std::string type_name;

            // This is usually obtained from `e.what()`.
            std::string message;
        };
        struct ExceptionInfoWithNested : ExceptionInfo
        {
            // The nested exception, if any.
            std::exception_ptr nested_exception;
        };
        // This is called when an exception needs to be converted to a string.
        // Return the information on your custom exception type, if they don't inherit from `std::exception`.
        // Do nothing (or throw) to let some other module handle this.
        [[nodiscard]] virtual std::optional<ExceptionInfoWithNested> OnExplainException(const std::exception_ptr &e) const {(void)e; return {};}
    };

    // A pointer to a class derived from `BasicModule`.
    class ModulePtr
    {
        std::unique_ptr<BasicModule> ptr;

        template <std::derived_from<BasicModule> T, typename ...P>
        requires std::constructible_from<T, P &&...>
        friend ModulePtr MakeModule(P &&... params);

      public:
        constexpr ModulePtr() {}
        constexpr ModulePtr(std::nullptr_t) {}

        [[nodiscard]] explicit operator bool() const {return bool(ptr);}

        [[nodiscard]] BasicModule *get() const {return ptr.get();}
        [[nodiscard]] BasicModule &operator*() const {return *ptr;}
        [[nodiscard]] BasicModule *operator->() const {return ptr.get();}
    };
    // Allocates a new module as a `ModulePtr`.
    template <std::derived_from<BasicModule> T, typename ...P>
    requires std::constructible_from<T, P &&...>
    [[nodiscard]] ModulePtr MakeModule(P &&... params)
    {
        ModulePtr ret;
        ret.ptr = std::make_unique<T>(std::forward<P>(params)...);
        return ret;
    }

    enum class HardErrorKind {internal, user};
    // Aborts the application with an error.
    [[noreturn]] CFG_TA_API void HardError(std::string_view message, HardErrorKind kind = HardErrorKind::internal);

    // Don't touch this.
    namespace detail
    {
        // The global per-thread state.
        struct GlobalThreadState
        {
            BasicModule::SingleTestResults *current_test = nullptr;
            BasicModule::BasicAssertionInfo *current_assertion = nullptr;

            // Finds an argument in the currently running assertions.
            // Results in a hard error on failure.
            BasicModule::BasicAssertionInfo::StoredArg &FindStoredArgument(const BasicModule::SourceLocCounter &loc)
            {
                const BasicModule::BasicAssertionInfo *cur = current_assertion;
                while (cur)
                {
                    if (auto ret = cur->FindStoredArgumentByLocation(loc))
                        return const_cast<BasicModule::BasicAssertionInfo::StoredArg &>(*ret);
                    cur = cur->enclosing_assertion;
                }

                HardError("This `$(...)` isn't enclosed in any assertion macro.", HardErrorKind::user);
            }
        };
        [[nodiscard]] CFG_TA_API GlobalThreadState &ThreadState();

        // A tag to stop users from constructing our exception types.
        struct ConstructInterruptTestException
        {
            explicit ConstructInterruptTestException() = default;
        };
    }

    // We throw this to abort a test. Don't throw this manually.
    // You can catch and rethrow this before a `catch (...)` to still be able to abort tests inside one.
    struct InterruptTestException
    {
        // For internal use.
        constexpr InterruptTestException(detail::ConstructInterruptTestException) {}
    };

    // A compile-time string.
    template <std::size_t N>
    struct ConstString
    {
        char str[N]{};

        static constexpr std::size_t size = N - 1;

        consteval ConstString() {}
        consteval ConstString(const char (&new_str)[N])
        {
            if (new_str[N-1] != '\0')
                HardError("The input string must be null-terminated.");
            std::copy_n(new_str, size, str);
        }

        [[nodiscard]] constexpr std::string_view view() const
        {
            return {str, str + size};
        }
    };
    template <ConstString X>
    struct ConstStringTag
    {
        static constexpr auto value = X;
    };


    // --- PLATFORM INFORMATION ---

    namespace platform
    {
        // Whether the debugger is currently attached.
        // `false` if unknown or disabled with `CFG_TA_DETECT_DEBUGGER`
        CFG_TA_API bool IsDebuggerAttached();

        // Whether stdout is attached to a terminal.
        CFG_TA_API bool IsTerminalAttached();
    }


    // --- PRINTING AND STRINGS ---

    // Text color.
    enum class TextColor
    {
        // 16 colors palette.
        // The values are the foreground text colors. Add 10 to make background colors.
        none = 39,
        dark_black = 30,
        dark_red = 31,
        dark_green = 32,
        dark_yellow = 33,
        dark_blue = 34,
        dark_magenta = 35,
        dark_cyan = 36,
        dark_white = 37,
        light_black = 90,
        light_red = 91,
        light_green = 92,
        light_yellow = 93,
        light_blue = 94,
        light_magenta = 95,
        light_cyan = 96,
        light_white = 97,

        // Extended colors:
        // First 16 map to the ones listed above.
        // The next 216 are 6-bits-per-channel RGB: R*36 + G*6 + B.
        // The remaining 24 are shades of gray, from black to almost white.
        extended = 256,
        extended_end = extended + 256,
    };
    // Creates a 6-bit-per-channel extended terminal color. Each component must be `0 <= x < 6`.
    [[nodiscard]] constexpr TextColor TextColorRgb6(int r, int g, int b)
    {
        return TextColor(int(TextColor::extended) + 16 + r * 36 + g * 6 + b);
    }
    // Creates a grayscale color, with `n == 0` for black and `n == 24` (sic, not 23) for pure white.
    // `n` is clamped to `0..24`.
    [[nodiscard]] constexpr TextColor TextColorGrayscale24(int n)
    {
        if (n < 0)
            n = 0;
        else if (n >= 24)
            return TextColorRgb6(5,5,5);
        return TextColor(int(TextColor::extended) + 232 + n);
    }

    // Text style.
    struct TextStyle
    {
        TextColor color = TextColor::none;
        TextColor bg_color = TextColor::none;
        bool bold = false;
        bool italic = false;
        bool underline = false;

        friend bool operator==(const TextStyle &, const TextStyle &) = default;
    };

    // Configuration for printing text.
    struct Terminal
    {
        bool enable_color = platform::IsTerminalAttached();

        // The characters are written to this `std::vprintf`-style callback.
        // Defaults to `SetFileOutput(stdout)`.
        std::function<void(const char *format, std::va_list args)> output_func;

        CFG_TA_API Terminal();

        // Sets `output_func` to print to `stream`.
        CFG_TA_API void SetFileOutput(FILE *stream);

        // Prints a message using `output_func`.
        #ifdef __GNUC__
        __attribute__((__format__(__printf__, 2, 3)))
        #endif
        CFG_TA_API void Print(const char *format, ...) const;

        // Printing this string resets the text styles. It's always null-terminated.
        [[nodiscard]] CFG_TA_API std::string_view AnsiResetString() const;

        // Should be large enough.
        using AnsiDeltaStringBuffer = std::array<char, 100>;

        // Produces a string to switch between text styles, from `prev` to `cur`.
        // If the styles are the same, does nothing.
        [[nodiscard]] CFG_TA_API AnsiDeltaStringBuffer AnsiDeltaString(const TextStyle &&cur, const TextStyle &next) const;

        // This overload additionally performs `cur = next`.
        [[nodiscard]] AnsiDeltaStringBuffer AnsiDeltaString(TextStyle &cur, const TextStyle &next) const
        {
            AnsiDeltaStringBuffer ret = AnsiDeltaString(std::move(cur), next);
            cur = next;
            return ret;
        }
    };

    struct CommonStyles
    {
        TextStyle error = {.color = TextColor::light_red, .bold = true};
        TextStyle note = {.color = TextColor::light_blue, .bold = true};
    };

    // Common characters we use to print stuff.
    struct CommonChars
    {
        std::string note_prefix = "NOTE: ";

        // When printing a path, separates it from the line number.
        std::string filename_linenumber_separator;
        // When printing a path with a line number, this comes after the line number.
        std::string filename_linenumber_suffix;

        // Vertical bars, either standalone or in brackets.
        char32_t bar{};
        // Bottom brackets.
        char32_t bracket_bottom{};
        char32_t bracket_corner_bottom_left{};
        char32_t bracket_corner_bottom_right{};
        char32_t bracket_bottom_tail{};
        // Top brackets.
        char32_t bracket_top{};
        char32_t bracket_corner_top_left{};
        char32_t bracket_corner_top_right{};

        CommonChars()
        {
            EnableUnicode(true);
            EnableMsvcStylePaths(
                #if CFG_TA_MSVC_STYLE_ERRORS
                true
                #else
                false
                #endif
            );
        }

        void EnableUnicode(bool enable)
        {
            if (enable)
            {
                bar = 0x2502; // BOX DRAWINGS LIGHT VERTICAL
                bracket_bottom = 0x2500; // BOX DRAWINGS LIGHT HORIZONTAL
                bracket_corner_bottom_left = 0x2570; // BOX DRAWINGS LIGHT ARC UP AND RIGHT
                bracket_corner_bottom_right = 0x256f; // BOX DRAWINGS LIGHT ARC UP AND LEFT
                bracket_bottom_tail = 0x252c; // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
                bracket_top = 0x2500; // BOX DRAWINGS LIGHT HORIZONTAL
                bracket_corner_top_left = 0x256d; // BOX DRAWINGS LIGHT ARC DOWN AND RIGHT
                bracket_corner_top_right = 0x256e; // BOX DRAWINGS LIGHT ARC DOWN AND LEFT
            }
            else
            {
                bar = '|';
                bracket_bottom = '_';
                bracket_corner_bottom_left = '|';
                bracket_corner_bottom_right = '|';
                bracket_bottom_tail = '_';
                bracket_top = '-';
                bracket_corner_top_left = '|';
                bracket_corner_top_right = '|';
            }
        }

        void EnableMsvcStylePaths(bool enable)
        {
            if (enable)
            {
                filename_linenumber_separator = "(";
                filename_linenumber_suffix = "):";
            }
            else
            {
                filename_linenumber_separator = ":";
                filename_linenumber_suffix = ":";
            }
        }

        // Converts a source location to a string in the current preferred format.
        [[nodiscard]] std::string LocationToString(const BasicModule::SourceLoc &loc)
        {
            return CFG_TA_FORMAT("{}{}{}{}", loc.file, filename_linenumber_separator, loc.line, filename_linenumber_suffix);
        }
    };

    // Text processing functions.
    namespace text
    {
        [[nodiscard]] constexpr bool IsWhitespace(char ch)
        {
            return ch == ' ' || ch == '\t';
        }
        [[nodiscard]] constexpr bool IsAlphaLowercase(char ch)
        {
            return ch >= 'a' && ch <= 'z';
        }
        [[nodiscard]] constexpr bool IsAlphaUppercase(char ch)
        {
            return ch >= 'A' && ch <= 'Z';
        }
        [[nodiscard]] constexpr bool IsAlpha(char ch)
        {
            return IsAlphaLowercase(ch) || IsAlphaUppercase(ch);
        }
        // Whether `ch` is a letter or an other non-digit identifier character.
        [[nodiscard]] constexpr bool IsNonDigitIdentifierChar(char ch)
        {
            return ch == '_' || IsAlpha(ch);
        }
        [[nodiscard]] constexpr bool IsDigit(char ch)
        {
            return ch >= '0' && ch <= '9';
        }
        // Whether `ch` can be a part of an identifier.
        [[nodiscard]] constexpr bool IsIdentifierCharStrict(char ch)
        {
            return IsNonDigitIdentifierChar(ch) || IsDigit(ch);
        }
        // Same, but also allows `$`, which we use in our macro.
        [[nodiscard]] constexpr bool IsIdentifierChar(char ch)
        {
            if (ch == '$')
                return true; // Non-standard, but all modern compilers seem to support it, and we use it in our optional short macros.
            return IsIdentifierCharStrict(ch);
        }
        // Returns true if `name` is `"TA_ARG"` or one of its aliases.
        [[nodiscard]] constexpr bool IsArgMacroName(std::string_view name)
        {
            for (std::string_view alias : std::initializer_list<std::string_view>{
                "TA_ARG",
                #if CFG_TA_USE_DOLLAR
                "$"
                #endif
                CFG_TA_EXTRA_ARG_MACROS
            })
            {
                if (alias == name)
                    return true;
            }

            return false;
        }

        // A mini unicode library.
        namespace uni
        {
            // A placeholder value for invalid characters.
            constexpr char32_t default_char = 0xfffd;

            // Max bytes per character.
            constexpr std::size_t max_char_len = 4;

            // Given a byte, checks if it's the first byte of a multibyte character, or is a single-byte character.
            // Even if this function returns true, `byte` can be an invalid first byte.
            // To check for the byte validity, use `FirstByteToCharacterLength`.
            [[nodiscard]] constexpr bool IsFirstByte(char byte)
            {
                return (byte & 0b11000000) != 0b10000000;
            }

            // Given the first byte of a multibyte character (or a single-byte character), returns the amount of bytes occupied by the character.
            // Returns 0 if this is not a valid first byte, or not a first byte at all.
            [[nodiscard]] constexpr std::size_t FirstByteToCharacterLength(char first_byte)
            {
                if ((first_byte & 0b10000000) == 0b00000000) return 1; // Note the different bit pattern in this one.
                if ((first_byte & 0b11100000) == 0b11000000) return 2;
                if ((first_byte & 0b11110000) == 0b11100000) return 3;
                if ((first_byte & 0b11111000) == 0b11110000) return 4;
                return 0;
            }

            // Returns true if `ch` is a valid unicode ch (aka 'codepoint').
            [[nodiscard]] constexpr bool IsValidCharacterCode(char32_t ch)
            {
                return ch <= 0x10ffff;
            }

            // Returns the amount of bytes needed to represent a character.
            // If the character is invalid (use `IsValidCharacterCode` to check for validity) returns 4, which is the maximum possible length
            [[nodiscard]] constexpr std::size_t CharacterCodeToLength(char32_t ch)
            {
                if (ch <= 0x7f) return 1;
                if (ch <= 0x7ff) return 2;
                if (ch <= 0xffff) return 3;
                // Here `ch <= 0x10ffff`, or the character is invalid.
                // Mathematically the cap should be `0x1fffff`, but Unicode defines the max value to be lower.
                return 4;
            }

            // Encodes a character into UTF8.
            // The minimal buffer length can be determined with `CharacterCodeToLength`.
            // If the character is invalid, writes `default_char` instead.
            // No null-terminator is added.
            // Returns the amount of bytes written, equal to what `CharacterCodeToLength` would return.
            [[nodiscard]] constexpr std::size_t EncodeCharToBuffer(char32_t ch, char *buffer)
            {
                if (!IsValidCharacterCode(ch))
                    return EncodeCharToBuffer(default_char, buffer);

                std::size_t len = CharacterCodeToLength(ch);
                switch (len)
                {
                  case 1:
                    *buffer = char(ch);
                    break;
                  case 2:
                    *buffer++ = char(0b11000000 | (ch >> 6));
                    *buffer   = char(0b10000000 | (ch & 0b00111111));
                    break;
                  case 3:
                    *buffer++ = char(0b11100000 |  (ch >> 12));
                    *buffer++ = char(0b10000000 | ((ch >>  6) & 0b00111111));
                    *buffer   = char(0b10000000 | ( ch        & 0b00111111));
                    break;
                  case 4:
                    *buffer++ = char(0b11110000 |  (ch >> 18));
                    *buffer++ = char(0b10000000 | ((ch >> 12) & 0b00111111));
                    *buffer++ = char(0b10000000 | ((ch >> 6 ) & 0b00111111));
                    *buffer   = char(0b10000000 | ( ch        & 0b00111111));
                    break;
                }

                return len;
            }

            // Encodes one string into another.
            // We accept the string by reference to reuse the buffer, if any. Old contents are discarded.
            constexpr void Encode(std::u32string_view view, std::string &str)
            {
                str.clear();
                str.reserve(view.size() * max_char_len);
                for (char32_t ch : view)
                {
                    char buf[max_char_len];
                    std::size_t len = EncodeCharToBuffer(ch, buf);
                    str.append(buf, len);
                }
            }

            // Decodes a UTF8 character.
            // Returns a pointer to the first byte of the next character.
            // If `end` is not null, it'll stop reading at `end`. In this case `end` will be returned.
            [[nodiscard]] constexpr const char *FindNextCharacter(const char *data, const char *end = nullptr)
            {
                do
                    data++;
                while (data != end && !IsFirstByte(*data));

                return data;
            }

            // Returns a decoded character or `default_char` on failure.
            // If `end` is not null, it won't attempt to read past it.
            // If `next_char` is not null, it will be set to point to the next byte after the current character.
            // If `data == end`, returns '\0'. (If `end != 0` and `data > end`, also returns '\0'.)
            // If `data == 0`, returns '\0'.
            constexpr char32_t DecodeCharFromBuffer(const char *data, const char *end = nullptr, const char **next_char = nullptr)
            {
                // Stop if `data` is a null pointer.
                if (!data)
                {
                    if (next_char)
                        *next_char = nullptr;
                    return 0;
                }

                // Stop if we have an empty string.
                if (end && data >= end) // For `data >= end` to be well-defined, `end` has to be not null if `data` is not null.
                {
                    if (next_char)
                        *next_char = data;
                    return 0;
                }

                // Get character length.
                std::size_t len = FirstByteToCharacterLength(*data);

                // Stop if this is not a valid first byte.
                if (len == 0)
                {
                    if (next_char)
                        *next_char = FindNextCharacter(data, end);
                    return default_char;
                }

                // Handle single byte characters.
                if (len == 1)
                {
                    if (next_char)
                        *next_char = data+1;
                    return (unsigned char)*data;
                }

                // Stop if there is not enough characters left in `data`.
                if (end && end - data < std::ptrdiff_t(len))
                {
                    if (next_char)
                        *next_char = end;
                    return default_char;
                }

                // Extract bits from the first byte.
                char32_t ret = (unsigned char)*data & (0xff >> len); // `len + 1` would have the same effect as `len`, but it's longer to type.

                // For each remaining byte...
                for (std::size_t i = 1; i < len; i++)
                {
                    // Stop if it's a first byte of some character.
                    if (IsFirstByte(data[i]))
                    {
                        if (next_char)
                            *next_char = data + i;
                        return default_char;
                    }

                    // Extract bits and append them to the code.
                    ret = ret << 6 | ((unsigned char)data[i] & 0b00111111);
                }

                // Get next character position.
                if (next_char)
                    *next_char = data + len;

                return ret;
            }

            // Decodes one string into another.
            // We accept the string by reference to reuse the buffer, if any. Old contents are discarded.
            inline void Decode(std::string_view view, std::u32string &str)
            {
                str.clear();
                str.reserve(view.size());
                for (const char *cur = view.data(); cur - view.data() < std::ptrdiff_t(view.size());)
                    str += uni::DecodeCharFromBuffer(cur, view.data() + view.size(), &cur);
            }
        }

        // Demangles output from `typeid(...).name()`.
        class Demangler
        {
            #if CFG_TA_CXXABI_DEMANGLE
            char *buf_ptr = nullptr;
            std::size_t buf_size = 0;
            #endif

          public:
            CFG_TA_API Demangler();
            Demangler(const Demangler &) = delete;
            Demangler &operator=(const Demangler &) = delete;
            CFG_TA_API ~Demangler();

            // Demangles a name.
            // On GCC ang Clang invokes `__cxa_demangle()`, on MSVC returns the string unchanged.
            // The returned pointer remains as long as both the passed string and the class instance are alive.
            // Preserve the class instance between calls to potentially reuse the buffer.
            [[nodiscard]] CFG_TA_API const char *operator()(const char *name);
        };

        // Given an exception, tries to get an error message from it, using the current modules. Shouldn't throw.
        // Calls `func` at least once. The last call in the chain may or may not receive null `info` if this is an unknown exception.
        // Hence, for completely unknown exceptions, calls `func(nullptr)`.
        // Calls `func` multiple times for nested exceptions.
        CFG_TA_API void GetExceptionInfo(const std::exception_ptr &e, const std::function<void(const BasicModule::ExceptionInfo *info)> &func);

        // A class for composing 2D ASCII graphics.
        class TextCanvas
        {
          public:
            // Describes a cell, except for the character it stores.
            struct CellInfo
            {
                TextStyle style;
                bool important = false; // If this is true, will avoid overwriting this cell.
            };

          private:
            struct Line
            {
                std::u32string text;
                std::vector<CellInfo> info;
            };
            std::vector<Line> lines;

            CommonChars *chars = nullptr;

          public:
            TextCanvas(CommonChars *chars) : chars(chars) {}

            // Prints the canvas to a callback `func`, which is `(std::string_view string) -> void`.
            template <typename F>
            void PrintToCallback(const Terminal &terminal, F &&func) const
            {
                TextStyle cur_style;

                std::string buffer;

                for (const Line &line : lines)
                {
                    std::size_t segment_start = 0;

                    auto FlushSegment = [&](std::size_t end_pos)
                    {
                        if (segment_start == end_pos)
                            return;

                        uni::Encode(std::u32string_view(line.text.begin() + std::ptrdiff_t(segment_start), line.text.begin() + std::ptrdiff_t(end_pos)), buffer);
                        func(std::string_view(buffer));
                        segment_start = end_pos;
                    };

                    if (terminal.enable_color)
                    {
                        for (std::size_t i = 0; i < line.text.size(); i++)
                        {
                            if (line.text[i] == ' ')
                                continue;

                            FlushSegment(i);
                            func(std::string_view(terminal.AnsiDeltaString(cur_style, line.info[i].style).data()));
                        }
                    }

                    FlushSegment(line.text.size());

                    // Reset the style after the last line.
                    // Must do it before the line feed, otherwise the "core dumped" message also gets colored.
                    if (terminal.enable_color && &line == &lines.back() && cur_style != TextStyle{})
                        func(terminal.AnsiResetString());

                    func(std::string_view("\n"));
                }
            }

            // Prints to a `terminal` stream.
            CFG_TA_API void Print(const Terminal &terminal) const;

            // The number of lines.
            [[nodiscard]] CFG_TA_API std::size_t NumLines() const;

            // Resize the canvas to have at least the specified number of lines.
            CFG_TA_API void EnsureNumLines(std::size_t size);

            // Resize the line to have at least the specified number of characters.
            CFG_TA_API void EnsureLineSize(std::size_t line_number, std::size_t size);

            // Inserts the line before the specified line index (or at the bottom of the canvas if given the number of lines).
            CFG_TA_API void InsertLineBefore(std::size_t line_number);

            // Whether a cell is free, aka has `.important == false`.
            [[nodiscard]] CFG_TA_API bool IsCellFree(std::size_t line, std::size_t column) const;

            // Checks if the space is free in the canvas.
            // Examines a single line (at number `line`), starting at `column - gap`, checking `width + gap*2` characters.
            // Returns false if at least one character has `.important == true`.
            [[nodiscard]] CFG_TA_API bool IsLineFree(std::size_t line, std::size_t column, std::size_t width, std::size_t gap) const;

            // Looks for a free space in the canvas.
            // Searches for `width + gap*2` consecutive cells with `.important == false`.
            // Starts looking at `(column - gap, starting_line)`, and proceeds downwards until it finds the free space,
            // which could be below the canvas.
            // Moves down in increments of `vertical_step`.
            [[nodiscard]] CFG_TA_API std::size_t FindFreeSpace(std::size_t starting_line, std::size_t column, std::size_t height, std::size_t width, std::size_t gap, std::size_t vertical_step) const;

            // Accesses the character for the specified cell. The cell must exist.
            [[nodiscard]] CFG_TA_API char32_t &CharAt(std::size_t line, std::size_t pos);

            // Accesses the cell info for the specified cell. The cell must exist.
            [[nodiscard]] CFG_TA_API CellInfo &CellInfoAt(std::size_t line, std::size_t pos);

            // Draws a text.
            // Returns `text.size()`.
            CFG_TA_API std::size_t DrawText(std::size_t line, std::size_t start, std::u32string_view text, const CellInfo &info = {.style = {}, .important = true});
            // Draws a UTF8 text. Returns the text size after converting to UTF32.
            CFG_TA_API std::size_t DrawText(std::size_t line, std::size_t start, std::string_view text, const CellInfo &info = {.style = {}, .important = true});

            // Draws a horizontal row of `ch`, starting at `(column, line_start)`, of width `width`.
            // If `skip_important == true`, don't overwrite important cells.
            // Returns `width`.
            CFG_TA_API std::size_t DrawRow(char32_t ch, std::size_t line, std::size_t column, std::size_t width, bool skip_important, const CellInfo &info = {.style = {}, .important = true});

            // Draws a vertical column of `ch`, starting at `(column, line_start)`, of height `height`.
            // If `skip_important == true`, don't overwrite important cells.
            CFG_TA_API void DrawColumn(char32_t ch, std::size_t line_start, std::size_t column, std::size_t height, bool skip_important, const CellInfo &info = {.style = {}, .important = true});

            // Draws a horziontal bracket: `|___|`. Vertical columns skip important cells, but the bottom bar doesn't.
            // The left column is on `column_start`, and the right one is on `column_start + width - 1`.
            CFG_TA_API void DrawHorBracket(std::size_t line_start, std::size_t column_start, std::size_t height, std::size_t width, const CellInfo &info = {.style = {}, .important = true});

            // Draws a little 1-high top bracket.
            CFG_TA_API void DrawOverline(std::size_t line, std::size_t column_start, std::size_t width, const CellInfo &info = {.style = {}, .important = true});
        };

        // Parsing C++ expressions.
        namespace expr
        {
            // The state of the parser state machine.
            enum class CharKind
            {
                normal,
                string, // A string literal (not raw), not including things outside quotes.
                character, // A character literal, not including things outside quotes.
                string_escape_slash, // Escaping slashes in a string literal.
                character_escape_slash, // Escaping slashes in a character literal.
                raw_string, // A raw string literal, starting from `(` and until the closing `"` inclusive.
                raw_string_initial_sep // A raw string literal, from the opening `"` to the `(` exclusive.
            };

            // `emit_char` is `(const char &ch, CharKind kind) -> void`.
            // It's called for every character, classifying it. The character address is guaranteed to be in `expr`.
            // `function_call` is `(std::string_view name, std::string_view args, std::size_t depth) -> void`.
            // It's called for every pair of parentheses. `args` is the contents of parentheses, possibly with leading and trailing whitespace.
            // `name` is the identifier preceding the `(`, without whitespace. It can be empty, or otherwise invalid.
            // `depth` is the parentheses nesting depth, starting at 0.
            template <typename EmitCharFunc, typename FunctionCallFunc>
            constexpr void ParseExpr(std::string_view expr, EmitCharFunc &&emit_char, FunctionCallFunc &&function_call)
            {
                CharKind state = CharKind::normal;

                // The previous character.
                char prev_ch = '\0';
                // The current identifier. Only makes sense in `state == normal`.
                std::string_view identifier;
                // Points at the start of the initial separator of a raw string.
                const char *raw_string_sep_start = nullptr;
                // The separator at the end of the raw string.
                std::string_view raw_string_sep;

                struct Entry
                {
                    // The identifier preceding the `(`, such as the function name. if any.
                    std::string_view ident;
                    // Points to the beginning of the arguments, right after `(`.
                    const char *args = nullptr;
                };

                // A stack of `()` parentheses.
                // This should be a vector, by my Clang 16 can't handle them in constexpr calls yet...
                Entry parens_stack[256];
                std::size_t parens_stack_pos = 0;

                for (const char &ch : expr)
                {
                    const CharKind prev_state = state;

                    switch (state)
                    {
                      case CharKind::normal:
                        if (ch == '"' && prev_ch == 'R')
                        {
                            state = CharKind::raw_string_initial_sep;
                            raw_string_sep_start = &ch + 1;
                        }
                        else if (ch == '"')
                        {
                            state = CharKind::string;
                        }
                        else if (ch == '\'')
                        {
                            // This condition handles `'` digit separators.
                            if (identifier.empty() || &identifier.back() + 1 != &ch || !IsDigit(identifier.front()))
                                state = CharKind::character;
                        }
                        else if (IsIdentifierChar(ch))
                        {
                            // We reset `identifier` lazily here, as opposed to immediately,
                            // to allow function calls with whitespace between the identifier and `(`.
                            if (!IsIdentifierChar(prev_ch))
                                identifier = {};

                            if (identifier.empty())
                                identifier = {&ch, 1};
                            else
                                identifier = {identifier.data(), identifier.size() + 1};
                        }
                        else
                        {
                            if constexpr (!std::is_null_pointer_v<FunctionCallFunc>)
                            {
                                if (ch == '(')
                                {
                                    if (parens_stack_pos >= std::size(parens_stack))
                                        HardError("Too many nested parentheses.");
                                    parens_stack[parens_stack_pos++] = {
                                        .ident = identifier,
                                        .args = &ch + 1,
                                    };
                                    identifier = {};
                                }
                                else if (ch == ')' && parens_stack_pos > 0)
                                {
                                    parens_stack_pos--;
                                    function_call(parens_stack[parens_stack_pos].ident, std::string_view(parens_stack[parens_stack_pos].args, &ch), parens_stack_pos);
                                }
                            }
                        }
                        break;
                      case CharKind::string:
                        if (ch == '"')
                            state = CharKind::normal;
                        else if (ch == '\\')
                            state = CharKind::string_escape_slash;
                        break;
                      case CharKind::character:
                        if (ch == '\'')
                            state = CharKind::normal;
                        else if (ch == '\\')
                            state = CharKind::character_escape_slash;
                        break;
                      case CharKind::string_escape_slash:
                        state = CharKind::string;
                        break;
                      case CharKind::character_escape_slash:
                        state = CharKind::character;
                        break;
                      case CharKind::raw_string_initial_sep:
                        if (ch == '(')
                        {
                            state = CharKind::raw_string;
                            raw_string_sep = {raw_string_sep_start, &ch};
                        }
                        break;
                      case CharKind::raw_string:
                        if (ch == '"')
                        {
                            std::string_view content(raw_string_sep_start, &ch);
                            if (content.size() >/*sic*/ raw_string_sep.size() && content[content.size() - raw_string_sep.size() - 1] == ')' && content.ends_with(raw_string_sep))
                                state = CharKind::normal;
                        }
                        break;
                    }

                    if (prev_state != CharKind::normal && state == CharKind::normal)
                        identifier = {};

                    CharKind fixed_state = state;
                    if (prev_state == CharKind::string || prev_state == CharKind::character || prev_state == CharKind::raw_string)
                        fixed_state = prev_state;

                    if constexpr (!std::is_null_pointer_v<EmitCharFunc>)
                        emit_char(ch, fixed_state);

                    prev_ch = ch;
                }
            }

            // C++ keyword classification for highlighting.
            enum class KeywordKind {generic, value, op};

            // Visual settings for printing exceptions.
            struct Style
            {
                // A piece of an expression that doesn't fit into the categories below.
                TextStyle normal;
                // Punctuation.
                TextStyle punct = {.bold = true};
                // Keywords.
                TextStyle keyword_generic = {.color = TextColor::light_blue, .bold = true};
                TextStyle keyword_value = {.color = TextColor::dark_magenta, .bold = true};
                TextStyle keyword_op = {.color = TextColor::light_white, .bold = true};
                // Identifiers written in all caps, probably macros.
                TextStyle all_caps = {.color = TextColor::dark_red};
                // Numbers.
                TextStyle number = {.color = TextColor::dark_green, .bold = true};
                // User-defined literal on a number, starting with `_`. For my sanity, literals not starting with `_` are colored like the rest of the number.
                TextStyle number_suffix = {.color = TextColor::dark_green};
                // A string literal; everything between the quotes inclusive.
                TextStyle string = {.color = TextColor::dark_cyan, .bold = true};
                // Stuff before the opening `"`.
                TextStyle string_prefix = {.color = TextColor::dark_cyan};
                // Stuff after the closing `"`.
                TextStyle string_suffix = {.color = TextColor::dark_cyan};
                // A character literal.
                TextStyle character = {.color = TextColor::dark_yellow, .bold = true};
                TextStyle character_prefix = {.color = TextColor::dark_yellow};
                TextStyle character_suffix = {.color = TextColor::dark_yellow};
                // A raw string literal; everything between the parentheses exclusive.
                TextStyle raw_string = {.color = TextColor::light_blue, .bold = true};
                // Stuff before the opening `"`.
                TextStyle raw_string_prefix = {.color = TextColor::dark_magenta};
                // Stuff after the closing `"`.
                TextStyle raw_string_suffix = {.color = TextColor::dark_magenta};
                // Quotes, parentheses, and everything between them.
                TextStyle raw_string_delimiters = {.color = TextColor::dark_magenta, .bold = true};

                // Keywords classification. The lists should be mutually exclusive.
                std::map<std::string, KeywordKind, std::less<>> highlighted_keywords = {
                    {"alignas", KeywordKind::generic},
                    {"alignof", KeywordKind::generic},
                    {"asm", KeywordKind::generic},
                    {"auto", KeywordKind::generic},
                    {"bool", KeywordKind::generic},
                    {"break", KeywordKind::generic},
                    {"case", KeywordKind::generic},
                    {"catch", KeywordKind::generic},
                    {"char", KeywordKind::generic},
                    {"char16_t", KeywordKind::generic},
                    {"char32_t", KeywordKind::generic},
                    {"char8_t", KeywordKind::generic},
                    {"class", KeywordKind::generic},
                    {"co_await", KeywordKind::generic},
                    {"co_return", KeywordKind::generic},
                    {"co_yield", KeywordKind::generic},
                    {"concept", KeywordKind::generic},
                    {"const_cast", KeywordKind::generic},
                    {"const", KeywordKind::generic},
                    {"consteval", KeywordKind::generic},
                    {"constexpr", KeywordKind::generic},
                    {"constinit", KeywordKind::generic},
                    {"continue", KeywordKind::generic},
                    {"decltype", KeywordKind::generic},
                    {"default", KeywordKind::generic},
                    {"delete", KeywordKind::generic},
                    {"do", KeywordKind::generic},
                    {"double", KeywordKind::generic},
                    {"dynamic_cast", KeywordKind::generic},
                    {"else", KeywordKind::generic},
                    {"enum", KeywordKind::generic},
                    {"explicit", KeywordKind::generic},
                    {"export", KeywordKind::generic},
                    {"extern", KeywordKind::generic},
                    {"float", KeywordKind::generic},
                    {"for", KeywordKind::generic},
                    {"friend", KeywordKind::generic},
                    {"goto", KeywordKind::generic},
                    {"if", KeywordKind::generic},
                    {"inline", KeywordKind::generic},
                    {"int", KeywordKind::generic},
                    {"long", KeywordKind::generic},
                    {"mutable", KeywordKind::generic},
                    {"namespace", KeywordKind::generic},
                    {"new", KeywordKind::generic},
                    {"noexcept", KeywordKind::generic},
                    {"operator", KeywordKind::generic},
                    {"private", KeywordKind::generic},
                    {"protected", KeywordKind::generic},
                    {"public", KeywordKind::generic},
                    {"register", KeywordKind::generic},
                    {"reinterpret_cast", KeywordKind::generic},
                    {"requires", KeywordKind::generic},
                    {"return", KeywordKind::generic},
                    {"short", KeywordKind::generic},
                    {"signed", KeywordKind::generic},
                    {"sizeof", KeywordKind::generic},
                    {"static_assert", KeywordKind::generic},
                    {"static_cast", KeywordKind::generic},
                    {"static", KeywordKind::generic},
                    {"struct", KeywordKind::generic},
                    {"switch", KeywordKind::generic},
                    {"template", KeywordKind::generic},
                    {"thread_local", KeywordKind::generic},
                    {"throw", KeywordKind::generic},
                    {"try", KeywordKind::generic},
                    {"typedef", KeywordKind::generic},
                    {"typeid", KeywordKind::generic},
                    {"typename", KeywordKind::generic},
                    {"union", KeywordKind::generic},
                    {"unsigned", KeywordKind::generic},
                    {"using", KeywordKind::generic},
                    {"virtual", KeywordKind::generic},
                    {"void", KeywordKind::generic},
                    {"volatile", KeywordKind::generic},
                    {"wchar_t", KeywordKind::generic},
                    {"while", KeywordKind::generic},
                    // ---
                    {"false", KeywordKind::value},
                    {"nullptr", KeywordKind::value},
                    {"this", KeywordKind::value},
                    {"true", KeywordKind::value},
                    // ---
                    {"and_eq", KeywordKind::op},
                    {"and", KeywordKind::op},
                    {"bitand", KeywordKind::op},
                    {"bitor", KeywordKind::op},
                    {"compl", KeywordKind::op},
                    {"not_eq", KeywordKind::op},
                    {"not", KeywordKind::op},
                    {"or_eq", KeywordKind::op},
                    {"or", KeywordKind::op},
                    {"xor_eq", KeywordKind::op},
                    {"xor", KeywordKind::op},
                };
            };

            // Pretty-prints an expression to a canvas.
            // Returns `expr.size()`.
            CFG_TA_API std::size_t DrawExprToCanvas(TextCanvas &canvas, const Style &style, std::size_t line, std::size_t start, std::string_view expr);
        }

        // Escapes a string, writes the result to `out_iter`. Includes quotes automatically.
        template <typename It>
        constexpr void EscapeString(std::string_view source, It out_iter, bool double_quotes)
        {
            *out_iter++ = "'\""[double_quotes];

            for (char signed_ch : source)
            {
                unsigned char ch = (unsigned char)signed_ch;

                bool should_escape = (ch < ' ') || ch == 0x7f || (ch == (double_quotes ? '"' : '\''));

                if (!should_escape)
                {
                    *out_iter++ = signed_ch;
                    continue;
                }

                switch (ch)
                {
                    case '\0': *out_iter++ = '\\'; *out_iter++ = '0'; break;
                    case '\'': *out_iter++ = '\\'; *out_iter++ = '\''; break;
                    case '\"': *out_iter++ = '\\'; *out_iter++ = '"'; break;
                    case '\\': *out_iter++ = '\\'; *out_iter++ = '\\'; break;
                    case '\a': *out_iter++ = '\\'; *out_iter++ = 'a'; break;
                    case '\b': *out_iter++ = '\\'; *out_iter++ = 'b'; break;
                    case '\f': *out_iter++ = '\\'; *out_iter++ = 'f'; break;
                    case '\n': *out_iter++ = '\\'; *out_iter++ = 'n'; break;
                    case '\r': *out_iter++ = '\\'; *out_iter++ = 'r'; break;
                    case '\t': *out_iter++ = '\\'; *out_iter++ = 't'; break;
                    case '\v': *out_iter++ = '\\'; *out_iter++ = 'v'; break;

                  default:
                    // The syntax with braces is from C++23. Without braces the escapes could consume extra characters on the right.
                    // Octal escapes don't do that, but they're just inherently ugly.
                    char buffer[7]; // 7 bytes for: \ x { N N } \0
                    std::snprintf(buffer, sizeof buffer, "\\x{%02x}", ch);
                    for (char *ptr = buffer; *ptr;)
                        *out_iter++ = *ptr++;
                    break;
                }
            }

            *out_iter++ = "'\""[double_quotes];
        }
    }

    // The base for modules that print stuff.
    struct BasicPrintingModule : virtual BasicModule
    {
      protected:
        ~BasicPrintingModule() = default;

      public:
        BasicPrintingModule() = default;
        BasicPrintingModule(const BasicPrintingModule &) = default;
        BasicPrintingModule(BasicPrintingModule &&) = default;
        BasicPrintingModule &operator=(const BasicPrintingModule &) = default;
        BasicPrintingModule &operator=(BasicPrintingModule &&) = default;

        Terminal terminal;
        CommonStyles common_styles;
        CommonChars common_chars;

        virtual void EnableUnicode(bool enable)
        {
            common_chars.EnableUnicode(enable);
        }

      protected:
        CFG_TA_API void PrintNote(std::string_view text) const;
    };


    // --- STRING CONVERSIONS ---

    // Don't specialize this, specialize `ToString` defined below.
    template <typename T, typename = void>
    struct DefaultToString
    {
        std::string operator()(const T &value) const
        {
            #if CFG_TA_FORMAT_SUPPORTS_DEBUG_STRINGS
            if constexpr (std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, std::string> || std::is_same_v<T, std::wstring>)
            {
                return CFG_TA_FORMAT("{:?}", value);
            }
            else
            #endif
            {
                return CFG_TA_FORMAT("{}", value);
            }
        }
    };
    // You can specialize this for your types.
    template <typename T, typename = void>
    struct ToString : DefaultToString<T> {};


    // --- TEST RUNNER ---

    // Don't touch this.
    namespace detail
    {
        // `TA_ARG` expands to this.
        // Stores a pointer into an `AssertWrapper` where it will write the argument as a string.
        struct ArgWrapper
        {
            BasicModule::BasicAssertionInfo::StoredArg *target = nullptr;

            ArgWrapper(std::string_view file, int line, int counter)
                : target(&ThreadState().FindStoredArgument(BasicModule::SourceLocCounter{.file = file, .line = line, .counter = counter}))
            {
                target->state = BasicModule::BasicAssertionInfo::StoredArg::State::in_progress;
            }
            ArgWrapper(const ArgWrapper &) = default;
            ArgWrapper &operator=(const ArgWrapper &) = default;

            // The method name is wonky to assist with our parsing.
            template <typename T>
            T &&_ta_handle_arg_(int counter, T &&arg) &&
            {
                (void)counter; // Unused, but passing it helps with parsing.
                target->value = ToString<std::remove_cvref_t<T>>{}(arg);
                target->state = BasicModule::BasicAssertionInfo::StoredArg::State::done;
                return std::forward<T>(arg);
            }
        };

        // An intermediate base class that `AssertWrapper<T>` inherits from.
        struct BasicAssertWrapper : BasicModule::BasicAssertionInfo
        {
            bool finished = false;

            CFG_TA_API BasicAssertWrapper();
            BasicAssertWrapper(const BasicAssertWrapper &) = delete;
            BasicAssertWrapper &operator=(const BasicAssertWrapper &) = delete;

            // Checks `value`, reports an error if it's false. In any case, returns `value` unchanged.
            CFG_TA_API bool operator()(bool value);

          protected:
            CFG_TA_API ~BasicAssertWrapper();
        };

        template <bool IsSoftAssertion, ConstString RawString, ConstString ExpandedString, ConstString FileName, int LineNumber>
        struct AssertWrapper : BasicAssertWrapper
        {
            // The number of arguments.
            static constexpr std::size_t num_args = []{
                std::size_t ret = 0;
                text::expr::ParseExpr(RawString.view(), nullptr, [&](std::string_view name, std::string_view args, std::size_t depth)
                {
                    (void)args;
                    (void)depth;
                    if (text::IsArgMacroName(name))
                        ret++;
                });
                return ret;
            }();

            // The values of the arguments.
            std::array<StoredArg, num_args> stored_args;

            struct CounterIndexPair
            {
                int counter = 0;
                std::size_t index = 0;
            };

            // Information about the arguments.
            struct ArgData
            {
                // Information about each individual argument.
                std::array<ArgInfo, num_args> info{};

                // Maps `__COUNTER__` values to argument indices. Sorted by counter, but the values might not be consecutive.
                std::array<CounterIndexPair, num_args> counter_to_arg_index{};

                // Arguments in the order they should be printed. Which is: highest depth first, then smaller counter values first.
                std::array<std::size_t, num_args> args_in_draw_order{};
            };

            static constexpr ArgData arg_data = []{
                ArgData ret;

                if constexpr (num_args > 0)
                {
                    // Parse expanded string.
                    std::size_t pos = 0;
                    text::expr::ParseExpr(ExpandedString.view(), nullptr, [&](std::string_view name, std::string_view args, std::size_t depth)
                    {
                        if (name != "_ta_handle_arg_")
                            return;

                        if (pos >= num_args)
                            HardError("More `$(...)`s than expected.");

                        ArgInfo &new_info = ret.info[pos];
                        new_info.depth = depth;

                        for (const char &ch : args)
                        {
                            if (text::IsDigit(ch))
                                new_info.counter = new_info.counter * 10 + (ch - '0');
                            else if (ch == ',')
                                break;
                            else
                                HardError("Lexer error: Unexpected character after the counter macro.");
                        }

                        CounterIndexPair &new_pair = ret.counter_to_arg_index[pos];
                        new_pair.index = pos;
                        new_pair.counter = new_info.counter;

                        pos++;
                    });
                    if (pos != num_args)
                        HardError("Less `$(...)`s than expected.");

                    // Parse raw string.
                    pos = 0;
                    text::expr::ParseExpr(RawString.view(), nullptr, [&](std::string_view name, std::string_view args, std::size_t depth)
                    {
                        (void)depth;

                        if (!text::IsArgMacroName(name))
                            return;

                        if (pos >= num_args)
                            HardError("More `$(...)`s than expected.");

                        ArgInfo &this_info = ret.info[pos];

                        this_info.expr_offset = std::size_t(args.data() - RawString.view().data());
                        this_info.expr_size = args.size();

                        this_info.ident_offset = std::size_t(name.data() - RawString.view().data());
                        this_info.ident_size = name.size();

                        // Trim side whitespace from `args`.
                        std::string_view trimmed_args = args;
                        while (!trimmed_args.empty() && text::IsWhitespace(trimmed_args.front()))
                            trimmed_args.remove_prefix(1);
                        while (!trimmed_args.empty() && text::IsWhitespace(trimmed_args.back()))
                            trimmed_args.remove_suffix(1);

                        // Decide if we should draw a bracket for this argument.
                        for (char ch : trimmed_args)
                        {
                            // Whatever the condition is, it should trigger for all arguments with nested arguments.
                            if (!text::IsIdentifierChar(ch))
                            {
                                this_info.need_bracket = true;
                                break;
                            }
                        }

                        pos++;
                    });
                    if (pos != num_args)
                        HardError("Less `$(...)`s than expected.");

                    // Sort `counter_to_arg_index` by counter, to allow binary search.
                    // Sorting is necessary when the arguments are nested.
                    std::sort(ret.counter_to_arg_index.begin(), ret.counter_to_arg_index.end(),
                        [](const CounterIndexPair &a, const CounterIndexPair &b){return a.counter < b.counter;}
                    );

                    // Fill and sort `args_in_draw_order`.
                    for (std::size_t i = 0; i < num_args; i++)
                        ret.args_in_draw_order[i] = i;
                    std::sort(ret.args_in_draw_order.begin(), ret.args_in_draw_order.end(), [&](std::size_t a, std::size_t b)
                    {
                        if (auto d = ret.info[a].depth <=> ret.info[b].depth; d != 0)
                            return d > 0;
                        if (auto d = ret.info[a].counter <=> ret.info[b].counter; d != 0)
                            return d < 0;
                        return false;
                    });
                }

                return ret;
            }();

            static constexpr BasicModule::SourceLoc location{.file = FileName.view(), .line = LineNumber};
            const BasicModule::SourceLoc &SourceLocation() const override {return location;}

            bool IsSoft() const override {return IsSoftAssertion;}
            std::string_view Expr() const override {return RawString.view();}
            std::span<const ArgInfo> ArgsInfo() const override {return arg_data.info;}
            std::span<const std::size_t> ArgsInDrawOrder() const override {return arg_data.args_in_draw_order;}
            std::span<const StoredArg> StoredArgs() const override {return stored_args;}

            const StoredArg *FindStoredArgumentByLocation(const BasicModule::SourceLocCounter &loc) const override
            {
                if (loc.line != LineNumber || loc.file != FileName.view())
                    return nullptr;

                auto it = std::partition_point(arg_data.counter_to_arg_index.begin(), arg_data.counter_to_arg_index.end(),
                    [&](const CounterIndexPair &pair){return pair.counter < loc.counter;}
                );
                if (it == arg_data.counter_to_arg_index.end() || it->counter != loc.counter)
                    return nullptr;

                return &stored_args[it->index];
            }
        };

        struct BasicTest : BasicModule::BasicTestInfo
        {
            virtual void Run() const = 0;
        };

        // A comparator for test names, that orders `/` before any other character.
        struct TestNameLess
        {
            bool operator()(std::string_view a, std::string_view b) const
            {
                std::size_t i = 0;
                while (true)
                {
                    // If `a` runs out first, return true.
                    // If `b` or both run out first, return false.
                    if (i >= a.size())
                        return i < b.size();
                    if (i >= b.size())
                        return false;

                    // If exactly one of the chars is a `/`, return true if it's in `a`.
                    if (int d = (a[i] == '/') - (b[i] == '/'))
                        return d > 0;

                    if (a[i] != b[i])
                        return a[i] < b[i];

                    i++;
                }
                return false;
            }
        };

        struct GlobalState
        {
            // All those are filled by the registration code:

            // Those must be in sync: [
            // All tests.
            std::vector<const BasicTest *> tests;
            // Maps test names to indices in `tests`.
            std::map<std::string_view, std::size_t, TestNameLess> name_to_test_index;
            // ]

            // Maps each test name, and each prefix (for test `foo/bar/baz` includes `foo/bar/baz`, `foo/bar`, and `foo`)
            //   to the preferred execution order.
            // The order matches the registration order, with prefixes keeping the first registration order.
            std::map<std::string_view, std::size_t, TestNameLess> name_prefixes_to_order;

            // Sorts test `indices` in the preferred execution order.
            CFG_TA_API void SortTestListInExecutionOrder(std::span<std::size_t> indices) const;
        };
        [[nodiscard]] CFG_TA_API GlobalState &State();

        // Stores singletons derived from `BasicTest`.
        template <std::derived_from<BasicTest> T>
        inline const T test_singleton{};

        // Registers a test. Pass a pointer to an instance of `test_singleton<??>`.
        CFG_TA_API void RegisterTest(const BasicTest *singleton);

        // An implementation of `BasicTest` for a specific test.
        // `P` is a pointer to the test function, see `DETAIL_TA_TEST()` for details.
        template <auto P, ConstString TestName, ConstString LocFile, int LocLine>
        struct SpecificTest : BasicTest
        {
            static constexpr bool test_name_is_valid = []{
                if (TestName.view().empty())
                    return false;
                if (!std::all_of(TestName.view().begin(), TestName.view().end(), [](char ch){return text::IsIdentifierCharStrict(ch) || ch == '/';}))
                    return false;
                if (std::adjacent_find(TestName.view().begin(), TestName.view().end(), [](char a, char b){return a == '/' && b == '/';}) != TestName.view().end())
                    return false;
                if (TestName.view().starts_with('/') || TestName.view().ends_with('/'))
                    return false;
                return true;
            }();
            static_assert(test_name_is_valid, "Test names can only contain letters, digits, underscores, and slashes as separators; can't start or end with a slash or contain consecutive slashes.");

            std::string_view Name() const override
            {
                return TestName.view();
            }
            BasicModule::SourceLoc Location() const override
            {
                return {LocFile.view(), LocLine};
            }

            void Run() const override
            {
                P({}/* Name tag. */);
            }
        };

        // Touch to register a test. `T` is `SpecificTest<??>`.
        template <typename T>
        inline const auto register_test_helper = []{RegisterTest(&test_singleton<T>); return nullptr;}();
    }

    // Use this to run tests.
    struct Runner
    {
        std::vector<ModulePtr> modules;

        // Fills `modules` arrays with all the default modules. Old contents are destroyed.
        CFG_TA_API void SetDefaultModules();

        // Handles the command line arguments in argc&argv style.
        // If `ok` is null and something goes wrong, aborts the application. If `ok` isn't null, sets it to true on success or to false on failure.
        // `argv[0]` is ignored.
        void ProcessFlags(int argc, char **argv, bool *ok = nullptr)
        {
            ProcessFlags([argc = argc-1, argv = argv+1]() mutable -> std::optional<std::string_view>
            {
                if (argc <= 0)
                    return {};
                argc--;
                return *argv++;
            }, ok);
        }
        // Handles command line arguments from a list of strings.
        // If `ok` is null and something goes wrong, aborts the application. If `ok` isn't null, sets it to true on success or to false on failure.
        // The first element is not ignored (not considered to be the program name).
        template <std::ranges::input_range R = std::initializer_list<std::string_view>>
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string_view>
        void ProcessFlags(R &&range, bool *ok = nullptr)
        {
            ProcessFlags([it = range.begin(), end = range.end()]() mutable -> std::optional<std::string_view>
            {
                if (it == end)
                    return {};
                return std::string_view(*it++);
            }, ok);
        }
        // The most low-level function to process command line flags.
        // `next_flag()` should return the next flag, or null if none.
        // If `ok` is null and something goes wrong, aborts the application. If `ok` isn't null, sets it to true on success or to false on failure.
        CFG_TA_API void ProcessFlags(std::function<std::optional<std::string_view>()> next_flag, bool *ok = nullptr);


        // Runs all tests.
        CFG_TA_API int Run();

        // Removes all modules of type `T` or derived from `T`.
        template <typename T>
        void RemoveModules()
        {
            std::erase_if(modules, [](const ModulePtr &ptr){return dynamic_cast<const T *>(ptr.get());});
        }

        // Calls `func` for every module of type `T` or derived from `T`.
        // Causes a hard error if there's no such modules.
        template <typename T, typename F>
        void FindModule(F &&func)
        {
            bool found = false;
            for (const auto &m : modules)
            {
                if (auto base = dynamic_cast<T *>(m.get()))
                {
                    found = true;
                    func(*base);
                }
            }
            if (!found)
                HardError(CFG_TA_FORMAT("No such module in the list: `{}`.", text::Demangler{}(typeid(T).name())), HardErrorKind::user);
        }

        // Configures every `BasicPrintingModule` to print to `stream`.
        void SetOutputStream(FILE *stream)
        {
            SetTerminalSettings([&](Terminal &terminal)
            {
                terminal.SetFileOutput(stream);
            });
        }

        // Configures every `BasicPrintingModule` to print to `stream`.
        void SetEnableColor(bool enable)
        {
            SetTerminalSettings([&](Terminal &terminal)
            {
                terminal.enable_color = enable;
            });
        }

        // Sets the output stream for every module that prints stuff.
        void SetEnableUnicode(bool enable)
        {
            for (const auto &m : modules)
            {
                if (auto base = dynamic_cast<BasicPrintingModule *>(m.get()))
                    base->EnableUnicode(enable);
            }
        }

        // Calls `func` on `Terminal` of every `BasicPrintingModule`.
        void SetTerminalSettings(std::function<void(Terminal &terminal)> func)
        {
            for (const auto &m : modules)
            {
                if (auto base = dynamic_cast<BasicPrintingModule *>(m.get()))
                    func(base->terminal);
            }
        }
    };


    // --- BUILT-IN MODULES ---

    namespace modules
    {
        // Responds to `--help` by printing the flags provided by all other modules.
        struct HelpPrinter : BasicPrintingModule
        {
            flags::SimpleFlag help_flag;

            CFG_TA_API HelpPrinter();
            std::vector<flags::BasicFlag *> GetFlags() override;
            void OnUnknownFlag(std::string_view flag, bool &abort) override;
            void OnMissingFlagArgument(std::string_view flag, const flags::BasicFlag &flag_obj, bool &abort) override;
        };

        // Responds to various command line flags to configure other printing modules.
        struct PrintingConfigurator : BasicModule
        {
            flags::BoolFlag flag_color;
            flags::BoolFlag flag_unicode;

            CFG_TA_API PrintingConfigurator();
            std::vector<flags::BasicFlag *> GetFlags() override;
        };

        // Prints the test names as they're being run.
        struct ProgressPrinter : BasicPrintingModule
        {
            // This goes right before each test/group name.
            std::string chars_test_prefix = "\xE2\x97\x8F "; // BLACK CIRCLE, then a space.
            // This is used when reentering a group after a failed test.
            std::string chars_test_prefix_continuing_group = "\xE2\x97\x8B "; // WHITE CIRCLE, then a space.
            // The is used for indenting test names/groups.
            std::string chars_indentation_guide = "\xC2\xB7   "; // MIDDLE DOT, then a space.
            // This is printed after the test counter and before the test names/groups (and before their indentation guides).
            std::string chars_test_counter_separator = " \xE2\x94\x82  "; // BOX DRAWINGS LIGHT VERTICAL, with some spaces around it.
            // This is printed when a test fails, right before the test name.
            std::string chars_test_failed = "TEST FAILED: ";

            // The message when a test starts.
            TextStyle style_name = {.color = TextColor::light_green, .bold = true};
            // The message when a test group starts.
            TextStyle style_group_name = {.color = TextColor::dark_green};
            // This is used to print a group name when reentering it after a failed test.
            TextStyle style_continuing_group = {.color = TextColor::light_black};
            // The indentation guides for nested test starts.
            TextStyle style_indentation_guide = {.color = TextColorGrayscale24(8), .bold = true};
            // The test index.
            TextStyle style_index = {.color = TextColor::light_green, .bold = true};
            // The total test count printed after each test index.
            TextStyle style_total_count = {.color = TextColor::dark_green};
            // The failed test counter.
            TextStyle style_failed_count = {.color = TextColor::light_red};
            // The line that separates the test counter from the test names/groups.
            TextStyle style_gutter_border = {.color = TextColorGrayscale24(10), .bold = true};

            // The name of a failed test, printed when it fails.
            TextStyle style_failed_name = {.color = TextColor::light_red, .bold = true};
            // The name of a group of a failed test, printed when the test fails.
            TextStyle style_failed_group_name = {.color = TextColor::dark_red};

          private:
            struct State
            {
                std::size_t test_counter = 0;
                std::vector<std::string_view> stack;
                // A copy of the stack from the previous test, if it has failed.
                // We use it to repeat the group names again, to show where we're restarting from.
                std::vector<std::string_view> failed_test_stack;
            };
            State state;

          public:
            void EnableUnicode(bool enable) override;
            void OnPreRunTests(const RunTestsInfo &data) override;
            void OnPreRunSingleTest(const SingleTestInfo &data) override;
            void OnPostRunSingleTest(const SingleTestResults &data) override;
        };

        // Prints the results of a run.
        struct ResultsPrinter : BasicPrintingModule
        {
            // No tests to run.
            TextStyle style_no_tests = {.color = TextColor::light_blue, .bold = true};
            // All tests passed.
            TextStyle style_all_passed = {.color = TextColor::light_green, .bold = true};
            // Some tests passed, this part shows how many have passed.
            TextStyle style_num_passed = {.color = TextColor::light_green};
            // Some tests passed, this part shows how many have failed.
            TextStyle style_num_failed = {.color = TextColor::light_red, .bold = true};

            void OnPostRunTests(const RunTestsResults &data) override;
        };

        // Prints failed assertions.
        struct AssertionPrinter : BasicPrintingModule
        {
            // Whether we should print the values of `$(...)` in the expression.
            bool decompose_expression = true;
            // Whether we should print the enclosing assertions.
            bool print_assertion_stack = true;

            // The primary error message.
            // Uses `common_styles.error` as a style.
            std::u32string chars_assertion_failed = U"ASSERTION FAILED:";
            // The enclosing assertions.
            TextStyle style_in_assertion = {.color = TextColor::light_magenta, .bold = true};
            std::u32string chars_in_assertion = U"WHILE CHECKING ASSERTION:";

            // How to print the filename.
            TextStyle style_filename = {.color = TextColor::none};

            // How to print the assertion macro itself.
            TextStyle style_assertion_macro = {.color = TextColor::light_red, .bold = true};
            std::u32string chars_assertion_macro_prefix = U"TA_CHECK( ";
            std::u32string chars_assertion_macro_prefix_soft = U"TA_SOFT_CHECK( ";
            std::u32string chars_assertion_macro_suffix = U" )";

            // Styles for the expression tokens.
            text::expr::Style style_expr;

            // The argument colors. They are cycled in this order.
            std::vector<TextStyle> style_arguments = {
                {.color = TextColorRgb6(1,4,1), .bold = true},
                {.color = TextColorRgb6(1,3,5), .bold = true},
                {.color = TextColorRgb6(1,0,5), .bold = true},
                {.color = TextColorRgb6(5,1,0), .bold = true},
                {.color = TextColorRgb6(5,4,0), .bold = true},
                {.color = TextColorRgb6(0,4,3), .bold = true},
                {.color = TextColorRgb6(0,5,5), .bold = true},
                {.color = TextColorRgb6(3,1,5), .bold = true},
                {.color = TextColorRgb6(4,0,2), .bold = true},
                {.color = TextColorRgb6(5,2,1), .bold = true},
                {.color = TextColorRgb6(4,5,3), .bold = true},
            };
            // This is used for brackets above expressions.
            TextStyle style_overline = {.color = TextColor::light_magenta, .bold = true};
            // This is used to dim the unwanted parts of expressions.
            TextColor color_dim = TextColor::light_black;

            // Labels a subexpression that had a nested assertion failure in it.
            std::u32string chars_in_this_subexpr = U"in here";
            // Same, but when there's more than one subexpression. This should never happen.
            std::u32string chars_in_this_subexpr_inexact = U"in here?";

            // Indent the printed code by this many spaces.
            // The indentation is only applied once, since we don't print multiline code.
            std::size_t printed_code_indentation = 4;

            void OnAssertionFailed(const BasicAssertionInfo &data) override;
        };

        // A generic module to analyze exceptions.
        // `E` is the exception base class.
        // `F` is a functor to get the error string from an exception, defaults to `e.what()`.
        template <typename E, typename F = void>
        struct GenericExceptionAnalyzer : BasicModule
        {
            std::optional<ExceptionInfoWithNested> OnExplainException(const std::exception_ptr &e) const override
            {
                try
                {
                    std::rethrow_exception(e);
                }
                catch (E &e)
                {
                    ExceptionInfoWithNested ret;
                    ret.type_name = text::Demangler{}(typeid(e).name());
                    if constexpr (std::is_void_v<F>)
                        ret.message = e.what();
                    else
                        ret.message = F{}(e);

                    try
                    {
                        std::rethrow_if_nested(e);
                    }
                    catch (...)
                    {
                        ret.nested_exception = std::current_exception();
                    }

                    return std::move(ret);
                }
            }
        };
        // Analyzes exceptions derived from `std::exception`.
        using DefaultExceptionAnalyzer = GenericExceptionAnalyzer<std::exception>;

        // Prints any uncaught exceptions.
        struct ExceptionPrinter : BasicPrintingModule
        {
            TextStyle style_exception_type = {.color = TextColor::dark_magenta};
            TextStyle style_exception_message = {.color = TextColor::light_blue};

            std::string chars_error = "UNCAUGHT EXCEPTION:";
            std::string chars_unknown_exception = "Unknown exception.";
            std::string chars_indent_type = "  ";
            std::string chars_indent_message = "    ";
            std::string chars_type_suffix = ": ";

            void OnUncaughtException(const std::exception_ptr &e) override;
        };

        // Detects whether the debugger is attached in a platform-specific way.
        struct DebuggerDetector : BasicModule
        {
            // If this is not set, will check whether the debugger is attached when an assertion fails, and break if it is.
            std::optional<bool> break_on_failure;

            flags::BoolFlag flag_break;

            CFG_TA_API DebuggerDetector();

            std::vector<flags::BasicFlag *> GetFlags() override;

            CFG_TA_API bool IsDebuggerAttached() const;
            void OnAssertionFailed(const BasicAssertionInfo &data) override;
        };

        // A little module that examines `DebuggerDetector` and notifies you when it detected a debugger.
        struct DebuggerStatePrinter : BasicPrintingModule
        {
            void OnPreRunTests(const RunTestsInfo &data) override;
        };
    }
}

// Manually support quoting strings if the formatting library can't do that.
#if !CFG_TA_FORMAT_SUPPORTS_DEBUG_STRINGS
namespace ta_test
{
    template <typename Void>
    struct DefaultToString<char, Void>
    {
        std::string operator()(char value) const
        {
            char ret[12]; // Should be at most 9: `'\x??'\0`, but throwing in a little extra space.
            text::EscapeString({&value, 1}, ret, false);
            return ret;
        }
    };
    template <typename Void>
    struct DefaultToString<std::string, Void>
    {
        std::string operator()(const std::string &value) const
        {
            std::string ret;
            ret.reserve(value.size() + 2); // +2 for quotes. This assumes the happy scenario without any escapes.
            text::EscapeString(value, std::back_inserter(ret), true);
            return ret;
        }
    };
    template <typename Void>
    struct DefaultToString<std::string_view, Void>
    {
        std::string operator()(const std::string_view &value) const
        {
            std::string ret;
            ret.reserve(value.size() + 2); // +2 for quotes. This assumes the happy scenario without any escapes.
            text::EscapeString(value, std::back_inserter(ret), true);
            return ret;
        }
    };
    template <typename Void> struct DefaultToString<      char *, Void> {std::string operator()(const char *value) const {return ToString<std::string_view>{}(value);}};
    template <typename Void> struct DefaultToString<const char *, Void> {std::string operator()(const char *value) const {return ToString<std::string_view>{}(value);}};
    // Somehow this catches const arrays too:
    template <std::size_t N, typename Void> struct DefaultToString<char[N], Void> {std::string operator()(const char *value) const {return ToString<std::string_view>{}(value);}};
}
#endif
