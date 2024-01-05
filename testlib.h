#pragma once

#include <algorithm>
#include <array>
#include <cctype>
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
#include <regex>
#include <set>
#include <span>
#include <string_view>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>
#include <version>


// --- CONFIGURATION MACROS ---

// Whether we're building as a shared library.
#ifndef CFG_TA_SHARED
#define CFG_TA_SHARED 0
#endif
// The import/export macro we use on all non-inline functions.
// Probably shouldn't define this directly, prefer setting `CFG_TA_SHARED`.
#ifndef CFG_TA_API
#  if CFG_TA_SHARED
#    ifdef _WIN32
#      define CFG_TA_API __declspec(dllimport)
#    else
#      define CFG_TA_API __attribute__((__visibility__("default")))
#    endif
#  else
#    define CFG_TA_API
#  endif
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
#define CFG_TA_CXX_STANDARD 23
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
// This location format is used for internal error messages.
// Most user-facing messages don't use this, as they're printed by modules, which are configured separately.
#if CFG_TA_MSVC_STYLE_ERRORS
#define DETAIL_TA_INTERNAL_ERROR_LOCATION_FORMAT "{}({})"
#else
#define DETAIL_TA_INTERNAL_ERROR_LOCATION_FORMAT "{}:{}"
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
#  ifdef __cpp_lib_format
#    define CFG_TA_USE_LIBFMT 0
#  elif __has_include(<fmt/format.h>)
#    define CFG_TA_USE_LIBFMT 1
#  else
#    error ta_test needs a compiler supporting `#include <format>`, or installed libfmt, or a custom formatting library to be specified.
#  endif
#endif

// The namespace of the formatting library.
#ifndef CFG_TA_FMT_NAMESPACE
#if CFG_TA_USE_LIBFMT
#include <fmt/format.h>
#include <fmt/ranges.h> // At least for `fmt::range_format_kind`.
#define CFG_TA_FMT_NAMESPACE ::fmt
#else
#include <format>
#define CFG_TA_FMT_NAMESPACE ::std
#endif
#endif

// Whether the formatting library has a `vprint(FILE *, ...)`. If it's not there, we format to a temporary buffer first.
// 0 = none, 1 = `vprint_[non]unicode`, 2 = `vprint`.
#ifndef CFG_TA_FMT_HAS_FILE_VPRINT
#  if CFG_TA_USE_LIBFMT
#    define CFG_TA_FMT_HAS_FILE_VPRINT 2
#  else
#    ifdef __cpp_lib_print
#      define CFG_TA_FMT_HAS_FILE_VPRINT 1
#    else
#      define CFG_TA_FMT_HAS_FILE_VPRINT 0
#    endif
#  endif
#endif
// Whether the formatting library can classify and format ranges.
// 0 = none, 1 = `format_kind` variable, 2 = `range_format_kind` trait class.
#ifndef CFG_TA_FMT_HAS_RANGE_FORMAT
#  if CFG_TA_USE_LIBFMT
#    define CFG_TA_FMT_HAS_RANGE_FORMAT 2
#  else
#    ifdef __cpp_lib_format_ranges
#      define CFG_TA_FMT_HAS_RANGE_FORMAT 1
#    else
#      define CFG_TA_FMT_HAS_RANGE_FORMAT 0
#    endif
#  endif
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

// Warning pragmas to ignore warnings about unused values.
#ifndef CFG_TA_IGNORE_UNUSED_VALUE
#ifdef __GNUC__
#define CFG_TA_IGNORE_UNUSED_VALUE(...) _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunused-value\"") __VA_ARGS__ _Pragma("GCC diagnostic pop")
#else
#define CFG_TA_IGNORE_UNUSED_VALUE(...)
#endif
#endif

#ifndef CFG_TA_NODISCARD_LAMBDA
#  if CFG_TA_CXX_STANDARD >= 23
#    define CFG_TA_NODISCARD_LAMBDA [[nodiscard]]
#  elif __clang__
// Clang rejects this in C++20 mode, but just switching to C++23 isn't always an option, since Clang 16 chokes on libstdc++ 13's `<ranges>` only in C++23 mode.
#    define CFG_TA_NODISCARD_LAMBDA _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wc++2b-extensions\"") [[nodiscard]] _Pragma("clang diagnostic pop")
#  elif __GNUC__
// GCC accepts this even in C++20 mode, so whatever.
#    define CFG_TA_NODISCARD_LAMBDA [[nodiscard]]
#  else
#    define CFG_TA_NODISCARD_LAMBDA
#  endif
#endif

// --- INTERFACE MACROS ---

// Define a test. Must be followed by `{...}`.
// `name` is the test name without quotes, it must be a valid identifier.
// Use `/` as a separator to make test groups: `group/sub_group/test_foo`. There must be no spaces around the slashes.
#define TA_TEST(name) DETAIL_TA_TEST(name)

// Check condition, immediately fail the test if false.
// The condition can be followed by a custom message, then possibly by format arguments.
// The custom message is evaluated only if the condition is false, so you can use expensive calls in it.
// Returns the same boolean that was passed as the condition.
// Usage:
//     TA_CHECK(x == 42);
//     TA_CHECK($(x) == 42); // `$` will print the value of `x` on failure.
//     TA_CHECK($(x) == 42)("Checking stuff!"); // Add a custom message.
//     TA_CHECK($(x) == 42)("Checking {}!", "stuff"); // Custom message with formatting.
#define TA_CHECK(...) DETAIL_TA_CHECK("TA_CHECK", #__VA_ARGS__, __VA_ARGS__)
// Equivalent to `TA_CHECK(false)`, except the printed message is slightly different.
// Also accepts an optional message: `TA_FAIL("Checking {}!", "stuff");`.
#define TA_FAIL DETAIL_TA_CHECK("", "", false)
// Stops the test immediately, not necessarily failing it.
// Equivalent to throwing `ta_test::InterruptTestException`.
#define TA_INTERRUPT_TEST DETAIL_TA_INTERRUPT_TEST

// Can only be used inside of `TA_CHECK(...)`. Wrap a subexpression in this to print its value if the assertion fails.
// Those can be nested inside one another.
// The expansion is enclosed in `(...)`, which lets you use it e.g. as a sole function argument: `func $(var)`.
#define TA_ARG(...) DETAIL_TA_ARG(__COUNTER__, __VA_ARGS__)
#if CFG_TA_USE_DOLLAR
#define $(...) DETAIL_TA_ARG(__COUNTER__, __VA_ARGS__)
#endif

// Checks that `...` throws an exception (it can even contain more than one statement), otherwise fails the test immediately.
// Returns the information about the exception, which you can additionally validate.
#define TA_MUST_THROW(...) \
    DETAIL_TA_MUST_THROW("TA_MUST_THROW", #__VA_ARGS__, __VA_ARGS__)

// Logs a formatted line. The log is printed only on test failure.
// Example:
//     TA_LOG("Hello!");
//     TA_LOG("x = {}", 42);
// The trailing `\n`, if any, is ignored.
#define TA_LOG(...) DETAIL_TA_LOG(__VA_ARGS__)
// Creates a scoped log message. It's printed only if this line is in scope when something fails.
// Unlike `TA_LOG()`, the message can be printed multiple times, if there are multiple failures in this scope.
// The trailing `\n`, if any, is ignored.
// The code calls this a "scoped log", and "context" means something else in the code.
#define TA_CONTEXT(...) DETAIL_TA_CONTEXT(__VA_ARGS__)
// Like `TA_CONTEXT`, but only evaluates the message when needed.
// This means you need to make sure none of your variables dangle, and that they have sane values for the entire lifetime of this context.
// Can evaluate the message more than once. You can utilize this to display the current variable values.
#define TA_CONTEXT_LAZY(...) DETAIL_TA_CONTEXT_LAZY(__VA_ARGS__)

// Repeats the test for all values in the range `...`. (Either a `{...}` list or a C++20 range.)
// `name` is the name for logging purposes, it must be a valid identifier.
// You can't use any local variables in `...`, that's a compilation error.
//   Use `TA_GENERATE_FUNC(...)` with `ta_test::RangeToGeneratorFunc(...)` to do that.
// Accepts a second optional parameter, of type `ta_test::GeneratorFlags`, like `RangeToGeneratorFunc()`.
#define TA_GENERATE(name, ...) DETAIL_TA_GENERATE(name, __VA_ARGS__)

// Repeats the test for all values returned by the lambda.
// Usage: `T x = TA_GENERATE_FUNC(name, lambda);`.
// The lambda must be `(bool &repeat) -> ??`. The return type can be anything, possibly a reference.
// Saves the return value from the lambda and returns it from `TA_GENERATE(...)`.
// `repeat` receives `true` by default. If it remains true, the test will be restarted and the lambda will be called again to compute the new value.
// If it's set to false, the
//  by const reference: `const decltype(func(...)) &`.
// Note that this means that if `func()` returns by reference, the possible lack of constness of the reference is preserved:
//   Lambda returns | TA_GENERATE_FUNC returns
//   ---------------|-------------------------
//             T    | const T &
//             T &  |       T &
//             T && |       T &&
// The lambda won't be called again, or at least not until its recreated to restart the sequence, if necessary.
// The lambda is not evaluated/constructed at all when reentering `TA_GENERATE_FUNC(...)`, if we already have one from the previous run.
// WARNING!! Since the lambda will outlive the test, make sure your captures don't dangle.
#define TA_GENERATE_FUNC(name, ...) DETAIL_TA_GENERATE_FUNC(name, __VA_ARGS__)


// --- INTERNAL MACROS ---

#define DETAIL_TA_CAT(x, y) DETAIL_TA_CAT_(x, y)
#define DETAIL_TA_CAT_(x, y) x##y

#define DETAIL_TA_TEST(name) \
    inline void _ta_test_func(::ta_test::meta::ConstStringTag<#name>); \
    constexpr auto _ta_registration_helper(::ta_test::meta::ConstStringTag<#name>) -> decltype(void(::std::integral_constant<\
        const std::nullptr_t *, &::ta_test::detail::register_test_helper<\
            ::ta_test::detail::SpecificTest<static_cast<void(*)(\
                ::ta_test::meta::ConstStringTag<#name>\
            )>(_ta_test_func),\
            []{CFG_TA_BREAKPOINT(); ::std::terminate();},\
            #name, __FILE__, __LINE__>\
        >\
    >{})) {} \
    inline void _ta_test_func(::ta_test::meta::ConstStringTag<#name>)

#define DETAIL_TA_CHECK(macro_name_, str_, ...) \
    /* `~` is what actually performs the asesrtion. We need something with a high precedence. */\
    ~::ta_test::detail::AssertWrapper<macro_name_, str_, #__VA_ARGS__, __FILE__, __LINE__>(\
        [&]([[maybe_unused]]::ta_test::detail::BasicAssertWrapper &_ta_assert){_ta_assert.EvalCond(__VA_ARGS__);},\
        []{CFG_TA_BREAKPOINT(); ::std::terminate();}\
    )\
    .DETAIL_TA_ADD_MESSAGE

#define DETAIL_TA_FAIL(macro_name_) DETAIL_TA_CHECK(macro_name_, "", false)

#define DETAIL_TA_INTERRUPT_TEST (throw ::ta_test::InterruptTestException{})

#define DETAIL_TA_ADD_MESSAGE(...) \
    AddMessage([&](auto &&_ta_add_message){_ta_add_message(__VA_ARGS__);})

#define DETAIL_TA_ARG(counter, ...) \
    /* Note the outer parentheses, they allow this to be transparently used e.g. as a single function parameter. */\
    /* Passing `counter` the second time is redundant, but helps with our parsing. */\
    (_ta_assert.BeginArg(counter)._ta_handle_arg_(counter, __VA_ARGS__))

#define DETAIL_TA_MUST_THROW(macro_name_, str_, ...) \
    /* `~` is what actually performs the asesrtion. We need something with a high precedence. */\
    ~::ta_test::detail::MustThrowWrapper::Make<__FILE__, __LINE__, macro_name_, str_>(\
        [&]{CFG_TA_IGNORE_UNUSED_VALUE(__VA_ARGS__;)},\
        []{CFG_TA_BREAKPOINT(); ::std::terminate();}\
    )\
    .DETAIL_TA_ADD_MESSAGE

#define DETAIL_TA_LOG(...) \
    ::ta_test::detail::AddLogEntry(CFG_TA_FMT_NAMESPACE::format(__VA_ARGS__))
#define DETAIL_TA_CONTEXT(...) \
    ::ta_test::detail::ScopedLogGuard DETAIL_TA_CAT(_ta_context,__COUNTER__)(CFG_TA_FMT_NAMESPACE::format(__VA_ARGS__))
#define DETAIL_TA_CONTEXT_LAZY(...) \
    ::ta_test::detail::ScopedLogGuardLazy DETAIL_TA_CAT(_ta_context,__COUNTER__)([&]{return CFG_TA_FMT_NAMESPACE::format(__VA_ARGS__);})

#define DETAIL_TA_GENERATE(name, ...) \
    ::ta_test::detail::Generate<#name, __FILE__, __LINE__, __COUNTER__>([/*non-capturing*/]{return ::ta_test::RangeToGeneratorFunc(__VA_ARGS__);})

#define DETAIL_TA_GENERATE_FUNC(name, ...) \
    ::ta_test::detail::Generate<#name, __FILE__, __LINE__, __COUNTER__>([&]{return __VA_ARGS__;})

// --- ENUM FLAGS MACROS ---

// Synthesizes operators for a enum of flags: `&`, `|`, and `~`. Also multiplication by a bool.
#define DETAIL_TA_FLAG_OPERATORS(name_) DETAIL_TA_FLAG_OPERATORS_CUSTOM(static, name_)
// Same, but works at class scope.
#define DETAIL_TA_FLAG_OPERATORS_IN_CLASS(name_) DETAIL_TA_FLAG_OPERATORS_CUSTOM(friend, name_)
// Same, but lets you specify a custom decl-specifier-seq.
#define DETAIL_TA_FLAG_OPERATORS_CUSTOM(prefix_, name_) \
    [[nodiscard, maybe_unused]] prefix_ name_ operator&(name_ a, name_ b) {return name_(::std::underlying_type_t<name_>(a) & ::std::underlying_type_t<name_>(b));} \
    [[nodiscard, maybe_unused]] prefix_ name_ operator|(name_ a, name_ b) {return name_(::std::underlying_type_t<name_>(a) | ::std::underlying_type_t<name_>(b));} \
    [[nodiscard, maybe_unused]] prefix_ name_ operator~(name_ a) {return name_(~::std::underlying_type_t<name_>(a));} \
    [[maybe_unused]] prefix_ name_ &operator&=(name_ &a, name_ b) {return a = a & b;} \
    [[maybe_unused]] prefix_ name_ &operator|=(name_ &a, name_ b) {return a = a | b;} \
    [[nodiscard, maybe_unused]] prefix_ name_ operator*(name_ a, bool b) {return b ? a : name_{};} \
    [[nodiscard, maybe_unused]] prefix_ name_ operator*(bool a, name_ b) {return a ? b : name_{};} \
    [[maybe_unused]] prefix_ name_ &operator*=(name_ &a, bool b) {return a = a * b;}


namespace ta_test
{
    struct Runner;
    struct BasicModule;
    class ModuleLists;

    // The exit codes we're using. This is mostly for reference.
    enum class ExitCode
    {
        ok = 0,
        test_failed = 1, // One or more tests failed.
        bad_command_line_arguments = 2, // A generic issue with command line arguments.
        no_test_name_match = 3, // `--include` or `--exclude` didn't match any tests.
    };

    // We try to classify the hard errors into interal ones and user-induced ones, but this is only an approximation.
    enum class HardErrorKind {internal, user};
    // Aborts the application with an error.
    [[noreturn]] CFG_TA_API void HardError(std::string_view message, HardErrorKind kind = HardErrorKind::internal);

    // We throw this to abort a test (not necessarily fail it).
    // You can catch and rethrow this before a `catch (...)` to still be able to abort tests inside one.
    // You could throw this manually, but I don't see why you'd want to.
    struct InterruptTestException {};

    // Metaprogramming helpers.
    namespace meta
    {
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

            [[nodiscard]] consteval std::string_view view() const &
            {
                return {str, str + size};
            }
            [[nodiscard]] consteval std::string_view view() const && = delete;
        };
        template <std::size_t A, std::size_t B>
        [[nodiscard]] consteval ConstString<A + B - 1> operator+(const ConstString<A> &a, const ConstString<B> &b)
        {
            ConstString<A + B - 1> ret;
            std::copy_n(a.str, a.size, ret.str);
            std::copy_n(b.str, b.size, ret.str + a.size);
            return ret;
        }
        template <std::size_t A, std::size_t B>
        [[nodiscard]] consteval ConstString<A + B - 1> operator+(const ConstString<A> &a, const char (&b)[B])
        {
            return a + ConstString<B>(b);
        }
        template <std::size_t A, std::size_t B>
        [[nodiscard]] consteval ConstString<A + B - 1> operator+(const char (&a)[A], const ConstString<B> &b)
        {
            return ConstString<A>(a) + b;
        }
        template <ConstString X>
        struct ConstStringTag
        {
            static constexpr auto value = X;
        };

        // The lambda overloader.
        template <typename ...P>
        struct Overload : P... {using P::operator()...;};
        template <typename ...P>
        Overload(P...) -> Overload<P...>;

        // Always returns `false`.
        template <typename, typename...>
        struct AlwaysFalse : std::false_type {};

        // Tag dispatch helper.
        template <auto>
        struct ValueTag {};

        // Extracts the class type from a member pointer type.
        template <typename T>
        struct MemberPointerClass {};
        template <typename T, typename C>
        struct MemberPointerClass<T C::*> {using type = C;};

        // Returns true if `X` and `Y` have the same type and are equal.
        template <auto X, auto Y>
        struct ValuesAreEqual : std::false_type {};
        template <auto X>
        struct ValuesAreEqual<X, X> : std::true_type {};
    }

    namespace text
    {
        // Character classification functions.
        namespace chars
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
            // Whether `ch` is a punctuation character.
            // Unlike the standard function, we don't reject invisible characters here. Importantly, we do reject unicode.
            [[nodiscard]] constexpr bool IsPunct(char ch)
            {
                return ch >= 0 && ch <= 127 && !IsIdentifierChar(ch);
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

            // Skips whitespace characters, if any.
            constexpr void SkipWhitespace(const char *&ch)
            {
                while (IsWhitespace(*ch))
                    ch++;
            }

            // Advances `ch` until one of the characters in `sep` is found,
            // or until an unbalanced closing bracket (one of: `)]}`), or until the second top-level opening bracket.
            // Then gives back the trailing whitespace, if any.
            // but we ignore the contents of "..." and '...' strings, and ignore matching characters inside of `(...)`, `[...]`, or `{...}`.
            // We don't check the type of brackets, treating them all as equivalent, but if we find an unbalanced closing bracket, we stop immediately.
            constexpr void TryFindUnprotectedSeparator(const char *&ch, std::string_view sep)
            {
                const char *const first_ch = ch;

                char quote_ch = '\0';
                int depth = 0;

                bool first_paren = false;

                while (*ch)
                {
                    if (quote_ch)
                    {
                        if (*ch == '\\')
                        {
                            ch++;
                            if (*ch == '\0')
                                break; // Incomplete escape at the end of string.
                        }
                        else if (*ch == quote_ch)
                        {
                            quote_ch = '\0';
                        }
                    }
                    else
                    {
                        if (depth == 0 && sep.find(*ch) != std::string_view::npos)
                            break; // Found separator.

                        if (*ch == '"' || *ch == '\'')
                        {
                            quote_ch = *ch;
                        }
                        else if (*ch == '(' || *ch == '[' || *ch == '{')
                        {
                            if (first_paren)
                                break; // Second top-level opening bracket.
                            first_paren = true;
                            depth++;
                        }
                        else if (*ch == ')' || *ch == ']' || *ch == '}')
                        {
                            depth--;
                            if (depth < 0)
                                break; // Unbalanced bracket.
                        }
                    }

                    ch++;
                }

                // Unskip trailing whitespace.
                while (ch > first_ch && IsWhitespace(ch[-1]))
                    ch--;
            }
        }

        // A mini unicode library.
        namespace uni
        {
            // A placeholder value for invalid characters.
            constexpr char32_t default_char = 0xfffd;
            // The largest valid character.
            constexpr char32_t max_char_value = 0x10ffff;

            // Max bytes per character.
            constexpr std::size_t max_char_len = 4;

            // Given a byte, checks if it's the first byte of a multibyte character, or is a single-byte character.
            // Even if this function returns true, `byte` can be an invalid first byte.
            // To check for the byte validity, use `FirstByteToCharacterLength`.
            [[nodiscard]] constexpr bool IsFirstByte(char byte)
            {
                return (byte & 0b11000000) != 0b10000000;
            }

            // Counts the number of codepoints (usually characters) in a valid UTF8 string, by counting the bytes matching `IsFirstByte()`.
            [[nodiscard]] constexpr std::size_t CountFirstBytes(std::string_view string)
            {
                return std::size_t(std::count_if(string.begin(), string.end(), IsFirstByte));
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
                return ch <= max_char_value;
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
            // The buffer length can be `max_char_len` (or use `CharacterCodeToLength` for the precise byte length).
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

        // String escaping.
        namespace escape
        {
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

            // Unescapes a string.
            // Writes the string to the `output` iterator. Returns the error message on failure, or empty string on success.
            // Tries to support all standard escapes, except for `\N{...}` named characters, because that would be stupid.
            // We also don't support the useless `\?`.
            // If `quote_char` isn't zero, we expect it before and after the string.
            template <typename Iter>
            std::string UnescapeString(const char *&source, char quote_char, Iter output, bool only_single_char)
            {
                // Consumes digits for an escape sequence.
                // If `hex == true` those are hex digits, otherwise octal.
                // `max_digits` is how many digits we can consume, or `-1` to consume as many as possible, or `-2` to wait for a `}`.
                // `max_value` is the max value we're allowing, inclusive.
                // Writes the resulting number to `result`. Returns the error on failure, or an empty string on success.
                auto ConsumeDigits = [&] CFG_TA_NODISCARD_LAMBDA (char32_t &result, bool hex, int max_digits, char32_t max_value) -> std::string
                {
                    result = 0;

                    int i = 0;
                    while (true)
                    {
                        bool is_digit = false;
                        bool is_decimal = false;
                        bool is_hex_lowercase = false;
                        bool is_hex_uppercase = false;

                        if (hex)
                        {
                            is_digit =
                                (is_decimal = *source >= '0' && *source <= '9') ||
                                (is_hex_lowercase = *source >= 'a' && *source <= 'f') ||
                                (is_hex_uppercase = *source >= 'A' && *source <= 'F');
                        }
                        else
                        {
                            is_decimal = true;
                            is_digit = *source >= '0' && *source <= '7';
                        }

                        if (!is_digit)
                        {
                            if (max_digits < 0 && i > 0)
                                break;
                            else
                                return CFG_TA_FMT_NAMESPACE::format("Expected {} digit in escape sequence.", hex ? "hexadecimal" : "octal");
                        }

                        char32_t new_result = result * (hex ? 16 : 8) + char32_t(is_decimal ? *source - '0' : is_hex_lowercase ? *source - 'a' + 10 : *source - 'A' + 10);
                        if (new_result > max_value)
                            return "Escape sequence is out of range.";

                        result = new_result;

                        source++;
                        i++;
                        if (i == max_digits)
                            break;
                    }

                    if (max_digits == -2)
                    {
                        #if 0
                        { // Prevent the following '}' from messing up the folding in Clangd!
                        #endif

                        if (*source != '}')
                            return "Expected `}` to close the escape sequence.";
                        source++;
                    }

                    return "";
                };

                if (quote_char)
                {
                    if (*source != quote_char)
                        return CFG_TA_FMT_NAMESPACE::format("Expected opening `{}`.", quote_char);
                    source++;
                }

                std::size_t char_counter = 0;

                while (*source != '\0' && *source != quote_char)
                {
                    char_counter++;
                    if (only_single_char && char_counter == 2)
                        break;

                    if (*source != '\\')
                    {
                        *output++ = *source++;
                        continue;
                    }
                    source++;

                    switch (*source++)
                    {
                        case 'N': source--; return "Named character escapes are not supported.";

                        case '\'': *output++ = '\''; break;
                        case '"':  *output++ = '"';  break;
                        case '\\': *output++ = '\\'; break;
                        case 'a':  *output++ = '\a'; break;
                        case 'b':  *output++ = '\b'; break;
                        case 'f':  *output++ = '\f'; break;
                        case 'n':  *output++ = '\n'; break;
                        case 'r':  *output++ = '\r'; break;
                        case 't':  *output++ = '\t'; break;
                        case 'v':  *output++ = '\v'; break;

                      case 'o':
                        {
                            if (*source != '{')
                                return "Expected `{` to begin the escape sequence.";
                            source++;
                            char32_t n = 0;
                            if (auto error = ConsumeDigits(n, false, -2, 0xff); !error.empty())
                                return error;
                            *output++ = char(n);
                        }
                        break;

                      case 'x':
                        {
                            char32_t n = 0;
                            if (*source == '{')
                            {
                                source++;
                                if (auto error = ConsumeDigits(n, true, -2, 0xff); !error.empty())
                                    return error;
                            }
                            else
                            {
                                if (auto error = ConsumeDigits(n, true, -1, 0xff); !error.empty())
                                    return error;
                            }
                            *output++ = char(n);
                        }
                        break;

                      case 'u':
                      case 'U':
                        {
                            char32_t n = 0;
                            if (*source == '{')
                            {
                                source++;
                                if (auto error = ConsumeDigits(n, true, -2, uni::max_char_value); !error.empty())
                                    return error;
                            }
                            else
                            {
                                if (auto error = ConsumeDigits(n, true, source[-1] == 'u' ? 4 : 8, uni::max_char_value); !error.empty())
                                    return error;
                            }

                            char buffer[uni::max_char_len];
                            std::size_t len = uni::EncodeCharToBuffer(n, buffer);
                            if (only_single_char && n > 0x7f)
                                return "Escape sequence is too large for this character type.";
                            for (std::size_t i = 0; i < len; i++)
                                *output++ = buffer[i];
                        }
                        break;

                      default:
                        source--;
                        if (*source >= '0' && *source <= '7')
                        {
                            char32_t n = 0;
                            if (auto error = ConsumeDigits(n, false, 3, 0xff); !error.empty())
                                return error;
                            *output++ = char(n);
                            break;
                        }
                        return "Invalid escape sequence.";
                    }
                }

                if (only_single_char && char_counter == 0)
                    return "Expected a character.";

                if (quote_char)
                {
                    if (*source != quote_char)
                        return CFG_TA_FMT_NAMESPACE::format("Expected closing `{}`.", quote_char);
                    source++;
                }

                return "";
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

        // Caches type names produced by `Demangler`.
        template <typename T>
        [[nodiscard]] const std::string &TypeName()
        {
            static const std::string ret = Demangler{}(typeid(T).name());
            return ret;
        }

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
                            if (identifier.empty() || &identifier.back() + 1 != &ch || !chars::IsDigit(identifier.front()))
                                state = CharKind::character;
                        }
                        else if (chars::IsIdentifierChar(ch))
                        {
                            // We reset `identifier` lazily here, as opposed to immediately,
                            // to allow function calls with whitespace between the identifier and `(`.
                            if (!chars::IsIdentifierChar(prev_ch))
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
        }

        // Returns true if the test name `name` matches regex `regex`.
        // Currently this matches the whole name or any prefix ending at `/` (including or excluding `/`).
        [[nodiscard]] CFG_TA_API bool TestNameMatchesRegex(std::string_view name, const std::regex &regex);
    }

    // String conversions.
    namespace string_conv
    {
        // This imitates `std::range_format`, except we don't deal with unescaped strings.
        enum class RangeKind
        {
            disabled, // Not a range.
            sequence, // [...]
            set, // {...}
            map, // {A: B, C: D}
            string, // "..."
        };


        // --- TO STRING ---

        // You normally shouldn't specialize this, specialize `ToStringTraits` defined below.
        // `DefaultToStringTraits` uses this for types that don't support the debug format `"{:?}"`.
        template <typename T, typename = void>
        struct DefaultFallbackToStringTraits
        {
            std::string operator()(const T &value) const
            // `std::formattable` is from C++23, and could be unavailable.
            requires std::default_initializable<CFG_TA_FMT_NAMESPACE::formatter<T, char>>
            {
                return CFG_TA_FMT_NAMESPACE::format("{}", value);
            }
        };

        // Don't specialize this, specialize `ToStringTraits` defined below.
        // `ToStringTraits` inherits from this by default.
        template <typename T, typename = void>
        struct DefaultToStringTraits
        {
            std::string operator()(const T &value) const
            requires requires {DefaultFallbackToStringTraits<T>{}(value);}
            {
                // There seems to be no way to use `std::
                if constexpr (requires(CFG_TA_FMT_NAMESPACE::formatter<T> f){f.set_debug_format();})
                    return CFG_TA_FMT_NAMESPACE::format("{:?}", value);
                else
                    return DefaultFallbackToStringTraits<T>{}(value);
            }
        };

        // You can specialize this for your types.
        template <typename T, typename = void>
        struct ToStringTraits : DefaultToStringTraits<T> {};

        // Whether `ToString()` works on `T`.
        // Ignores cvref-qualifiers (well, except volatile).
        template <typename T>
        concept SupportsToString = requires(const T &t){ToStringTraits<std::remove_cvref_t<T>>{}(t);};

        // Converts `value` to a string using `ToStringTraits`.
        // We don't support non-const ranges for now, and we probably shouldn't (don't want to mess up user's stateful views?).
        template <typename T>
        requires std::is_same_v<T, std::remove_cvref_t<T>>
        [[nodiscard]] std::string ToString(const T &value)
        {
            return ToStringTraits<std::remove_cvref_t<T>>{}(value);
        }

        // --- TO STRING SPECIALIZATIONS ---

        // Throw in some fallback formatters to escape strings, for format libraries that don't support this yet.
        template <>
        struct DefaultFallbackToStringTraits<char>
        {
            CFG_TA_API std::string operator()(char value) const;
        };
        template <>
        struct DefaultFallbackToStringTraits<std::string_view>
        {
            CFG_TA_API std::string operator()(std::string_view value) const;
        };
        // libstdc++ 13 has a broken non-SFINAE-friendly `formatter<const char *>::set_debug_string()`, which causes issues.
        // `std::string_view` formatter doesn't have this issue, so we just use it here instead.
        // Bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=112832
        template <> struct DefaultToStringTraits<std::string > : DefaultToStringTraits<std::string_view> {};
        template <> struct DefaultToStringTraits<      char *> : DefaultToStringTraits<std::string_view> {};
        template <> struct DefaultToStringTraits<const char *> : DefaultToStringTraits<std::string_view> {};
        // Somehow this catches const arrays too.
        template <std::size_t N> struct DefaultFallbackToStringTraits<char[N]> : DefaultFallbackToStringTraits<std::string_view> {};

        // `ToStringTraits` serializes this as is, without escaping or quotes.
        struct ExactString
        {
            std::string string;
        };
        template <>
        struct DefaultToStringTraits<ExactString>
        {
            std::string operator()(ExactString value) const
            {
                return std::move(value.string);
            }
        };

        // Type names.
        template <>
        struct DefaultToStringTraits<std::type_index>
        {
            CFG_TA_API std::string operator()(std::type_index value) const;
        };
        template <>
        struct DefaultToStringTraits<std::type_info> : DefaultToStringTraits<std::type_index> {};

        // Ranges.
        #if CFG_TA_FMT_HAS_RANGE_FORMAT == 0
        // This replaces `std::format_kind` (or `fmt::range_format_kind`) if the formatting library can't format ranges.
        // You can specialize this.
        // This classifier never returns `RangeKind::string`, just like the standard one.
        // NOTE: The standard (and libfmt) versions of this variable return junk for `std::string[_view]`, so ours does too, and we never use it on those types.
        template <typename T>
        constexpr RangeKind range_format_kind = []{
            if constexpr (!std::ranges::input_range<T>)
            {
                // A bit weird to check `input_range` when converting TO a string, but this is what `std::format_kind` uses,
                // and we should probably be consistent with it.
                return RangeKind::disabled;
            }
            else if constexpr (std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<T>>, T>)
            {
                // Straight from cppreference. Probably for ranges that contain instances of the same type, like `std::filesystem::path`?
                return RangeKind::disabled;
            }
            else if constexpr (requires{typename T::key_type;})
            {
                using Elem = std::remove_cvref_t<std::ranges::range_reference_t<T>>;
                // `std::tuple_size_v` is SFINAE-unfriendly, so we're using `std::tuple_size` instead.
                // This condition is slightly simplified compared to what cppreference uses, but whatever.
                if constexpr (requires{requires std::tuple_size<Elem>::value == 2;})
                    return RangeKind::map;
                else
                    return RangeKind::set;
            }
            else
            {
                return RangeKind::sequence;
            }
        }();

        // Range formatter.
        template <typename T>
        requires(range_format_kind<T> != RangeKind::disabled)
        struct DefaultToStringTraits<T>
        {
            std::string operator()(const T &value) const
            {
                if constexpr (range_format_kind<T> == RangeKind::string)
                {
                    if constexpr (std::contiguous_iterator<std::ranges::iterator_t<T>>)
                    {
                        std::basic_string_view<std::ranges::range_value_t<T>> view(std::to_address(value.begin()), std::to_address(value.end()));
                        return (ToString)(view);
                    }
                    else
                    {
                        std::basic_string<std::ranges::range_value_t<T>> string(value.begin(), value.end());
                        return (ToString)(string);
                    }
                }
                else
                {
                    constexpr bool use_braces = range_format_kind<T> != RangeKind::sequence;
                    std::string ret;
                    ret += "[{"[use_braces];
                    for (bool first = true; const auto &elem : value)
                    {
                        if (first)
                            first = false;
                        else
                            ret += ", ";

                        if constexpr (range_format_kind<T> == RangeKind::map)
                        {
                            ret += (ToString)(std::get<0>(elem));
                            ret += ": ";
                            ret += (ToString)(std::get<1>(elem));
                        }
                        else
                        {
                            ret += (ToString)(elem);
                        }
                    }
                    ret += "]}"[use_braces];
                    return ret;
                }
            }
        };

        template <typename T>
        concept TupleLike = requires{std::tuple_size<T>::value;}; // Note, `std::tuple_size_v` would be SFINAE-unfriendly.

        // Tuple formatter.
        template <TupleLike T>
        struct DefaultToStringTraits<T>
        {
            std::string operator()(const T &value) const
            {
                std::string ret = "(";
                [&]<std::size_t ...I>(std::index_sequence<I...>){
                    ([&]{
                        if constexpr (I > 0)
                            ret += ", ";
                        ret += (ToString)(std::get<I>(value));
                    }(), ...);
                }(std::make_index_sequence<std::tuple_size_v<T>>{});
                ret += ")";
                return ret;
            }
        };
        #endif


        // --- FROM STRING ---

        template <typename T>
        concept ScalarConvertibleFromString = (std::is_integral_v<T> && sizeof(T) <= sizeof(long long)) || (std::is_floating_point_v<T> && sizeof(T) <= sizeof(long double));

        // Calls the most suitable `std::strto*` for the specified type.
        // The return type might be wider than `T`.
        template <ScalarConvertibleFromString T>
        [[nodiscard]] auto strto_low(const char *str, char **str_end, int base = 0)
        {
            if constexpr (std::is_integral_v<T>)
            {
                constexpr bool use_long = sizeof(T) <= sizeof(long);
                constexpr bool is_signed = std::is_signed_v<T>;

                if constexpr (use_long && is_signed)
                    return std::strtol(str, str_end, base);
                else if constexpr (use_long && !is_signed)
                    return std::strtoul(str, str_end, base);
                else if constexpr (!use_long && is_signed)
                    return std::strtoll(str, str_end, base);
                else // !use_long && !is_signed
                    return std::strtoull(str, str_end, base);
            }
            else
            {
                if constexpr (sizeof(T) <= sizeof(float))
                    return std::strtof(str, str_end);
                else if constexpr (sizeof(T) <= sizeof(double))
                    return std::strtod(str, str_end);
                else if constexpr (sizeof(T) <= sizeof(long double))
                    return std::strtold(str, str_end);
            }
        }

        // Wraps `strto_low` to check for various errors. Reports errors by setting `*str_end` to `str`.
        template <ScalarConvertibleFromString T>
        [[nodiscard]] T strto(const char *str, const char **str_end, int base = 0)
        {
            if (std::isspace((unsigned char)*str))
            {
                *str_end = str;
                return 0;
            }

            char *end = const_cast<char *>(str);
            errno = 0; // `strto*` appears to indicate out-of-range errors only by setting `errno`.
            auto raw_result = (strto_low<T>)(str, &end, base);
            if (end == str || errno != 0)
            {
                *str_end = str;
                return 0;
            }

            T result = T(raw_result);

            // Check roundtrip conversion.
            if constexpr (!std::is_same_v<decltype(raw_result), T>)
            {
                // This wouldn't work for `signed T <-> unsigned T`, but we should never have sign mismatch here.
                if (decltype(raw_result)(result) != raw_result)
                {
                    *str_end = str;
                    return 0;
                }
            }

            *str_end = end;
            return result;
        }

        // Don't specialize this, specialize `FromStringTraits`.
        template <typename T>
        struct DefaultFromStringTraits {};

        template <typename T>
        struct FromStringTraits : DefaultFromStringTraits<T>
        {
            // Since we use `std::strtoX` for scalars internally, this forces us to use null-terminated strings for everything,
            //   so we can't accept a `std::string_view` here.
            // Returns the error message, or empty on success. `target` will always start value-initialized.
            // std::string operator()(T &target, const char *&string) const;
        };

        // Whether `FromString()` works on `T`. `T` must be cvref-unqualified.
        template <typename T>
        concept SupportsFromString =
            std::is_same_v<T, std::remove_cvref_t<T>>
            && requires(T &target, const char *&string)
            {
                { FromStringTraits<T>{}(target, string) } -> std::same_as<std::string>;
            };


        // --- FROM STRING SPECIALIZATIONS ---

        // Scalars.
        template <ScalarConvertibleFromString T>
        struct DefaultFromStringTraits<T>
        {
            [[nodiscard]] std::string operator()(T &target, const char *&string) const
            {
                const char *end = string;
                target = (strto<T>)(string, &end, 0);
                if (end == string)
                    return CFG_TA_FMT_NAMESPACE::format("Expected {}.", text::TypeName<T>());
                string = end;
                return "";
            }
        };

        // Single character.
        template <>
        struct DefaultFromStringTraits<char>
        {
            [[nodiscard]] std::string operator()(char &target, const char *&string) const
            {
                return text::escape::UnescapeString(string, '\'', &target, true);
            }
        };

        // Ranges.

        // Classifies the range for converting from a string. You almost never want to specialize this.
        // Normally you want to specialize `std::format_kind` or `fmt::range_format_kind` or our `ta_test::string_conv::range_format_kind`,
        //   depending on what formatting library you use.
        template <typename T>
        constexpr RangeKind from_string_range_format_kind =
        #if CFG_TA_FMT_HAS_RANGE_FORMAT == 0
            range_format_kind<T>;
        #else
            []{
                if constexpr (!std::input_range<T>)
                {
                    return RangeKind::disabled;
                }
                else
                {
                    auto value =
                    #if CFG_TA_FMT_HAS_RANGE_FORMAT == 1
                        CFG_TA_FMT_NAMESPACE::format_kind<T>;
                    #elif CFG_TA_FMT_HAS_RANGE_FORMAT == 2
                        CFG_TA_FMT_NAMESPACE::range_format_kind<T>::value;
                    #else
                    #error Invalid `CFG_TA_FMT_HAS_RANGE_FORMAT` value.
                    #endif
                    return
                        value == CFG_TA_FMT_NAMESPACE::format_kind::sequence ? RangeKind::sequence :
                        value == CFG_TA_FMT_NAMESPACE::format_kind::set      ? RangeKind::set :
                        value == CFG_TA_FMT_NAMESPACE::format_kind::map      ? RangeKind::map :
                        value == CFG_TA_FMT_NAMESPACE::format_kind::string || value == CFG_TA_FMT_NAMESPACE::format_kind::debug_string ? RangeKind::string :
                        RangeKind::disabled;
                }
            }();
        #endif

        // Our classifier returns junk for `std::string[_view]` by default, so we need those overrides.
        template <>
        inline constexpr RangeKind from_string_range_format_kind<std::string> = RangeKind::string;
        template <>
        inline constexpr RangeKind from_string_range_format_kind<std::string_view> = RangeKind::string;

        // Adjusts a range element type to prepare it for converting from string.
        template <typename T>
        struct AdjustRangeElemToConvertFromString {using type = T;};
        // Remove constness from a map key.
        template <typename T, typename U>
        struct AdjustRangeElemToConvertFromString<std::pair<const T, U>> {using type = std::pair<T, U>;};

        // Whether `T` is a range that possibly could be converted from string. We further constrain this concept below.
        template <typename T>
        concept RangeSupportingFromStringWeak = from_string_range_format_kind<T> != RangeKind::disabled;
        // Whether we can `.emplace_back()` to this range.
        // This is nice, because we can insert first, and then operate on a reference.
        template <typename T>
        concept RangeSupportingEmplaceBack = RangeSupportingFromStringWeak<T> &&
            std::is_same_v<typename AdjustRangeElemToConvertFromString<T>::type, T> &&
            requires(T &target) {requires std::is_same_v<decltype(target.emplace_back()), std::ranges::range_value_t<T> &>;};
        // Whether we can `.push_back()` to this range. Unsure what container would actually need this over `emplace_back()`, perhaps something non-standard?
        template <typename T>
        concept RangeSupportingPushBack = RangeSupportingFromStringWeak<T> &&
            requires(T &target, std::ranges::range_value_t<T> &&e){target.push_back(std::move(e));};
        // Whether we can `.insert()` to this range.
        template <typename T>
        concept RangeSupportingInsert = RangeSupportingFromStringWeak<T> &&
            requires(T &target, std::ranges::range_value_t<T> &&e){requires std::is_same_v<decltype(target.insert(std::move(e)).second), bool>;};
        // Whether this range can be converted from a string.
        template <typename T>
        concept RangeSupportingFromString = RangeSupportingEmplaceBack<T> || RangeSupportingPushBack<T> || RangeSupportingInsert<T>;

        // The actual code for ranges. Escaped strings are handled here too.
        template <RangeSupportingFromString T>
        struct DefaultFromStringTraits<T>
        {
            [[nodiscard]] std::string operator()(T &target, const char *&string) const
            {
                if constexpr (from_string_range_format_kind<T> == RangeKind::string)
                {
                    return text::escape::UnescapeString(string, '"', std::back_inserter(target), false);
                }
                else
                {
                    constexpr bool is_associative = from_string_range_format_kind<T> != RangeKind::sequence;
                    constexpr char brace_open = "[{"[is_associative];
                    constexpr char brace_close = "]}"[is_associative];

                    // Consume opening brace.
                    if (*string != brace_open)
                        return CFG_TA_FMT_NAMESPACE::format("Expected opening `{}`.", brace_open);
                    string++;

                    bool first = true;

                    while (true)
                    {
                        text::chars::SkipWhitespace(string);

                        // Stop on closing brace.
                        if (*string == brace_close)
                        {
                            string++;
                            break;
                        }

                        // Consume comma.
                        if (first)
                        {
                            first = false;
                        }
                        else
                        {
                            if (*string == ',')
                                string++;
                            else
                                return CFG_TA_FMT_NAMESPACE::format("Expected `,` or closing `{}`.", brace_close);

                            text::chars::SkipWhitespace(string);
                        }

                        using Elem = typename AdjustRangeElemToConvertFromString<std::ranges::range_value_t<T>>::type;

                        auto ConsumeElem = [&] CFG_TA_NODISCARD_LAMBDA (Elem &target) -> std::string
                        {
                            if constexpr (from_string_range_format_kind<T> == RangeKind::map)
                            {
                                // Note that `tuple_size_v` would not be SFINAE-friendly.
                                // Using `requires` for SFINAE-friendliness.
                                static_assert(requires{requires std::tuple_size<Elem>::value == 2;}, "Range kind is set to `map`, but its element type is not a 2-tuple.");

                                std::string error = FromStringTraits<std::tuple_element_t<0, Elem>>{}(std::get<0>(target), string);
                                if (!error.empty())
                                    return error;
                                text::chars::SkipWhitespace(string);

                                if (*string != ':')
                                    return "Expected `:` after the element key.";
                                string++;

                                text::chars::SkipWhitespace(string);
                                return FromStringTraits<std::tuple_element_t<1, Elem>>{}(std::get<1>(target), string);
                            }
                            else
                            {
                                return FromStringTraits<Elem>{}(target, string);
                            }
                        };

                        // Insert the element.
                        if constexpr (RangeSupportingInsert<T>)
                        {
                            const char *old_string = string;
                            Elem elem{};
                            std::string error = ConsumeElem(elem);
                            if (!error.empty())
                                return error;
                            if (!target.insert(std::move(elem)).second)
                            {
                                string = old_string;
                                return "Duplicate key.";
                            }
                        }
                        else if constexpr (RangeSupportingEmplaceBack<T>)
                        {
                            std::string error = ConsumeElem(target.emplace_back());
                            if (!error.empty())
                                return error;
                        }
                        else if constexpr (RangeSupportingPushBack<T>)
                        {
                            Elem elem{};
                            std::string error = ConsumeElem(elem);
                            if (!error.empty())
                                return error;
                            target.push_back(std::move(elem));
                        }
                        else
                        {
                            static_assert(meta::AlwaysFalse<T>::value, "Internal error: Unknown container flavor.");
                        }
                    }

                    return "";
                }
            }
        };

        // Tuples.

        template <TupleLike T>
        struct DefaultFromStringTraits<T>
        {
            [[nodiscard]] std::string operator()(T &target, const char *&string) const
            {
                if (*string != '(')
                    return "Expected opening `(`.";
                string++;

                std::string error;

                bool fail = [&]<std::size_t ...I>(std::index_sequence<I...>){
                    return ([&]{
                        if constexpr (I != 0)
                        {
                            text::chars::SkipWhitespace(string);

                            if (*string != ',')
                            {
                                error = "Expected `,`.";
                                return true;
                            }
                            string++;
                        }

                        text::chars::SkipWhitespace(string);

                        error = FromStringTraits<std::tuple_element_t<I, T>>{}(std::get<I>(target), string);
                        return !error.empty();
                    }() || ...);
                }(std::make_index_sequence<std::tuple_size_v<T>>{});

                if (fail)
                    return error;

                text::chars::SkipWhitespace(string);

                if (*string != ')')
                    return "Expected closing `)`.";
                string++;

                return "";
            }
        };
    }

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
            virtual bool ProcessFlag(const Runner &runner, BasicModule &this_module, std::string_view input, std::function<std::optional<std::string_view>()> request_arg) = 0;
        };

        // A command line flag taking no arguments.
        struct SimpleFlag : BasicFlag
        {
            std::string flag;

            using Callback = std::function<void(const Runner &runner, BasicModule &this_module)>;
            Callback callback;

            SimpleFlag(std::string flag, std::string help_desc, Callback callback)
                : BasicFlag(std::move(help_desc)), flag(std::move(flag)), callback(std::move(callback))
            {}

            std::string HelpFlagSpelling() const override
            {
                return "--" + flag;
            }

            bool ProcessFlag(const Runner &runner, BasicModule &this_module, std::string_view input, std::function<std::optional<std::string_view>()> request_arg) override
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

            using Callback = std::function<void(const Runner &runner, BasicModule &this_module, bool value)>;
            Callback callback;

            BoolFlag(std::string flag, std::string help_desc, Callback callback)
                : BasicFlag(std::move(help_desc)), flag(std::move(flag)), callback(std::move(callback))
            {}

            std::string HelpFlagSpelling() const override
            {
                return "--[no-]" + flag;
            }

            bool ProcessFlag(const Runner &runner, BasicModule &this_module, std::string_view input, std::function<std::optional<std::string_view>()> request_arg) override
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
            char short_flag = '\0'; // Zero if none.

            using Callback = std::function<void(const Runner &runner, BasicModule &this_module, std::string_view value)>;
            Callback callback;

            // `short_flag` can be zero if none.
            StringFlag(std::string flag, char short_flag, std::string help_desc, Callback callback)
                : BasicFlag(std::move(help_desc)), flag(std::move(flag)), short_flag(short_flag), callback(std::move(callback))
            {}

            std::string HelpFlagSpelling() const override
            {
                std::string ret;
                if (short_flag)
                {
                    ret += '-';
                    ret += short_flag;
                    ret += ',';
                }
                return ret + "--" + flag + " ...";
            }

            bool ProcessFlag(const Runner &runner, BasicModule &this_module, std::string_view input, std::function<std::optional<std::string_view>()> request_arg) override
            {
                (void)request_arg;

                if (!input.starts_with('-'))
                    return false;
                input.remove_prefix(1);

                // The short form.
                if (short_flag && input == std::string_view(&short_flag, 1))
                {
                    auto arg = request_arg();
                    if (!arg)
                        return false;
                    callback(runner, this_module, *arg);
                    return true;
                }

                if (!input.starts_with('-'))
                    return false;
                input.remove_prefix(1);

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

    // This lets you determine the stack of assertions (and other things) that are currently executing.
    // Also this manages the logs.
    namespace context
    {
        // --- CONTEXT STACK ---

        // A single entry in the context stack.
        // You can add your own classes derived from this, if you add custom modules that can process them.
        struct BasicFrame
        {
          protected:
            BasicFrame() = default;
            BasicFrame(const BasicFrame &) = default;
            BasicFrame &operator=(const BasicFrame &) = default;
            virtual ~BasicFrame() = default;
        };
        using Context = std::span<const std::shared_ptr<const BasicFrame>>;

        [[nodiscard]] CFG_TA_API Context CurrentContext();

        // While this object is alive, the thing passed to it will be included in the context stack which is printed on some failures.
        // This is a low-level feature, you probably shouldn't use it directly. We have higher level mechanisms built on top of it.
        class FrameGuard
        {
            const BasicFrame *frame_ptr = nullptr;

          public:
            // Stores a frame pointer in the stack. Make sure it doesn't dangle.
            // Note, can't pass a reference here, because it would be ambiguous with the copy constructor
            //   when we inherit from both the `BasicFrame` and the `FrameGuard`.
            // If we could use a reference, we'd need a second deleted constructor to reject rvalues.
            // Can pass a null pointer here, then we do nothing.
            CFG_TA_API explicit FrameGuard(std::shared_ptr<const BasicFrame> frame) noexcept;

            FrameGuard(const FrameGuard &) = delete;
            FrameGuard &operator=(const FrameGuard &) = delete;

            [[nodiscard]] explicit operator bool() const {return bool(frame_ptr);}

            // Removes the frame as if the guard was destroyed. Repeated calls have no effect.
            // This can only be called if this is the last element in the stack, otherwise you get a hard error.
            CFG_TA_API void Reset();

            CFG_TA_API ~FrameGuard();
        };

        // --- LOGS ---

        // A single entry in the log, either scoped or unscoped.
        class LogEntry
        {
            std::size_t incremental_id = 0;

            std::string message;

            std::string (*message_refresh_func)(const void *data) = nullptr;
            const void *message_refresh_data = nullptr;

            void FixMessage()
            {
                if (message.ends_with('\n'))
                    message.pop_back();
            }

          public:
            // The constructors are primarily for internal use.

            LogEntry() {}

            // A fixed message.
            LogEntry(std::size_t incremental_id, std::string &&message)
                : incremental_id(incremental_id), message(std::move(message))
            {
                FixMessage();
            }
            // A generated message. Doesn't own the function.
            template <typename F>
            LogEntry(std::size_t incremental_id, const F &generate_message)
            requires requires{generate_message();}
                : incremental_id(incremental_id),
                message_refresh_func([](const void *data)
                {
                    return (*static_cast<const F *>(data))();
                }),
                message_refresh_data(&generate_message)
            {}

            // The incremental ID of the log entry.
            [[nodiscard]] std::size_t IncrementalId() const {return incremental_id;}

            // This will be called automatically, you don't need to touch it (and can't, since it's non-const).
            // Regenerates the message using the stored function, if any.
            void RefreshMessage()
            {
                if (message_refresh_func)
                {
                    message = message_refresh_func(message_refresh_data);
                    FixMessage();
                }
            }

            // The message. Can be lazy, so this is not thread-safe.
            // Can theoretically throw when evaluating the lazy message, keep that in mind.
            [[nodiscard]] std::string_view Message() const
            {
                return message;
            }
        };

        // The current scoped log. The unscoped log sits in the `BasicModule::RunSingleTestResults`.
        // None of the pointers will be null.
        [[nodiscard]] CFG_TA_API std::span<const LogEntry *const> CurrentScopedLog();
    }

    // Information about a single exception, without nesting.
    struct SingleException
    {
        // The exception we're analyzing.
        std::exception_ptr exception;
        // The exception type. This is set to `typeid(void)` if the type is unknown.
        std::type_index type = typeid(void);
        // This is usually obtained from `e.what()`.
        std::string message;

        [[nodiscard]] bool IsTypeKnown() const {return type != typeid(void);}

        // Obtains the type name from `type`, using `text::Demangler`.
        // If `IsTypeKnown() == false`, returns an empty string instead.
        [[nodiscard]] CFG_TA_API std::string GetTypeName() const;
    };

    // Given an exception, tries to get an error message from it, using the current modules. Shouldn't throw.
    // Returns a vector with at least one element. The last element may or may not have `IsTypeKnown() == false`, and other elements will always be known.
    CFG_TA_API void AnalyzeException(const std::exception_ptr &e, const std::function<void(SingleException elem)> &func);


    // --- MODULE BASE ---

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
        // A source location with a value of `__COUNTER__`.
        struct SourceLocWithCounter : SourceLoc
        {
            int counter = 0;
            friend auto operator<=>(const SourceLocWithCounter &, const SourceLocWithCounter &) = default;
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
            // Mostly for internal use. Used to call certain functions on every module.
            const ModuleLists *modules = nullptr;

            // The number of tests to run.
            std::size_t num_tests = 0;
            // The total number of known tests, including the skipped ones.
            std::size_t num_tests_with_skipped = 0;
        };
        struct RunTestsProgress : RunTestsInfo
        {
            std::vector<const BasicTestInfo *> failed_tests;
        };
        struct RunTestsResults : RunTestsProgress {};
        // This is called first, before any tests run.
        virtual void OnPreRunTests(const RunTestsInfo &data) {(void)data;}
        // This is called after all tests run.
        virtual void OnPostRunTests(const RunTestsResults &data) {(void)data;}

        // Describes a generator created with `TA_GENERATE(...)`.
        struct BasicGenerator
        {
            BasicGenerator() = default;
            BasicGenerator(const BasicGenerator &) = delete;
            BasicGenerator &operator=(const BasicGenerator &) = delete;
            virtual ~BasicGenerator() = default;

            // The source location.
            [[nodiscard]] virtual const BasicModule::SourceLocWithCounter &GetLocation() const = 0;
            // The identifier passed to `TA_GENERATE(...)`.
            [[nodiscard]] virtual std::string_view GetName() const = 0;

            // The return type.
            // Note that `std::type_index` can't encode cvref-qualifiers, see `GetTypeFlags()` for those.
            [[nodiscard]] virtual std::type_index GetType() const = 0;

            enum class TypeFlags
            {
                unqualified = 0,
                const_ = 1 << 0,
                volatile_ = 1 << 1,
                lvalue_ref = 1 << 2,
                rvalue_ref = 1 << 3,
                any_ref = lvalue_ref | rvalue_ref,
            };
            DETAIL_TA_FLAG_OPERATORS_IN_CLASS(TypeFlags)
            using enum TypeFlags;
            // Returns cvref-qualifiers of the type, since they don't fit into `std::type_index`.
            [[nodiscard]] virtual TypeFlags GetTypeFlags() const = 0;

            // Demangles the type from `GetType()`, and adds qualifiers from `GetTypeFlags()`.
            [[nodiscard]] CFG_TA_API std::string GetTypeName() const;

            // Whether the last generated value is the last one for this generator.
            // Note that when the generator is operating under a module override, the system doesn't respect this variable (see below).
            [[nodiscard]] bool IsLastValue() const {return !repeat;}

            // This is false when the generator is reached for the first time and didn't generate a value yet.
            // Calling `GetValue()` in that case will crash.
            [[nodiscard]] virtual bool HasValue() const = 0;

            // Whether `string_conv::ToString()` works for this generated type.
            [[nodiscard]] virtual bool ValueConvertibleToString() const = 0;
            // Converts the current `GetValue()` to a string, or returns an empty string if `ValueConvertibleToString()` is false.
            [[nodiscard]] virtual std::string ValueToString() const = 0;

            // Whether this value is custom (as in from `--generate ...gen=value`), as opposed to being naturally generated.
            [[nodiscard]] bool IsCustomValue() const {return this_value_is_custom;}

            // This is incremented every time a new value is generated.
            // You can treat this as a 1-based value index.
            [[nodiscard]] std::size_t NumGeneratedValues() const {return num_generated_values;}
            // This is incremented every time a new custom value is inserted (as in by `--generate ...gen=value`).
            // You can treat this as a 1-based value index.
            [[nodiscard]] std::size_t NumCustomValues() const {return num_custom_values;}

            // Mostly for internal use. Generates the next value and updates `repeat`.
            virtual void Generate() = 0;

            enum class OverrideStatus
            {
                // This generator doesn't have an override attached to it.
                no_override,
                // Override invoked successfully.
                success,
                // Override invoked, but there are no move values to be generated.
                // Unlike `HasValue()`, this lets you dynamically back out from a generation.
                no_more_values,
            };

            // For internal use. Calls an override registered with `OnRegisterGeneratorOverride()`, if any.
            [[nodiscard]] CFG_TA_API OverrideStatus RunGeneratorOverride();

            // Inserting custom values:

            // Whether the value type can be created from a string using `string_conv::FromStringTraits`.
            [[nodiscard]] virtual bool ValueConvertibleFromString() const = 0;

            // Replaces the current value with the one parsed from the string.
            // Returns the error message on failure, or an empty string on success.
            // Advances `string` to the next unused character, or to the failure position on error.
            // If `ValueConvertibleFromString() == false`, will always return an error.
            // On failure, can corrupt the current object. You should probably abort everything in that case.
            [[nodiscard]] virtual std::string ReplaceValueFromString(const char *&string) = 0;

            // Returns true if the type has overloaded `==` and `ValueConvertibleFromString() == true`.
            [[nodiscard]] virtual bool ValueEqualityComparableToString() const = 0;
            // Parses the value from a string, then compares it with the current value using `==`, writing the result to `equal`.
            // Returns the parsing error if any. Also returns an error if `ValueEqualityComparableToString()` is false.
            // Writes `equal = false` on any failure, including if the generator holds no value.
            [[nodiscard]] virtual std::string ValueEqualsToString(const char *&string, bool &equal) const = 0;

          protected:
            // `Generate()` updates this to indicate whether more values will follow.
            bool repeat = true;

            // Whether the current value is custom, as opposed to being naturally generated.
            bool this_value_is_custom = false;

            // How many values were naturally generated.
            std::size_t num_generated_values = 0;
            // How many custom values were used.
            std::size_t num_custom_values = 0;

            // Optional.
            BasicModule *overriding_module = nullptr;

            // `BasicTypedGenerator` needs to wrap references to store them in a `std::variant`.
            // This also nicely disambiguates types in the variant, generated values from custom values.
            template <typename T>
            struct ValueWrapper
            {
                T value;

                // This lets us copy-elide the construction from a lambda return value.
                template <typename U>
                ValueWrapper(U &&func, bool &repeat)
                    : value(std::forward<U>(func)(repeat))
                {}
            };
        };

        // The generator for a specific type.
        // `ReturnType` may or may not be a reference.
        template <typename ReturnType>
        struct BasicTypedGenerator : BasicGenerator
        {
          protected:
            // Perhaps we could use two optionals instead of a single variant to make failed custom value parse not clobber the existing value,
            // but we don't really need this now (we don't recover from parse errors, and we shouldn't have a useful value in storage when parsing anyway).

            using StorageVariant = std::variant<
                // 0. Nothing.
                std::monostate,
                // 1. Naturally generated value. A pointer if the type is a reference, or a value otherwise.
                ValueWrapper<ReturnType>,
                // 2. A custom value. Stores the object.
                std::remove_cvref_t<ReturnType>
            >;

            // This must be mutable because we have custom value overrides. They are stored by value here,
            // and we might need to return them by a non-const reference from a const function.
            mutable StorageVariant storage;

            static constexpr bool supports_from_string =
                std::default_initializable<std::remove_cvref_t<ReturnType>> &&
                string_conv::SupportsFromString<std::remove_cvref_t<ReturnType>>;

            static constexpr bool supports_from_string_and_equality = []{
                if constexpr (supports_from_string)
                    return std::equality_comparable<std::remove_cvref_t<ReturnType>>;
                else
                    return false;
            }();

          public:
            [[nodiscard]] std::type_index GetType() const override final
            {
                return typeid(ReturnType);
            }
            [[nodiscard]] TypeFlags GetTypeFlags() const override final
            {
                return
                    TypeFlags::const_ * std::is_const_v<ReturnType> |
                    TypeFlags::volatile_ * std::is_volatile_v<ReturnType> |
                    TypeFlags::lvalue_ref * std::is_lvalue_reference_v<ReturnType> |
                    TypeFlags::rvalue_ref * std::is_rvalue_reference_v<ReturnType>;
            }

            [[nodiscard]] bool HasValue() const override final
            {
                return storage.index() == 1 || storage.index() == 2;
            }

            [[nodiscard]] bool ValueConvertibleToString() const override final
            {
                return string_conv::SupportsToString<ReturnType>;
            }

            [[nodiscard]] std::string ValueToString() const override final
            {
                if constexpr (string_conv::SupportsToString<ReturnType>)
                    return string_conv::ToString(GetValue());
                else
                    return {};
            }

            [[nodiscard]] const ReturnType &GetValue() const
            {
                return std::visit(meta::Overload{
                    [](std::monostate) -> const ReturnType &
                    {
                        HardError("The generator somehow holds no value."); // This shouldn't normally happen.
                    },
                    [](const ValueWrapper<ReturnType> &value) -> const ReturnType &
                    {
                        return value.value;
                    },
                    [](std::remove_cvref_t<ReturnType> &value) -> const ReturnType &
                    {
                        return value;
                    }
                }, storage);
            }

            // Inserting custom values:

            [[nodiscard]] bool ValueConvertibleFromString() const override final
            {
                return supports_from_string;
            }

            [[nodiscard]] std::string ReplaceValueFromString(const char *&string) override final
            {
                if constexpr (supports_from_string)
                {
                    struct Guard
                    {
                        BasicTypedGenerator &self;
                        bool ok = false;

                        ~Guard()
                        {
                            if (!ok)
                                self.storage.template emplace<0>();
                        }
                    };

                    auto &target = storage.template emplace<2>();
                    Guard guard{.self = *this};

                    std::string error = string_conv::FromStringTraits<std::remove_cvref_t<ReturnType>>{}(target, string);
                    if (!error.empty())
                        return error;

                    guard.ok = true;

                    this->this_value_is_custom = true;
                    this->num_custom_values++;
                    return "";
                }
                else
                {
                    return "This type can't be deserialized from a string.";
                }
            }

            [[nodiscard]] virtual bool ValueEqualityComparableToString() const override
            {
                return supports_from_string_and_equality;
            }

            [[nodiscard]] virtual std::string ValueEqualsToString(const char *&string, bool &equal) const override
            {
                equal = false;
                if constexpr (supports_from_string_and_equality)
                {
                    if (!HasValue())
                        return "";

                    std::remove_cvref_t<ReturnType> value{};
                    std::string error = string_conv::FromStringTraits<std::remove_cvref_t<ReturnType>>{}(value, string);
                    if (!error.empty())
                        return error;

                    equal = value == GetValue();
                    return "";
                }
                else if constexpr (supports_from_string)
                {
                    return "This type doesn't overload the equality comparison.";
                }
                else
                {
                    return "This type can't be deserialized from a string.";
                }
            }
        };

        struct RunSingleTestInfo
        {
            const RunTestsProgress *all_tests = nullptr;
            const BasicTestInfo *test = nullptr;

            // The generator stack.
            // This starts empty when entering the test for the first time.
            // Reaching `TA_GENERATE` can push or modify the last element of the stack.
            // Right after `OnPostRunSingleTest`, any trailing elements with `.IsLastValue() == true` are pruned.
            // If the stack isn't empty after that, the test is restarted with the same stack.
            std::vector<std::unique_ptr<const BasicGenerator>> generator_stack;

            // True when entering the test for the first time, as opposed to repeating it because of a generator.
            // This is set to `generator_stack.empty()` when entering the test.
            bool is_first_generator_repetition = false;
        };
        struct RunSingleTestProgress : RunSingleTestInfo
        {
            // This is guaranteed to not contain any lazy log statements.
            std::vector<context::LogEntry> unscoped_log;

            // Which generator in `RunSingleTestInfo::generator_stack` we expect to hit next, or `generator_stack.size()` if none.
            // This starts at `0` every time the test is entered.
            // When exiting a test normally, this should be at `generator_stack.size()`, otherwise you have a non-deterministic failure in your tests.
            std::size_t generator_index = 0;

            // You can set this to true to break after the test.
            mutable bool should_break = false;
        };
        struct RunSingleTestResults : RunSingleTestProgress
        {
            // Whether the current test has failed.
            // When generators are involved, this refers only to the current repetition.
            bool failed = false;

            // True if we're about to leave the test for the last time.
            // This should be equivalent to `generator_stack.empty()`. This is set right before leaving the test.
            bool is_last_generator_repetition = false;
        };
        // This is called before every single test runs.
        virtual void OnPreRunSingleTest(const RunSingleTestInfo &data) {(void)data;}
        // This is called after every single test runs.
        virtual void OnPostRunSingleTest(const RunSingleTestResults &data) {(void)data;}

        struct GeneratorCallInfo
        {
            const RunSingleTestProgress *test = nullptr;
            const BasicGenerator *generator = nullptr;

            // Whether we're generating a new value, or just reusing the existing one.
            bool generating_new_value = false;
        };

        // This is called after every `TA_GENERATE(...)`.
        virtual void OnPostGenerate(const GeneratorCallInfo &data) {(void)data;}

        // Return true if you want this module to have special control over this generator.
        // If you do this, you must override `OnGeneratorOverride()`, see below.
        // This also changes the behavior of `TA_GENERATE(...)` slightly, it will generate new values between tests and
        // not when the control flow reaches it (except for the first time it's reached).
        virtual bool OnRegisterGeneratorOverride(const RunSingleTestProgress &test, const BasicGenerator &generator) {(void)test; (void)generator; return false;}
        // If you returned true from `OnRegisterGeneratorOverride()`, this function will be called instead of `generator.Generate()`.
        // You must call `generator.Generate()` (possibly several times to skip values) or `generator.ReplaceValueFromString()`.
        // Returning true from this means that there's no more values (unlike non-overridden generators, we can back out from a generation without knowing
        //   which value is the last one beforehand).
        // You must return true from this when the generator is exhausted, `IsLastValue()` is ignored when an override is active.
        virtual bool OnOverrideGenerator(const RunSingleTestProgress &test, BasicGenerator &generator) {(void)test; (void)generator; return false;}

        // --- FAILING TESTS ---

        // This is called when a test fails for any reason, followed by a more specific callback (see below).
        // Note that the test can continue to run after this, if this is a delayed (soft) failure.
        // Note that this is called at most once per test, even if after a soft failure something else fails.
        virtual void OnPreFailTest(const RunSingleTestProgress &data) {(void)data;}

        struct BasicAssertionExpr
        {
          protected:
            ~BasicAssertionExpr() = default;

          public:
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
        };
        struct BasicAssertionInfo : context::BasicFrame
        {
            // You can set this to true to trigger a breakpoint.
            mutable bool should_break = false;

            // The enclosing assertion, if any.
            const BasicAssertionInfo *enclosing_assertion = nullptr;

            // Where the assertion is located in the source.
            [[nodiscard]] virtual const SourceLoc &SourceLocation() const = 0;

            // Returns the user message.
            // This is lazy, the first call evaluates the message. This means it's not thread-safe.
            [[nodiscard]] virtual std::optional<std::string_view> GetUserMessage() const = 0;

            // The assertion is printed as a sequence of the elements below:

            // A fixed string, such as the assertion macro name itself, or its call parentheses.
            struct DecoFixedString {std::string_view string;};
            // An expression that should be printed with syntax highlighting.
            struct DecoExpr {std::string_view string;};
            // An expression with syntax highlighting and argument values. More than one per assertion weren't tested.
            struct DecoExprWithArgs {const BasicAssertionExpr *expr = nullptr;};
            // `std::monostate` indicates that there should be no more
            using DecoVar = std::variant<std::monostate, DecoFixedString, DecoExpr, DecoExprWithArgs>;
            // Returns one of the elements to be printed.
            [[nodiscard]] virtual DecoVar GetElement(int index) const = 0;
        };
        // Called when an assertion fails.
        virtual void OnAssertionFailed(const BasicAssertionInfo &data) {(void)data;}

        // Called when an exception falls out of an assertion or out of the entire test (in the latter case `assertion` will be null).
        // `assertion` is provided solely to allow you to do `assertion->should_break = true`. If you just want to print the failure context,
        // use `namespace context` instead, it will give you the same assertion and more.
        virtual void OnUncaughtException(const RunSingleTestInfo &test, const BasicAssertionInfo *assertion, const std::exception_ptr &e) {(void)test; (void)assertion; (void)e;}

        // A compile-time information about a `TA_MUST_THROW(...)` call.
        struct MustThrowStaticInfo
        {
            // The source location of `TA_MUST_THROW(...)`.
            BasicModule::SourceLoc loc;
            // The macro name used, e.g. `TA_MUST_THROW`.
            std::string_view macro_name;
            // The spelling of the macro argument.
            std::string_view expr;
        };
        // A runtime information about a `TA_MUST_THROW(...)` call
        struct MustThrowDynamicInfo
        {
          protected:
            ~MustThrowDynamicInfo() = default;

          public:
            // This is lazy, the first call constructs the message.
            // This makes it not thread-safe.
            virtual std::optional<std::string_view> GetUserMessage() const = 0;
        };
        // This in the context stack means that a `TA_MUST_THROW(...)` is currently executing.
        struct MustThrowInfo : context::BasicFrame
        {
            // Never null.
            const MustThrowStaticInfo *static_info = nullptr;
            // Never null.
            const MustThrowDynamicInfo *dynamic_info = nullptr;
        };
        // This in the context stack means that we're currently checking `CaughtException` returned from `TA_MUST_THROW(...)`.
        struct CaughtExceptionInfo : context::BasicFrame
        {
            std::vector<SingleException> elems;
            // Never null.
            const MustThrowStaticInfo *static_info = nullptr;
            // This is only available until the end of the full expression where `TA_MUST_THROW(...)` was initially executed.
            std::weak_ptr<const MustThrowDynamicInfo> dynamic_info;
        };

        // This is called when `TA_MUST_THROW` doesn't throw an exception.
        // If `should_break` ends up true (it's false by default), then a breakpoint at the call site will be triggered.
        virtual void OnMissingException(const MustThrowInfo &data, bool &should_break) {(void)data; (void)should_break;}

        // --- MISC ---

        struct ExplainedException
        {
            // The exception type. You must set this, since `typeid(void)` is reserved for unknown exceptions.
            std::type_index type = typeid(void);
            // The exception message, normally from `e.what()`.
            std::string message;
            // The nested exception, if any.
            std::exception_ptr nested_exception;
        };
        // This is called when an exception needs to be converted to a string.
        // Return the information on your custom exception type, if they don't inherit from `std::exception`.
        // Do nothing (or throw) to let some other module handle this.
        [[nodiscard]] virtual std::optional<ExplainedException> OnExplainException(const std::exception_ptr &e) const {(void)e; return {};}

        // This is called before entering try/catch blocks, so you can choose between that and just executing directly. (See `--catch`.)
        // `should_catch` defaults to true.
        // This is NOT called by `TA_MUST_THROW(...)`.
        virtual void OnPreTryCatch(bool &should_catch) {(void)should_catch;}


        // All virtual functions of this interface must be listed here.
        // See `class ModuleLists` for how this list is used.
        #define DETAIL_TA_MODULE_FUNCS(x) \
            x(GetFlags) \
            x(OnUnknownFlag) \
            x(OnMissingFlagArgument) \
            x(OnFilterTest) \
            x(OnPreRunTests) \
            x(OnPostRunTests) \
            x(OnPreRunSingleTest) \
            x(OnPostRunSingleTest) \
            x(OnPostGenerate) \
            x(OnRegisterGeneratorOverride) \
            /* `OnOverrideGenerator` isn't needed */ \
            x(OnPreFailTest) \
            x(OnAssertionFailed) \
            x(OnUncaughtException) \
            x(OnMissingException) \
            x(OnExplainException) \
            x(OnPreTryCatch) \

        enum class InterfaceFunction
        {
            #define DETAIL_TA_X(func_) func_,
            DETAIL_TA_MODULE_FUNCS(DETAIL_TA_X)
            #undef DETAIL_TA_X
            _count [[maybe_unused]],
        };
        // For internal use, don't use and don't override. Returns the mask of functions implemented by this class.
        [[nodiscard]] virtual unsigned int Detail_ImplementedFunctionsMask() const = 0;
        // For internal use. Returns true if the specified function is overriden in the derived class.
        [[nodiscard]] bool ImplementsFunction(InterfaceFunction func) const
        {
            return Detail_ImplementedFunctionsMask() & (1 << int(func));
        }
    };


    // --- MODULE STORAGE ---

    // Thread state, modules.
    namespace detail
    {
        // The global per-thread state.
        struct GlobalThreadState
        {
            BasicModule::RunSingleTestResults *current_test = nullptr;
            BasicModule::BasicAssertionInfo *current_assertion = nullptr;

            // This is used to print (or just examine) the current context.
            // All currently running assertions go there, and possibly other things.
            std::vector<std::shared_ptr<const context::BasicFrame>> context_stack;
            // This is used to deduplicate the `context_stack` elements.
            std::set<const context::BasicFrame *> context_stack_set;

            // Each log statement (scoped or not) receives an incremental thread-specific ID.
            std::size_t log_id_counter = 0;
            // The current scoped log, which is what `context::CurrentScopedLog()` returns.
            // The unscoped log sits in `BasicModule::RunSingleTestResults`.
            std::vector<context::LogEntry *> scoped_log;

            // Gracefully fails the current test, if not already failed.
            // Call this first, before printing any messages.
            CFG_TA_API void FailCurrentTest();
        };
        [[nodiscard]] CFG_TA_API GlobalThreadState &ThreadState();

        // Returns true if `P` is a member function pointer of a class other than `BasicModule`.
        template <auto P>
        struct IsOverriddenModuleFunction
            : std::bool_constant<!std::is_same_v<typename meta::MemberPointerClass<decltype(P)>::type, BasicModule>>
        {};

        // Inherits from a user module, and checks which virtual functions were overriden.
        template <typename T>
        struct ModuleWrapper final : T
        {
            using T::T;

            unsigned int Detail_ImplementedFunctionsMask() const override final
            {
                constexpr unsigned int ret = []{
                    unsigned int value = 0;
                    #define DETAIL_TA_X(func_) \
                        if constexpr (detail::IsOverriddenModuleFunction<&T::func_>::value) \
                            value |= 1 << int(BasicModule::InterfaceFunction::func_);
                    DETAIL_TA_MODULE_FUNCS(DETAIL_TA_X)
                    #undef DETAIL_TA_X
                    return value;
                }();
                return ret;
            }
        };
    }


    // A pointer to a class derived from `BasicModule`.
    class ModulePtr
    {
        std::unique_ptr<BasicModule> ptr;

        template <std::derived_from<BasicModule> T, typename ...P>
        requires std::constructible_from<detail::ModuleWrapper<T>, P &&...>
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
    requires std::constructible_from<detail::ModuleWrapper<T>, P &&...>
    [[nodiscard]] ModulePtr MakeModule(P &&... params)
    {
        ModulePtr ret;
        ret.ptr = std::make_unique<detail::ModuleWrapper<T>>(std::forward<P>(params)...);
        return ret;
    }

    // A non-owning wrapper on top of a module list.
    // Additionally stores lists of modules implemeting certain functions, to optimize the calls to them.
    // It's constructed once we start running tests, since that's when the module becomes frozen,
    // and then becomes the only thing modules can use to interact with the test runner, since there's no way for them to obtain a runner reference.
    class ModuleLists
    {
        std::span<const ModulePtr> all_modules;
        std::array<std::vector<BasicModule *>, std::size_t(BasicModule::InterfaceFunction::_count)> lists;

      public:
        ModuleLists() {}
        ModuleLists(std::span<const ModulePtr> all_modules)
            : all_modules(all_modules)
        {
            for (std::size_t i = 0; i < std::size_t(BasicModule::InterfaceFunction::_count); i++)
            {
                lists[i].reserve(all_modules.size());
                for (const auto &m : all_modules)
                {
                    if (m->ImplementsFunction(BasicModule::InterfaceFunction(i)))
                        lists[i].push_back(m.get());
                }
            }
        }

        // Returns all stored modules.
        [[nodiscard]] std::span<const ModulePtr> AllModules() const {return all_modules;}

        // Calls `func` for every module of type `T` or derived from `T`.
        // `func` is `(T &module) -> bool`. If `func` returns true, the function stops immediately and also returns true.
        template <typename T, typename F>
        bool FindModule(F &&func) const
        {
            for (const auto &m : all_modules)
            {
                if (auto base = dynamic_cast<T *>(m.get()))
                {
                    if (func(*base))
                        return true;
                }
            }
            return false;
        }

        // Get a list of all modules implementing function `F`.
        template <BasicModule::InterfaceFunction F>
        requires(F >= BasicModule::InterfaceFunction{} && F < BasicModule::InterfaceFunction::_count)
        [[nodiscard]] std::span<BasicModule *const> GetModulesImplementing() const
        {
            return lists[std::size_t(F)];
        }

        // Calls a specific function for every module.
        // The return values are ignored. If you need them, call manually using `GetModulesImplementing()`.
        template <auto F, typename ...P>
        requires std::is_member_function_pointer_v<decltype(F)>
        void Call(P &&... params) const
        {
            constexpr BasicModule::InterfaceFunction func_enum = []{
                #define DETAIL_TA_X(func_) \
                    if constexpr (meta::ValuesAreEqual<F, &BasicModule::func_>::value) \
                        return BasicModule::InterfaceFunction::func_; \
                    else
                DETAIL_TA_MODULE_FUNCS(DETAIL_TA_X)
                #undef DETAIL_TA_X
                static_assert(meta::AlwaysFalse<meta::ValueTag<F>>::value, "Bad member function pointer.");
            }();
            for (auto *m : GetModulesImplementing<func_enum>())
                (m->*F)(params...); // No forwarding because there's more than one call.
        }
    };


    namespace platform
    {
        // Whether the debugger is currently attached.
        // `false` if unknown or disabled with `CFG_TA_DETECT_DEBUGGER`
        CFG_TA_API bool IsDebuggerAttached();

        // Whether stdout (or stderr, depending on the argument) is attached to a terminal.
        CFG_TA_API bool IsTerminalAttached(bool is_stderr);
    }

    namespace output
    {
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
            bool enable_color = false;

            // The characters are written to this `std::vprintf`-style callback.
            // Defaults to `SetFileOutput(stdout)`.
            std::function<void(std::string_view fmt, CFG_TA_FMT_NAMESPACE::format_args args)> output_func;

            Terminal() : Terminal(stdout) {}

            // Sets `output_func` to print to `stream`.
            // Also guesses `enable_color` (always false when `stream` is neither `stdout` nor `stderr`).
            CFG_TA_API Terminal(FILE *stream);

            // Prints a message using `output_func`. Unlike `Print`, doesn't accept `TextStyle`s directly.
            // Prefer `Print()`.
            CFG_TA_API void PrintLow(std::string_view fmt, CFG_TA_FMT_NAMESPACE::format_args args) const;

            // Resets the text style when constructed and when destructed.
            // Can't be constructed manually, use `MakeStyleGuard()`.
            class StyleGuard
            {
                friend Terminal;

                Terminal &terminal;
                int exception_counter = 0;
                TextStyle cur_style;

                CFG_TA_API StyleGuard(Terminal &terminal);

              public:
                StyleGuard(const StyleGuard &) = delete;
                StyleGuard &operator=(const StyleGuard &) = delete;

                CFG_TA_API ~StyleGuard();

                // Pokes the terminal to reset the style, usually before and after running user code.
                CFG_TA_API void ResetStyle();
            };

            [[nodiscard]] StyleGuard MakeStyleGuard()
            {
                return StyleGuard(*this);
            }

            // --- MANUAL ANSI ESCAPE SEQUENCE API ---

            // Printing this string resets the text styles. It's always null-terminated.
            [[nodiscard]] CFG_TA_API std::string_view AnsiResetString() const;

            // Should be large enough.
            using AnsiDeltaStringBuffer = std::array<char, 100>;

            // Produces a string to switch between text styles, from `prev` to `cur`.
            // If the styles are the same, does nothing.
            //
            [[nodiscard]] CFG_TA_API AnsiDeltaStringBuffer AnsiDeltaString(const StyleGuard &&cur, const TextStyle &next) const;

            // This overload additionally performs `cur = next`.
            [[nodiscard]] AnsiDeltaStringBuffer AnsiDeltaString(StyleGuard &cur, const TextStyle &next) const
            {
                AnsiDeltaStringBuffer ret = AnsiDeltaString(std::move(cur), next);
                cur.cur_style = next;
                return ret;
            }

            // --- HIGH-LEVEL PRINTING ---

            // Prints all arguments using `output_func`. This overload doesn't support text styles.
            template <typename ...P>
            void Print(CFG_TA_FMT_NAMESPACE::format_string<P...> fmt, P &&... args) const
            {
                PrintLow(fmt.get(), CFG_TA_FMT_NAMESPACE::make_format_args(args...)); // It seems we don't need to forward `args...`.
            }

            // For internal use! Not `private` only because we need to write a formatter for it.
            // This is generated internally by `Print`, and is fed to `std::format()`.
            // When printed, it prints the delta between `cur_style` and `new_style`, then does `cur_style = new_style`.
            struct PrintableAnsiDelta
            {
                const Terminal &terminal;
                Terminal::StyleGuard &cur_style;
                TextStyle new_style;
            };

          private:
            // Replaces `TextStyle` with `PrintableAnsiDelta` for the template arguments of `std::format_string<...>`.
            template <typename T>
            using WrapStyleTypeForFormatString = std::conditional_t<std::is_same_v<std::remove_cvref_t<T>, TextStyle>, PrintableAnsiDelta, T>;

            // Replaces objects of type `TextStyle` with `PrintableAnsiDelta` for `std::format(...)`.
            template <typename T>
            static decltype(auto) WrapStyleForFormatString(const Terminal &terminal, StyleGuard &cur_style, T &&target)
            {
                if constexpr (std::is_same_v<std::remove_cvref_t<T>, TextStyle>)
                    return PrintableAnsiDelta{.terminal = terminal, .cur_style = cur_style, .new_style = target};
                else
                    return std::forward<T>(target);
            }

          public:
            // Prints all arguments using `output_func`. This overload supports text styles.
            template <typename ...P>
            void Print(StyleGuard &cur_style, CFG_TA_FMT_NAMESPACE::format_string<WrapStyleTypeForFormatString<P>...> fmt, P &&... args) const
            {
                // It seems we don't need to forward `args...`.
                PrintLow(fmt.get(), CFG_TA_FMT_NAMESPACE::make_format_args(WrapStyleForFormatString(*this, cur_style, args)...));
            }
        };

        namespace expr
        {
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
                // Numbers.
                TextStyle number = {.color = TextColor::dark_green, .bold = true};
                // User-defined literal on a number, starting with `_`. For my sanity, literals not starting with `_` are colored like the rest of the number.
                TextStyle number_suffix = {.color = TextColor::dark_green};
                // A string literal; everything between the quotes inclusive.
                TextStyle string = {.color = TextColor::dark_yellow, .bold = true};
                // Stuff before the opening `"`.
                TextStyle string_prefix = {.color = TextColor::dark_yellow};
                // Stuff after the closing `"`.
                TextStyle string_suffix = {.color = TextColor::dark_yellow};
                // A character literal.
                TextStyle character = {.color = TextColor::dark_magenta, .bold = true};
                TextStyle character_prefix = {.color = TextColor::dark_magenta};
                TextStyle character_suffix = {.color = TextColor::dark_magenta};
                // A raw string literal; everything between the parentheses exclusive.
                TextStyle raw_string = {.color = TextColor::dark_cyan, .bold = true};
                // Stuff before the opening `"`.
                TextStyle raw_string_prefix = {.color = TextColor::dark_cyan};
                // Stuff after the closing `"`.
                TextStyle raw_string_suffix = {.color = TextColor::dark_cyan};
                // Quotes, parentheses, and everything between them.
                TextStyle raw_string_delimiters = {.color = TextColor::light_blue, .bold = true};

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
        }

        // Common strings and text styles.
        struct CommonData
        {
            // Styles:

            // Error messages.
            TextStyle style_error = {.color = TextColor::light_red, .bold = true};
            // "While doing X" messages
            TextStyle style_stack_frame = {.color = TextColor::light_magenta, .bold = true};
            // "Warning" messages.
            TextStyle style_warning = {.color = TextColor::light_magenta, .bold = true};
            // "Note" messages.
            TextStyle style_note = {.color = TextColor::light_blue, .bold = true};
            // File paths.
            TextStyle style_path = {.color = TextColor::none};
            // The offending macro call.
            TextStyle style_failed_macro = {.color = TextColor::none, .bold = true};
            // Highlighted expressions.
            expr::Style style_expr;
            // The custom messages that can be optionally passed to `TA_CHECK` and `TA_MUST_THROW`.
            TextStyle style_user_message = {.color = TextColor::none, .bold = true};

            // Characters:

            std::string warning_prefix = "WARNING: ";
            std::string note_prefix = "NOTE: ";

            // When printing a path, separates it from the line number.
            std::string filename_linenumber_separator;
            // When printing a path with a line number, this comes after the line number.
            std::string filename_linenumber_suffix;

            // A comma with optional spacing around it.
            std::string spaced_comma = ", ";

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

            // Other:

            // When we print a macro call, it's indented by this many spaces.
            std::size_t code_indentation = 4;

            // Whether to pad the argument of `TA_CHECK()` and other macros with a space on each side.
            // We can't check if the user actually had spaces, so we add them ourselves.
            // They look nice anyway.
            bool spaces_in_macro_call_parentheses = true;
            // Same, but for the regular non-macro functions.
            bool spaces_in_func_call_parentheses = false;

            CommonData()
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
                    filename_linenumber_suffix = ")";
                }
                else
                {
                    filename_linenumber_separator = ":";
                    filename_linenumber_suffix = "";
                }
            }

            // Converts a source location to a string in the current preferred format.
            [[nodiscard]] std::string LocationToString(const BasicModule::SourceLoc &loc) const
            {
                return CFG_TA_FMT_NAMESPACE::format("{}{}{}{}", loc.file, filename_linenumber_separator, loc.line, filename_linenumber_suffix);
            }
        };

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

            const CommonData *data = nullptr;

          public:
            TextCanvas(const CommonData *data) : data(data) {}

            [[nodiscard]] const CommonData *GetCommonData() const {return data;}

            // Prints to a `terminal` stream.
            CFG_TA_API void Print(const Terminal &terminal, Terminal::StyleGuard &cur_style) const;

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

            // Draws a string.
            // Wanted to call this `DrawText`, but that conflicts with a WinAPI macro! >:o
            // Returns `text.size()`.
            CFG_TA_API std::size_t DrawString(std::size_t line, std::size_t start, std::u32string_view text, const CellInfo &info = {.style = {}, .important = true});
            // Draws a UTF8 text. Returns the text size after converting to UTF32.
            CFG_TA_API std::size_t DrawString(std::size_t line, std::size_t start, std::string_view text, const CellInfo &info = {.style = {}, .important = true});

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

        namespace expr
        {
            // Pretty-prints an expression with syntax highlighting. Returns `expr.size()`.
            // If `style` is not specified, uses the one from the canvas.
            CFG_TA_API std::size_t DrawToCanvas(TextCanvas &canvas, std::size_t line, std::size_t start, std::string_view expr, const Style *style = nullptr);
        }

        // Uses the current modules to print the context stack. See `namespace context` above.
        // If `skip_last_frame` is specified and is the last frame, that frame is not printed.
        CFG_TA_API void PrintContext(Terminal::StyleGuard &cur_style, const context::BasicFrame *skip_last_frame = nullptr, context::Context con = context::CurrentContext());
        // Same, but only prints a single context frame.
        CFG_TA_API void PrintContextFrame(Terminal::StyleGuard &cur_style, const context::BasicFrame &frame);

        // Prints the current log, using the current modules.
        // Returns true if at least one module has printed something.
        CFG_TA_API void PrintLog(Terminal::StyleGuard &cur_style);
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

        output::Terminal terminal;
        output::CommonData common_data;

        virtual void EnableUnicode(bool enable)
        {
            common_data.EnableUnicode(enable);
        }

        // This is called whenever the context information needs to be printed.
        // Return true if this type of context frame is known to you and you handled it, then the other modules won't receive this call.
        // Do nothing and return false if you don't know this context frame type.
        virtual bool PrintContextFrame(output::Terminal::StyleGuard &cur_style, const context::BasicFrame &frame) {(void)cur_style; (void)frame; return false;}
        // This is called to print the log.
        // Return true to prevent other modules from receiving this call.
        // `unscoped_log` can alternatively be obtained from `BasicModule::RunSingleTestResults`.
        // `scoped_log` can alternatively be obtained from `context::CurrentScopedLog()`.
        virtual bool PrintLogEntries(output::Terminal::StyleGuard &cur_style, std::span<const context::LogEntry> unscoped_log, std::span<const context::LogEntry *const> scoped_log) {(void)cur_style; (void)unscoped_log; (void)scoped_log; return false;}

      protected:
        CFG_TA_API void PrintWarning(output::Terminal::StyleGuard &cur_style, std::string_view text) const;
        CFG_TA_API void PrintNote(output::Terminal::StyleGuard &cur_style, std::string_view text) const;
    };


    // --- STACK TRACE HELPERS ---

    // Don't use this, use `Trace<...>` defined below.
    class BasicTrace : public context::BasicFrame, public context::FrameGuard
    {
        bool enabled = false;
        bool accept_args = false;
        BasicModule::SourceLoc loc;
        std::string_view func;
        std::vector<std::string> func_args;
        std::vector<std::string> template_args;

      protected:
        BasicTrace() : FrameGuard(nullptr) {}

        BasicTrace(
            bool enabled, bool accept_args,
            std::string_view file, int line, std::string_view func,
            std::vector<std::string> func_args, std::vector<std::string> template_args)
            : FrameGuard(enabled ? std::shared_ptr<const BasicFrame>(std::shared_ptr<void>{}, this) : nullptr), // A non-owning shared pointer.
            enabled(enabled), accept_args(accept_args), loc{file, line}, func(func),
            func_args(std::move(func_args)), template_args(std::move(template_args))
        {}

        ~BasicTrace() = default;

      public:
        BasicTrace &operator=(const BasicTrace &) = delete;
        BasicTrace &operator=(BasicTrace &&) = delete;

        // Whether we have a location information here.
        // If you call the inherited `Reset()`, this will still return true, even though the object will not be in the stack.
        [[nodiscard]] explicit operator bool() const {return enabled;}

        [[nodiscard]] const BasicModule::SourceLoc &GetLocation() const {return loc;}
        [[nodiscard]] const std::vector<std::string>  &GetFuncArgs() const  & {return func_args;}
        [[nodiscard]]       std::vector<std::string> &&GetFuncArgs()       && {return std::move(func_args);}
        [[nodiscard]] const std::vector<std::string>  &GetTemplateArgs() const  & {return template_args;}
        [[nodiscard]]       std::vector<std::string> &&GetTemplateArgs()       && {return std::move(template_args);}
        [[nodiscard]] std::string_view GetFuncName() const {return func;}

        template <typename ...P>
        BasicTrace &AddArgs(const P &... args)
        {
            if (accept_args)
                (void(func_args.push_back(string_conv::ToString(args))), ...);
            return *this;
        }

        template <typename ...P>
        BasicTrace &AddTemplateTypes()
        {
            if (accept_args)
                (void(template_args.push_back(text::TypeName<P>())), ...);
            return *this;
        }
        template <typename ...P>
        BasicTrace &AddTemplateValues(const P &... args)
        {
            if (accept_args)
                (void(template_args.push_back(string_conv::ToString(args))), ...);
            return *this;
        }
    };
    // A tag for a `Trace` constructor, constructs a null object that doesn't trace.
    struct NoTrace {};
    // Add this as the last function parameter: `Trace<"MyFunc"> trace = {}` to show this function call as the context when a test fails.
    // You can optionally do `trace.AddArgs/AddTemplateTypes/AddTemplateValues()` to also display information about the function arguments.
    // Those functions are not lazy, and calculate the new strings immediately, so don't overuse them.
    // You can construct from `NoTrace{}` instead of `{}` to disable tracing for that specific object.
    // You can call the inherited `Reset()` to disarm if a `Trace` to remove it from the stack.
    // You can copy and move `Trace` to propagate the location and function name (moving also `Reset()`s the original).
    // Copying/moving constructs a live instance even if the original was `Reset()`, but not if it was constructed from `NoTrace()`.
    template <meta::ConstString FuncName>
    class Trace : public BasicTrace
    {
      public:
        // Default constructor - traces normally.
        // Don't pass any parameters manually.
        Trace(std::string_view file = __builtin_FILE(), int line = __builtin_LINE(), std::string_view func = FuncName.view())
            : BasicTrace(true, true, file, line, func, {}, {})
        {}
        Trace(NoTrace) {}

        Trace(const Trace &other) : Trace(static_cast<const BasicTrace &>(other)) {}
        Trace(const BasicTrace &other)
            : BasicTrace(bool(other), false, other.GetLocation().file, other.GetLocation().line, other.GetFuncName(), other.GetFuncArgs(), other.GetTemplateArgs())
        {}
        Trace(Trace &&other) : Trace(static_cast<BasicTrace &&>(other)) {}
        Trace(BasicTrace &&other)
            // Calling `Reset()` in this manner happens in an unspecified order relative to the other arguments,
            // but it doesn't matter as long as its not UB, as it doesn't affect the values of the arguments.
            : BasicTrace((other.Reset(), bool(other)), false, other.GetLocation().file, other.GetLocation().line, other.GetFuncName(), std::move(other).GetFuncArgs(), std::move(other).GetTemplateArgs())
        {}
    };


    // Macro internals, except `TA_MUST_THROW(...)`.
    namespace detail
    {
        // --- ASSERTIONS ---

        // `TA_ARG` ultimately expands to this.
        // Stores a pointer to a `StoredArg` in an `AssertWrapper` where it will write the argument as a string.
        struct ArgWrapper
        {
            BasicModule::BasicAssertionInfo *assertion = nullptr;
            BasicModule::BasicAssertionExpr::StoredArg *target = nullptr;

            // Raises a hard error if the assertion owning this argument isn't currently running in this thread.
            CFG_TA_API void EnsureAssertionIsRunning();

            ArgWrapper(BasicModule::BasicAssertionInfo &assertion, BasicModule::BasicAssertionExpr::StoredArg &target)
                : assertion(&assertion), target(&target)
            {
                EnsureAssertionIsRunning();
                target.state = BasicModule::BasicAssertionExpr::StoredArg::State::in_progress;
            }
            ArgWrapper(const ArgWrapper &) = default;
            ArgWrapper &operator=(const ArgWrapper &) = default;

            // The method name is wonky to assist with our parsing.
            template <typename T>
            T &&_ta_handle_arg_(int counter, T &&arg) &&
            {
                (void)counter; // Unused, but passing it helps with parsing.
                EnsureAssertionIsRunning();
                target->value = string_conv::ToString(arg);
                target->state = BasicModule::BasicAssertionExpr::StoredArg::State::done;
                return std::forward<T>(arg);
            }
        };

        // An intermediate base class that `AssertWrapper<T>` inherits from.
        // You can also inherit custom assertion classes from this, if they don't need the expression decomposition provided by `AssertWrapper<T>`.
        class BasicAssertWrapper : public BasicModule::BasicAssertionInfo
        {
            bool condition_value = false;

            // User condition was evaluated to completion.
            bool condition_value_known = false;

            // This is called to evaluate the user condition.
            void (*condition_func)(BasicAssertWrapper &self, const void *data) = nullptr;
            const void *condition_data = nullptr;

            // Call to trigger a breakpoint at the macro call site.
            void (*break_func)() = nullptr;

            // This is called to get the optional user message (null if no message).
            std::string (*message_func)(const void *data) = nullptr;
            const void *message_data = nullptr;
            // The user message is cached here.
            mutable std::optional<std::string> message_cache;

            // Pushes and pops this into the assertion stack.
            struct AssertionStackGuard
            {
                BasicAssertWrapper &self;

                AssertionStackGuard(BasicAssertWrapper &self)
                    : self(self)
                {
                    GlobalThreadState &thread_state = ThreadState();
                    if (!thread_state.current_test)
                        HardError("This thread doesn't have a test currently running, yet it tries to use an assertion.");

                    auto &cur = thread_state.current_assertion;
                    self.enclosing_assertion = cur;
                    cur = &self;
                }

                AssertionStackGuard(const AssertionStackGuard &) = delete;
                AssertionStackGuard &operator=(const AssertionStackGuard &) = delete;

                ~AssertionStackGuard()
                {
                    // We don't check `finished` here. It can be false when a nested assertion fails.

                    GlobalThreadState &thread_state = ThreadState();
                    if (thread_state.current_assertion != &self)
                        HardError("Something is wrong. Are we in a coroutine that was transfered to a different thread in the middle on an assertion?");

                    thread_state.current_assertion = const_cast<BasicModule::BasicAssertionInfo *>(self.enclosing_assertion);
                }
            };

            // This is invoked when the assertion finishes evaluating.
            struct Evaluator
            {
                BasicAssertWrapper &self;
                CFG_TA_API void operator~();
            };

          public:
            // Note the weird variable name, it helps with our macro syntax that adds optional messages.
            Evaluator DETAIL_TA_ADD_MESSAGE{*this};

            template <typename F>
            BasicAssertWrapper(const F &func, void (*break_func)())
                : break_func(break_func)
            {
                condition_func = [](BasicAssertWrapper &self, const void *data)
                {
                    return (*static_cast<const F *>(data))(self);
                };
                condition_data = &func;
            }

            BasicAssertWrapper(const BasicAssertWrapper &) = delete;
            BasicAssertWrapper &operator=(const BasicAssertWrapper &) = delete;

            template <typename T>
            void EvalCond(T &&value)
            {
                // Using `? :` to force a contextual bool conversion.
                condition_value = std::forward<T>(value) ? true : false;
                condition_value_known = true;
            }

            template <typename F>
            Evaluator &AddMessage(const F &func)
            {
                if (!condition_value)
                {
                    message_func = [](const void *data)
                    {
                        std::string ret;
                        (*static_cast<const F *>(data))([&]<typename ...P>(CFG_TA_FMT_NAMESPACE::format_string<P...> format, P &&... args)
                        {
                            ret = CFG_TA_FMT_NAMESPACE::format(std::move(format), std::forward<P>(args)...);
                        });
                        return ret;
                    };
                    message_data = &func;
                }
                return DETAIL_TA_ADD_MESSAGE;
            }

            CFG_TA_API std::optional<std::string_view> GetUserMessage() const override;

            virtual ArgWrapper BeginArg(int counter) = 0;
        };

        template <meta::ConstString MacroName, meta::ConstString RawString, meta::ConstString ExpandedString, meta::ConstString FileName, int LineNumber>
        struct AssertWrapper : BasicAssertWrapper, BasicModule::BasicAssertionExpr
        {
            using BasicAssertWrapper::BasicAssertWrapper;

            // The number of arguments.
            static constexpr std::size_t num_args = []{
                std::size_t ret = 0;
                text::expr::ParseExpr(RawString.view(), nullptr, [&](std::string_view name, std::string_view args, std::size_t depth)
                {
                    (void)args;
                    (void)depth;
                    if (text::chars::IsArgMacroName(name))
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
                            if (text::chars::IsDigit(ch))
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

                        if (!text::chars::IsArgMacroName(name))
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
                        while (!trimmed_args.empty() && text::chars::IsWhitespace(trimmed_args.front()))
                            trimmed_args.remove_prefix(1);
                        while (!trimmed_args.empty() && text::chars::IsWhitespace(trimmed_args.back()))
                            trimmed_args.remove_suffix(1);

                        // Decide if we should draw a bracket for this argument.
                        for (char ch : trimmed_args)
                        {
                            // Whatever the condition is, it should trigger for all arguments with nested arguments.
                            if (!text::chars::IsIdentifierChar(ch))
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

            std::string_view Expr() const override {return RawString.view();}
            std::span<const ArgInfo> ArgsInfo() const override {return arg_data.info;}
            std::span<const std::size_t> ArgsInDrawOrder() const override {return arg_data.args_in_draw_order;}
            std::span<const StoredArg> StoredArgs() const override {return stored_args;}

            const BasicModule::SourceLoc &SourceLocation() const override {return location;}
            DecoVar GetElement(int index) const override
            {
                if constexpr (RawString.view().empty())
                {
                    // `TA_FAIL` uses this.
                    return std::monostate{};
                }
                else
                {
                    static constexpr meta::ConstString name_with_paren = MacroName + "(";
                    if (index == 0)
                        return DecoFixedString{.string = name_with_paren.view()};
                    else if (index == 1)
                        return DecoExprWithArgs{.expr = this};
                    else if (index == 2)
                        return DecoFixedString{.string = ")"};
                    else
                        return std::monostate{};
                }
            }

            [[nodiscard]] ArgWrapper BeginArg(int counter) override
            {
                auto it = std::partition_point(arg_data.counter_to_arg_index.begin(), arg_data.counter_to_arg_index.end(),
                    [&](const CounterIndexPair &pair){return pair.counter < counter;}
                );
                if (it == arg_data.counter_to_arg_index.end() || it->counter != counter)
                    HardError("`TA_CHECK` isn't aware of this `$(...)`.");

                return {*this, stored_args[it->index]};
            }
        };

        // --- TESTS ---

        struct BasicTest : BasicModule::BasicTestInfo
        {
            virtual void Run() const = 0;

            // Magically trigger a breakpoint at the test declaration.
            virtual void Breakpoint() const = 0;
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
        // `B` is a lambda that triggers a breakpoint in the test location itself when called.
        template <auto P, auto B, meta::ConstString TestName, meta::ConstString LocFile, int LocLine>
        struct SpecificTest : BasicTest
        {
            static constexpr bool test_name_is_valid = []{
                if (TestName.view().empty())
                    return false;
                if (!std::all_of(TestName.view().begin(), TestName.view().end(), [](char ch){return text::chars::IsIdentifierCharStrict(ch) || ch == '/';}))
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

            void Breakpoint() const override
            {
                B();
            }
        };

        // Touch to register a test. `T` is `SpecificTest<??>`.
        template <typename T>
        inline const auto register_test_helper = []{RegisterTest(&test_singleton<T>); return nullptr;}();

        // --- LOGS ---

        [[nodiscard]] CFG_TA_API std::size_t GenerateLogId();
        CFG_TA_API void AddLogEntry(std::string &&message);

        class BasicScopedLogGuard
        {
            context::LogEntry *entry = nullptr;

          protected:
            CFG_TA_API BasicScopedLogGuard(context::LogEntry *entry);

          public:
            BasicScopedLogGuard(const BasicScopedLogGuard &) = delete;
            BasicScopedLogGuard &operator=(const BasicScopedLogGuard &) = delete;

            CFG_TA_API ~BasicScopedLogGuard();
        };

        class ScopedLogGuard : BasicScopedLogGuard
        {
            context::LogEntry entry;

          public:
            ScopedLogGuard(std::string &&message)
                : BasicScopedLogGuard(&entry), entry(GenerateLogId(), std::move(message))
            {}
        };

        template <typename F>
        class ScopedLogGuardLazy : BasicScopedLogGuard
        {
            F func;
            context::LogEntry entry;

          public:
            ScopedLogGuardLazy(F &&func)
                : BasicScopedLogGuard(&entry), func(std::move(func)), entry(GenerateLogId(), this->func)
            {}
        };

        // --- GENERATORS ---

        template <meta::ConstString Name, meta::ConstString LocFile, int LocLine, int LocCounter, typename ReturnType, typename F>
        struct SpecificGenerator : BasicModule::BasicTypedGenerator<ReturnType>
        {
            using BasicModule::BasicGenerator::overriding_module;

            F func;

            template <typename G>
            SpecificGenerator(G &&make_func) : func(std::forward<G>(make_func)()) {}

            static constexpr BasicModule::SourceLocWithCounter location = {
                BasicModule::SourceLoc{
                    .file = LocFile.view(),
                    .line = LocLine,
                },
                LocCounter,
            };

            const BasicModule::SourceLocWithCounter &GetLocation() const override
            {
                return location;
            }

            std::string_view GetName() const override
            {
                return Name.view();
            }

            void Generate() override
            {
                // We could try to somehow conditionally assign here if possible.
                // I tried, got some weird build error in some edge case, and decided not to bother.
                this->storage.template emplace<1>(func, this->repeat);

                this->this_value_is_custom = false;
                this->num_generated_values++;
            }
        };

        // Using a concept instead of a `static_assert`, because I can't find where to put the `static_assert` to make Clangd report on it.
        template <meta::ConstString Name>
        concept IsValidGeneratorName =
            !Name.view().empty() && // Not empty.
            !text::chars::IsDigit(Name.view().front()) && // Doesn't start with a digit.
            std::all_of(Name.view().begin(), Name.view().end(), text::chars::IsIdentifierCharStrict); // Only valid characters.

        template <
            // Manually specified:
            meta::ConstString Name, meta::ConstString LocFile, int LocLine, int LocCounter,
            // Deduced:
            typename F,
            // Computed:
            typename UserFuncType = decltype(std::declval<F &&>()()),
            typename ReturnType = decltype(std::declval<UserFuncType &>()(std::declval<bool &>()))
        >
        requires IsValidGeneratorName<Name>
        [[nodiscard]] const ReturnType &Generate(F &&func)
        {
            auto &thread_state = ThreadState();
            if (!thread_state.current_test)
                HardError("Can't use `TA_GENERATE(...)` when no test is running.", HardErrorKind::user);

            using GeneratorType = SpecificGenerator<Name, LocFile, LocLine, LocCounter, ReturnType, UserFuncType>;

            GeneratorType *this_generator = nullptr;

            bool creating_new_generator = false;

            if (thread_state.current_test->generator_index < thread_state.current_test->generator_stack.size())
            {
                // Revisiting a generator.
                auto *this_untyped_generator = thread_state.current_test->generator_stack[thread_state.current_test->generator_index].get();

                this_generator = const_cast<GeneratorType *>(dynamic_cast<const GeneratorType *>(this_untyped_generator));

                // Make sure this is the right generator.
                // Since the location is a part of the type, this nicely checks for the location equality.
                // This is one of the two determinism checks, the second one is in runner's `Run()` to make sure we visited all generators.
                if (!this_generator)
                {
                    // Theoretically we can have two different generators at the same line, but I think this message is ok even in that case.
                    HardError(CFG_TA_FMT_NAMESPACE::format(
                        "Invalid non-deterministic use of generators. "
                        "Was expecting to reach the generator at `" DETAIL_TA_INTERNAL_ERROR_LOCATION_FORMAT "`, "
                        "but instead reached a different one at `" DETAIL_TA_INTERNAL_ERROR_LOCATION_FORMAT "`.",
                        this_untyped_generator->GetLocation().file, this_untyped_generator->GetLocation().line,
                        LocFile.view(), LocLine
                    ), HardErrorKind::user);
                }
            }
            else
            {
                // Visiting a generator for the first time.

                creating_new_generator = true;

                if (thread_state.current_test->generator_index != thread_state.current_test->generator_stack.size())
                    HardError("Something is wrong with the generator index."); // This should never happen.

                auto new_generator = std::make_unique<GeneratorType>(std::forward<F>(func));
                this_generator = new_generator.get();

                // Possibly accept an override.
                for (const auto &m : thread_state.current_test->all_tests->modules->GetModulesImplementing<BasicModule::InterfaceFunction::OnRegisterGeneratorOverride>())
                {
                    if (m->OnRegisterGeneratorOverride(*thread_state.current_test, *this_generator))
                    {
                        this_generator->overriding_module = m;
                        break;
                    }
                }

                thread_state.current_test->generator_stack.push_back(std::move(new_generator));
            }

            const bool next_value = thread_state.current_test->generator_index + 1 == thread_state.current_test->generator_stack.size();

            // Advance the generator if needed.
            if (next_value && (!this_generator->overriding_module || creating_new_generator))
            {
                switch (this_generator->RunGeneratorOverride())
                {
                  case BasicModule::BasicGenerator::OverrideStatus::no_override:
                    this_generator->Generate();
                    break;
                  case BasicModule::BasicGenerator::OverrideStatus::success:
                    // Nothing.
                    break;
                  case BasicModule::BasicGenerator::OverrideStatus::no_more_values:
                    HardError(
                        CFG_TA_FMT_NAMESPACE::format(
                            "Generator `{}` was overriden to generate no values. This is not supported, you must avoid reaching the generator in the first place.",
                            this_generator->GetName()
                        ),
                        HardErrorKind::user
                    );
                    break;
                }
            }

            // Post callback.
            BasicModule::GeneratorCallInfo callback_data{
                .test = thread_state.current_test,
                .generator = this_generator,
                .generating_new_value = next_value,
            };
            thread_state.current_test->all_tests->modules->Call<&BasicModule::OnPostGenerate>(callback_data);

            thread_state.current_test->generator_index++;

            return this_generator->GetValue();
        }
    }

    enum class GeneratorFlags
    {
        // Don't emit a hard error if the range is empty, instead throw `InterruptTestException` to abort the test.
        // Has no effect when exceptions are disabled.
        allow_empty_range = 1 << 0,
    };
    DETAIL_TA_FLAG_OPERATORS(GeneratorFlags)
    using enum GeneratorFlags;

    // Converts a C++20 range to a functor usable with `TA_GENERATE_FUNC(...)`.
    // `TA_GENERATE(...)` calls it internally. But you might want to call it manually,
    // because `TA_GENERATE(...)` prevents you from using any local variables, for safety.
    template <std::ranges::input_range T>
    [[nodiscard]] auto RangeToGeneratorFunc(T &&range, GeneratorFlags flags = {})
    {
        class Functor
        {
            std::remove_cvref_t<T> range;
            std::ranges::iterator_t<std::remove_cvref_t<T>> iter;

          public:
            explicit Functor(T &&range, GeneratorFlags flags)
                : range(std::forward<T>(range)), iter(this->range.begin())
            {
                if (iter == this->range.end())
                {
                    if (bool(flags & GeneratorFlags::allow_empty_range))
                        throw InterruptTestException{};
                    else
                        HardError("Empty generator range.", HardErrorKind::user);
                }
            }

            // `iter` would go stale on copy.
            Functor(const Functor &) = delete;
            Functor &operator=(const Functor &) = delete;

            decltype(auto) operator()(bool &repeat)
            {
                decltype(auto) ret = *iter;
                repeat = ++iter != range.end();
                if constexpr (std::is_reference_v<decltype(ret)>)
                    return decltype(ret)(ret);
                else
                    return ret;
            }
        };
        return Functor(std::forward<T>(range), flags);
    }
    // Those overload accept `{...}` initializer lists. They also conveniently add support for C arrays.
    // Note the lvalue overload being non-const, this way it can accept both const and non-const arrays, same as `std::to_array`.
    template <typename T, std::size_t N> requires(N > 0)
    [[nodiscard]] auto RangeToGeneratorFunc(T (&range)[N], GeneratorFlags flags = {})
    {
        return (RangeToGeneratorFunc)(std::to_array(range), flags);
    }
    template <typename T, std::size_t N> requires(N > 0)
    [[nodiscard]] auto RangeToGeneratorFunc(T (&&range)[N], GeneratorFlags flags = {})
    {
        return (RangeToGeneratorFunc)(std::to_array(std::move(range)), flags);
    }


    // --- ANALYZING EXCEPTIONS ---

    // Designates or more components of `CaughtException`.
    enum class ExceptionElem
    {
        // The exception itself, not something nested in it.
        // This should be zero, to keep it the default.
        top_level = 0,
        // The most-nested exception.
        most_nested,
        // The exception itself and all nested exceptions.
        all,
    };
    using enum ExceptionElem;
    // Either an index of an exception element in `CaughtException`, or a enum designating one or more elements.
    using ExceptionElemVar = std::variant<ExceptionElem, int>;

    template <>
    struct string_conv::DefaultToStringTraits<ExceptionElem>
    {
        CFG_TA_API std::string operator()(const ExceptionElem &value) const;
    };
    template <>
    struct string_conv::DefaultToStringTraits<ExceptionElemVar>
    {
        CFG_TA_API std::string operator()(const ExceptionElemVar &value) const;
    };

    // Internals of `CaughtException`.
    namespace detail
    {
        CFG_TA_API const std::vector<ta_test::SingleException> &GetEmptyExceptionListSingleton();

        // A CRTP base.
        // `Ref` is a self reference that we return from chained functions, normally either `const Derived &` or `Derived &&`.
        // If `IsWrapper` is false, `Derived` must have a `??shared_ptr<BasicModule::CaughtExceptionInfo>?? _get_state() const` private member (with this class as a friend).
        // We'll examine the exception details pointed by that member. You can return any kind of a shared pointer or a reference to one.
        // Of `IsWrapepr` is true, `Derived` must have a `const ?? &_get_state() const` private member that returns an object also inherited from this base,
        // to which we'll forward all calls.
        template <typename Derived, typename Ref, bool IsWrapper>
        class BasicCaughtExceptionInterface
        {
            // If `IsWrapper == true`, returns the object we're wrapping that must inherit from this template too. Can return by value.
            // If `IsWrapper == false`, returns a `const BasicModule::CaughtExceptionInfo *`.
            // All of our members are `const` anyway, so we only have a const overload here.s
            [[nodiscard]] decltype(auto) State() const
            {
                return static_cast<const Derived &>(*this)._get_state();
            }

            // This is what you should return from functions returning `Ref`.
            // It's either a reference to `*this` or to `state` (coming from `State()`), depending on the type of `Ref`.
            [[nodiscard]] Ref ReturnedRef(auto &&state) const
            {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(State())>, std::remove_cvref_t<Ref>>)
                    return std::move(state);
                else
                    return std::move(const_cast<Derived &>(static_cast<const Derived &>(*this)));
            }

          protected:
            // This is initially `protected` because it makes no sense to call it directly on rvalues.
            // Derived classes can make it public.

            // When you're manually examining this exception with `TA_CHECK(...)`, create this object beforehand.
            // While it exists, all failed assertions will mention that they happened while examnining this exception.
            // All high-level functions below do this automatically, and redundant contexts are silently ignored.
            [[nodiscard]] context::FrameGuard MakeContextGuard() const
            {
                if constexpr (IsWrapper)
                    return State().FrameGuard();
                else
                {
                    // This nicely handles null state.
                    return context::FrameGuard(State());
                }
            }

          public:
            // Returns all stored nested exceptions, in case you want to examine them manually.
            // Prefer the high-level functions below.
            [[nodiscard]] const std::vector<SingleException> &GetElems() const
            {
                if constexpr (IsWrapper)
                    return State().GetElems();
                else
                {
                    if (State())
                        return State()->elems;
                    else
                        return GetEmptyExceptionListSingleton(); // This is a little stupid, but probably better than a `HardError()`?
                }
            }

            // Checks that the exception message matches the regex.
            // The entire message must match, not just a part of it.
            Ref CheckMessage(/* elem = top_level, */ std::string_view regex, Trace<"CheckMessage"> trace = {}) const
            {
                // No need to wrap this.
                trace.AddArgs(regex);
                return CheckMessage(ExceptionElem::top_level, regex, std::move(trace));
            }
            // Checks that the exception message matches the regex.
            // The entire message must match, not just a part of it.
            Ref CheckMessage(ExceptionElemVar elem, std::string_view regex, Trace<"CheckMessage"> trace = {}) const
            {
                if constexpr (IsWrapper)
                {
                    trace.Reset();
                    decltype(auto) state = State();
                    state.CheckMessage(elem, regex, std::move(trace));
                    return ReturnedRef(state);
                }
                else
                {
                    trace.AddArgs(elem, regex);
                    std::regex r(regex.begin(), regex.end());
                    [[maybe_unused]] auto context = MakeContextGuard();
                    ForElem(elem, [&](const SingleException &elem)
                    {
                        TA_CHECK( std::regex_match(elem.message, r) )("Expected the exception message to match regex `{}`, but got `{}`.", regex, elem.message);
                        return false;
                    });
                    return static_cast<Ref>(*this);
                }
            }

            // Checks that the exception type is exactly `T`.
            template <typename T>
            Ref CheckExactType(ExceptionElemVar elem = ExceptionElem::top_level, Trace<"CheckExactType"> trace = {}) const
            {
                if constexpr (IsWrapper)
                {
                    trace.Reset();
                    decltype(auto) state = State();
                    state.template CheckExactType<T>(elem, std::move(trace));
                    return ReturnedRef(state);
                }
                else
                {
                    trace.AddTemplateTypes<T>().AddArgs(elem);
                    [[maybe_unused]] auto context = MakeContextGuard();
                    ForElem(elem, [&](const SingleException &elem)
                    {
                        TA_CHECK( $(elem.type) == $(typeid(T)) )("Expected the exception type to be exactly `{}`, but got `{}`.", text::TypeName<T>(), string_conv::ToString(elem.type));
                        return false;
                    });
                    return static_cast<Ref>(*this);
                }
            }

            // Checks that the exception type derives from `T`.
            template <typename T>
            requires std::is_class_v<T>
            Ref CheckDerivedType(ExceptionElemVar elem = ExceptionElem::top_level, Trace<"CheckDerivedType"> trace = {}) const
            {
                if constexpr (IsWrapper)
                {
                    trace.Reset();
                    decltype(auto) state = State();
                    state.template CheckDerivedType<T>(elem, std::move(trace));
                    return ReturnedRef(state);
                }
                else
                {
                    trace.AddTemplateTypes<T>().AddArgs(elem);
                    [[maybe_unused]] auto context = MakeContextGuard();
                    ForElem(elem, [&](const SingleException &elem)
                    {
                        try
                        {
                            std::rethrow_exception(elem.exception);
                        }
                        catch (const T &) {}
                        catch (...)
                        {
                            TA_FAIL("Expected the exception type to inherit from `{}`, but got `{}`.", text::TypeName<T>(), elem.GetTypeName());
                        }

                        return false;
                    });
                    return static_cast<Ref>(*this);
                }
            }

            // Calls `func` for one or more elements, depending on `kind`.
            // `func` is `(const SingleException &elem) -> bool`. If it returns true, the whole function stops and also returns true.
            template <typename F>
            bool ForElem(ExceptionElemVar elem, F &&func) const
            {
                if constexpr (IsWrapper)
                    return State().ForElem(elem, std::forward<F>(func));
                else
                {
                    if (elem.valueless_by_exception())
                        HardError("Invalid `ExceptionElemVar` variant.");
                    if (!State() || State()->elems.empty())
                        return false; // Should be good enough. This shouldn't normally happen.
                    return std::visit(meta::Overload{
                        [&](ExceptionElem elem)
                        {
                            switch (elem)
                            {
                              case ExceptionElem::top_level:
                                return std::forward<F>(func)(State()->elems.front());
                              case ExceptionElem::most_nested:
                                return std::forward<F>(func)(State()->elems.back());
                              case ExceptionElem::all:
                                for (const SingleException &elem : State()->elems)
                                {
                                    if (func(elem))
                                        return true;
                                }
                                return false;
                            }
                            HardError("Invalid `ExceptionElem` enum.", HardErrorKind::user);
                        },
                        [&](int index)
                        {
                            if (index < 0 || std::size_t(index) >= State()->elems.size())
                            {
                                TA_FAIL("Exception element index {} is out of range, have {} elements.", index, State()->elems.size());
                                return false;
                            }
                            return func(State()->elems[std::size_t(index)]);
                        },
                    }, elem);
                }
            }
        };
    }

    // This is what `TA_MUST_THROW(...)` returns.
    // Stores a list of nested `SingleException`s, plus the information about the macro call that produced it.
    class CaughtException
        // Most user-facing functions are in this base, because reasons.
        // You can look at the comment on `ta_test::detail::MustThrowWrapper::Evaluator` for why we do this, if you're interested.
        : public detail::BasicCaughtExceptionInterface<CaughtException, const CaughtException &, false>
    {
        // This is a `shared_ptr` to allow `MakeContextGuard()` to outlive this object without causing UB.
        std::shared_ptr<BasicModule::CaughtExceptionInfo> state;

        // For the CRTP base.
        friend detail::BasicCaughtExceptionInterface<CaughtException, const CaughtException &, false>;
        const decltype(state) &_get_state() const
        {
            return state;
        }

      public:
        CaughtException() {}

        // This is primarily for internal use.
        CFG_TA_API explicit CaughtException(
            const BasicModule::MustThrowStaticInfo *static_info,
            std::weak_ptr<const BasicModule::MustThrowDynamicInfo> dynamic_info,
            const std::exception_ptr &e
        );

        // Returns false for default-constructed or moved-from instances.
        // This is not in the base class because it's impossible to call on any other class derived from it,
        //   unless you use the functional notation, which isn't a use case.
        [[nodiscard]] explicit operator bool() const {return bool(state);}
    };


    // Internals of `TA_MUST_THROW(...)`.
    namespace detail
    {
        // `TA_MUST_THROW(...)` expands to this.
        class MustThrowWrapper
        {
            // `final` removes the Clang warning about a non-virtual destructor.
            struct Info final : BasicModule::MustThrowDynamicInfo
            {
                MustThrowWrapper &self;

                BasicModule::MustThrowInfo info;

                Info(MustThrowWrapper &self, const BasicModule::MustThrowStaticInfo *static_info)
                    : self(self)
                {
                    info.static_info = static_info;
                    info.dynamic_info = this;
                }

                CFG_TA_API std::optional<std::string_view> GetUserMessage() const;
            };
            // This is a `shared_ptr` to allow us to detect when the object goes out of scope.
            // After that we refuse to evaluate the user message.
            std::shared_ptr<Info> info;

            // This is called to hopefully throw the user exception.
            void (*body_func)(const void *data) = nullptr;
            const void *body_data = nullptr;

            // Call to trigger a breakpoint at the macro call site.
            void (*break_func)() = nullptr;

            // This is called to get the optional user message (null if no message).
            std::string (*message_func)(const void *data) = nullptr;
            const void *message_data = nullptr;
            // The user message is cached here.
            mutable std::optional<std::string> message_cache;

            template <typename F>
            MustThrowWrapper(const F &func, void (*break_func)(), const BasicModule::MustThrowStaticInfo *static_info)
                : info(std::make_shared<Info>(*this, static_info)),
                body_func([](const void *data) {(*static_cast<const F *>(data))();}),
                body_data(&func),
                break_func(break_func)
            {}

            class Evaluator;

            // See comment on `class Evaluator` for what this is.
            class TemporaryCaughtException
                : public detail::BasicCaughtExceptionInterface<TemporaryCaughtException, TemporaryCaughtException &&, true>
            {
                CaughtException underlying;

                friend class Evaluator;
                TemporaryCaughtException(CaughtException &&underlying) : underlying(std::move(underlying)) {}

                // For the CRTP baes.
                friend detail::BasicCaughtExceptionInterface<TemporaryCaughtException, TemporaryCaughtException &&, true>;
                const CaughtException &_get_state() const
                {
                    return underlying;
                }

              public:
                CaughtException operator~() &&
                {
                    return std::move(underlying);
                }
            };

            // There's a lot of really moist code here.
            // * We want to return `CaughtException` from `TA_MUST_THROW(...)`, but we can't just do that, because we also must support optional messages,
            //   and there's no way to make them work if we immediately return the right type.
            // * Therefore, `TA_MUST_THROW(...)` expands to `~foo(...).DETAIL_TA_ADD_MESSAGE`, and `~` is doing all the work, regardless if there are
            //   parentheses on the right with the user message. `DETAIL_TA_ADD_MESSAGE`, when not expanded as a macro, is an instance of `class Evaluator`.
            // * This works perfectly for `TA_CHECK(...)`, but here in `TA_MUST_THROW(...)` the user can do a oneliner: `TA_MUST_THROW(...).Check...(foo)`,
            //   and that messes up with the priority of `~`.
            // * To work around all that, we do something really stupid. There are three different classes with the exact same interface,
            //   all inheriting from `BasicCaughtExceptionInterface`. `CaughtException` is one of them, and it behaves as you would expect.
            //   `Evaluator` also inherits from it, but any function that you call on it would first evaluate the `TA_MUST_THROW(...)` body instead of
            //   waiting for `~` to do that. Additionally, whenever `CaughtException` would `return *this`, `Evaluator` instead returns an instance of
            //   `class TemporaryCaughtException` BY VALUE, which is basically a clone of `CaughtException`, except that it also overloads `~` to
            //   convert it to a `CaughtException` (the conversion itself is almost a no-op).
            //   Also `TemporaryCaughtException`, whenever it needs to `return *this`, does this by rvalue reference, unlike `CaughtException`
            //   which does so by a const lvalue reference.
            class Evaluator
                : public detail::BasicCaughtExceptionInterface<Evaluator, TemporaryCaughtException, true>
            {
                MustThrowWrapper &self;

                friend MustThrowWrapper;
                Evaluator(MustThrowWrapper &self) : self(self) {}

                // For the CRTP baes.
                friend detail::BasicCaughtExceptionInterface<Evaluator, TemporaryCaughtException, true>;
                TemporaryCaughtException _get_state() const
                {
                    return operator~();
                }

              public:
                // This must be const because `_get_state()` is const.
                CFG_TA_API CaughtException operator~() const;
            };

          public:
            // The weird name helps with supporting optional messages.
            Evaluator DETAIL_TA_ADD_MESSAGE = *this;

            // Makes an instance of this class.
            template <meta::ConstString File, int Line, meta::ConstString MacroName, meta::ConstString Expr, typename F>
            [[nodiscard]] static MustThrowWrapper Make(const F &func, void (*break_func)())
            {
                static const BasicModule::MustThrowStaticInfo info = []{
                    BasicModule::MustThrowStaticInfo ret;
                    ret.loc = {.file = File.view(), .line = Line};
                    ret.macro_name = MacroName.view();
                    ret.expr = Expr.view();
                    return ret;
                }();

                return MustThrowWrapper(func, break_func, &info);
            }

            MustThrowWrapper(const MustThrowWrapper &) = delete;
            MustThrowWrapper &operator=(const MustThrowWrapper &) = delete;

            // This doesn't return `Evaluator &`, even though that would make sense for consistency.
            // Doing that would prevent a `TA_MUST_THROW(...).Check...()` oneliner from working.
            template <typename F>
            Evaluator &AddMessage(const F &func)
            {
                message_func = [](const void *data)
                {
                    std::string ret;
                    (*static_cast<const F *>(data))([&]<typename ...P>(CFG_TA_FMT_NAMESPACE::format_string<P...> format, P &&... args)
                    {
                        ret = CFG_TA_FMT_NAMESPACE::format(std::move(format), std::forward<P>(args)...);
                    });
                    return ret;
                };
                message_data = &func;
                return DETAIL_TA_ADD_MESSAGE;
            }
        };
    }


    // --- TEST RUNNER ---

    // Use this to run tests.
    struct Runner
    {
        std::vector<ModulePtr> modules;

        // NOTE: Some functions here are marked `const` even though they can access the non-const modules.
        // They can't modify the module list though, and can't start the run.
        // This is necessary to be able to pass the reference to `Runner` around while looping over modules,
        // without somebody yeeting a module from under us.

        // Fills `modules` arrays with all the default modules. Old contents are destroyed.
        CFG_TA_API void SetDefaultModules();

        // Handles the command line arguments in argc&argv style.
        // If `ok` is null and something goes wrong, aborts the application. If `ok` isn't null, sets it to true on success or to false on failure.
        // `argv[0]` is ignored.
        void ProcessFlags(int argc, char **argv, bool *ok = nullptr) const
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
        void ProcessFlags(R &&range, bool *ok = nullptr) const
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
        CFG_TA_API void ProcessFlags(std::function<std::optional<std::string_view>()> next_flag, bool *ok = nullptr) const;


        // Runs all tests.
        CFG_TA_API int Run();

        // Removes all modules of type `T` or derived from `T`.
        template <typename T>
        void RemoveModules()
        {
            std::erase_if(modules, [](const ModulePtr &ptr){return dynamic_cast<const T *>(ptr.get());});
        }

        // Calls `func` for every module of type `T` or derived from `T`.
        // `func` is `(T &module) -> bool`. If `func` returns true, the function stops immediately and also returns true.
        template <typename T, typename F>
        bool FindModule(F &&func) const
        {
            for (const auto &m : modules)
            {
                if (auto base = dynamic_cast<T *>(m.get()))
                {
                    if (func(*base))
                        return true;
                }
            }
            return false;
        }

        // Configures every `BasicPrintingModule` to print to `stream`.
        // Also automatically enables/disables color.
        void SetOutputStream(FILE *stream) const
        {
            SetTerminalSettings([&](output::Terminal &terminal)
            {
                terminal = output::Terminal(stream);
            });
        }

        // Configures every `BasicPrintingModule` to print to `stream`.
        void SetEnableColor(bool enable) const
        {
            SetTerminalSettings([&](output::Terminal &terminal)
            {
                terminal.enable_color = enable;
            });
        }

        // Sets the output stream for every module that prints stuff.
        void SetEnableUnicode(bool enable) const
        {
            for (const auto &m : modules)
            {
                if (auto base = dynamic_cast<BasicPrintingModule *>(m.get()))
                    base->EnableUnicode(enable);
            }
        }

        // Calls `func` on `Terminal` of every `BasicPrintingModule`.
        void SetTerminalSettings(std::function<void(output::Terminal &terminal)> func) const
        {
            for (const auto &m : modules)
            {
                if (auto base = dynamic_cast<BasicPrintingModule *>(m.get()))
                    func(base->terminal);
            }
        }
    };

    // A simple way to run the tests.
    // Copypaste the body into your code if you need more customization.
    inline int RunSimple(int argc, char **argv)
    {
        ta_test::Runner runner;
        runner.SetDefaultModules();
        runner.ProcessFlags(argc, argv);
        return runner.Run();
    }


    // --- BUILT-IN MODULES ---

    namespace modules
    {
        // --- BASES ---

        // Inherit modules from this when they need to print exception contents.
        // We use inheritance instead of composition to allow mass customization of all modules using this.
        struct BasicExceptionContentsPrinter
        {
          public:
            output::TextStyle style_exception_type = {.color = output::TextColor::dark_magenta};
            output::TextStyle style_exception_message = {.color = output::TextColor::light_blue};

            std::string chars_unknown_exception = "Unknown exception.";
            std::string chars_indent_type = "    ";
            std::string chars_indent_message = "        ";
            std::string chars_type_suffix = ": ";

            std::function<void(
                const BasicExceptionContentsPrinter &self,
                const output::Terminal &terminal,
                output::Terminal::StyleGuard &cur_style,
                const std::exception_ptr &e
            )> print_callback;

          protected:
            CFG_TA_API BasicExceptionContentsPrinter();
            CFG_TA_API void PrintException(const output::Terminal &terminal, output::Terminal::StyleGuard &cur_style, const std::exception_ptr &e) const;
        };

        // --- MODULES ---

        // Responds to `--help` by printing the flags provided by all other modules.
        struct HelpPrinter : BasicPrintingModule
        {
            // Pad flag spelling with spaces to be at least this long.
            // We could detect this automatically, but A: that's more work, and B: then very long flags would cause worse formatting for all other flags.
            int expected_flag_width = 0;

            flags::SimpleFlag flag_help;

            CFG_TA_API HelpPrinter();
            std::vector<flags::BasicFlag *> GetFlags() override;
            void OnUnknownFlag(std::string_view flag, bool &abort) override;
            void OnMissingFlagArgument(std::string_view flag, const flags::BasicFlag &flag_obj, bool &abort) override;
        };

        // Responds to `--include` and `--exclude` to select which tests to run.
        struct TestSelector : BasicModule
        {
            flags::StringFlag flag_include;
            flags::StringFlag flag_exclude;

            struct Pattern
            {
                bool exclude = false;

                std::string regex_string;
                std::regex regex;

                bool was_used = false;
            };
            std::vector<Pattern> patterns;

            CFG_TA_API TestSelector();
            std::vector<flags::BasicFlag *> GetFlags() override;
            void OnFilterTest(const BasicTestInfo &test, bool &enable) override;
            void OnPreRunTests(const RunTestsInfo &data) override;

            CFG_TA_API static flags::StringFlag::Callback GetFlagCallback(bool exclude);
        };

        // Responds to `--generate` to override the generated values.
        struct GeneratorOverridder : BasicPrintingModule
        {
            // A sequence of generator overrides coming from a `--generate`.
            struct GeneratorOverrideSeq
            {
                struct Entry
                {
                    std::string_view generator_name;

                    // If false, don't generate anything by default unless explicitly enabled.
                    bool enable_values_by_default = true;

                    struct CustomValue
                    {
                        std::string_view value;

                        std::shared_ptr<GeneratorOverrideSeq> custom_generator_seq;

                        // Next rule index in `rules` (or its size if no next rule).
                        std::size_t next_rule = 0;
                    };

                    // Custom values provided by the user, using the `=...`syntax.
                    // Anything listed here is skipped during natural generation, and none of the rules below apply to those.
                    std::vector<CustomValue> custom_values;

                    // Add or remove a certain index range.
                    // This corresponds to `#...` and `-#...` syntax.
                    struct RuleIndex
                    {
                        bool add = true;

                        // 0-based, half-open range.
                        std::size_t begin = 0;
                        std::size_t end = std::size_t(-1);

                        // Need default constructor here to make `std::variant` (`RuleVar`) understand that it's default-constructible.
                        constexpr RuleIndex() {}
                    };
                    // Remove a certain value.
                    // This corresponds to the `-=...` syntax.
                    struct RuleRemoveValue
                    {
                        std::string_view value;

                        // Need default constructor here to make `std::variant` (`RuleVar`) understand that it's default-constructible.
                        constexpr RuleRemoveValue() {}
                    };
                    using RuleVar = std::variant<RuleIndex, RuleRemoveValue>;

                    struct Rule
                    {
                        RuleVar var;

                        // If not null, this replaces the rest of the program for those values.
                        std::shared_ptr<GeneratorOverrideSeq> custom_generator_seq;
                    };

                    std::vector<Rule> rules;
                };

                std::vector<Entry> entries;
            };

            struct Entry
            {
                std::regex test_regex;

                // Don't read from this, call `OriginalArgument()` instead.
                // The storage for `OriginalArgument()`. We must store it, because a parsed `GeneratorOverrideSeq` depends on it,
                // (because this allows us to print nice errors by knowing relative positions of the `std::string_view`s in original string).
                // We must use `std::vector<char>` instead of `std::string` to avoid dangling references when reallocating the vector of entries.
                std::vector<char> original_argument_storage;

                GeneratorOverrideSeq seq;

                // The string that was given as a parameter to `--generate`.
                // This is null-terminated and points to `original_argument_storage`.
                [[nodiscard]] CFG_TA_API std::string_view OriginalArgument() const;
            };

            flags::StringFlag flag_override;
            flags::SimpleFlag flag_local_help;

            std::vector<Entry> entries;

            struct TestState
            {
                const Entry *entry = nullptr;

                std::span<const GeneratorOverrideSeq::Entry> remaining_program;

                struct Elem
                {
                    std::size_t generator_index = 0;
                    // The first element here is the one that was consumed by this generator.
                    std::span<const GeneratorOverrideSeq::Entry> remaining_program;

                    // How many custom values we've already inserted.
                    std::size_t num_used_custom_values = 0;
                };
                // Some of those can get stale, but we prune the trailing elements every time we create a new generator.
                std::vector<Elem> elems;

                // Need this for `std::optional<TestState>` to register the default-constructibility.
                TestState() {}
            };
            std::optional<TestState> test_state;

            CFG_TA_API GeneratorOverridder();

            CFG_TA_API std::vector<flags::BasicFlag *> GetFlags() override;

            void OnPreRunTests(const RunTestsInfo &data) override;
            void OnPostRunSingleTest(const RunSingleTestResults &data) override;
            bool OnRegisterGeneratorOverride(const RunSingleTestProgress &test, const BasicGenerator &generator) override;
            bool OnOverrideGenerator(const RunSingleTestProgress &test, BasicGenerator &generator) override;

            // Parses a `GeneratorOverrideSeq` object. `target` must initially be empty.
            // Returns the error on failure, or an empty string on success.
            // The `string` must remain alive, we're storing pointers into it.
            // `is_nested` should be false by default, and will be set to true when parsing nested sequences.
            // This consumes the trailing space.
            [[nodiscard]] CFG_TA_API std::string ParseGeneratorOverrideSeq(GeneratorOverrideSeq &target, const char *&string, bool is_nested);

            struct FlagErrorDetails
            {
                struct Elem
                {
                    std::string marker;
                    const char *location = nullptr;
                };
                std::vector<Elem> elems;

                FlagErrorDetails() {}
                FlagErrorDetails(const char *location) : elems{{.marker = "^", .location = location}} {}
                FlagErrorDetails(std::vector<Elem> elems) : elems(std::move(elems)) {}
            };

            // Fails with a hard error, and points to a specific location in this flag.
            // `location` must point into `entry.OriginalArgument()`.
            [[noreturn]] CFG_TA_API void HardErrorInFlag(std::string_view message, const Entry &entry, FlagErrorDetails details, HardErrorKind kind);
        };

        // Responds to various command line flags to configure the output of all printing modules.
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
            std::string chars_test_prefix;
            // This is used when reentering a group/test after a failed test.
            std::string chars_test_prefix_continuing;
            // The is used for indenting test names/groups.
            std::string chars_indentation;
            // This is printed once before the indentation.
            std::string chars_pre_indentation = " ";
            // This is printed after the test counter and before the test names/groups (and before their indentation guides).
            std::string chars_test_counter_separator;
            // The prefix before the failed test counter, ir any.
            std::string chars_failed_test_count_prefix = " [";
            // The suffix after the failed test counter, ir any.
            std::string chars_failed_test_count_suffix = "]";
            // The prefix before the failed generator repetition counter, ir any.
            std::string chars_failed_repetition_count_prefix = " [";
            // The suffix after the failed generator repetition counter, ir any.
            std::string chars_failed_repetition_count_suffix = "]";
            // This is printed after the generator repetition counter, if any.
            std::string chars_repetition_counter_separator;
            // Same, but is used when we need to shift the separator one character to the right
            std::string chars_repetition_counter_separator_diagonal;
            // The prefix for the generated value index, which is per generator.
            std::string chars_generator_index_prefix = "[";
            // This is added before the generator index if it's a custom value inserted via `--generator ...gen=value`.
            std::string chars_generator_custom_index_prefix = "*";
            // The suffix for the generated value index, which is per generator.
            std::string chars_generator_index_suffix = "]";
            // Separates the generated value (if any) from the generator name and the prefix.
            std::string chars_generator_value_separator = " = ";
            // The length limits on the printed generated values, before and after the ellipsis.
            // If the value length is less than the sum of those two plus the length of the ellipsis, it's printed completely.
            std::size_t max_generator_value_prefix_length = 120;
            std::size_t max_generator_value_suffix_length = 40;
            // The ellipsis in the long generated values.
            std::string chars_generator_value_ellipsis = "<...>";
            // This is printed when a test fails, right before the test name.
            std::string chars_test_failed = "TEST FAILED: ";
            // This is printed before the details of a failed test. It's repeated to fill the required width.
            std::string chars_test_failed_separator;
            // This is printed after the details of a failed test. It's repeated to fill the required width.
            std::string chars_test_failed_ending_separator;
            // This is printed when first starting tests.
            std::string chars_starting_tests = "Running tests...";
            // This is printed when resuming tests after a test failure.
            std::string chars_continuing_tests = "Continuing...";
            // This is printed at the end, before the list of failed tests.
            std::string chars_summary_tests_failed = "FOLLOWING TESTS FAILED:";
            // A vertical bar after the test name in the summary and before the source location.
            std::string chars_summary_path_separator;

            // Width for `chars_test_failed_separator`.
            // Intentionally not trying to figure out the true terminal width, a fixed value looks good enough.
            std::size_t separator_line_width = 100;

            // Optional message at startup when some tests are skipped.
            output::TextStyle style_skipped_tests = {.color = output::TextColor::light_blue, .bold = true};
            // The prefix before the name of the starting test.
            output::TextStyle style_prefix = {.color = output::TextColor::dark_green};
            // Same, but when reentering a group after a failure.
            output::TextStyle style_prefix_continuing = {.color = output::TextColor::light_black};
            // The message when a test starts.
            output::TextStyle style_name = {.color = output::TextColor::light_white, .bold = true};
            // The message when a test group starts.
            output::TextStyle style_group_name = {.color = output::TextColor::dark_white};
            // This is used to print a group name when reentering it after a failed test.
            output::TextStyle style_continuing_group = {.color = output::TextColor::light_black};
            // The indentation guides for nested test starts.
            output::TextStyle style_indentation_guide = {.color = output::TextColorGrayscale24(8)};
            // The test index.
            output::TextStyle style_index = {.color = output::TextColor::light_white, .bold = true};
            // The test index, when printed repeatedly, such as when repeating a test because of a generator.
            output::TextStyle style_index_repeated = {.color = output::TextColor::light_black, .bold = true};
            // The total test count printed after each test index.
            output::TextStyle style_total_count = {.color = output::TextColor::light_black};
            // The failed test counter.
            output::TextStyle style_failed_count = {.color = output::TextColor::light_red, .bold = true};
            // Some decorations around the failed test counter.
            output::TextStyle style_failed_count_decorations = {.color = output::TextColor::dark_magenta};
            // The line that separates the test counter from the test names/groups (or from the repetition counter, if any).
            output::TextStyle style_gutter_border = {.color = output::TextColorGrayscale24(10)};
            // The generator repetition counter of the specific test.
            output::TextStyle style_repetition_total_count = {.color = output::TextColor::dark_cyan};
            // The number of failed generator repetitions of the specific test.
            output::TextStyle style_repetition_failed_count = {.color = output::TextColor::light_red, .bold = true};
            // The brackets around that counter.
            output::TextStyle style_repetition_failed_count_decorations = {.color = output::TextColor::dark_magenta};
            // This line separate the repetition counters, if any, from the test names/groups.
            output::TextStyle style_repetition_border = {.color = output::TextColorGrayscale24(10)};
            struct StyleGenerator
            {
                // The prefix before the generator name.
                output::TextStyle prefix = {.color = output::TextColor::light_blue};
                // The generator name.
                output::TextStyle name = {.color = output::TextColor::dark_white};
                // The index of the value.
                output::TextStyle index = {.color = output::TextColor::light_white, .bold = true};
                // The index of the value when it's inserted by a command-line flag.
                output::TextStyle index_custom = {.color = output::TextColor::light_green, .bold = true};
                // The brackets around the index.
                output::TextStyle index_brackets = {.color = output::TextColor::light_black};
                // Separates the generated value from the generator name and index.
                output::TextStyle value_separator = {.color = output::TextColor::light_black};
                // The generated value, if printable.
                output::TextStyle value = {.color = output::TextColor::light_blue, .bold = true};
                // The ellipsis in the generated value, if it's too long.
                output::TextStyle value_ellipsis = {.color = output::TextColor::light_black, .bold = true};
            };
            // The normal run of a generator.
            StyleGenerator style_generator{};
            // Re-printing the existing value of a generator after a failure.
            StyleGenerator style_generator_repeated = {
                .prefix = {.color = output::TextColor::light_black},
                .name = {.color = output::TextColor::light_black, .bold = true},
                .index = {.color = output::TextColor::light_black, .bold = true},
                .index_custom = {.color = output::TextColor::light_black, .bold = true},
                .index_brackets = {.color = output::TextColor::light_black},
                .value_separator = {.color = output::TextColor::light_black},
                .value = {.color = output::TextColor::light_black, .bold = true},
                .value_ellipsis = {.color = output::TextColor::light_black},
            };
            // Printing a list of failed generators.
            StyleGenerator style_generator_failed = {
                .prefix = {.color = output::TextColor::dark_red},
                .name = {.color = output::TextColor::light_red},
                .index = {.color = output::TextColor::light_red, .bold = true},
                .index_custom = {.color = output::TextColor::light_red, .bold = true},
                .index_brackets = {.color = output::TextColor::dark_red},
                .value_separator = {.color = output::TextColor::dark_red},
                .value = {.color = output::TextColor::light_red, .bold = true},
                .value_ellipsis = {.color = output::TextColor::light_black, .bold = true},
            };
            // When printing a per-test summary of failed generators, this is the number of failed repetitions.
            output::TextStyle style_repetitions_summary_failed_count = {.color = output::TextColor::light_red, .bold = true};
            // When printing a per-test summary of failed generators, this is the total number of repetitions.
            output::TextStyle style_repetitions_summary_total_count = {.color = output::TextColor::dark_red};

            // The name of a failed test, printed when it fails.
            output::TextStyle style_failed_name = {.color = output::TextColor::light_yellow, .bold = true};
            // The name of a group of a failed test, printed when the test fails.
            output::TextStyle style_failed_group_name = {.color = output::TextColor::light_yellow};
            // The style for a horizontal line that's printed after a test failure message, before any details.
            output::TextStyle style_test_failed_separator = {.color = output::TextColor::dark_red};
            // This line is printed after all details on the test failure.
            output::TextStyle style_test_failed_ending_separator = {.color = output::TextColorGrayscale24(10)};
            // Style for `chars_starting_tests`.
            output::TextStyle style_starting_tests = {.color = output::TextColor::light_black, .bold = true};
            // Style for `chars_continuing_tests`.
            output::TextStyle style_continuing_tests = {.color = output::TextColor::dark_yellow};

            // The name of a failed test, printed at the end.
            output::TextStyle style_summary_failed_name = {.color = output::TextColor::light_red, .bold = true};
            // The name of a group of a failed test, printed at the end.
            output::TextStyle style_summary_failed_group_name = {.color = output::TextColor::dark_red};
            // Separates failed test names from their source locations.
            output::TextStyle style_summary_path_separator = {.color = output::TextColorGrayscale24(10)};
            // The source locations of the failed tests.
            output::TextStyle style_summary_path = {.color = output::TextColor::none};

          protected:
            struct State
            {
                // How many characters are needed to represent the total test count.
                std::size_t num_tests_width = 0;

                std::size_t test_counter = 0;
                std::vector<std::string_view> stack;
                // A copy of the stack from the previous test, if it has failed.
                // We use it to repeat the group names again, to show where we're restarting from.
                std::vector<std::string_view> failed_test_stack;

                struct PerTest
                {
                    // The generator repetition counter for the current test.
                    std::size_t repetition_counter = 0;

                    struct FailedGenerator
                    {
                        std::string name;
                        std::size_t index = 0; // 1-based
                        std::optional<std::string> value;
                        SourceLocWithCounter location;

                        // This should be unique enough.
                        [[nodiscard]] friend bool operator==(const FailedGenerator &a, const FailedGenerator &b)
                        {
                            return a.location == b.location && a.index == b.index;
                        }

                        // Could add more information here, but don't currently need it.
                    };
                    // A list of failed generator stacks.
                    std::vector<std::vector<FailedGenerator>> failed_generator_stacks;

                    // The last known character width for `repetition_counter` and `failed_generator_stacks.size()` together.
                    // This is reset to `-1` on a repetition failure, because we don't need to print the diagonal decoration after the bulky failure message.
                    std::size_t last_repetition_counters_width = std::size_t(-1);

                    // Whether the previous test has failed.
                    bool prev_failed = false;

                    struct PerRepetition
                    {
                        // Whether we already printed the `repetition_counter`.
                        bool printed_counter = false;

                        // Whether the previous repetition of this test has failed.
                        bool prev_rep_failed = false;
                    };
                    PerRepetition per_repetition;
                };
                // Per-test state.
                PerTest per_test;
            };
            State state;

            // Takes a string, and if it's too long, decides which part of it to omit.
            struct GeneratorValueShortener
            {
                bool is_short = true;

                // If the value is long, this is the prefix and the suffix that we should print.
                std::string_view long_prefix;
                std::string_view long_suffix;

                CFG_TA_API GeneratorValueShortener(std::string_view value, std::string_view ellipsis, std::size_t max_prefix, std::size_t max_suffix);
            };

            // Splits `name` at every `/`, and calls `func` for every segment.
            // `func` is `(std::string_view segment, bool is_last_segment) -> void`.
            static void SplitNameToSegments(std::string_view name, auto &&func)
            {
                auto it = name.begin();

                while (true)
                {
                    auto new_it = std::find(it, name.end(), '/');

                    func(std::string_view(it, new_it), new_it == name.end());

                    if (new_it == name.end())
                        break;
                    it = new_it + 1;
                }
            }

            // This is used to convert a sequence of test names to what looks like a tree.
            // `stack` must start empty before calling this the first time, and is left in an unspecified state after the last call.
            // `push_segment` is called every time we're entering a new tree node.
            // `push_segment` is `(std::size_t segment_index, std::string_view segment, bool is_last_segment) -> void`.
            // `segment` is one of the `/`-separated parts of the `name`. `segment_index` is the index of that part.
            // `is_last_segment` is whether this is the last segment in `name`.
            static void ProduceTree(std::vector<std::string_view> &stack, std::string_view name, auto &&push_segment)
            {
                std::size_t segment_index = 0;

                SplitNameToSegments(name, [&](std::string_view segment, bool is_last_segment)
                {
                    // Pop the tail off the stack.
                    if (segment_index < stack.size() && stack[segment_index] != segment)
                        stack.resize(segment_index);

                    if (segment_index >= stack.size())
                    {
                        push_segment(std::size_t(segment_index), std::string_view(segment), is_last_segment);

                        // Push to the stack.
                        stack.push_back(segment);
                    }

                    segment_index++;
                });
            }

            enum class TestCounterStyle {none, normal, repeated};
            CFG_TA_API void PrintContextLinePrefix(output::Terminal::StyleGuard &cur_style, const RunTestsProgress &all_tests, TestCounterStyle test_counter_style) const;
            CFG_TA_API void PrintContextLineIndentation(output::Terminal::StyleGuard &cur_style, std::size_t depth, std::size_t skip_characters) const;

            // Prints the entire line describing a generator.
            // `repeating_info == true` means that we're printing this not because a new value got generated,
            // but because we're providing the context again after an error.
            CFG_TA_API void PrintGeneratorInfo(output::Terminal::StyleGuard &cur_style, const RunSingleTestProgress &test, const BasicGenerator &generator, bool repeating_info);

          public:
            CFG_TA_API ProgressPrinter();

            void EnableUnicode(bool enable) override;
            void OnPreRunTests(const RunTestsInfo &data) override;
            void OnPostRunTests(const RunTestsResults &data) override;
            void OnPreRunSingleTest(const RunSingleTestInfo &data) override;
            void OnPostRunSingleTest(const RunSingleTestResults &data) override;
            void OnPostGenerate(const GeneratorCallInfo &data) override;
            void OnPreFailTest(const RunSingleTestProgress &data) override;
        };

        // Prints the results of a run.
        struct ResultsPrinter : BasicPrintingModule
        {
            // The number of skipped tests.
            output::TextStyle style_num_skipped = {.color = output::TextColor::light_blue};
            // No tests to run.
            output::TextStyle style_no_tests = {.color = output::TextColor::light_blue, .bold = true};
            // All tests passed.
            output::TextStyle style_all_passed = {.color = output::TextColor::light_green, .bold = true};
            // Some tests passed, this part shows how many have passed.
            output::TextStyle style_num_passed = {.color = output::TextColor::light_green};
            // Some tests passed, this part shows how many have failed.
            output::TextStyle style_num_failed = {.color = output::TextColor::light_red, .bold = true};

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
            std::u32string chars_assertion_failed = U"Assertion failed";
            // Same, but used when no expression is provided (e.g. by `TA_FAIL`).
            // Slightly weird string here.
            // Thought about "test manually failed", which sounds good until it's called internally
            // by the library, and then it should no longer count as "manually".
            std::u32string chars_assertion_failed_no_cond = U"Failure";
            // The enclosing assertions.
            std::u32string chars_in_assertion = U"While checking assertion:";

            // The argument colors. They are cycled in this order.
            std::vector<output::TextStyle> style_arguments = {
                {.color = output::TextColorRgb6(1,4,1), .bold = true},
                {.color = output::TextColorRgb6(1,3,5), .bold = true},
                {.color = output::TextColorRgb6(1,0,5), .bold = true},
                {.color = output::TextColorRgb6(5,1,0), .bold = true},
                {.color = output::TextColorRgb6(5,4,0), .bold = true},
                {.color = output::TextColorRgb6(0,4,3), .bold = true},
                {.color = output::TextColorRgb6(0,5,5), .bold = true},
                {.color = output::TextColorRgb6(3,1,5), .bold = true},
                {.color = output::TextColorRgb6(4,0,2), .bold = true},
                {.color = output::TextColorRgb6(5,2,1), .bold = true},
                {.color = output::TextColorRgb6(4,5,3), .bold = true},
            };
            // This is used for brackets above expressions.
            output::TextStyle style_overline = {.color = output::TextColor::light_magenta, .bold = true};
            // This is used to dim the unwanted parts of expressions.
            output::TextColor color_dim = output::TextColor::light_black;

            // Labels a subexpression that had a nested assertion failure in it.
            std::u32string chars_in_this_subexpr = U"in here";
            // Same, but when there's more than one subexpression. This should never happen.
            std::u32string chars_in_this_subexpr_inexact = U"in here?";

            void OnAssertionFailed(const BasicAssertionInfo &data) override;
            bool PrintContextFrame(output::Terminal::StyleGuard &cur_style, const context::BasicFrame &frame) override;

            CFG_TA_API void PrintAssertionFrameLow(output::Terminal::StyleGuard &cur_style, const BasicAssertionInfo &data, bool is_most_nested) const;
        };

        // Responds to `text::PrintLog()` to print the current log.
        // Does nothing by itself, is only used by the other modules.
        struct LogPrinter : BasicPrintingModule
        {
            output::TextStyle style_message = {.color = output::TextColor::dark_cyan};

            std::string chars_message_prefix = "// ";

            // The current position in the unscoped log vector, to avoid printing the same stuff twice. We reset this when we start a new test.
            // We intentionally re-print the scoped logs every time they're needed.
            std::size_t unscoped_log_pos = 0;

            void OnPreRunSingleTest(const RunSingleTestInfo &data) override;
            void OnPostRunSingleTest(const RunSingleTestResults &data) override;
            bool PrintLogEntries(output::Terminal::StyleGuard &cur_style, std::span<const context::LogEntry> unscoped_log, std::span<const context::LogEntry *const> scoped_log) override;
        };

        // A generic module to analyze exceptions.
        // `E` is the exception base class.
        // `F` is a functor to get the error string from an exception, defaults to `e.what()`.
        template <typename E, typename F = void>
        struct GenericExceptionAnalyzer : BasicModule
        {
            std::optional<ExplainedException> OnExplainException(const std::exception_ptr &e) const override
            {
                try
                {
                    std::rethrow_exception(e);
                }
                catch (E &e)
                {
                    ExplainedException ret;
                    ret.type = typeid(e);
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
        struct ExceptionPrinter : BasicPrintingModule, BasicExceptionContentsPrinter
        {
            std::string chars_error = "Uncaught exception:";

            void OnUncaughtException(const RunSingleTestInfo &test, const BasicAssertionInfo *assertion, const std::exception_ptr &e) override;
        };

        // Prints things related to `TA_MUST_THROW()`.
        struct MustThrowPrinter : BasicPrintingModule, BasicExceptionContentsPrinter
        {
            std::string chars_expected_exception = "Expected exception:";
            std::string chars_while_expecting_exception = "While expecting exception here:";
            std::string chars_exception_contents = "While analyzing exception:";
            std::string chars_throw_location = "Thrown here:";

            void OnMissingException(const MustThrowInfo &data, bool &should_break) override;
            bool PrintContextFrame(output::Terminal::StyleGuard &cur_style, const context::BasicFrame &frame) override;

            CFG_TA_API void PrintFrame(
                output::Terminal::StyleGuard &cur_style,
                const BasicModule::MustThrowStaticInfo &static_info,
                const BasicModule::MustThrowDynamicInfo *dynamic_info, // Optional.
                const BasicModule::CaughtExceptionInfo *caught, // Optional. If set, we're analyzing a caught exception. If null, we're looking at a macro call.
                bool is_most_nested // Must be false if `caught` is set.
            );
        };

        // Prints stack traces coming from `ta_test::Trace`.
        struct TracePrinter : BasicPrintingModule
        {
            std::u32string chars_func_name_prefix = U"In function: ";

            bool PrintContextFrame(output::Terminal::StyleGuard &cur_style, const context::BasicFrame &frame) override;
        };

        // Detects whether the debugger is attached in a platform-specific way.
        // Responds to `--debug`, `--break`, `--catch` to override the debugger detection.
        struct DebuggerDetector : BasicModule
        {
            // If this is not set, will check whether the debugger is attached when an assertion fails, and break if it is.
            std::optional<bool> break_on_failure;
            std::optional<bool> catch_exceptions;

            flags::BoolFlag flag_common;
            flags::BoolFlag flag_break;
            flags::BoolFlag flag_catch;

            CFG_TA_API DebuggerDetector();

            std::vector<flags::BasicFlag *> GetFlags() override;

            CFG_TA_API bool IsDebuggerAttached() const;
            void OnAssertionFailed(const BasicAssertionInfo &data) override;
            void OnUncaughtException(const RunSingleTestInfo &test, const BasicAssertionInfo *assertion, const std::exception_ptr &e) override;
            void OnMissingException(const MustThrowInfo &data, bool &should_break) override;
            void OnPreTryCatch(bool &should_catch) override;
            void OnPostRunSingleTest(const RunSingleTestResults &data) override;
        };

        // A little module that examines `DebuggerDetector` and notifies you when it detected a debugger.
        struct DebuggerStatePrinter : BasicPrintingModule
        {
            void OnPreRunTests(const RunTestsInfo &data) override;
        };
    }
}

template <>
struct CFG_TA_FMT_NAMESPACE::formatter<ta_test::output::Terminal::PrintableAnsiDelta, char>
{
    constexpr auto parse(std::basic_format_parse_context<char> &parse_ctx)
    {
        return parse_ctx.begin();
    }

    template <typename OutputIt>
    constexpr auto format(const ta_test::output::Terminal::PrintableAnsiDelta &arg, std::basic_format_context<OutputIt, char> &format_ctx) const
    {
        return CFG_TA_FMT_NAMESPACE::format_to(format_ctx.out(), "{}", arg.terminal.AnsiDeltaString(arg.cur_style, arg.new_style).data());
    }
};
