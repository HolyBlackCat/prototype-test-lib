#pragma once

// The proper preprocessor or bust.
#if defined(_MSC_VER) && !defined(__clang__) && (!defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL == 1)
#error The standard-conformant MSVC preprocessor is required, enable it with `/Zc:preprocessor`.
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem> // To make a `path` formatter.
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
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

#if __cpp_lib_source_location
#include <source_location>
#endif


// --- CONFIGURATION MACROS ---

// NOTE: Unless otherwise specified, those should have the same value in every translation unit, or you risk ODR violations!

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
#      define CFG_TA_API_CLASS
#    else
#      define CFG_TA_API __attribute__((__visibility__("default")))
#      define CFG_TA_API_CLASS __attribute__((__visibility__("default")))
#    endif
#  else
#    define CFG_TA_API
#    define CFG_TA_API_CLASS
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

// How to trigger a breakpoint.
// By default we should be only running this when a debugger is attached, so it's not a big deal that those seem to terminate a program if no debugger is attached.
// The logic is mostly copied from `SDL_TriggerBreakpoint()`.
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

// If this is set to false, replaces `$[...]` with `TA_ARG(...)`. Use this if `$` is taken by some other library.
// You can set this on a per-translation-unit basis.
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

// Whether we should clean up type names by removing "class" from them, and other similar strings.
// This should only be necessary on MSVC.
#ifndef CFG_TA_CLEAN_UP_TYPE_NAMES
#ifdef _MSC_VER
#define CFG_TA_CLEAN_UP_TYPE_NAMES 1
#else
#define CFG_TA_CLEAN_UP_TYPE_NAMES 0
#endif
#endif

// A comma-separated list of macro names defined to be equivalent to `TA_ARG`.
// `$` and `TA_ARG` are added automatically.
// NOTE! This should have the same value in all TUs to avoid ODR violations.
#ifndef CFG_TA_EXTRA_ARG_MACROS
#define CFG_TA_EXTRA_ARG_MACROS
#endif

// Whether to use libfmt instead of `std::format`.
// If you manually override the formatting function using other macros below, this will be ignored.
#ifndef CFG_TA_USE_LIBFMT
#  define CFG_TA_USE_LIBFMT 0
#endif
#if CFG_TA_USE_LIBFMT
#  if !__has_include(<fmt/format.h>)
#    error Taut was configured to use libfmt, but it's not installed.
#  endif
#else
#  ifndef __cpp_lib_format
#    error Taut was configured to use `std::format`, but your standard library doesn't support it. Switch to libfmt with `-DCFG_TA_USE_LIBFMT=1`.
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

// If true and if the formatting library supports range formatting, use its native implementation instead of ours (this includes tuple-like types).
// This is disabled by default, because our implementation takes advantage of our extended type support (non-char strings, `std::filesystem::path`, etc).
#ifndef CFG_TA_FMT_ALLOW_NATIVE_RANGE_FORMATTING
#define CFG_TA_FMT_ALLOW_NATIVE_RANGE_FORMATTING 0
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
#ifndef CFG_TA_FMT_HAS_RANGE_FORMATTING
#  if CFG_TA_USE_LIBFMT
#    define CFG_TA_FMT_HAS_RANGE_FORMATTING 2
#  else
#    ifdef __cpp_lib_format_ranges
#      define CFG_TA_FMT_HAS_RANGE_FORMATTING 1
#    else
#      define CFG_TA_FMT_HAS_RANGE_FORMATTING 0
#    endif
#  endif
#endif
// Whether the formatting library uses its own non-standard `string_view`-like type.
// Incorrectly setting this to true shouldn't have much effect, except for perhaps slightly slower build times.
#ifndef CFG_TA_FMT_USES_CUSTOM_STRING_VIEW
#  if CFG_TA_USE_LIBFMT
#    define CFG_TA_FMT_USES_CUSTOM_STRING_VIEW 1 // They always use it, even on newer standards, to preserve ABI compatibility.
#  else
#    define CFG_TA_FMT_USES_CUSTOM_STRING_VIEW 0
#  endif
#endif

// Whether we should try to detect the debugger and break on failed assertions, on platforms where we know how to do so.
// NOTE: Only touch this if including the debugger detection code in the binary is somehow problematic.
// If you just want to temporarily disable automatic breakpoints, pass `--no-debug` or configure `modules::DebuggerDetector` in `ta_test::Runner` instead.
#ifndef CFG_TA_DETECT_DEBUGGER
#define CFG_TA_DETECT_DEBUGGER 1
#endif

// Whether we should try to detect stdout or stderr being attached to an interactive terminal.
// If this is disabled, we assume not having a terminal, so the colored output is disabled by default.
// On Windows this also disables configuring the terminal (setting encoding to UTF-8 and enabling ANSI sequences), so you'd need to do that yourself.
// If you just want to disable the colors by default, it's better to call `ta_test::Runner::SetEnableColor()` than touching this.
#ifndef CFG_TA_DETECT_TERMINAL
#define CFG_TA_DETECT_TERMINAL 1
#endif

// Warning pragmas to ignore warnings about unused values.
// E.g. `TA_MUST_THROW(...)` calls this for its argument.
#ifndef CFG_TA_IGNORE_UNUSED_VALUE
#ifdef __GNUC__
#define CFG_TA_IGNORE_UNUSED_VALUE(...) _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunused-value\"") __VA_ARGS__ _Pragma("GCC diagnostic pop")
#else
#define CFG_TA_IGNORE_UNUSED_VALUE(...)
#endif
#endif

// The pretty function name.
// We could use `std::source_location`, but I've had issues with it with at least one combination of Clang+libc++ in the past, so I'm prefering the extensions.
#ifdef _MSC_VER
#define CFG_TA_THIS_FUNC_NAME __FUNCSIG__
#else
#define CFG_TA_THIS_FUNC_NAME __PRETTY_FUNCTION__
#endif

// The `[[nodiscard]]` attribute for lambdas. This is only used internally.
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

// `$[...]` needs to preserve the argument value to later print it if the assertion fails.
// It does so either by copying the value itself (for sufficiently trivial types), or by converting to a string immediately and storing that string.
// One of the "triviallness" requirements is fitting within this storage and alignment.
// Size 32 was chosen as the largest common `std::string` size (on x64 libstdc++ and MSVC STL, while libc++ uses 24).
// Alignment 16 was chosen as the popular SIMD alignment.
// Note that those must be large enough to fit `std::string`, there's a compile-time check for that.
#ifndef CFG_TA_ARG_STORAGE_SIZE
#define CFG_TA_ARG_STORAGE_SIZE 32
#endif
#ifndef CFG_TA_ARG_STORAGE_ALIGNMENT
#define CFG_TA_ARG_STORAGE_ALIGNMENT 16
#endif


// --- INTERFACE MACROS ---

// Define a test, e.g. `TA_TEST(name) {body}`.
// `name` is the test name without quotes and without spaces. You can use letters, digits, and `_`.
// Use `/` as a separator to make test groups: `group/sub_group/test_foo`. There must be no spaces around the slashes.
// The grouping only affects the reporting output (and sometimes the execution order, to run the entire group together).
// The name can be followed by flags of type `ta_test::TestFlags`, e.g. `ta_test::disabled` to disable this test by default.
#define TA_TEST DETAIL_TA_TEST

// Check condition. If it's false or throws, the test is marked as false, and also `InterruptTestException` is thrown to quickly exit the test.
// You can wrap any part of the condition in `$[...]` to print it on failure (there can be several, possibly nested).
// Usage:
//     TA_CHECK( x == 42 ); // You won't know the value of `x` on failure.
//     TA_CHECK( $[x] == 42 ); // `$` will print the value of `x` on failure.
// The first parenthesis can be followed by another one, containing extra parameters that are evaluated on failure. Following overloads are available:
//     TA_CHECK(condition)(message...)
//     TA_CHECK(condition)(flags)
//     TA_CHECK(condition)(flags, message...)
//     TA_CHECK(condition)(flags, source_loc)
//     TA_CHECK(condition)(flags, source_loc, message...)
// Where:
// * `message...` is a string literal, possibly followed by format arguments (as for `std::format`).
// * `flags` is an instance of `ta_test::AssertFlags`. In particular, passing `ta_test::soft` disables throwing on failure.
// * `source_loc` should rarely be used. It's an instance of `SourceLoc`,
//     which lets you override the filename and line number that will be displayed in the error message.
// More examples:
//     TA_CHECK( $[x] == 42 )("Checking stuff!"); // Add a custom message.
//     TA_CHECK( $[x] == 42 )("Checking {}!", "stuff"); // Custom message with formatting.
//     TA_CHECK( $[x] == 42 )(ta_test::soft); // Let the test continue after a failure.
#define TA_CHECK(...) DETAIL_TA_CHECK("TA_CHECK", #__VA_ARGS__, __VA_ARGS__)
// Equivalent to `TA_CHECK(false)`, except the printed message is slightly different.
// Like `TA_CHECK(...)`, can be followed by optional arguments.
// Usage:
//     TA_FAIL;
//     TA_FAIL("Stuff failed!"); // With a message.
//     TA_FAIL("Stuff {}!", "failed"); // With a message and formatting.
//     TA_FAIL(ta_test::soft); // Let the test continue after the failure.
#define TA_FAIL DETAIL_TA_FAIL

#if CFG_TA_USE_DOLLAR
// Can only be used inside of `TA_CHECK(...)`. Wrap a subexpression in this to print its value if the assertion fails.
// Those can be nested inside one another.
#define $ DETAIL_TA_ARG
#else
// A fallback replacement for `$`. We don't enable it by default to enforce a consistent style.
#define TA_ARG DETAIL_TA_ARG
#endif

// Checks that the argument throws an exception (then argument can contain more than one statement, and may contain semicolons).
// If there is no exception, fails the test and throws `InterruptTestException{}` to quickly stop it.
// Returns an instance of `ta_test::CaughtException`, which contains information about the exception and lets you validate its type and message.
// Example usage:
//     TA_MUST_THROW( throw std::runtime_error("Foo!") );
//     TA_MUST_THROW( throw std::runtime_error("Foo!") ).CheckMessage("Foo!"); // E.g. check message for an exact match.
//     TA_MUST_THROW( throw std::runtime_error("Foo!") ).CheckMessageRegex(".*!"); // Or with a regex (must match the whole string).
//     TA_MUST_THROW( std::vector<int> x; x.at(0); ).CheckMessageRegex(".*!"); // Multiple statements are allowed.
// Like `TA_CHECK(...)`, can be followed by a second parenthesis with optional parameters. Following overloads are available:
//     TA_MUST_THROW(body)(message...)
//     TA_MUST_THROW(body)(flags)
//     TA_MUST_THROW(body)(flags, message...)
// Where:
// * `message...` is a string literal, possibly followed by format arguments (as for `std::format`).
// * `flags` is an instance of `ta_test::AssertFlags`. In particular, passing `ta_test::soft` disables throwing on failure.
// Unlike `TA_CHECK(...)`, `TA_MUST_THROW` doesn't support overriding the source location information.
#define TA_MUST_THROW(...) \
    DETAIL_TA_MUST_THROW("TA_MUST_THROW", #__VA_ARGS__, __VA_ARGS__)

// Logs a formatted line. It's only printed on test failure, at most once per test.
// Example:
//     TA_LOG("Hello!");
//     TA_LOG("x = {}", 42);
// The trailing `\n`, if any, is ignored.
// Can also accept `std::source_location` (or `ta_test::SourceLoc`) directly.
#define TA_LOG DETAIL_TA_LOG
// Creates a scoped log message. It's printed only if this line is in scope on test failure.
// Unlike `TA_LOG()`, the message can be printed multiple times, if there are multiple failures in this scope.
// The trailing `\n`, if any, is ignored.
// Can also accept `std::source_location` (or `ta_test::SourceLoc`) directly. Then also prints the current function name (not the one in `source_location`).
// Source location can be followed by a function name, which defaults to __PRETTY_FUNCTION__/__FUNCSIG__.
// The code calls this a "scoped log", and "context" means something else in the code.
#define TA_CONTEXT DETAIL_TA_CONTEXT
// Like `TA_CONTEXT`, but only evaluates the message when needed.
// This means you need to make sure none of your variables dangle, and that they have sane values for the entire lifetime of this context.
// Can evaluate the message more than once. You can utilize this to display the current variable values.
// This version doesn't accept `std::source_location` (or `ta_test::SourceLoc`).
#define TA_CONTEXT_LAZY DETAIL_TA_CONTEXT_LAZY

// Repeats the test for all values in the range, which is either a braced list or a C++20 range.
// Example usage: `int x = TA_GENERATE(foo, {1,2,3});`.
// `name` is the name for logging purposes, it must be a valid identifier. It's also used for controlling the generator from the command line.
// You can't use any local variables in `...`, that's a compilation error.
//   This is an artificial limitation for safety reasons, to prevent accidental dangling.
//   Use `TA_GENERATE_FUNC(...)` with `ta_test::RangeToGeneratorFunc(...)` to do that.
// Accepts an optional parameter before the range, of type `ta_test::GeneratorFlags`, same as `RangeToGeneratorFunc()`.
//   E.g. pass `ta_test::interrupt_test_if_empty` to allow empty ranges.
#define TA_GENERATE DETAIL_TA_GENERATE

// Repeats the test for all values returned by the lambda.
// Usage: `T x = TA_GENERATE_FUNC(name, [flags,] lambda);`.
// NOTE: Since the lambda will outlive the test, make sure your captures don't dangle.
// The lambda must be `(bool &repeat) -> ??`. The return type can be anything, possibly a reference.
// Saves the return value from the lambda and returns it from `TA_GENERATE(...)`.
// `repeat` receives `true` by default. If it remains true, the test will be restarted and the lambda will be called again to compute the new value.
// If it's set to false, the return value becomes the last one in the sequence, and the lambda is discarded.
// Example usage:
//     int x = TA_GENERATE_FUNC(blah, [i = 10](bool &repeat) mutable {repeat = i < 15; return i++;});
// The value is returned by const reference: `const decltype(func(...)) &`.
// Note that this means that if `func()` returns by reference, the possible lack of constness of the reference is preserved:
//   Lambda returns | TA_GENERATE_FUNC returns
//   ---------------|-------------------------
//             T    | const T &
//             T &  |       T &
//             T && |       T &&
// The lambda is not evaluated/constructed at all when reentering `TA_GENERATE_FUNC(...)`, if we already have one from the previous run.
// We guarantee that the lambda isn't copied or moved AT ALL.
// The lambda can be preceded by an optional parameter of type `ta_test::GeneratorFlags`.
// Or you can pass an instance of `ta_test::GenerateFuncParam`, which combines the flags and the lambda into one object.
#define TA_GENERATE_FUNC DETAIL_TA_GENERATE_FUNC

// A version of `TA_GENERATE` for generating types (and other template parameters, such as constant values or templates).
// Despite how the syntax looks like, the whole test is repeated for each type/value, not just the braced block.
// Example usage:
//     TA_GENERATE_PARAM(typename T, int, float, double)
//     {
//         T x = 42;
//         // ...
//     };
// Note the trailing `;`.
// The full syntax is `TA_GENERATE_PARAM(kind name, list [,flags])`
// * The first argument must be a template parameter declaration, where `name` is a valid identifier.
//     If `kind` isn't one of: `typename`, `class`, `auto` (such as a specific type, a concept, or a template template parameter), it must be parenthesized.
//       Here's an example with a template template parameter:
//           TA_GENERATE_PARAM((template <typename...> typename) T, std::vector, std::deque)
//           {
//               T<int> x = {1,2,3};
//               // ...
//           };
//     Certain non-type parameters are impossible to express with this syntax (pointers/references to functions/arrays),
//       because a part of the type goes after the parameter name. For those, either typedef the type, or just use `auto`.
// * The list can be parenthesized. It must be parenthesized if flags are specified, if the list is empty, or if it starts with `(` (e.g. starts with a C-style cast).
// * The optional `flags` is an instace of `ta_test::GeneratorFlags`.
// The `{...}` body is a lambda (with `[&]` capture) that is called immediately. You can return something from it, then the macro returns the same thing.
//   As usual, the default return type is `auto`, but you can add `-> ...` after the macro to change the type (and possibly return by reference).
//   Like with `std::visit`, the return type can't depend on the template parameter.
// You can extract the argument list from a template instead of hardcoding it, by putting `ta_test::expand` before it like this:
//     TA_GENERATE_PARAM(typename T, ta_test::expand, std::tuple<A, B, C>)
// Which is equivalent to:
//     TA_GENERATE_PARAM(typename T, A, B, C)
// `ta_test::expand` must be followed by exactly one type, which must be a template specialization `T<U...>` (where `U` can be anything, not necessarily a type).
// Here's an example with `expand` and flags:
//     TA_GENERATE_PARAM(typename T, (ta_test::expand, std::tuple<A,B,C>), ta_test::interrupt_test_if_empty)
#define TA_GENERATE_PARAM DETAIL_TA_GENERATE_PARAM

// Repeats the test several times, once for each of the several code fragments (a spin on `TA_GENERATE(...)`).
// Example usage:
//     TA_SELECT(foo)
//     {
//         TA_VARIANT(a) {...}
//         TA_VARIANT(b) {...}
//         TA_VARIANT(c) {...}
//     }
// `foo` and `a`,`b`,`c` are names for logging purposes, and for controlling the test flow from a command line.
// The braces after `TA_VARIANT(...)` can be omitted, if the body is only a single line (like in an `if`).
// `TA_SELECT` accepts a second optional parameter of type `ta_test::GeneratorFlags`, which in particular lets you customize the behavior if no variants are enabled.
// You can enable and disable variants at runtime, by enclosing one in an `if`.
// This implies that the braces after `TA_SELECT(...)` can contain more than just the variants.
// When `TA_SELECT` is first reached, we run a variant discovery pass: the body is executed from the `{`, but without entering any of the variants.
//   The list of reached variants is preserved, and then the execution jumps to the first discovered variant, and when it finishes,
//   jumps to the closing brace of `TA_SELECT(...)`. When `TA_SELECT` is then reached again after the test restarts, it only executes the next variant.
// The body of `TA_SELECT(...)` is internally a `switch`, so e.g. you can't have any variables outside of variants. Put your variables before `TA_SELECT(...)`.
// Using `continue;` or `break;` (both have the same effect) inside of a `TA_VARIANT` jumps to its `}`,
//   and using `break` outside of a variant stops the variant discovery and jumps to the first variant.
//   This isn't entirely intentional design; our macros use a loop internally so those keywords have to do *something*,
//   so the only reasonable option was to make them do this.
// We guarantee that exactly one variant is executed for a `TA_SELECT(...)`.
#define TA_SELECT DETAIL_TA_SELECT
// Marks one of the several code fragments to be executed by `TA_VARIANT(...)`. See that macro for details.
#define TA_VARIANT DETAIL_TA_VARIANT


// --- INTERNAL MACROS ---

#define DETAIL_TA_NULL(...)
#define DETAIL_TA_IDENTITY(...) __VA_ARGS__

#define DETAIL_TA_EXPECT_EMPTY()

// Like `DETAIL_TA_IDENTITY(...)`, but errors out if the argument is empty.
#define DETAIL_TA_NONEMPTY_IDENTITY(...) DETAIL_TA_NONEMPTY_IDENTITY_CAT(DETAIL_TA_NONEMPTY_IDENTITY_A_,__VA_OPT__(0))(__VA_ARGS__)
#define DETAIL_TA_NONEMPTY_IDENTITY_A_() DETAIL_TA_NONEMPTY_IDENTITY_B(x)
#define DETAIL_TA_NONEMPTY_IDENTITY_A_0(...) __VA_ARGS__
#define DETAIL_TA_NONEMPTY_IDENTITY_B()
#define DETAIL_TA_NONEMPTY_IDENTITY_CAT(x, y) DETAIL_TA_NONEMPTY_IDENTITY_CAT_(x, y)
#define DETAIL_TA_NONEMPTY_IDENTITY_CAT_(x, y) x##y

#define DETAIL_TA_STR(...) DETAIL_TA_STR_(__VA_ARGS__)
#define DETAIL_TA_STR_(...) #__VA_ARGS__

#define DETAIL_TA_CAT(x, ...) DETAIL_TA_CAT_(x, __VA_ARGS__)
#define DETAIL_TA_CAT_(x, ...) x##__VA_ARGS__

#define DETAIL_TA_TEST(name, .../*flags*/) \
    inline void _ta_test_func(::ta_test::meta::ConstStringTag<#name>); \
    /* This must be non-inline, because we want to repeat registration for each TU, to detect source location mismatches. */\
    /* But the test body is inline to reduce bloat when tests are in headers. */\
    [[maybe_unused]] static constexpr auto _ta_test_registration_helper(::ta_test::meta::ConstStringTag<#name>) -> decltype(void(::std::integral_constant<\
        const std::nullptr_t *, &::ta_test::detail::register_test_helper<\
            ::ta_test::detail::SpecificTest<static_cast<void(*)(\
                ::ta_test::meta::ConstStringTag<#name>\
            )>(_ta_test_func),\
            []{CFG_TA_BREAKPOINT();},\
            #name, __FILE__, __LINE__\
            __VA_OPT__(,) __VA_ARGS__>\
        >\
    >{})) {} \
    inline void _ta_test_func(::ta_test::meta::ConstStringTag<#name>)

#define DETAIL_TA_CHECK(macro_name_, str_, ...) \
    /* `~` is what actually performs the asesrtion. We need something with a high precedence. */\
    ~::ta_test::detail::AssertWrapper(macro_name_, {__FILE__, __LINE__}, str_, #__VA_ARGS__,\
        [&]([[maybe_unused]]::ta_test::detail::AssertWrapper &_ta_assert){_ta_assert.EvalCond(__VA_ARGS__);},\
        []{CFG_TA_BREAKPOINT();}\
    )\
    .DETAIL_TA_ADD_EXTRAS

#define DETAIL_TA_FAIL DETAIL_TA_CHECK("", "", false)

#define DETAIL_TA_ADD_EXTRAS(...) \
    AddExtras([&](auto &&_ta_add_extras){_ta_add_extras(__VA_ARGS__);})

#define DETAIL_TA_ARG \
    _ta_assert._ta_arg_(__COUNTER__)

#define DETAIL_TA_MUST_THROW(macro_name_, str_, ...) \
    /* `~` is what actually performs the asesrtion. We need something with a high precedence. */\
    ~::ta_test::detail::MustThrowWrapper::Make<__FILE__, __LINE__, macro_name_, str_>(\
        [&]{CFG_TA_IGNORE_UNUSED_VALUE(__VA_ARGS__;)},\
        []{CFG_TA_BREAKPOINT(); ::std::terminate();}\
    )\
    .DETAIL_TA_ADD_EXTRAS

#define DETAIL_TA_LOG(...) \
    ::ta_test::detail::AddLogEntry(__VA_ARGS__)
#define DETAIL_TA_CONTEXT(...) \
    ::ta_test::detail::ScopedLogGuard DETAIL_TA_CAT(_ta_context,__COUNTER__)(CFG_TA_THIS_FUNC_NAME, __VA_ARGS__)
#define DETAIL_TA_CONTEXT_LAZY(...) \
    ::ta_test::detail::ScopedLogGuardLazy DETAIL_TA_CAT(_ta_context,__COUNTER__)([&]{return CFG_TA_FMT_NAMESPACE::format(__VA_ARGS__);})

#define DETAIL_TA_GENERATE(name, ...) \
    ::ta_test::detail::GenerateValue<#name, __FILE__, __LINE__, __COUNTER__>([/*non-capturing*/]{return ::ta_test::GenerateFuncParam(::ta_test::RangeToGeneratorFunc(__VA_ARGS__));})

#define DETAIL_TA_GENERATE_FUNC(name, ...) \
    ::ta_test::detail::GenerateValue<#name, __FILE__, __LINE__, __COUNTER__>([&]{return ::ta_test::GenerateFuncParam(__VA_ARGS__);})

#define DETAIL_TA_GENERATE_PARAM(param, ...) \
    ::ta_test::detail::ParamGenerator<\
        __FILE__, __LINE__, __COUNTER__, \
        /* Parameter name string. */\
        DETAIL_TA_STR(DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME(param)), \
        /* Bake the parameter list into a lambda. */\
        ::ta_test::meta::Overload{ \
            /* Hardcoded parameter list. */\
            []<DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND(param) ..._ta_test_P>() \
            { \
                return []<typename _ta_test_F, typename ..._ta_test_Q>() \
                { \
                    if constexpr (sizeof...(_ta_test_P) == 0) return std::array<void(*)(_ta_test_F &&, _ta_test_Q &&...), 0>{}; \
                    else return ::std::array{+[](_ta_test_F &&f, _ta_test_Q &&... q) -> decltype(auto) {return f.template operator()<_ta_test_P>(std::forward<_ta_test_Q>(q)...);}...}; \
                }; \
            }, \
            /* Parameter list expanded from a user type. */\
            []<::ta_test::ExpandTag, typename _ta_test_L>() \
            { \
                return []<template <DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND(param)...> typename _ta_test_X, DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND(param) ..._ta_test_P>(::ta_test::meta::TypeTag<_ta_test_X<_ta_test_P...>>) \
                { \
                    return []<typename _ta_test_F, typename ..._ta_test_Q>() \
                    { \
                        if constexpr (sizeof...(_ta_test_P) == 0) return std::array<void(*)(_ta_test_F &&, _ta_test_Q &&...), 0>{}; \
                        return ::std::array{+[](_ta_test_F &&f, _ta_test_Q &&... q) -> decltype(auto) {return f.template operator()<_ta_test_P>(std::forward<_ta_test_Q>(q)...);}...}; \
                    }; \
                }(::ta_test::meta::TypeTag<_ta_test_L>{}); \
            } \
        } \
        .template operator()<DETAIL_TA_GENERATE_PARAM_EXTRACT_LIST(__VA_ARGS__)>(), \
        /* A lambda to convert arguments to strings. */\
        /* If you change the return type here, you must also change the one in `ParamNameFunc::operator()`. */\
        ::ta_test::detail::ParamNameFunc<[]<DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND(param) _ta_test_NameOf>(::ta_test::meta::PreferenceTagB) -> std::string_view \
        {return ::ta_test::detail::ParseEntityNameFromString<CFG_TA_THIS_FUNC_NAME>();}> \
        >( DETAIL_TA_GENERATE_PARAM_EXTRACT_FLAGS(__VA_ARGS__) ) \
    ->* [&]<DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND(param) DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME(param)>()

#define DETAIL_TA_SELECT(name, ...) \
    for (auto _ta_test_variant = ::ta_test::detail::VariantGenerator<__FILE__, __LINE__, __COUNTER__, #name>(__VA_ARGS__); _ta_test_variant.LoopCondition();) \
    switch (_ta_test_variant.SelectTarget()) \
    case decltype(_ta_test_variant)::Enum{}:

#define DETAIL_TA_VARIANT(name) \
    DETAIL_TA_VARIANT_LOW(name, __COUNTER__)

#define DETAIL_TA_VARIANT_LOW(name, counter) \
    /* This `if` handles the first (registration) pass. */\
    if (true) { \
        _ta_test_variant.template RegisterVariant<counter, #name>(); \
        /* In the second pass, this lets us break from the `TA_SELECT(...)` switch, */\
        if (false) \
            DETAIL_TA_CAT(_ta_test_variant_exit_label_, counter): break; \
    } else \
    /* The second pass jumps here. */\
    case typename decltype(_ta_test_variant)::Enum(counter): \
    /* During the second pass, this first runs the user code, and then breaks us out of the `TA_SELECT(...)`. */\
    for ([[maybe_unused]] bool DETAIL_TA_CAT(_ta_test_variant_flag_, counter) : {false, true}) \
    if (true) { \
        /* Exit from `TA_SELECT(...)` after running the user code during the second pass. */\
        if (DETAIL_TA_CAT(_ta_test_variant_flag_, counter)) goto DETAIL_TA_CAT(_ta_test_variant_exit_label_, counter); \
        /* Run the user code in the second pass. */\
        else goto DETAIL_TA_CAT(_ta_test_variant_start_label_, counter); \
    } \
    /* Here we need a dummy loop to catch user's `break` and not letting it resume the `TA_SELECT(...)`. */\
    else while (false) \
    /* User code execution starts here. */\
    DETAIL_TA_CAT(_ta_test_variant_start_label_, counter):

// Internal macro for `DETAIL_TA_GENERATE_PARAM(...)`.
// If the argument starts with one of: `typename`, `class`, `auto`, then returns that word. Otherwise the argument must be of the form `(a)b`, then returns `a`.
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND(...) \
    DETAIL_TA_CAT(DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND_C_, \
        DETAIL_TA_CAT( \
            DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND_B_, \
            DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND_A __VA_ARGS__ \
        ) \
    ) \
    /* Extra closing: */ )
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND_A(...) 0 (__VA_ARGS__)
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND_B_0(...) 0 (__VA_ARGS__)
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND_B_DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND_A
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND_C_0(...) __VA_ARGS__ DETAIL_TA_NULL(
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND_C_typename typename DETAIL_TA_NULL(
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND_C_class class DETAIL_TA_NULL(
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_KIND_C_auto auto DETAIL_TA_NULL(

// Internal macro for `DETAIL_TA_GENERATE_PARAM(...)`.
// The argument must start with `(...)` or one of: `typename`, `class`, `auto`. Returns the argument without that part.
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME(...) \
    DETAIL_TA_CAT(DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME_C_, \
        DETAIL_TA_CAT(DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME_B_, \
            DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME_A __VA_ARGS__ \
        ) \
    )
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME_A(...) 0
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME_B_0 0
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME_B_DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME_A
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME_C_0
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME_C_typename
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME_C_class
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_NAME_C_auto

// Internal macro for `DETAIL_TA_GENERATE_PARAM(...)`.
// If `...` is `(a) b`, returns `a`. Otherwise returns `...` unchanged.
// Errors out if `...` is empty (but not if it starts with an empty (...)`.
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_LIST(...) DETAIL_TA_CAT( DETAIL_TA_GENERATE_PARAM_EXTRACT_LIST_C_, DETAIL_TA_GENERATE_PARAM_EXTRACT_LIST_A(__VA_ARGS__) )
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_LIST_A(...) DETAIL_TA_GENERATE_PARAM_EXTRACT_LIST_B __VA_ARGS__ )
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_LIST_B(...) 0 __VA_ARGS__ DETAIL_TA_NULL(
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_LIST_C_0
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_LIST_C_DETAIL_TA_GENERATE_PARAM_EXTRACT_LIST_B DETAIL_TA_NONEMPTY_IDENTITY(

// Internal macro for `DETAIL_TA_GENERATE_PARAM(...)`.
// If `...` is `(a), b`, returns `b`. If the argument doesn't start with `(`, returns nothing.
// Should error out if `(...)` isn't followed by `,`, or if `,` isn't followed by anything.
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_FLAGS(...) DETAIL_TA_CAT( DETAIL_TA_GENERATE_PARAM_EXTRACT_FLAGS_B_, DETAIL_TA_GENERATE_PARAM_EXTRACT_FLAGS_A __VA_ARGS__ ) )
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_FLAGS_A(...) 0
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_FLAGS_B_0 DETAIL_TA_GENERATE_PARAM_EXTRACT_FLAGS_C(
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_FLAGS_B_DETAIL_TA_GENERATE_PARAM_EXTRACT_FLAGS_A DETAIL_TA_NULL(
#define DETAIL_TA_GENERATE_PARAM_EXTRACT_FLAGS_C(x, ...) DETAIL_TA_EXPECT_EMPTY(x) __VA_ARGS__

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

    namespace output
    {
        struct Terminal;
    }

    namespace detail
    {
        class GenerateValueHelper;
        struct SpecificGeneratorGenerateGuard;
        template <typename T> struct ModuleWrapper;
    }


    // The exit codes we're using. This is mostly for reference.
    enum class ExitCode
    {
        ok = 0,
        no_tests_to_run = 0, // There are no tests to run. It's moot if this should be an error, currently it's not.
        test_failed = 1, // One or more tests failed.
        bad_command_line_arguments = 3, // A generic issue with command line arguments.
        no_test_name_match = 4, // `--include` or `--exclude` didn't match any tests.
    };

    // We try to classify the hard errors into interal ones and user-induced ones, but this is only an approximation.
    enum class HardErrorKind {internal, user};
    // Aborts the application with an error. Mostly for internal use.
    [[noreturn]] CFG_TA_API void HardError(std::string_view message, HardErrorKind kind = HardErrorKind::internal);

    // We throw this to abort a test (not necessarily fail it).
    // You can catch and rethrow this before a `catch (...)` to still be able to abort tests inside one.
    // You could throw this manually, but I don't see why you'd want to.
    struct InterruptTestException {};

    // Whether the current test is in the process of failing.
    // This is useful if you use soft assertion, and want to manually stop on failure.
    // If no test is currently running, returns false.
    [[nodiscard]] CFG_TA_API bool IsFailing();

    // Flags for `TA_TEST(...)`. Pass them after the name, as an optional parameter.
    enum class TestFlags
    {
        // Disables this test. It can only be enabled with `--force-include`.
        disabled = 1 << 0,
    };
    DETAIL_TA_FLAG_OPERATORS(TestFlags)
    using enum TestFlags;

    // Flags for `TA_CHECK(...)`. Pass them before the condition, as an optional parameter.
    enum class AssertFlags
    {
        hard = 0,
        // Don't throw `InterruptTestException` on failure, but the test still fails.
        soft = 1 << 0,
    };
    DETAIL_TA_FLAG_OPERATORS(AssertFlags)
    using enum AssertFlags;

    // Flags for `TA_GENERATE(...)` and others.
    enum class GeneratorFlags
    {
        // By default if the same generator is reached twice during one test execution, the second call simply returns a copy of the first value.
        // With this flag, each call generates a new value (essentially giving you a cartesian product).
        // This flag prevents a read from the storage, but doesn't prevent a write to it. (So the flag has no effect when reaching a generator for the first time,
        //   and has no effect on the future calls to the same generator).
        new_value_when_revisiting = 1 << 0,
        // Don't emit a hard error if the range is empty, instead throw `InterruptTestException` to abort the test.
        interrupt_test_if_empty = 1 << 1,
        // Generate no elements.
        // This causes a hard error, or, if `interrupt_test_if_empty` is also set, throws an `InterruptTestException`.
        // That is, unless `--generate` is used to add custom values to this generator.
        // This is primarily useful when generating from a callback. When generating from a range, this has the same effect as passing an empty range.
        // The callback or range are still used to deduce the return type, but are otherwise ignored.
        generate_nothing = 1 << 2,
    };
    DETAIL_TA_FLAG_OPERATORS(GeneratorFlags)
    using enum GeneratorFlags;

    // Arguments of `TA_GENERATE_FUNC(...)` are passed to the constructor of this class.
    // You can pass an instance of this directly to `TA_GENERATE_FUNC(...)` too.
    // You're expected to use CTAD with this. We go to lengths to ensure zero moves for the functor, which we need e.g. for ranges.
    template <std::invocable<bool &> F, bool HasFlags>
    struct GenerateFuncParam
    {
        // This is optional. If you provide no initializer, the flag-less specialization will be choosen.
        GeneratorFlags flags{};

        // This is usually a non-reference or an lvalue reference. But you could also make this an rvalue reference.
        F func{};

        using FuncRefType = F &&;
    };
    // Flag-less specialization.
    template <std::invocable<bool &> F>
    struct GenerateFuncParam<F, false>
    {
        static constexpr GeneratorFlags flags{}; // Default flags.
        F func{};
        using FuncRefType = F &&;
    };
    template <typename F> GenerateFuncParam(GeneratorFlags, F &&) -> GenerateFuncParam<F, true>;
    template <typename F> GenerateFuncParam(F &&) -> GenerateFuncParam<F, false>;

    struct ExpandTag {};
    // Pass this to `TA_GENERATE_PARAM(...)` to expand the argument list from a single type.
    // See comments on that macro for details.
    inline constexpr ExpandTag expand;

    // A simple source location.
    // Not using `std::source_location` to allow custom arguments, and just in cast it's not available.
    struct SourceLoc
    {
        std::string_view file;
        int line = 0;

        constexpr SourceLoc() {}
        constexpr SourceLoc(std::string_view file, int line) : file(file), line(line) {}
        #if __cpp_lib_source_location
        SourceLoc(std::source_location loc) : file(loc.file_name()), line(int(loc.line())) {}
        #endif

        struct Current
        {
            explicit Current() = default;
        };
        // The current source location. Pass `ta_test::SourceLoc::Current{}`.
        constexpr SourceLoc(Current, std::string_view file = __builtin_FILE(), int line = __builtin_LINE())
            : SourceLoc(file, line)
        {}

        friend auto operator<=>(const SourceLoc &, const SourceLoc &) = default;
    };
    // A source location with a value of `__COUNTER__`.
    struct SourceLocWithCounter : SourceLoc
    {
        int counter = 0;

        constexpr SourceLocWithCounter() {}
        constexpr SourceLocWithCounter(std::string_view file, int line, int counter) : SourceLoc(file, line), counter(counter) {}
        constexpr SourceLocWithCounter(SourceLoc loc, int counter) : SourceLoc(loc), counter(counter) {}

        friend auto operator<=>(const SourceLocWithCounter &, const SourceLocWithCounter &) = default;
    };

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

        // Tag dispatch helpers.
        template <typename> struct TypeTag {};
        template <auto> struct ValueTag {};

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

        // Those are used to prioritize function overloads.
        struct PreferenceTagB {};
        struct PreferenceTagA : PreferenceTagB {};
    }

    namespace text
    {
        // Character manipulation.
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
            [[nodiscard]] constexpr bool IsNonDigitIdentifierCharStrict(char ch)
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
                return IsNonDigitIdentifierCharStrict(ch) || IsDigit(ch);
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
                    "$", // Unconditionally, without checking `CFG_TA_USE_DOLLAR`, to avoid ODR violations.
                    CFG_TA_EXTRA_ARG_MACROS
                })
                {
                    if (alias == name)
                        return true;
                }

                return false;
            }

            // Whether `name` is a non-empty valid idenfitier.
            [[nodiscard]] constexpr bool IsIdentifierStrict(std::string_view name)
            {
                return !name.empty() && IsNonDigitIdentifierCharStrict(name.front()) && std::all_of(name.begin() + 1, name.end(), IsIdentifierCharStrict);
            }

            // Given a byte, checks if it's the first byte of a multibyte UTF-8 character, or is a single-byte character.
            // Even if this function returns true, `byte` can be an invalid first byte, that has to be tested separately.
            [[nodiscard]] constexpr bool IsFirstUtf8Byte(char byte)
            {
                return (byte & 0b11000000) != 0b10000000;
            }

            // Counts the number of codepoints (usually characters) in a valid UTF8 string, by counting the bytes matching `IsFirstUtf8Byte()`.
            [[nodiscard]] constexpr std::size_t NumUtf8Chars(std::string_view string)
            {
                return std::size_t(std::count_if(string.begin(), string.end(), IsFirstUtf8Byte));
            }

            // Skips whitespace characters, if any.
            constexpr void SkipWhitespace(const char *&ch)
            {
                while (IsWhitespace(*ch))
                    ch++;
            }
        }

        // Escaping/unescaping and converting strings between different encodings.
        namespace encoding
        {
            // A [?] character that's used as a fallback on some errors.
            constexpr char32_t fallback_char = 0xfffd;

            template <typename T>
            concept CharType =
                std::is_same_v<T, char> || std::is_same_v<T, wchar_t> ||
                std::is_same_v<T, char8_t> || std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;

            // Calls `f` for each character type we support. The rest of the arguments are forwarded as is.
            #define DETAIL_TA_FOR_EACH_CHAR_TYPE(f, ...) \
                f(char     __VA_OPT__(,) __VA_ARGS__) \
                f(wchar_t  __VA_OPT__(,) __VA_ARGS__) \
                f(char8_t  __VA_OPT__(,) __VA_ARGS__) \
                f(char16_t __VA_OPT__(,) __VA_ARGS__) \
                f(char32_t __VA_OPT__(,) __VA_ARGS__) \

            // Calls `f` for each combination of two character types we support. The rest of the arguments are forwarded as is.
            #define DETAIL_TA_FOR_EACH_CHAR_TYPE_2(f, ...) \
                DETAIL_TA_FOR_EACH_CHAR_TYPE(f, char     __VA_OPT__(,) __VA_ARGS__) \
                DETAIL_TA_FOR_EACH_CHAR_TYPE(f, wchar_t  __VA_OPT__(,) __VA_ARGS__) \
                DETAIL_TA_FOR_EACH_CHAR_TYPE(f, char8_t  __VA_OPT__(,) __VA_ARGS__) \
                DETAIL_TA_FOR_EACH_CHAR_TYPE(f, char16_t __VA_OPT__(,) __VA_ARGS__) \
                DETAIL_TA_FOR_EACH_CHAR_TYPE(f, char32_t __VA_OPT__(,) __VA_ARGS__) \


            namespace low
            {
                // Returns true if `ch` is larger than allowed in Unicode.
                [[nodiscard]] constexpr bool CodepointIsTooLarge(char32_t ch) {return ch > 0x10ffff;}

                // Returns true if `ch` is a high surrogate (first element of a pair).
                [[nodiscard]] constexpr bool CodepointIsHighSurrotate(char32_t ch) {return ch >= 0xd800 && ch <= 0xdbff;}
                // Returns true if `ch` is a low surrogate (second element of a pair).
                [[nodiscard]] constexpr bool CodepointIsLowSurrotate(char32_t ch) {return ch >= 0xdc00 && ch <= 0xdfff;}
                // Returns true if `ch` is either element of a surrogate pair.
                [[nodiscard]] constexpr bool CodepointIsSurrotate(char32_t ch) {return CodepointIsHighSurrotate(ch) || CodepointIsLowSurrotate(ch);}

                // Returns true if `ch` is not a valid codepoint, either because it's too large or because it's reserved for surrogate pairs.
                [[nodiscard]] constexpr bool CodepointIsInvalid(char32_t ch) {return CodepointIsTooLarge(ch) || CodepointIsSurrotate(ch);}

                // Checks the codepoint as if by `CodepointIsInvalid(ch)`. Returns the error message on failure, or null on success.
                [[nodiscard]] constexpr const char *ValidateCodepoint(char32_t ch)
                {
                    if (CodepointIsTooLarge(ch))
                        return "Invalid codepoint, larger than 0x10ffff.";
                    if (CodepointIsSurrotate(ch))
                        return "Invalid codepoint, range 0xd800-0xdfff is reserved for surrogate pairs.";
                    return nullptr;
                }

                // Encodes a single character to UTF-8 or UTF-16 or UTF-32. Gracefully recovers from failures.
                // If `encode` is true, it's a potentially multicharacter "code point". This is a good default.
                // If `encode` is false, this is a "code unit", which is directly cast to the target type.
                // In any case, performs a range check on `ch` and returns an error on failure. But also writes a fallback character in that case.
                // Returns null on success or the error message on failure. Gracefully recovers by writing `fallback_char` to the output.
                // If `ch` is not `char32_t`, it's first converted to an unsigned version of itself, and then extended to `char32_t`.
                template <CharType InChar, CharType OutChar>
                const char *EncodeOne(InChar ch, bool encode, std::basic_string<OutChar> &output);

                #define DETAIL_TA_X(T, U) extern template CFG_TA_API const char *EncodeOne(T ch, bool encode, std::basic_string<U> &output);
                DETAIL_TA_FOR_EACH_CHAR_TYPE_2(DETAIL_TA_X)
                #undef DETAIL_TA_X

                // Like `EncodeOne`, but also escapes the character, and is limited to UTF-8 output and `char32_t` input for simplicity. Never fails.
                // `quote_char` is the quote character that needs escaping, either `"` or `'`. Set this to `0` to escape both.
                // If `encode == false`, always escapes the character.
                CFG_TA_API void EncodeAndEscapeOne(char32_t ch, bool encode, char quote_char, std::string &output);

                // Decodes a single character from `source`. Returns the error message or null on success.
                // Gracefully recovers from failures, always fills `output_char` and advances the pointer.
                // When passing the result to `Encode{,AndEscape}One()`, set `encode = true` if this returned null, and to `false` if this returned an error.
                // `end` is optional. If specified, it's the end of the input string.
                template <CharType T>
                [[nodiscard]] const char *DecodeOne(const T *&source, const T *end, char32_t &output_char);

                #define DETAIL_TA_X(T) extern template CFG_TA_API const char *DecodeOne(const T *&source, const T *end, char32_t &output_char);
                DETAIL_TA_FOR_EACH_CHAR_TYPE(DETAIL_TA_X)
                #undef DETAIL_TA_X

                // Decodes and unescapes a single character or escape sequence. Returns the error message or null on success.
                // Unlike other functions above, this DOESN'T gracefully recover from failures.
                // On failure, `source` will point to the error location, but `output_char` and `output_encode` will have indeterminate values.
                // If `output_encode` is false, the `output_char` is a code unit rather than a code point,
                //   i.e. should be casted directly to the target type without encoding. See `EncodeOne()` for details.
                // This is limited to UTF-8 input for simplicity.
                [[nodiscard]] CFG_TA_API const char *DecodeAndUnescapeOne(const char *&source, const char *end, char32_t &output_char, bool &output_encode);


                // The literal prefix for character type `T`.
                template <CharType T> inline constexpr std::string_view type_prefix = {};
                template <> inline constexpr std::string_view type_prefix<wchar_t> = "L";
                template <> inline constexpr std::string_view type_prefix<char8_t> = "u8";
                template <> inline constexpr std::string_view type_prefix<char16_t> = "u";
                template <> inline constexpr std::string_view type_prefix<char32_t> = "U";

                // If `source` starts with `type_prefix<T>`, skips it and returns true. Otherwise returns false.
                template <CharType T>
                bool SkipTypePrefix(const char *&source);

                #define DETAIL_TA_X(T) extern template CFG_TA_API bool SkipTypePrefix<T>(const char *&source);
                DETAIL_TA_FOR_EACH_CHAR_TYPE(DETAIL_TA_X)
                #undef DETAIL_TA_X
            }


            // Parses a double-quoted escaped string. Returns the error on failure or null on success.
            // Can write out-of-range characters to `output` due to escapes.
            // If `allow_prefix == true`, will silently ignore the literal prefix for this character type.
            template <CharType OutChar>
            [[nodiscard]] std::string ParseQuotedString(const char *&source, bool allow_prefix, std::basic_string<OutChar> &output);

            #define DETAIL_TA_X(OutChar) extern template CFG_TA_API std::string ParseQuotedString(const char *&source, bool allow_prefix, std::basic_string<OutChar> &output);
            DETAIL_TA_FOR_EACH_CHAR_TYPE(DETAIL_TA_X)
            #undef DETAIL_TA_X

            // Parses a single-quoted escaped character. Returns the error on failure or null on success.
            // Can write an out-of-range character to `output` due to escapes.
            // If `allow_prefix == true`, will silently ignore the literal prefix for this character type.
            template <CharType OutChar>
            [[nodiscard]] std::string ParseQuotedChar(const char *&source, bool allow_prefix, OutChar &output);

            #define DETAIL_TA_X(OutChar) extern template CFG_TA_API std::string ParseQuotedChar(const char *&source, bool allow_prefix, OutChar &output);
            DETAIL_TA_FOR_EACH_CHAR_TYPE(DETAIL_TA_X)
            #undef DETAIL_TA_X

            // Appends a quoted escaped string to `output`.
            // Silently ignores encoding errors in input, and tries to escape them.
            // If `add_prefix == true`, adds the proper literal prefix for this character type.
            template <CharType InChar>
            void MakeQuotedString(std::basic_string_view<InChar> source, char quote, bool add_prefix, std::string &output);

            #define DETAIL_TA_X(InChar) extern template CFG_TA_API void MakeQuotedString(std::basic_string_view<InChar> source, char quote, bool add_prefix, std::string &output);
            DETAIL_TA_FOR_EACH_CHAR_TYPE(DETAIL_TA_X)
            #undef DETAIL_TA_X

            // Converts `source` to a different encoding, appends to `output`. Silently ignores encoding errors.
            template <CharType InChar, CharType OutChar>
            void ReencodeRelaxed(std::basic_string_view<InChar> source, std::basic_string<OutChar> &output);

            #define DETAIL_TA_X(InChar, OutChar) extern template CFG_TA_API void ReencodeRelaxed(std::basic_string_view<InChar> source, std::basic_string<OutChar> &output);
            DETAIL_TA_FOR_EACH_CHAR_TYPE_2(DETAIL_TA_X)
            #undef DETAIL_TA_X
        }

        namespace type_name_details
        {
            template <typename T>
            constexpr std::string_view RawTypeName() {return CFG_TA_THIS_FUNC_NAME;}

            // This is only valid for `T = int`. Using a template to hopefully prevent redundant calculations.
            template <typename T = int>
            constexpr std::size_t prefix_len = RawTypeName<T>().rfind("int");

            // This is only valid for `T = int`. Using a template to hopefully prevent redundant calculations.
            template <typename T = int>
            constexpr std::size_t suffix_len = RawTypeName<T>().size() - prefix_len<T> - 3;

            // On MSVC, removes `class` and other unnecessary strings from type names.
            // Returns the new length.
            // It's recommended to include the null terminator in `size`, then we also null-terminate the resulting string and include it in the resulting length.
            constexpr std::size_t CleanUpTypeName(char *buffer, std::size_t size)
            {
                #if !CFG_TA_CLEAN_UP_TYPE_NAMES
                (void)buffer;
                return size;
                #else
                std::string_view view(buffer, size); // Yes, with the null at the end.

                auto RemoveTypePrefix = [&](std::string_view to_remove)
                {
                    std::size_t region_start = 0;
                    std::size_t source_pos = 0;
                    std::size_t target_pos = 0;
                    while (true)
                    {
                        source_pos = view.find(to_remove, source_pos);
                        if (source_pos == std::string_view::npos)
                            break;
                        if (source_pos == 0 || !chars::IsIdentifierCharStrict(view[source_pos - 1]))
                        {
                            std::size_t n = source_pos - region_start;
                            std::copy_n(view.begin() + region_start, n, buffer + target_pos);
                            target_pos += n;
                            source_pos += to_remove.size();
                            region_start = source_pos;
                        }
                    }
                    std::size_t n = view.size() - region_start;
                    std::copy_n(view.begin() + region_start, n, buffer + target_pos);
                    target_pos += n;
                    view = std::string_view(view.data(), target_pos);
                };

                RemoveTypePrefix("struct ");
                RemoveTypePrefix("class ");
                RemoveTypePrefix("union ");
                RemoveTypePrefix("enum ");

                return view.size();
                #endif
            }

            template <std::size_t N>
            struct BufferAndLen
            {
                std::array<char, N> buffer;
                std::size_t len = 0;
            };

            template <typename T>
            constexpr auto storage = []{
                #if !CFG_TA_CLEAN_UP_TYPE_NAMES
                // On GCC and Clang, return the name as is.
                constexpr auto raw_name = RawTypeName<T>();
                std::array<char, raw_name.size() - prefix_len<> - suffix_len<> + 1> ret{};
                std::copy_n(raw_name.begin() + prefix_len<>, ret.size() - 1, ret.begin());
                return ret;
                #else
                // On MSVC, strip `class ` and some other junk strings.
                constexpr auto trimmed_name = []{
                    constexpr auto raw_name = RawTypeName<T>();
                    BufferAndLen<raw_name.size() - prefix_len<> - suffix_len<> + 1> ret{};
                    std::copy_n(raw_name.begin() + prefix_len<>, ret.buffer.size() - 1, ret.buffer.begin());

                    ret.len = CleanUpTypeName(ret.buffer.data(), ret.buffer.size());
                    return ret;
                }();

                std::array<char, trimmed_name.len> ret{};
                std::copy_n(trimmed_name.buffer.begin(), trimmed_name.len, ret.begin());
                return ret;
                #endif
            }();
        }

        // Returns the type name (using `__PRETTY_FUNCTION__` or `__FUNCSIG__`, depending on the compiler).
        template <typename T>
        [[nodiscard]] constexpr std::string_view TypeName()
        {
            return std::string_view(type_name_details::storage<T>.data(), type_name_details::storage<T>.size() - 1);
        }

        // Demangles output from `typeid(...).name()`.
        // It's recommended to only use it for runtime names, preferring `TypeName()` for compile-time stuff.
        class Demangler
        {
            #if CFG_TA_CXXABI_DEMANGLE && !CFG_TA_CLEAN_UP_TYPE_NAMES
            char *buf_ptr = nullptr;
            std::size_t buf_size = 0;
            #elif !CFG_TA_CXXABI_DEMANGLE && CFG_TA_CLEAN_UP_TYPE_NAMES
            std::string buf;
            #elif !CFG_TA_CXXABI_DEMANGLE && !CFG_TA_CLEAN_UP_TYPE_NAMES
            // Nothing.
            #else
            #error Invalid configuration.
            #endif

          public:
            CFG_TA_API Demangler();
            Demangler(const Demangler &) = delete;
            Demangler &operator=(const Demangler &) = delete;
            CFG_TA_API ~Demangler();

            // Demangles a name.
            // On GCC ang Clang invokes `__cxa_demangle()`, on MSVC returns the string unchanged.
            // The returned pointer remains valid as long as both the passed string and the class instance are alive.
            // Preserve the class instance between calls to potentially reuse the buffer.
            [[nodiscard]] CFG_TA_API const char *operator()(const char *name);
        };

        namespace regex
        {
            // Constructs a regex from a string.
            // Not calling the constructor directly helps with the compilation times.
            [[nodiscard]] CFG_TA_API std::regex ConstructRegex(std::string_view string);

            // Checks if the regex matches the whole string.
            // Not calling `std::regex_match()` directly helps with the compilation times.
            [[nodiscard]] CFG_TA_API bool WholeStringMatchesRegex(std::string_view str, const std::regex &regex);

            // Returns true if the test name `name` matches regex `regex`.
            // Currently this matches the whole name or any prefix ending at `/` (including or excluding `/`).
            [[nodiscard]] CFG_TA_API bool TestNameMatchesRegex(std::string_view name, const std::regex &regex);
        }
    }

    // String conversions.
    namespace string_conv
    {
        // --- MISC ---

        // This imitates `std::range_format`, except we don't deal with unescaped strings.
        enum class RangeKind
        {
            disabled, // Not a range.
            sequence, // [...]
            set, // {...}
            map, // {A: B, C: D}
            string, // "..."
        };

        // Whether `T` is a tuple or a similar class.
        // Some formatters below use this.
        template <typename T>
        concept TupleLike = requires{std::tuple_size<T>::value;}; // Note, `std::tuple_size_v` would be SFINAE-unfriendly.

        // The default value of `ClarifyTypeInMixedTypeContexts` (see below).
        // Don't specialize this, this is specialized by the library internally.
        template <typename T>
        struct DefaultClarifyTypeInMixedTypeContexts : std::false_type {};

        // If true, when printed alongside values of different types (currently only in `TA_GENERATE_PARAM(...)`), also print the type.
        // You can specialize this.
        template <typename T>
        struct ClarifyTypeInMixedTypeContexts : DefaultClarifyTypeInMixedTypeContexts<T> {};

        // All scalars look the same otherwise (arithmetic types and pointers, separately),
        // except `char`s (which are printed as single-quoted characters) and `nullptr`, which weirdly counts as a scalar,
        // and is printed as `nullptr` (because of our custom trait; `std::format` prints it as `0x0`).
        template <typename T> requires(std::is_scalar_v<T> && !std::is_same_v<T, char> && !std::is_same_v<T, std::nullptr_t>)
        struct DefaultClarifyTypeInMixedTypeContexts<T> : std::true_type {};


        // --- TO STRING ---

        // You normally shouldn't specialize this, specialize `ToStringTraits` defined below.
        // `DefaultToStringTraits` uses this for types that don't support the debug format `"{:?}"`.
        template <typename T, typename = void>
        struct DefaultFallbackToStringTraits
        {
            std::string operator()(const T &value) const
            // `std::formattable` is from C++23, and could be unavailable, so we check the formatter this way instead.
            requires std::default_initializable<CFG_TA_FMT_NAMESPACE::formatter<T, char>>
            {
                return CFG_TA_FMT_NAMESPACE::format("{}", value);
            }
        };

        // Don't specialize this, specialize `ToStringTraits` defined below.
        // `ToStringTraits` inherits from this by default.
        // This is what the library specializes internally.
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
        requires std::is_same_v<T, std::remove_cvref_t<T>> && SupportsToString<T>
        [[nodiscard]] std::string ToString(const T &value)
        {
            return ToStringTraits<std::remove_cvref_t<T>>{}(value);
        }

        // --- TO STRING SPECIALIZATIONS ---

        // A `nullptr`. We override this to print `nullptr`, rather than the default `0x0`.
        template <>
        struct DefaultToStringTraits<std::nullptr_t>
        {
            CFG_TA_API std::string operator()(std::nullptr_t) const;
        };

        // Some standard enums.
        template <>
        struct string_conv::DefaultToStringTraits<AssertFlags>
        {
            CFG_TA_API std::string operator()(AssertFlags value) const;
        };

        // Strings and characters.
        // `char`-based ones are supposed to be supported, but not all standard libraries have them yet. The other ones seem to not be supported at all.
        // We just use our own formatter for everything, just for simplicity. They should be mostly equivalent to the standard one.
        // If you decide to replace all those with `DefaultFallbackToStringTraits`, be aware that the range formatter will try to pick up strings instead,
        //   probably should disable that somehow.
        // Also libstdc++ 13 has a broken non-SFINAE-friendly `formatter<const char *>::set_debug_string()`, which causes issues,
        //   but the `std::string_view` formatter is fine, so we can just use it instead.
        template <text::encoding::CharType T>
        struct DefaultToStringTraits<T>
        {
            std::string operator()(T value) const
            {
                std::string ret;
                text::encoding::MakeQuotedString(std::basic_string_view{&value, 1}, '\'', true, ret);
                return ret;
            }
        };
        template <text::encoding::CharType T>
        struct DefaultToStringTraits<std::basic_string_view<T>>
        {
            std::string operator()(std::basic_string_view<T> value) const
            {
                std::string ret;
                text::encoding::MakeQuotedString(value, '"', true, ret);
                return ret;
            }
        };
        template <text::encoding::CharType T, typename ...P>
        struct DefaultToStringTraits<std::basic_string<T, P...>> : DefaultToStringTraits<std::basic_string_view<T>> {};
        template <text::encoding::CharType T>
        struct DefaultToStringTraits<T *> : DefaultToStringTraits<std::basic_string_view<T>> {};
        template <text::encoding::CharType T>
        struct DefaultToStringTraits<const T *> : DefaultToStringTraits<std::basic_string_view<T>> {};
        // This catches const arrays too.s
        template <text::encoding::CharType T, std::size_t N>
        struct DefaultToStringTraits<T[N]> : DefaultToStringTraits<std::basic_string_view<T>> {};

        // std::filesystem::path:
        // It seems `std::format` doesn't support it, but `libfmt` does. It's easier to just provide a formatter unconditionally.
        // If we decide to use libfmt's formatter, must test its behavior on unicode characters, as e.g. MSVC STL would throw in `.string()` in default locale.
        template <> struct DefaultToStringTraits<std::filesystem::path> {CFG_TA_API std::string operator()(const std::filesystem::path &path) const;};


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

        // Ranges:

        // This wraps `std::format_kind` (or `fmt::range_format_kind`), or implements it from scratch if the formatting library doesn't understand ranges.
        // You can specialize this.
        // This classifier never returns `RangeKind::string`, just like the standard one.
        // NOTE: The standard (and libfmt) versions of this variable return junk for `std::string[_view]`, so ours does too, and we never use it on those types.
        template <typename T>
        constexpr RangeKind range_format_kind =
        #if CFG_TA_FMT_HAS_RANGE_FORMATTING
        []{
            if constexpr (!std::ranges::input_range<T>)
            {
                return RangeKind::disabled;
            }
            else
            {
                #if CFG_TA_FMT_HAS_RANGE_FORMATTING == 1
                auto value = CFG_TA_FMT_NAMESPACE::format_kind<T>;
                #elif CFG_TA_FMT_HAS_RANGE_FORMATTING == 2
                auto value = CFG_TA_FMT_NAMESPACE::range_format_kind<T, char>::value;
                #else
                #error Invalid `CFG_TA_FMT_HAS_RANGE_FORMATTING` value.
                #endif
                return
                    value == CFG_TA_FMT_NAMESPACE::range_format::sequence ? RangeKind::sequence :
                    value == CFG_TA_FMT_NAMESPACE::range_format::set      ? RangeKind::set :
                    value == CFG_TA_FMT_NAMESPACE::range_format::map      ? RangeKind::map :
                    value == CFG_TA_FMT_NAMESPACE::range_format::string || value == CFG_TA_FMT_NAMESPACE::range_format::debug_string ? RangeKind::string :
                    RangeKind::disabled;
            }
        }();
        #else
        []{
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
                if constexpr (requires{typename T::mapped_type; requires std::tuple_size<Elem>::value == 2;})
                    return RangeKind::map;
                else
                    return RangeKind::set;
            }
            else
            {
                return RangeKind::sequence;
            }
        }();
        #endif

        #if !CFG_TA_FMT_ALLOW_NATIVE_RANGE_FORMATTING || !CFG_TA_FMT_HAS_RANGE_FORMATTING
        // Range formatter.
        template <typename T>
        requires
            (range_format_kind<T> != RangeKind::disabled) &&
            SupportsToString<std::ranges::range_value_t<T>> &&
            std::is_same_v<std::remove_cvref_t<std::ranges::range_reference_t<T>>, std::ranges::range_value_t<T>>
        struct DefaultToStringTraits<T>
        {
            std::string operator()(const T &value) const
            {
                if constexpr (range_format_kind<T> == RangeKind::string)
                {
                    static_assert(text::encoding::CharType<std::ranges::range_value_t<T>>, "Range printing mode is set to `string`, but it doesn't contain characters.");

                    if constexpr (
                        (
                            std::is_same_v<std::ranges::range_reference_t<T>, std::ranges::range_value_t<T> &> ||
                            std::is_same_v<std::ranges::range_reference_t<T>, const std::ranges::range_value_t<T> &>
                        ) &&
                        std::contiguous_iterator<std::ranges::iterator_t<T>>
                    )
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
                            using std::get;
                            ret += (ToString)(get<0>(elem));
                            ret += ": ";
                            ret += (ToString)(get<1>(elem));
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

        // Tuple formatter.
        // This reject `std::array`, that goes to the range formatter. I'd prefer `(...)`, but `std::format` (and libfmt) uses `[...]` here.
        template <TupleLike T>
        requires
            (range_format_kind<T> == RangeKind::disabled) &&
            ([]<std::size_t ...I>(std::index_sequence<I...>){
                return (SupportsToString<std::tuple_element_t<I, T>> && ...);
            }(std::make_index_sequence<std::tuple_size_v<T>>{}))
        struct DefaultToStringTraits<T>
        {
            std::string operator()(const T &value) const
            {
                using std::get;
                std::string ret = "(";
                [&]<std::size_t ...I>(std::index_sequence<I...>){
                    ([&]{
                        if constexpr (I > 0)
                            ret += ", ";
                        ret += (ToString)(get<I>(value));
                    }(), ...);
                }(std::make_index_sequence<std::tuple_size_v<T>>{});
                ret += ")";
                return ret;
            }
        };
        #endif

        // std::optional:

        // `std::format` doesn't support it, but libfmt prints it as `none` or `optional(42)`. This looks good to me, so I also use this format.
        // Reimplementing it here unconditionally, just in case libfmt decides to change the format in the future.

        template <typename T>
        struct ToStringTraits<std::optional<T>>
        {
            std::string operator()(const std::optional<T> &value) const
            {
                if (value)
                    return "optional(" + (ToString)(*value) + ")";
                else
                    return "none";
            }
        };

        // std::variant:

        // `std::format` doesn't support it, and libfmt has a weird format prone to ambiguities (`variant(value)`).

        namespace string_conv_detail
        {
            static constexpr std::string_view variant_valueless_by_exception = "valueless_by_exception";

            // Returns the type name for `I`th element of `std::variant<P...>`.
            // Disambiguates the names for duplicate types with a `#i` suffix, where `i` is `.index()`.
            template <std::size_t I, typename ...P>
            std::string VariantElemTypeName()
            {
                using type = std::variant_alternative_t<I, std::variant<P...>>;
                std::string ret(text::TypeName<type>());

                constexpr bool ambiguous_type = (std::is_same_v<type, P> + ...) > 1;
                if constexpr (ambiguous_type)
                {
                    ret += '#';
                    ret += std::to_string(I);
                }
                return ret;
            }
        }

        template <typename ...P>
        struct ToStringTraits<std::variant<P...>>
        {
            std::string operator()(const std::variant<P...> &value) const
            {
                if (value.valueless_by_exception())
                {
                    return std::string(string_conv_detail::variant_valueless_by_exception);
                }
                else
                {
                    static const auto names = []<std::size_t ...I>(std::index_sequence<I...>){
                        return std::array{string_conv_detail::VariantElemTypeName<I, P...>()...};
                    }(std::make_index_sequence<sizeof...(P)>{});
                    return "(" + names[value.index()] + ")" +
                        std::visit([](const auto &elem){return (ToString)(elem);}, value);
                }
            }
        };


        // --- LAZY TO STRING ---

        // Normally `$[...]` immediately converts the argument to a string and saves it, in case the assertion fails later and we need to print the value.
        // But this is often inefficient, so we provide an alternative behavior, where `$[...]` copies its argument itself,
        //   and then later converts to a string if necessary.
        // This is enabled by default for scalars, some trivial types and some others, but you can manually
        //   opt your classes into this by specializing `CopyForLazyStringConversion` (see below).
        // You can also save some OTHER state instead of copying, see below.

        // Don't specialize this, and don't use this directly. This is specialized internally by the library.
        // Specialize `MaybeLazyToString` instead.
        template <typename T>
        struct DefaultMaybeLazyToString
        {
            // Returns an object that will be converted to a string later.
            // If there's no function, the `ToString()` is instead called immediately.
            // U operator()(const T &source) const;
        };

        // Use this. You can specialize this for your types.
        template <typename T>
        struct MaybeLazyToString : DefaultMaybeLazyToString<T> {};

        // Whether this types specializes `MaybeLazyToString` to enable some form of lazy to-string conversion.
        // Note that the type must additionally match the `CFG_TA_ARG_STORAGE_SIZE` and `CFG_TA_ARG_STORAGE_ALIGNMENT` requirements, otherwise this is ignored.
        template <typename T>
        concept SupportsLazyToString = requires(MaybeLazyToString<std::remove_cvref_t<T>> trait, const std::remove_cvref_t<T> &value)
        {
            trait(value);
        };

        // Default specializations:

        // -- Copying the whole object, such as a simple scalar.

        // Don't specialize this, specialize `CopyForLazyStringConversion`.
        template <typename T> struct DefaultCopyForLazyStringConversion : std::false_type {};
        // You can specialize this. If true, those types will not be converted to a string by a
        template <typename T> struct CopyForLazyStringConversion : DefaultCopyForLazyStringConversion<T> {};

        template <typename T>
            requires(
                std::is_trivially_copy_constructible_v<T> &&
                std::is_trivially_move_constructible_v<T> &&
                std::is_trivially_copy_assignable_v<T> &&
                std::is_trivially_move_assignable_v<T> &&
                std::is_trivially_destructible_v<T>
            )
        struct DefaultCopyForLazyStringConversion<T> : std::true_type {};

        template <typename T> requires CopyForLazyStringConversion<T>::value
        struct DefaultMaybeLazyToString<T>
        {
            T operator()(const T &source) const {return source;}
        };

        // -- Copying as a string.

        // Copies a string-like object into a string.
        // This is a bit questionable, but is surely faster on the happy path?
        template <typename ...P>
        struct DefaultMaybeLazyToString<std::basic_string<P...>>
        {
            std::basic_string<P...> operator()(const std::basic_string<P...> &source) const {return source;}
        };
        template <typename ...P>
        struct DefaultMaybeLazyToString<std::basic_string_view<P...>>
        {
            std::basic_string<P...> operator()(const std::basic_string_view<P...> &source) const {return std::basic_string<P...>(source);}
        };


        // --- FROM STRING ---

        template <typename T>
        concept ScalarConvertibleFromString =
            (std::is_integral_v<T> && sizeof(T) <= sizeof(long long)) ||
            (std::is_floating_point_v<T> && sizeof(T) <= sizeof(long double));

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

        // Whether `FromString()` works on `T`, assuming it's already constructed somehow. `T` must be cvref-unqualified.
        template <typename T>
        concept SupportsFromStringWeak =
            std::is_same_v<T, std::remove_cvref_t<T>> &&
            requires(T &target, const char *&string)
            {
                { FromStringTraits<T>{}(target, string) } -> std::same_as<std::string>;
            };

        // Whether `FromString()` works on `T`. `T` must be default-constructible and cvref-unqualified.
        template <typename T>
        concept SupportsFromString = SupportsFromStringWeak<T> && std::default_initializable<T>;


        // --- FROM STRING SPECIALIZATIONS ---

        // Scalars.
        template <ScalarConvertibleFromString T>
        requires(!text::encoding::CharType<T>) // Reject character types, they are handled separately.
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

        // Null pointer.
        template <>
        struct DefaultFromStringTraits<std::nullptr_t>
        {
            [[nodiscard]] CFG_TA_API std::string operator()(std::nullptr_t &target, const char *&string) const;
        };

        // Strings and characters.
        template <text::encoding::CharType T>
        struct DefaultFromStringTraits<T>
        {
            [[nodiscard]] std::string operator()(T &target, const char *&string) const
            {
                return text::encoding::ParseQuotedChar(string, true, target);
            }
        };
        template <text::encoding::CharType T, typename ...P>
        struct DefaultFromStringTraits<std::basic_string<T, P...>>
        {
            [[nodiscard]] std::string operator()(std::basic_string<T, P...> &target, const char *&string) const
            {
                return text::encoding::ParseQuotedString(string, true, target);
            }
        };

        // std::filesystem::path
        template <>
        struct DefaultFromStringTraits<std::filesystem::path>
        {
            [[nodiscard]] CFG_TA_API std::string operator()(std::filesystem::path &target, const char *&string) const;
        };

        // Ranges.

        // Classifies the range for converting from a string. You almost never want to specialize this.
        // Normally you want to specialize `std::format_kind` or `fmt::range_format_kind` or our `ta_test::string_conv::range_format_kind`.
        template <typename T>
        constexpr RangeKind from_string_range_format_kind = range_format_kind<T>;

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
        concept RangeSupportingFromStringWeak =
            from_string_range_format_kind<T> != RangeKind::disabled &&
            SupportsFromStringWeak<typename AdjustRangeElemToConvertFromString<std::ranges::range_value_t<T>>::type>;
        // Whether we can `.emplace_back()` to this range.
        // This is nice, because we can insert first, and then operate on a reference.
        template <typename T>
        concept RangeSupportingEmplaceBack = RangeSupportingFromStringWeak<T> &&
            std::is_same_v<typename AdjustRangeElemToConvertFromString<T>::type, T> &&
            requires(T &target, std::ranges::range_value_t<T> &&value) {target.emplace_back() = std::move(value);};
        // Whether we can `.push_back()` to this range. Unsure what container would actually need this over `emplace_back()`, perhaps something non-standard?
        template <typename T>
        concept RangeSupportingPushBack = RangeSupportingFromStringWeak<T> &&
            std::default_initializable<typename AdjustRangeElemToConvertFromString<std::ranges::range_value_t<T>>::type> &&
            requires(T &target, typename AdjustRangeElemToConvertFromString<std::ranges::range_value_t<T>>::type &&e){target.push_back(std::move(e));};
        // Whether we can `.insert()` to this range.
        template <typename T>
        concept RangeSupportingInsert = RangeSupportingFromStringWeak<T> &&
            std::default_initializable<typename AdjustRangeElemToConvertFromString<std::ranges::range_value_t<T>>::type> &&
            requires(T &target, typename AdjustRangeElemToConvertFromString<std::ranges::range_value_t<T>>::type &&e){requires std::is_same_v<decltype(target.insert(std::move(e)).second), bool>;};
        // Whether this is a fixed-size tuple-like range.
        template <typename T>
        concept RangeSupportingFromStringAsFixedSize = RangeSupportingFromStringWeak<T> && (std::is_array_v<T> || TupleLike<T>);
        // Whether this range can be converted from a string.
        template <typename T>
        concept RangeSupportingFromString = RangeSupportingEmplaceBack<T> || RangeSupportingPushBack<T> || RangeSupportingInsert<T> || RangeSupportingFromStringAsFixedSize<T>;

        // The actual code for ranges. Escaped strings are handled here too.
        template <RangeSupportingFromString T>
        struct DefaultFromStringTraits<T>
        {
            [[nodiscard]] std::string operator()(T &target, const char *&string) const
            {
                if constexpr (from_string_range_format_kind<T> == RangeKind::string)
                {
                    static_assert(text::encoding::CharType<std::ranges::range_value_t<T>>, "Range deserialization mode is set to `string`, but it doesn't contain characters.");

                    std::basic_string<std::ranges::range_value_t<T>> buf;
                    const char *const old_string = string;
                    if (std::string error = FromStringTraits<decltype(buf)>{}(buf, string); !error.empty())
                        return error;

                    if constexpr (RangeSupportingFromStringAsFixedSize<T>)
                    {
                        if (buf.size() != target.size())
                        {
                            string = old_string;
                            return CFG_TA_FMT_NAMESPACE::format("Wrong string size, got {} but expected exactly {}.", buf.size(), target.size());
                        }
                    }

                    if constexpr (requires{target.reserve(std::size_t{});})
                        target.reserve(buf.size());

                    std::size_t i = 0;

                    // Insert the elements.
                    for (auto ch : buf)
                    {
                        if constexpr (RangeSupportingFromStringAsFixedSize<T>)
                        {
                            target[i++] = ch;
                        }
                        else if constexpr (RangeSupportingInsert<T>)
                        {
                            if (!target.insert(ch).second)
                            {
                                string = old_string;
                                return "Duplicate set element.";
                            }
                        }
                        else if constexpr (RangeSupportingPushBack<T>)
                        {
                            target.push_back(ch);
                        }
                        else if constexpr (RangeSupportingEmplaceBack<T>)
                        {
                            target.emplace_back() = ch; // This is probably useless.
                        }
                        else
                        {
                            static_assert(meta::AlwaysFalse<T>::value, "Internal error: Don't know how to append to this string-like container.");
                        }
                    }
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

                    std::size_t index = 0;

                    auto ProcessOneElement = [&] CFG_TA_NODISCARD_LAMBDA (bool *stop) -> std::string
                    {
                        text::chars::SkipWhitespace(string);

                        // Stop on closing brace.
                        if constexpr (!RangeSupportingFromStringAsFixedSize<T>)
                        {
                            if (*string == brace_close)
                            {
                                string++;
                                *stop = true;
                                return "";
                            }
                        }

                        // Consume comma.
                        if (index != 0)
                        {
                            if (*string == ',')
                                string++;
                            else
                                return RangeSupportingFromStringAsFixedSize<T> ? "Expected `,`." : CFG_TA_FMT_NAMESPACE::format("Expected `,` or closing `{}`.", brace_close);

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

                                using std::get;

                                std::string error = FromStringTraits<std::tuple_element_t<0, Elem>>{}(get<0>(target), string);
                                if (!error.empty())
                                    return error;
                                text::chars::SkipWhitespace(string);

                                if (*string != ':')
                                    return "Expected `:` after the key.";
                                string++;

                                text::chars::SkipWhitespace(string);
                                return FromStringTraits<std::tuple_element_t<1, Elem>>{}(get<1>(target), string);
                            }
                            else
                            {
                                return FromStringTraits<Elem>{}(target, string);
                            }
                        };

                        // Insert the element.
                        if constexpr (RangeSupportingFromStringAsFixedSize<T>)
                        {
                            std::string error = ConsumeElem(target[index]);
                            if (!error.empty())
                                return error;
                        }
                        else if constexpr (RangeSupportingInsert<T>)
                        {
                            const char *old_string = string;
                            Elem elem{};
                            std::string error = ConsumeElem(elem);
                            if (!error.empty())
                                return error;
                            if (!target.insert(std::move(elem)).second)
                            {
                                string = old_string;
                                return from_string_range_format_kind<T> == RangeKind::map ? "Duplicate key." : "Duplicate set element.";
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

                        index++;
                        return "";
                    };

                    if constexpr (RangeSupportingFromStringAsFixedSize<T>)
                    {
                        while (index < std::size(target))
                        {
                            if (std::string error = ProcessOneElement(nullptr); !error.empty())
                                return error;
                        }
                    }
                    else
                    {
                        while (true)
                        {
                            bool stop = false;
                            if (std::string error = ProcessOneElement(&stop); !error.empty())
                                return error;
                            if (stop)
                                break;
                        }
                    }

                    // Check the closing brace for fixed-size ranges.
                    if constexpr (RangeSupportingFromStringAsFixedSize<T>)
                    {
                        text::chars::SkipWhitespace(string);
                        if (*string != brace_close)
                            return CFG_TA_FMT_NAMESPACE::format("Expected closing `{}`.", brace_close);
                        string++;
                    }
                }

                return "";
            }
        };

        // Tuples.

        template <TupleLike T>
        requires
            (!RangeSupportingFromStringAsFixedSize<T>) &&
            ([]<std::size_t ...I>(std::index_sequence<I...>){
                return (SupportsFromStringWeak<std::tuple_element_t<I, T>> && ...);
            }(std::make_index_sequence<std::tuple_size_v<T>>{}))
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

                        using std::get;

                        error = FromStringTraits<std::tuple_element_t<I, T>>{}(get<I>(target), string);
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

        // std::optional

        template <SupportsFromString T>
        struct DefaultFromStringTraits<std::optional<T>>
        {
            [[nodiscard]] std::string operator()(std::optional<T> &target, const char *&string) const
            {
                // Using manual comparisons because it's unclear
                if (string[0] == 'n' &&
                    string[1] == 'o' &&
                    string[2] == 'n' &&
                    string[3] == 'e'
                )
                {
                    string += 4;
                    target = std::nullopt;
                    return "";
                }
                else if (
                    string[0] == 'o' &&
                    string[1] == 'p' &&
                    string[2] == 't' &&
                    string[3] == 'i' &&
                    string[4] == 'o' &&
                    string[5] == 'n' &&
                    string[6] == 'a' &&
                    string[7] == 'l'
                )
                {
                    string += 8;
                    text::chars::SkipWhitespace(string);
                    if (*string != '(')
                        return "Expected opening `(`.";
                    string++;
                    text::chars::SkipWhitespace(string);
                    std::string error = FromStringTraits<T>{}(target.emplace(), string);
                    if (!error.empty())
                        return error;
                    text::chars::SkipWhitespace(string);
                    if (*string != ')')
                        return "Expected closing `)`.";
                    string++;
                    return "";
                }
                else
                {
                    return "Expected `none` or `optional(...)`.";
                }
            }
        };

        template <SupportsFromString ...P>
        struct DefaultFromStringTraits<std::variant<P...>>
        {
            [[nodiscard]] std::string operator()(std::variant<P...> &target, const char *&string) const
            {
                if (std::strncmp(string, string_conv_detail::variant_valueless_by_exception.data(), string_conv_detail::variant_valueless_by_exception.size()) == 0)
                {
                    // string += string_conv_detail::variant_valueless_by_exception.size();
                    // Supporting this would require directly modifying the variant's memory? Everything else is likely not viable.
                    return "Deserializing `valueless_by_exception` variants is currently not supported.";
                }

                if (*string != '(')
                    return "Expected opening `(` before the variant type.";
                string++;
                text::chars::SkipWhitespace(string);

                // Prepare a list of names. Sort by decreasing length, to give longer strings priority.
                static const auto names = []{
                    auto ret = []<std::size_t ...I>(std::index_sequence<I...>){
                        return std::array{std::pair(string_conv_detail::VariantElemTypeName<I, P...>(), I)...};
                    }(std::make_index_sequence<sizeof...(P)>{});
                    std::sort(ret.begin(), ret.end(), [](const auto &a, const auto &b){return a.first.size() > b.first.size();});
                    return ret;
                }();

                auto iter = std::find_if(names.begin(), names.end(), [&](const auto &elem)
                {
                    return std::strncmp(string, elem.first.data(), elem.first.size()) == 0;
                });
                if (iter == names.end())
                {
                    std::string ret = "The variant type must be one of: ";
                    [&]<std::size_t ...I>(std::index_sequence<I...>){
                        ([&]{
                            if constexpr (I > 0)
                                ret += ", ";
                            ret += '`';
                            ret += string_conv_detail::VariantElemTypeName<I, P...>();
                            ret += '`';
                        }(), ...);
                    }(std::make_index_sequence<sizeof...(P)>{});
                    ret += ".";
                    return ret;
                }
                string += iter->first.size();
                text::chars::SkipWhitespace(string);

                if (*string != ')')
                    return "Expected closing `)` after the variant type.";
                string++;

                text::chars::SkipWhitespace(string);

                static constexpr auto funcs = []<std::size_t ...I>(std::index_sequence<I...>){
                    return std::array{
                        +[](std::variant<P...> &target, const char *&string) -> std::string
                        {
                            return FromStringTraits<P>{}(target.template emplace<I>(), string);
                        }...
                    };
                }(std::make_index_sequence<sizeof...(P)>{});

                return funcs[iter->second](target, string);
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
        // This can be used as a standalone scope guard, or as a base class.
        class FrameGuard
        {
            const BasicFrame *frame_ptr = nullptr;

          public:
            // Stores a frame pointer in the stack.
            // Can pass a null pointer here, then we do nothing.
            // The pointer can be non-owning. Then make sure it doesn't dangle!
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

        // A single text message in the log (either scoped or unscoped).
        class LogMessage
        {
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

            LogMessage() {}

            // A fixed message.
            LogMessage(std::string &&message)
                : message(std::move(message))
            {
                FixMessage();
            }
            // A generated message. Doesn't own the function.
            template <typename F>
            LogMessage(const F &generate_message)
            requires requires{generate_message();}
                : message_refresh_func([](const void *data)
                {
                    return (*static_cast<const F *>(data))();
                }),
                message_refresh_data(&generate_message)
            {}

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

        // A single logged source location.
        struct LogSourceLoc
        {
            SourceLoc loc;
            // The function name where the `TA_CONTEXT` call appears, regardless of what source location was passed to it.
            // Optional.
            std::string_view callee;
        };

        // A single log entry.
        struct LogEntry
        {
            std::size_t incremental_id = 0;
            std::variant<LogMessage, LogSourceLoc> var;
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
    // Normally runs the callback at least once. The last element may or may not have `IsTypeKnown() == false`, and other elements will always be known.
    // If `e` is null, does nothing (doesn't not run the callback).
    CFG_TA_API void AnalyzeException(const std::exception_ptr &e, const std::function<void(SingleException elem)> &func);


    // --- DATA TYPES ---

    // Various runtime data types. Should only be useful if you're writing custom modules.
    namespace data
    {
        // A compile-time description of a single `TA_TEST(...)`.
        struct BasicTest
        {
          protected:
            ~BasicTest() = default;

          public:
            // The name passed to the test macro.
            [[nodiscard]] virtual std::string_view Name() const = 0;

            // The optional flags passed to the test macro.
            [[nodiscard]] virtual TestFlags Flags() const = 0;

            // Where the test was declared.
            [[nodiscard]] virtual SourceLoc SourceLocation() const = 0;
        };

        // Information about starting a list of tests.
        struct RunTestsInfo
        {
            // Mostly for internal use. Used to call certain functions on every module.
            const ModuleLists *modules = nullptr;

            // The number of tests to run.
            std::size_t num_tests = 0;
            // The total number of known tests, including the skipped ones.
            std::size_t num_tests_with_skipped = 0;
        };
        // Information about a list of tests that's currently running.
        struct RunTestsProgress : RunTestsInfo
        {
            std::vector<const BasicTest *> failed_tests;

            // This counts total checks, no matter if failed or not: TA_CHECK, TA_MUST_THROW, FAIL.
            std::size_t num_checks_total = 0;
            // This counts only the failed checks.
            std::size_t num_checks_failed = 0;

            // How many tests ran in total, counting each generator repetition separately.
            std::size_t num_tests_with_repetitions_total = 0;
            // How many tests failed, counting each generator repetition separately.
            std::size_t num_tests_with_repetitions_failed = 0;
        };
        // Information about a finished list of tests.
        struct RunTestsResults : RunTestsProgress {};

        // Static information about the expression argument of `TA_CHECK(...)`.
        struct AssertionExprStaticInfo
        {
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
                // This should be automatically true for all arguments with nested arguments inside of them.
                bool need_bracket = false;
            };

            // The exact code passed to the assertion macro, as a string. Before macro expansion.
            std::string_view expr;

            // Information about each argument.
            std::vector<ArgInfo> args_info;
            // Indices of the arguments (0..N-1), sorted in the preferred draw order. The size of this matches `ArgsInfo().size()`.
            std::vector<std::size_t> args_in_draw_order;

          protected:
            AssertionExprStaticInfo() = default;
        };
        // Dynamic runtime information about the expression argument of `TA_CHECK(...)`.
        class AssertionExprDynamicInfo
        {
          public:
            // The current runtime state of the argument.
            enum class ArgState
            {
                not_started, // No value yet.
                in_progress, // Started calculating, but no value yet.
                done, // Has value.
            };

            const AssertionExprStaticInfo *static_info = nullptr;

            // The current state of an argument.
            // Causes a hard error if the index is out of range.
            [[nodiscard]] CFG_TA_API ArgState CurrentArgState(std::size_t index) const;
            // Returns the string representation of an argument.
            // Causes a hard error if the index is out of range, if the argument state isn't equal to `done`.
            // For some types this is lazy, and computes the string the first time it's called.
            [[nodiscard]] CFG_TA_API const std::string &CurrentArgValue(std::size_t index) const;

          protected:
            // Checks that the argument index is correct. Fails with a hard error if not.
            // Also validates `arg_buffers_pos` and `arg_metadata_offset`.
            CFG_TA_API void ValidateArgIndex(std::size_t index) const;

            // An index into `ThreadState().assertion_argument_buffers`.
            std::size_t arg_buffers_pos = 0;
            // An offset into `ThreadState().assertion_argument_metadata`.
            std::size_t arg_metadata_offset = 0;
        };
        // Information about a single `TA_CHECK(...)` call, both compile-time and runtime.
        // This is separated from `AssertionExprDynamicInfo` to theoretically allow multi-argument assertions,
        //   custom assertions with extra decorations around the call, etc.
        struct BasicAssertion : public context::BasicFrame
        {
            // You can set this to true to trigger a breakpoint.
            mutable bool should_break = false;

            // The enclosing assertion, if any.
            const BasicAssertion *enclosing_assertion = nullptr;

            // The assertion macro name, e.g. `TA_CHECK`.
            std::string_view macro_name;

            // Where the assertion is located in the source.
            // On failure this can be overridden to point somewhere else.
            [[nodiscard]] virtual const SourceLoc &SourceLocation() const = 0;

            // Returns the user message. Until the assertion fails, this is always empty.
            [[nodiscard]] virtual std::optional<std::string_view> UserMessage() const = 0;

            // The assertion is printed as a sequence of the elements below:

            // A fixed string, such as the assertion macro name itself, or its call parentheses.
            struct DecoFixedString {std::string_view string;};
            // An expression that should be printed with syntax highlighting.
            struct DecoExpr {std::string_view string;};
            // An expression with syntax highlighting and argument values. More than one per assertion weren't tested.
            struct DecoExprWithArgs {const AssertionExprDynamicInfo *expr = nullptr;};
            // `std::monostate` indicates that there is no more elements.
            using DecoVar = std::variant<std::monostate, DecoFixedString, DecoExpr, DecoExprWithArgs>;
            // Returns one of the elements to be printed.
            [[nodiscard]] virtual DecoVar GetElement(int index) const = 0;
        };

        // A compile-time information about a single `TA_MUST_THROW(...)` call.
        struct MustThrowStaticInfo
        {
            // The source location of `TA_MUST_THROW(...)`.
            SourceLoc loc;
            // The macro name used, e.g. `TA_MUST_THROW`.
            std::string_view macro_name;
            // The spelling of the macro argument.
            std::string_view expr;
        };
        // A runtime information about a single `TA_MUST_THROW(...)` call
        struct MustThrowDynamicInfo
        {
          protected:
            ~MustThrowDynamicInfo() = default;

          public:
            // This is set only if the exception is missing.
            virtual std::optional<std::string_view> UserMessage() const = 0;
        };
        // This in the context stack means that a `TA_MUST_THROW(...)` is currently executing.
        struct MustThrowInfo : context::BasicFrame
        {
            // You can set this to true to trigger a breakpoint.
            mutable bool should_break = false;

            // Never null.
            const MustThrowStaticInfo *static_info = nullptr;
            // Never null.
            const MustThrowDynamicInfo *dynamic_info = nullptr;
        };
        // This is the state stored in a `CaughtException`.
        struct CaughtExceptionInfo
        {
            std::vector<SingleException> elems;
            // Never null.
            const MustThrowStaticInfo *static_info = nullptr;
            // This is only available until the end of the full expression where `TA_MUST_THROW(...)` was initially executed.
            std::weak_ptr<const MustThrowDynamicInfo> dynamic_info;
        };
        // This in the context stack means that we're currently checking one or more elements of a `CaughtException` returned from `TA_MUST_THROW(...)`.
        struct CaughtExceptionContext : context::BasicFrame, context::FrameGuard
        {
            std::shared_ptr<const CaughtExceptionInfo> state;

            // Either the index into `caught_exception->elems` or `-1` if none.
            int active_elem = -1;

            // For internal use.
            // `state` can be null.
            // `active_elem` is either -1 or an index into `state->elems`.
            // `flags` affect how we check the correctness of `active_elem` (on soft failure a null instance is constructed).
            CFG_TA_API CaughtExceptionContext(std::shared_ptr<const CaughtExceptionInfo> state, int active_elem, AssertFlags flags, SourceLoc source_loc);
        };

        // Describes a generator created with `TA_GENERATE(...)`.
        struct BasicGenerator
        {
            friend detail::GenerateValueHelper;
            friend detail::SpecificGeneratorGenerateGuard;

            BasicGenerator() = default;
            BasicGenerator(const BasicGenerator &) = delete;
            BasicGenerator &operator=(const BasicGenerator &) = delete;
            virtual ~BasicGenerator() = default;

            // The source location.
            [[nodiscard]] virtual const SourceLocWithCounter &SourceLocation() const = 0;
            // The identifier passed to `TA_GENERATE(...)`.
            [[nodiscard]] virtual std::string_view Name() const = 0;

            // The return type. But without cvref-qualifiers, since `std::type_index` doesn't support those.
            [[nodiscard]] virtual std::type_index Type() const = 0;
            // Returns the name of the return type. Unlike `Type()` this properly reports cvref-qualifiers.
            [[nodiscard]] virtual std::string_view TypeName() const = 0;

            // The generator flags.
            [[nodiscard]] virtual GeneratorFlags Flags() const = 0;

            // Whether the last generated value is the last one for this generator.
            // Note that when the generator is operating under a module override, the system doesn't respect this variable (see below).
            [[nodiscard]] bool IsLastValue() const {return !repeat || callback_threw_exception || bool(Flags() & GeneratorFlags::generate_nothing);}

            // This is false when the generator is reached for the first time and didn't generate a value yet.
            // Calling `GetValue()` in that case will crash.
            [[nodiscard]] virtual bool HasValue() const = 0;

            // Returns true if the user callback threw an exception. This implies `IsLastValue()`.
            [[nodiscard]] bool CallbackThrewException() const {return callback_threw_exception;}

            // Whether `string_conv::ToString()` works for this generated type.
            [[nodiscard]] virtual bool ValueConvertibleToString() const = 0;
            // Converts the current `GetValue()` to a string, or returns an empty string if `ValueConvertibleToString()` is false.
            // Adding `noexcept` here for general sanity.
            [[nodiscard]] virtual std::string ValueToString() const noexcept = 0;

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

            // Returns the module that's currently controlling this generator, if any.
            [[nodiscard]] const BasicModule *OverridingModule() const {return overriding_module;}

            // Inserting custom values:

            // Whether the value type can be created from a string using `string_conv::FromStringTraits`.
            [[nodiscard]] virtual bool ValueConvertibleFromString() const = 0;

            // Replaces the current value with the one parsed from the string.
            // Returns the error message on failure, or an empty string on success.
            // Advances `string` to the next unused character, or to the failure position on error.
            // If `ValueConvertibleFromString() == false`, will always return an error.
            // On failure, can corrupt the current object. You should probably abort everything in that case.
            // Adding `noexcept` here for general sanity.
            [[nodiscard]] virtual std::string ReplaceValueFromString(const char *&string) noexcept = 0;

            // Returns true if the type has overloaded `==` and `ValueConvertibleFromString() == true`.
            [[nodiscard]] virtual bool ValueEqualityComparableToString() const = 0;
            // Parses the value from a string, then compares it with the current value using `==`, writing the result to `equal`.
            // Returns the parsing error if any. Also returns an error if `ValueEqualityComparableToString()` is false.
            // Writes `equal = false` on any failure, including if the generator holds no value.
            // Adding `noexcept` here for general sanity.
            [[nodiscard]] virtual std::string ValueEqualsToString(const char *&string, bool &equal) const noexcept = 0;

          protected:
            // `Generate()` updates this to indicate whether more values will follow.
            bool repeat = true;

            // This is set to true when the callback throws. Then the generator and all next ones will get pruned at the end of the test.
            bool callback_threw_exception = false;

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
            [[nodiscard]] std::type_index Type() const override final
            {
                return typeid(ReturnType);
            }

            [[nodiscard]] std::string_view TypeName() const override final
            {
                return text::TypeName<ReturnType>();
            }

            [[nodiscard]] bool HasValue() const override final
            {
                return storage.index() == 1 || storage.index() == 2;
            }

            [[nodiscard]] bool ValueConvertibleToString() const override final
            {
                return string_conv::SupportsToString<ReturnType>;
            }

            [[nodiscard]] std::string ValueToString() const noexcept override final
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

            [[nodiscard]] std::string ReplaceValueFromString(const char *&string) noexcept override final
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

            [[nodiscard]] virtual std::string ValueEqualsToString(const char *&string, bool &equal) const noexcept override
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

        // Information about starting a single test (possibly one of the generated repetitions).
        struct RunSingleTestInfo
        {
            const RunTestsProgress *all_tests = nullptr;
            const BasicTest *test = nullptr;

            // True when entering the test for the first time, as opposed to repeating it because of a generator.
            // This is set to `generator_stack.empty()` when entering the test.
            bool is_first_generator_repetition = false;
        };
        // Information about a single test that's currently running (possibly one of the generated repetitions).
        struct RunSingleTestProgress : RunSingleTestInfo
        {
            // You can set this to true to break after the test.
            mutable bool should_break = false;

            // Whether the current test has failed.
            // When generators are involved, this refers only to the current repetition.
            bool failed = false;

            // The generator stack.
            // This starts empty when entering the test for the first time.
            // Reaching `TA_GENERATE` can push or modify the last element of the stack.
            // Right after `OnPostRunSingleTest`, any trailing elements with `.IsLastValue() == true` are pruned.
            // If the stack isn't empty after that, the test is restarted with the same stack.
            // NOTE! If you want to pop from this (internally in the library), don't forget to run `OnPrePruneGenerator()` on the modules.
            std::vector<std::unique_ptr<const BasicGenerator>> generator_stack;

            // This is mostly internall stuff:

            // Unlike `generator_stack`, this doesn't persist between test repetition.
            // This remembers all visited generators, and maps them to the indices in `generator_stack`.
            // If a generator doesn't specify `new_value_when_revisiting`, it'll try to take a value from this map instead of generating a new one.
            // And if it does take a value from here, it's not added to the stack at all.
            std::map<SourceLocWithCounter, std::size_t> visited_generator_cache;

            // This is used to prevent recursive usage of generators.
            bool currently_in_generator = false;

            // This is guaranteed to not contain any lazy log statements.
            std::vector<context::LogEntry> unscoped_log;

            // Which generator in `RunSingleTestInfo::generator_stack` we expect to hit next, or `generator_stack.size()` if none.
            // This starts at `0` every time the test is entered.
            // When exiting a test normally, this should be at `generator_stack.size()`, otherwise you have a non-deterministic failure in your tests.
            std::size_t generator_index = 0;
        };
        // Information about a single finished test (possibly one of the generated repetitions).
        struct RunSingleTestResults : RunSingleTestProgress
        {
            // True if we're about to leave the test for the last time.
            // This should be equivalent to `generator_stack.empty()`. This is set right before leaving the test.
            bool is_last_generator_repetition = false;
        };

        // Describes a single `TA_GENERATE(...)` call at runtime.
        struct GeneratorCallInfo
        {
            const RunSingleTestProgress *test = nullptr;
            const BasicGenerator *generator = nullptr;

            // Whether we're generating a new value, or just reusing the existing one.
            bool generating_new_value = false;
        };

        // The result of analyzing an exception by one of our modules.
        struct ExplainedException
        {
            // The exception type. You must set this, since `typeid(void)` is reserved for unknown exceptions.
            std::type_index type = typeid(void);
            // The exception message, normally from `e.what()`.
            std::string message;
            // The nested exception, if any.
            std::exception_ptr nested_exception;
        };
    }

    // Per-thread state
    namespace detail
    {
        // Stores a copy of a `$[...]` argument, or its string representation.
        struct ArgBuffer
        {
            alignas(CFG_TA_ARG_STORAGE_ALIGNMENT) char buffer[CFG_TA_ARG_STORAGE_SIZE];

            ArgBuffer() = default;
            ArgBuffer(const ArgBuffer &) = delete;
            ArgBuffer &operator=(const ArgBuffer &) = delete;
        };

        template <typename T>
        concept FitsIntoArgStorage = sizeof(T) <= CFG_TA_ARG_STORAGE_SIZE && alignof(T) <= CFG_TA_ARG_STORAGE_ALIGNMENT;

        // A metadata for a single `ArgBuffer`.
        struct ArgMetadata
        {
            data::AssertionExprDynamicInfo::ArgState state = data::AssertionExprDynamicInfo::ArgState::not_started;

            // Destroys the object. This can be null if the object needs no cleanup.
            void (*cleanup_func)(ArgBuffer &buffer) = nullptr;

            // Converts the object to a string. Replaces it with that string, and returns it as is the next time.
            const std::string &(*to_string_func)(ArgMetadata &self, ArgBuffer &buffer) = nullptr;

            void Destroy(ArgBuffer &buffer) noexcept
            {
                if (cleanup_func)
                {
                    cleanup_func(buffer);
                    cleanup_func = nullptr;
                }
            }

            template <typename T>
            requires FitsIntoArgStorage<std::remove_cvref_t<T>>
            std::remove_cvref_t<T> &StoreValue(ArgBuffer &buffer, T &&value)
            {
                Destroy(buffer);

                using type = std::remove_cvref_t<T>;

                auto ret = ::new((void *)buffer.buffer) type(std::forward<T>(value));

                if constexpr (!std::is_trivially_destructible_v<type>)
                    cleanup_func = [](ArgBuffer &buffer) {std::launder(reinterpret_cast<type *>(buffer.buffer))->~type();};

                return *ret;
            }
        };

        // The global per-thread state.
        struct GlobalThreadState
        {
            data::RunSingleTestResults *current_test = nullptr;
            data::BasicAssertion *current_assertion = nullptr;

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

            // Assertion argument storage:
            // We're putting it here to reuse the heap allocations.

            // Byte arrays for the arguments. Outer vector is for nested assertions, inner vectors are for per-assertion arguments.
            // We can't use a flat vector, because it can't be extended without moving the existing elements,
            //   which would mess up non-trivially-relocatable elements (which we can't even check for in current C++).
            std::vector<std::vector<ArgBuffer>> assertion_argument_buffers;
            // The metadata is relocatable, so this is a flat vector.
            std::vector<ArgMetadata> assertion_argument_metadata;
            // Next index for `assertion_argument_buffers`.
            // We can't use the vector size for that, because we want to preserve the outer vector size to reuse the elements' buffers.
            std::size_t assertion_argument_buffers_pos = 0;

            // Gracefully fails the current test, if not already failed.
            // Call this first, before printing any messages.
            CFG_TA_API void FailCurrentTest();
        };
        [[nodiscard]] CFG_TA_API GlobalThreadState &ThreadState();
    }

    namespace platform
    {
        // Whether the debugger is currently attached.
        // `false` if unknown or disabled with `CFG_TA_DETECT_DEBUGGER`
        CFG_TA_API bool IsDebuggerAttached();

        // Whether stdout (or stderr, depending on the argument) is attached to a terminal.
        CFG_TA_API bool IsTerminalAttached(bool is_stderr);
    }


    // Macro internals, except `TA_MUST_THROW(...)`.
    namespace detail
    {
        // --- ASSERTIONS ---

        // `$[...]` ultimately expands to this.
        // Stores a pointer to a `StoredArg` in an `AssertWrapper` where it will write the argument as a string.
        struct ArgWrapper
        {
            data::BasicAssertion *assertion = nullptr;
            ArgBuffer *target_buffer = nullptr;
            ArgMetadata *target_metadata = nullptr;

            // Raises a hard error if the assertion owning this argument isn't currently running in this thread.
            CFG_TA_API void EnsureAssertionIsRunning();

            ArgWrapper(data::BasicAssertion &assertion, ArgBuffer &target_buffer, ArgMetadata &target_metadata)
                : assertion(&assertion), target_buffer(&target_buffer), target_metadata(&target_metadata)
            {
                EnsureAssertionIsRunning();
                target_metadata.state = data::AssertionExprDynamicInfo::ArgState::in_progress;
            }
            ArgWrapper(const ArgWrapper &) = default;
            ArgWrapper &operator=(const ArgWrapper &) = default;

            template <typename T>
            requires string_conv::SupportsToString<std::remove_cvref_t<T>>
            T &&operator[](T &&arg) &&
            {
                EnsureAssertionIsRunning();

                using type = std::remove_cvref_t<T>;

                static constexpr auto identity_to_string = [](ArgMetadata &self, ArgBuffer &buffer) -> const std::string &
                {
                    (void)self;
                    return *std::launder(reinterpret_cast<std::string *>(buffer.buffer));
                };

                // If we can copy the object itself (to then convert to string lazily), do it.
                if constexpr (FitsIntoArgStorage<type> && string_conv::SupportsLazyToString<type>)
                {
                    using traits = string_conv::MaybeLazyToString<type>;

                    target_metadata->StoreValue(*target_buffer, traits{}(std::as_const(arg)));
                    target_metadata->to_string_func = [](ArgMetadata &self, ArgBuffer &buffer) -> const std::string &
                    {
                        // Convert to a string.
                        using proxy_type = std::remove_cvref_t<decltype(traits{}(std::as_const(arg)))>;
                        std::string string = string_conv::ToString(*std::launder(reinterpret_cast<proxy_type *>(buffer.buffer)));

                        // Store the string as the new value.
                        auto &ret = self.StoreValue(buffer, std::move(string));
                        self.to_string_func = identity_to_string;

                        return ret;
                    };
                }
                else
                {
                    target_metadata->StoreValue(*target_buffer, string_conv::ToString(arg));
                    target_metadata->to_string_func = identity_to_string;
                }

                target_metadata->state = data::AssertionExprDynamicInfo::ArgState::done;
                return std::forward<T>(arg);
            }
        };

        struct AssertionExprStaticInfoImpl final : data::AssertionExprStaticInfo
        {
            struct CounterIndexPair
            {
                int counter = 0;
                std::size_t index = 0;
            };

            std::vector<CounterIndexPair> counter_to_arg_index;

            CFG_TA_API AssertionExprStaticInfoImpl(std::string_view raw_expr, std::string_view expanded_expr);
        };

        // An intermediate base class that `AssertWrapper<T>` inherits from.
        // You can also inherit custom assertion classes from this, if they don't need the expression decomposition provided by `AssertWrapper<T>`.
        class CFG_TA_API_CLASS AssertWrapper final : public data::BasicAssertion, public data::AssertionExprDynamicInfo
        {
            bool condition_value = false;

            // User condition was evaluated to completion.
            bool condition_value_known = false;

            // This is called to get the optional user message, flags, etc (null if none of that).
            void (*extras_func)(AssertWrapper &self, const void *data) = nullptr;
            const void *extras_data = nullptr;
            // The user message (if any) is written here on failure or when
            std::optional<std::string> user_message;

            // This is only set on failure.
            AssertFlags flags{};

            // Pushes and pops this into the assertion stack.
            // `Evaluator::operator~()` uses this.
            struct AssertionStackGuard
            {
                AssertWrapper &self;

                CFG_TA_API AssertionStackGuard(AssertWrapper &self);

                AssertionStackGuard(const AssertionStackGuard &) = delete;
                AssertionStackGuard &operator=(const AssertionStackGuard &) = delete;

                CFG_TA_API ~AssertionStackGuard();
            };

            // This is invoked when the assertion finishes evaluating.
            struct Evaluator
            {
                AssertWrapper &self;
                CFG_TA_API bool operator~();
            };

            // Call to trigger a breakpoint at the macro call site.
            void (*break_func)() = nullptr;

            // This can be overridden on failure, but not necessarily. Otherwise (and by defualt) points to the actual location.
            SourceLoc source_loc;

            // This is called to evaluate the user condition.
            void (*condition_func)(AssertWrapper &self, const void *data) = nullptr;
            const void *condition_data = nullptr;

            // Evaluates the user message and other extra parameters, if any.
            // Repeated calls hae no effect.
            CFG_TA_API void EvaluateExtras();

          public:
            // Note the weird variable name, it helps with our macro syntax that adds optional messages.
            Evaluator DETAIL_TA_ADD_EXTRAS{*this};

            CFG_TA_API AssertWrapper(std::string_view name, SourceLoc loc, void (*breakpoint_func)());

            template <typename F>
            AssertWrapper(std::string_view name, SourceLoc loc, std::string_view raw_expr, std::string_view expanded_expr, const F &func, void (*breakpoint_func)())
                : AssertWrapper(name, loc, breakpoint_func)
            {
                condition_func = [](AssertWrapper &self, const void *data)
                {
                    return (*static_cast<const F *>(data))(self);
                };
                condition_data = &func;

                static const AssertionExprStaticInfoImpl static_info_storage(raw_expr, expanded_expr);
                static_info = &static_info_storage;
            }

            AssertWrapper(const AssertWrapper &) = delete;
            AssertWrapper &operator=(const AssertWrapper &) = delete;

            template <typename T>
            void EvalCond(T &&value)
            {
                condition_value = (std::forward<T>(value) ? true : false); // Using `? :` to force a contextual bool conversion.
                condition_value_known = true;
            }

            template <typename F>
            Evaluator &AddExtras(const F &func)
            {
                extras_func = [](AssertWrapper &self, const void *data)
                {
                    (*static_cast<const F *>(data))(meta::Overload{
                        [&]<typename ...P>(AssertFlags flags)
                        {
                            self.flags = flags;
                        },
                        [&]<typename ...P>(AssertFlags flags, SourceLoc loc)
                        {
                            self.flags = flags;
                            self.source_loc = loc;
                        },
                        [&]<typename ...P>(CFG_TA_FMT_NAMESPACE::format_string<P...> format, P &&... args)
                        {
                            self.user_message = CFG_TA_FMT_NAMESPACE::format(std::move(format), std::forward<P>(args)...);
                        },
                        [&]<typename ...P>(AssertFlags flags, CFG_TA_FMT_NAMESPACE::format_string<P...> format, P &&... args)
                        {
                            self.flags = flags; // Do this first, in case formatting throws.
                            self.user_message = CFG_TA_FMT_NAMESPACE::format(std::move(format), std::forward<P>(args)...);
                        },
                        [&]<typename ...P>(AssertFlags flags, SourceLoc loc, CFG_TA_FMT_NAMESPACE::format_string<P...> format, P &&... args)
                        {
                            self.flags = flags; // Do this first, in case formatting throws.
                            self.source_loc = loc; // Same.
                            self.user_message = CFG_TA_FMT_NAMESPACE::format(std::move(format), std::forward<P>(args)...);
                        },
                    });
                };
                extras_data = &func;
                return DETAIL_TA_ADD_EXTRAS;
            }

            CFG_TA_API const SourceLoc &SourceLocation() const override;
            CFG_TA_API std::optional<std::string_view> UserMessage() const override;

            CFG_TA_API DecoVar GetElement(int index) const override;
            [[nodiscard]] CFG_TA_API ArgWrapper _ta_arg_(int counter);
        };


        // --- TESTS ---

        struct BasicTestImpl : data::BasicTest
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
            std::vector<const BasicTestImpl *> tests;
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

        // Stores singletons derived from `BasicTestImpl`.
        template <std::derived_from<BasicTestImpl> T>
        inline const T test_singleton{};

        // Registers a test. Pass a pointer to an instance of `test_singleton<??>`.
        CFG_TA_API void RegisterTest(const BasicTestImpl *singleton);

        // An implementation of `BasicTestImpl` for a specific test.
        // `P` is a pointer to the test function, see `DETAIL_TA_TEST()` for details.
        // `B` is a lambda that triggers a breakpoint in the test location itself when called.
        template <auto P, auto B, meta::ConstString TestName, meta::ConstString LocFile, int LocLine, TestFlags FlagsValue = TestFlags{}>
        struct SpecificTest final : BasicTestImpl
        {
            static constexpr bool test_name_is_valid = []{
                if (TestName.view().empty())
                    return false;
                // Intentionalyl allowing leading digits here.
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
            TestFlags Flags() const override
            {
                return FlagsValue;
            }
            SourceLoc SourceLocation() const override
            {
                return {LocFile.view(), LocLine};
            }

            void Run() const override
            {
                // `{}` is for the name tag.
                P({});
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

        // Generate the next incremental log message id.
        [[nodiscard]] CFG_TA_API std::size_t GenerateLogId();

        CFG_TA_API void AddLogEntryLow(std::string &&message);
        // `TA_CONTEXT` expands to this.
        template <typename ...P>
        void AddLogEntry(CFG_TA_FMT_NAMESPACE::format_string<P...> format, P &&... args)
        {
            AddLogEntryLow(CFG_TA_FMT_NAMESPACE::format(format, std::forward<P>(args)...));
        }
        CFG_TA_API void AddLogEntry(const SourceLoc &loc);

        class BasicScopedLogGuard
        {
            std::optional<context::LogEntry> entry;

          protected:
            CFG_TA_API BasicScopedLogGuard(context::LogEntry new_entry);

          public:
            BasicScopedLogGuard(const BasicScopedLogGuard &) = delete;
            BasicScopedLogGuard &operator=(const BasicScopedLogGuard &) = delete;

            CFG_TA_API ~BasicScopedLogGuard();
        };

        class ScopedLogGuard final : BasicScopedLogGuard
        {
            context::LogEntry entry;

          public:
            // User message.
            template <typename ...P>
            ScopedLogGuard(const char */*func_name*/, CFG_TA_FMT_NAMESPACE::format_string<P...> format, P &&... args)
                : BasicScopedLogGuard({GenerateLogId(), context::LogMessage{CFG_TA_FMT_NAMESPACE::format(format, std::forward<P>(args)...)}})
            {}
            // Source location.
            ScopedLogGuard(std::string_view func_name, const SourceLoc &loc)
                : BasicScopedLogGuard({GenerateLogId(), context::LogSourceLoc{loc, func_name}})
            {}
            // Source location with function name override.
            ScopedLogGuard(const char */*orig_func_name*/, const SourceLoc &loc, std::string_view func_name)
                : BasicScopedLogGuard({GenerateLogId(), context::LogSourceLoc{loc, func_name}})
            {}
        };

        template <typename F>
        class ScopedLogGuardLazy final : BasicScopedLogGuard
        {
            F func;

          public:
            ScopedLogGuardLazy(F &&func)
                // `this->func` isn't initialized here yet, but we don't read it yet, so it doesn't matter.
                : BasicScopedLogGuard({GenerateLogId(), context::LogMessage{this->func}}), func(std::move(func))
            {}
        };


        // --- GENERATORS ---

        // Used internally by `SpecificGenerator`.
        struct SpecificGeneratorGenerateGuard
        {
            data::BasicGenerator &self;
            bool ok = false;

            SpecificGeneratorGenerateGuard(data::BasicGenerator &self) : self(self) {}
            SpecificGeneratorGenerateGuard(const SpecificGeneratorGenerateGuard &) = delete;
            SpecificGeneratorGenerateGuard &operator=(const SpecificGeneratorGenerateGuard &) = delete;

            CFG_TA_API ~SpecificGeneratorGenerateGuard();
        };

        template <
            // Manually specified:
            meta::ConstString GeneratorName, meta::ConstString LocFile, int LocLine, int LocCounter, typename F,
            // Computed:
            typename UserFuncWrapperType = decltype(std::declval<F &&>()()),
            typename ReturnType = decltype(std::declval<UserFuncWrapperType &>().func(std::declval<bool &>()))
        >
        struct SpecificGenerator final : data::BasicTypedGenerator<ReturnType>
        {
            using data::BasicGenerator::overriding_module;

            using return_type = ReturnType;

            UserFuncWrapperType func;

            template <typename G>
            SpecificGenerator(G &&make_func) : func(std::forward<G>(make_func)()) {}

            static constexpr auto location = SourceLocWithCounter(LocFile.view(), LocLine, LocCounter);

            const SourceLocWithCounter &SourceLocation() const override
            {
                return location;
            }

            std::string_view Name() const override
            {
                return GeneratorName.view();
            }

            [[nodiscard]] GeneratorFlags Flags() const override final
            {
                // The flags have to sit in this class and not in `BasicGenerator`,
                // to allow us to RVO the `GenerateFuncParam` all the way into here.
                return func.flags;
            }

            void Generate() override
            {
                auto generate_value = [&]
                {
                    // This guard can kick this generator from the stack if it failed before generating the first value.
                    // We need this here, because otherwise the assertion failure handler would try to read the value from this generator, when it has none.
                    SpecificGeneratorGenerateGuard guard(*this);

                    // Here the default value of `repeat` is `true`.
                    // This is to match the programming pattern of `if (...) break;` with `if (...) repeat = false;`,
                    //   but I'm not sure if this is ultimately a good decision. Could flip this later.

                    // We could try to somehow conditionally assign here if possible.
                    // I tried, got some weird build error in some edge case, and decided not to bother.
                    this->storage.template emplace<1>(func.func, this->repeat);

                    // On success, decrement the assertion counter to counteract `TA_CHECK` incrementing it.
                    const_cast<data::RunTestsProgress *>(ThreadState().current_test->all_tests)->num_checks_total--;

                    guard.ok = true;
                    return true;
                };

                // Using the assertion macro to nicely print exceptions, and to fail the test if this throws something.
                // We can't rely on automatic test failure on exceptions, because this function can get called outside of the test-wide try-catch,
                //   when `--generate` is involved.
                TA_CHECK( generate_value() )("Generating a value in `TA_GENERATE(...)`.");

                this->this_value_is_custom = false;
                this->num_generated_values++;
            }
        };

        class GenerateValueHelper
        {
            // All those are set internally by `HandleGenerator()`:

            bool creating_new_generator = false;
            bool generating_new_value = false;

            // This is set internally by `HandleGenerator()` when we're sure that we don't want to pop the generator from the stack on failure.
            bool generator_stays_in_stack = false;

          public:
            // This must be filled with the source location.
            SourceLocWithCounter source_loc;

            // The caller places the current generator here.
            data::BasicGenerator *untyped_generator = nullptr;

            // The caller sets this if a new generator is being created. In that case it must have the same value as `untyped_generator`.
            // Note that `HandleGenerator()` moves from this variable, so it's always null in the destructor.
            std::unique_ptr<data::BasicGenerator> created_untyped_generator;

            CFG_TA_API GenerateValueHelper(SourceLocWithCounter source_loc);

            GenerateValueHelper(const GenerateValueHelper &) = delete;
            GenerateValueHelper &operator=(const GenerateValueHelper &) = delete;

            CFG_TA_API ~GenerateValueHelper();
            CFG_TA_API void HandleGenerator();
        };

        // `TA_GENERATE_FUNC(...)` expands to this.
        // `func` returns the user lambda (it's not the user lambda itself).
        template <
            // Manually specified:
            meta::ConstString GeneratorName, meta::ConstString LocFile, int LocLine, int LocCounter,
            // Deduced:
            typename F
        >
        requires(text::chars::IsIdentifierStrict(GeneratorName.view()))
        [[nodiscard]] auto GenerateValue(F &&func)
            -> const typename SpecificGenerator<GeneratorName, LocFile, LocLine, LocCounter, F>::return_type &
        {
            using GeneratorType = SpecificGenerator<GeneratorName, LocFile, LocLine, LocCounter, F>;

            auto &thread_state = ThreadState();
            if (!thread_state.current_test)
                HardError("Can't use `TA_GENERATE(...)` when no test is running.", HardErrorKind::user);

            GenerateValueHelper guard({{LocFile.view(), LocLine}, LocCounter});

            GeneratorType *typed_generator = nullptr;

            if (thread_state.current_test->generator_index < thread_state.current_test->generator_stack.size())
            {
                // Revisiting a generator.

                // This tells the scope guard that the generator is ready.
                // The `dynamic_cast` can fail after this, but that's a hard error anyway.
                guard.untyped_generator = const_cast<data::BasicGenerator *>(thread_state.current_test->generator_stack[thread_state.current_test->generator_index].get());
                typed_generator = dynamic_cast<GeneratorType *>(guard.untyped_generator);
            }
            else
            {
                // Visiting a generator for the first time.

                // Argh! How do we avoid this allocation if we just need to look at the `` flag?
                auto new_generator = std::make_unique<GeneratorType>(std::forward<F>(func));

                // Try to reuse a cached value.
                if (!bool(new_generator->Flags() & GeneratorFlags::new_value_when_revisiting))
                {
                    auto iter = thread_state.current_test->visited_generator_cache.find(guard.source_loc);
                    if (iter != thread_state.current_test->visited_generator_cache.end())
                    {
                        if (iter->second >= thread_state.current_test->generator_stack.size())
                            HardError("Cached generator index is somehow out of range?");
                        return dynamic_cast<GeneratorType &>(const_cast<data::BasicGenerator &>(*thread_state.current_test->generator_stack[iter->second])).GetValue();
                    }
                }

                guard.untyped_generator = typed_generator = new_generator.get();
                guard.created_untyped_generator = std::move(new_generator);
            }

            guard.HandleGenerator();

            return typed_generator->GetValue();
        }


        // Template parameter generators:

        // Parses a name from a `CFG_TA_THIS_FUNC_NAME` from a specially crafted lambda.
        template <meta::ConstString S>
        constexpr auto entity_name_from_string_storage = []
        {
            constexpr std::string_view view = S.view();
            #ifdef _MSC_VER
            // This always works, even if we're inside another `operator()`, because any extra `operator()`s are printed as `::()`.
            constexpr std::string_view prefix = "::operator ()<";
            constexpr std::string_view suffix = ">(void) const";
            #else
            // GCC prints `[with` before, and sometimes the template parameter kind (`auto` and such).
            // Clang prints `[` before, but sometimes also enclosing template parameters.
            // Hence we start with the parameter name. It seems it's always the last parameter, and it seems the parameters aren't sorted.
            constexpr std::string_view prefix = "_ta_test_NameOf = ";
            constexpr std::string_view suffix = "]";
            #endif
            static_assert(view.ends_with(suffix), "Failed to parse the template argument name, don't know how to do it on your compiler.");
            constexpr std::size_t n = view.rfind(prefix);
            static_assert(n != std::string_view::npos, "Failed to parse the template argument name, don't know how to do it on your compiler.");
            std::array<char, view.size() - n - prefix.size() - suffix.size() + 1> ret{};
            std::copy(view.data() + n + prefix.size(), view.data() + view.size() - suffix.size(), ret.begin());
            return ret;
        }();
        template <meta::ConstString S>
        [[nodiscard]] std::string_view ParseEntityNameFromString()
        {
            const auto &array = entity_name_from_string_storage<S>;
            return {array.data(), array.data() + array.size() - 1};
        }

        // This is used to get template parameter names. `F` is a lambda that returns a `__PRETTY_FUNC__`/`__FUNCSIG__`-based name.
        // The lambda must be generated by the macro because it can be an arbitrary template template parameter,
        // which makes it impossible to not generate.
        template <auto F>
        struct ParamNameFunc : decltype(F)
        {
            using decltype(F)::operator();

            // This catches NTTPs and tries to print them using our `ToString()` if possible.
            // The name `_ta_test_NameOf` must match what `TA_GENERATE_PARAM(...)` uses internally, and what `ParseEntityNameFromString()` expects.
            // `A` is unused, it receives `_ta_test_ParamMarker`.
            template <auto _ta_test_NameOf>
            // We always need at least some condition here, even if just `requires true`, to have priority over the inherited function.
            requires string_conv::SupportsToString<decltype(_ta_test_NameOf)>
            std::string_view operator()(meta::PreferenceTagA) const
            {
                // We need to do this crap because `F` returns `std::string_view`, and it should probably stay that way.
                static const std::string ret = []{
                    // Prefer `ToString()` if supported.
                    // Otherwise MSVC prints integers in hex, which is stupid.
                    std::string value = string_conv::ToString(_ta_test_NameOf);

                    // Prepend the type if necessary.
                    // Note, we don't need to remove constness here, it seems `decltype()` can never return const-qualified types here.
                    if constexpr (string_conv::ClarifyTypeInMixedTypeContexts<decltype(_ta_test_NameOf)>::value)
                        value = CFG_TA_FMT_NAMESPACE::format("({}){}", text::TypeName<decltype(_ta_test_NameOf)>(), value);

                    return value;
                }();
                return ret;
            }

            // Catch `expand`, and trigger an unconditional `static_assert`.
            // If it arrives here, it was misused.
            template <ExpandTag T>
            std::string operator()(meta::PreferenceTagA) const
            {
                static_assert(meta::AlwaysFalse<meta::ValueTag<T>>::value, "Incorrect use of `expand`.");
                return "";
            }
        };

        // Holds the generated parameter index for `TA_GENERATE_PARAM(...)`.
        // This is convertible to and from a string.
        template <std::size_t N, typename NameLambda>
        struct GeneratedParamIndex
        {
            static constexpr std::size_t size = N;

            std::size_t index = 0;

            friend bool operator==(GeneratedParamIndex, GeneratedParamIndex) = default;
        };

        // `TA_GENERATE_PARAM(...)` expands to this.
        template <meta::ConstString LocFile, int LocLine, int LocCounter, meta::ConstString GeneratorName, auto ListLambda, typename NameLambda>
        class ParamGenerator
        {
            GeneratorFlags flags{};

          public:
            constexpr ParamGenerator() {}
            ParamGenerator(GeneratorFlags flags) : flags(flags) {}

            template <typename F>
            [[nodiscard]] decltype(auto) operator->*(F &&func)
            {
                static constexpr auto arr = ListLambda.template operator()<F>();

                if (arr.empty())
                    flags |= GeneratorFlags::generate_nothing;

                auto name_lambda_wrapped = [](std::size_t i)
                {
                    if constexpr (arr.size() == 0)
                        return std::string_view{};
                    else
                        return ListLambda.template operator()<NameLambda, meta::PreferenceTagA>()[i](NameLambda{}, meta::PreferenceTagA{});
                };

                auto index = (GenerateValue<GeneratorName, LocFile, LocLine, LocCounter>)(
                    [this]{
                        return GenerateFuncParam(flags, [i = std::size_t{}](bool &repeat) mutable
                        {
                            repeat = i + 1 < arr.size();
                            return GeneratedParamIndex<arr.size(), decltype(name_lambda_wrapped)>{i++};
                        });
                    }
                );
                return arr[index.index](std::forward<F>(func));
            }
        };

        // This index type is used internally by `TA_SELECT(...)` and `TA_VARIANT(...)`.
        template <meta::ConstString LocFile, int LocLine, int LocCounter, meta::ConstString GeneratorName>
        struct VariantIndex
        {
            int value = 0;

            friend bool operator==(VariantIndex, VariantIndex) = default;

            struct State
            {
                std::map<int, std::string_view> index_to_string;
                std::map<std::string_view, int, std::less<>> string_to_index;
            };
            [[nodiscard]] static State &GlobalState()
            {
                static State ret;
                return ret;
            }
        };

        // `TA_SELECT(...) and TA_VARIANT(...)` expand to this.
        template <meta::ConstString LocFile, int LocLine, int LocCounter, meta::ConstString GeneratorName>
        requires(text::chars::IsIdentifierStrict(GeneratorName.view()))
        class VariantGenerator
        {
            using IndexType = VariantIndex<LocFile, LocLine, LocCounter, GeneratorName>;

            GeneratorFlags flags{};

            // The next pass number.
            // 0 = discovery pass, 1 = running the single variant, 2 = exiting.
            int pass_number = 0;

            // Which variants we're going to run.
            std::vector<int> enabled_variants;

            struct GeneratorFunctor
            {
                VariantGenerator &self;

                auto operator()() const
                {
                    bool is_empty = self.enabled_variants.empty();
                    return GenerateFuncParam(
                        self.flags | is_empty * GeneratorFlags::generate_nothing,
                        [variants = std::move(self.enabled_variants), index = std::size_t(0)](bool &repeat) mutable -> IndexType
                        {
                            if (index >= variants.size())
                                HardError("`TA_VARIANT(...)` index is out of range.");
                            int ret = variants[index++];
                            repeat = index < variants.size();
                            return IndexType{ret};
                        }
                    );
                }
            };

            // Touching this registers a variant.
            template <int VarCounter, meta::ConstString VarName>
            inline static const std::nullptr_t register_variant = []{
                auto &state = IndexType::GlobalState();
                // We silently ignore duplicate indices, just in case.
                if (state.index_to_string.try_emplace(VarCounter, VarName.view()).second)
                {
                    // For duplicate names, we keep the lower index.
                    auto [iter, inserted] = state.string_to_index.try_emplace(VarName.view(), VarCounter);
                    if (!inserted)
                        iter->second = std::min(iter->second, VarCounter);
                }

                return nullptr;
            }();

          public:
            // Opaque enum for the variant index.
            // Value `0` is reserved for the initial execution, where no variant is selected yet.
            // It's impossible to receive `__COUNTER__ == 0` in a variant, because even though the counter starts at zero,
            // we call it at least once before, in `TA_SELECT(...)` itself.
            enum class Enum : int {};

            VariantGenerator(GeneratorFlags flags = {})
                : flags(flags)
            {
                // When reentering the generator, skip the first pass.
                GlobalThreadState &thread_state = ThreadState();
                if (!thread_state.current_test)
                    HardError("Can't use `TA_SELECT(...)` when no test is running.");
                if (thread_state.current_test->generator_index < thread_state.current_test->generator_stack.size()
                    && bool(dynamic_cast<const SpecificGenerator<GeneratorName, LocFile, LocLine, LocCounter, GeneratorFunctor> *>(
                        thread_state.current_test->generator_stack[thread_state.current_test->generator_index].get()
                    ))
                )
                {
                    // Skip the first pass (variant discovery pass).
                    pass_number = 1;
                }
            }

            [[nodiscard]] bool LoopCondition()
            {
                return pass_number++ < 2; // Two passes.
            }

            [[nodiscard]] Enum SelectTarget()
            {
                if (pass_number != 2)
                {
                    // First pass.
                    return {};
                }
                else
                {
                    // Second pass.
                    return Enum(GenerateValue<GeneratorName, LocFile, LocLine, LocCounter>(GeneratorFunctor{*this}).value);
                }
            }

            // Touching this registers a variant, and calling it at runtime enables it for this run.
            template <int VarCounter, meta::ConstString VarName>
            requires(text::chars::IsIdentifierStrict(VarName.view()))
            auto RegisterVariant()
            {
                (void)std::integral_constant<const std::nullptr_t *, &register_variant<VarCounter, VarName>>{};
                if (pass_number != 1)
                    HardError("Why are we trying to register a variant in the second pass?");
                enabled_variants.push_back(VarCounter);
            }
        };
    }
    // Some internal specializations.
    template <std::size_t N, typename NameLambda>
    struct string_conv::DefaultToStringTraits<detail::GeneratedParamIndex<N, NameLambda>>
    {
        std::string operator()(detail::GeneratedParamIndex<N, NameLambda> value) const
        {
            return std::string(NameLambda{}(value.index));
        }
    };
    template <std::size_t N, typename NameLambda>
    struct string_conv::DefaultFromStringTraits<detail::GeneratedParamIndex<N, NameLambda>>
    {
        // If we have duplicate elements in the list, this returns the first one.
        [[nodiscard]] std::string operator()(detail::GeneratedParamIndex<N, NameLambda> &target, const char *&string) const
        {
            if constexpr (N == 0)
            {
                return "This type doesn't have any valid values.";
            }
            else
            {
                // Prepare a list of names. Sort by decreasing length, to give longer strings priority.
                static const std::vector<std::pair<std::string_view, std::size_t>> list = []{
                    std::vector<std::pair<std::string_view, std::size_t>> ret;
                    ret.reserve(N);
                    for (std::size_t i = 0; i < N; i++)
                        ret.emplace_back(NameLambda{}(i), i);
                    std::sort(ret.begin(), ret.end(), [](const auto &a, const auto &b){return a.first.size() > b.first.size();});
                    return ret;
                }();

                auto iter = std::find_if(list.begin(), list.end(), [&](const auto &p)
                {
                    return std::strncmp(string, p.first.data(), p.first.size()) == 0;
                });

                if (iter == list.end())
                {
                    std::string error = "Expected one of: ";

                    // To make sure we don't print the same element twice.
                    std::set<std::string_view> elems;

                    for (std::size_t i = 0; i < N; i++)
                    {
                        static_assert(std::is_same_v<decltype(NameLambda{}(i)), std::string_view>);
                        std::string_view elem = NameLambda{}(i);
                        if (!elems.insert(elem).second)
                            continue; // Refuse to print the same element twice.

                        if (i != 0)
                            error += ", ";

                        error += '`';
                        error += elem;
                        error += '`';
                    }
                    error += '.';
                    return error;
                }

                target.index = iter->second;
                string += iter->first.size();
                return "";
            }
        }
    };
    template <meta::ConstString LocFile, int LocLine, int LocCounter, meta::ConstString Name>
    struct string_conv::DefaultToStringTraits<detail::VariantIndex<LocFile, LocLine, LocCounter, Name>>
    {
        using Type = detail::VariantIndex<LocFile, LocLine, LocCounter, Name>;

        std::string operator()(Type value) const
        {
            auto it = Type::GlobalState().index_to_string.find(value.value);
            if (it == Type::GlobalState().index_to_string.end())
                HardError("Didn't find this `TA_VARIANT(...)` index in the map.");
            return std::string(it->second);
        }
    };
    template <meta::ConstString LocFile, int LocLine, int LocCounter, meta::ConstString Name>
    struct string_conv::DefaultFromStringTraits<detail::VariantIndex<LocFile, LocLine, LocCounter, Name>>
    {
        using Type = detail::VariantIndex<LocFile, LocLine, LocCounter, Name>;

        // If we have duplicate elements in the list, this returns the first one.
        [[nodiscard]] std::string operator()(Type &target, const char *&string) const
        {
            if (Type::GlobalState().string_to_index.empty())
                return "This `TA_SELECT(...)` has no variants.";
            const char *start = string;
            while (text::chars::IsIdentifierCharStrict(*string))
                string++;
            if (string == start)
                return "Expected the variant name.";
            auto iter = Type::GlobalState().string_to_index.find(std::string_view(start, string));
            if (iter == Type::GlobalState().string_to_index.end())
            {
                std::string error = "Expected the variant name, one of: ";
                bool first = true;
                for (const auto &elem : Type::GlobalState().string_to_index)
                {
                    if (first)
                        first = false;
                    else
                        error += ", ";

                    error += elem.first;
                }
                error += '.';
                return error;
            }
            target.value = iter->second;
            return "";
        }
    };

    // Converts a C++20 range to a functor usable with `TA_GENERATE_FUNC(...)`.
    // `TA_GENERATE(...)` calls it internally. But you might want to call it manually,
    // because `TA_GENERATE(...)` prevents you from using any local variables, for safety.
    template <std::ranges::input_range T>
    [[nodiscard]] auto RangeToGeneratorFunc(GeneratorFlags flags, T &&range)
    {
        struct Functor
        {
            std::remove_cvref_t<T> range{};
            std::ranges::iterator_t<std::remove_cvref_t<T>> iter{};

            explicit Functor(T &&range)
                : range(std::forward<T>(range)), iter(this->range.begin())
            {}

            // `iter` might go stale on copy.
            Functor(const Functor &) = delete;
            Functor &operator=(const Functor &) = delete;

            decltype(auto) operator()(bool &repeat)
            {
                // Check for the end of the range. Not in the constructor, because that doesn't play nice with `--generate`.
                if (iter == range.end())
                    HardError("Overflowed a generator range.");

                decltype(auto) ret = *iter;
                repeat = ++iter != range.end();
                if constexpr (std::is_reference_v<decltype(ret)>)
                    return decltype(ret)(ret);
                else
                    return ret;
            }
        };

        // Here we check emptiness before moving the range, which seems to be unavoidable here.
        bool is_empty = false;
        if constexpr (requires{range.empty();})
            is_empty = range.empty();
        else
            is_empty = range.begin() == range.end();
        if (is_empty)
            flags |= GeneratorFlags::generate_nothing;

        // Must get zero moves on the functor.
        return GenerateFuncParam(flags, Functor(std::forward<T>(range)));
    }
    // Those overload accept `{...}` initializer lists. They also conveniently add support for C arrays.
    // Note the lvalue overload being non-const, this way it can accept both const and non-const arrays, same as `std::to_array`.
    template <typename T, std::size_t N> requires(N > 0)
    [[nodiscard]] auto RangeToGeneratorFunc(GeneratorFlags flags, T (&range)[N])
    {
        return (RangeToGeneratorFunc)(flags, std::to_array(range));
    }
    template <typename T, std::size_t N> requires(N > 0)
    [[nodiscard]] auto RangeToGeneratorFunc(GeneratorFlags flags, T (&&range)[N])
    {
        return (RangeToGeneratorFunc)(flags, std::to_array(std::move(range)));
    }
    // Flag-less overloads. We use overloads to have flags before the range, to be consistent with everything else.
    template <std::ranges::input_range T>
    [[nodiscard]] auto RangeToGeneratorFunc(T &&range) {return (RangeToGeneratorFunc)(GeneratorFlags{}, std::forward<T>(range));}
    template <typename T, std::size_t N> requires(N > 0)
    [[nodiscard]] auto RangeToGeneratorFunc(T (&range)[N]) {return (RangeToGeneratorFunc)(GeneratorFlags{}, range);}
    template <typename T, std::size_t N> requires(N > 0)
    [[nodiscard]] auto RangeToGeneratorFunc(T (&&range)[N]) {return (RangeToGeneratorFunc)(GeneratorFlags{}, std::move(range));}


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
        // At least one exception (the top-level one or one of the nested).
        any,
    };
    using enum ExceptionElem;
    // Either an index of an exception element in `CaughtException`, or a enum designating one or more elements.
    using ExceptionElemVar = std::variant<ExceptionElem, int>;

    struct ExceptionElemsCombinedTag {explicit ExceptionElemsCombinedTag() = default;};
    // The functions checking exception message accept this in addition to `ExceptionElem` to check the whole message.
    inline constexpr ExceptionElemsCombinedTag combined;

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
    template <>
    struct string_conv::DefaultToStringTraits<ExceptionElemsCombinedTag>
    {
        CFG_TA_API std::string operator()(ExceptionElemsCombinedTag) const;
    };

    // Internals of `CaughtException`.
    namespace detail
    {
        CFG_TA_API const std::vector<ta_test::SingleException> &GetEmptyExceptionListSingleton();

        // A CRTP base.
        // `Ref` is a self reference that we return from chained functions, normally either `const Derived &` or `Derived &&`.
        // If `IsWrapper` is false, `Derived` must have a `??shared_ptr<BasicModule::CaughtExceptionInfo>?? _get_state() const` private member (with this class as a friend).
        // We'll examine the exception details pointed by that member. You can return any kind of a shared pointer or a reference to one.
        // Of `IsWrapper` is true, `Derived` must have a `const ?? &_get_state() const` private member that returns an object also inherited from this base,
        // to which we'll forward all calls.
        template <typename Derived, typename Ref, bool IsWrapper>
        class BasicCaughtExceptionInterface
        {
            // If `IsWrapper == true`, returns the object we're wrapping that must inherit from this template too.
            //     For a wrapper it can return by value, so don't call it more than once!
            // If `IsWrapper == false`, returns a `std::shared_ptr<BasicModule::CaughtExceptionInfo>`.
            // All of our members are `const` anyway, so we only have a const overload here.
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
            // This is initially `protected` because it makes no sense to call it directly on rvalues. (While it won't dangle, doing it shouldn't be necessary.)
            // Derived classes can make it public.

            // If you're manually examining this exception with `TA_CHECK(...)`, create an instance of this object first.
            // While it exists, all failed assertions will mention that they happened while examining this exception.
            // All high-level functions below do this automatically, and redundant contexts are silently ignored.
            // `index` is the element index (into `GetElems()`) of the element you're examining, or `-1` if not specified.
            // Fails the test if the is index invalid. `flags` affects how this validity check is handled.
            [[nodiscard]] data::CaughtExceptionContext MakeContextGuard(int index = -1, AssertFlags flags = {}, SourceLoc source_loc = SourceLoc::Current{}) const
            {
                if constexpr (IsWrapper)
                    return State().FrameGuard();
                else
                {
                    // This nicely handles null state and/or bad indices.
                    return data::CaughtExceptionContext(State(), index, flags, source_loc);
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

            // Concatenates the exception message with the messages from all nested exceptions, joining them with `separator`.
            [[nodiscard]] std::string CombinedMessage(std::string_view separator = "\n") const
            {
                if constexpr (IsWrapper)
                    return State().GetElems();
                else
                {
                    std::string ret;
                    if (!State())
                        return ret; // Always empty here, but should help with NRVO.
                    bool first = true;
                    for (const auto &elem : State()->elems)
                    {
                        if (first)
                            first = false;
                        else
                            ret += separator;
                        ret += elem.message;
                    }
                }
            }

            // Checks that the exception message is equal to a string.
            Ref CheckMessage(/* elem = top_level, */ std::string_view expected_message, AssertFlags flags = AssertFlags::hard, SourceLoc source_loc = SourceLoc::Current{}) const
            {
                // No need to wrap this.
                return CheckMessage(ExceptionElem::top_level, expected_message, flags, source_loc);
            }
            // Checks that the exception expected_message is equal to a string.
            Ref CheckMessage(ExceptionElemVar elem, std::string_view expected_message, AssertFlags flags = AssertFlags::hard, SourceLoc source_loc = SourceLoc::Current{}) const
            {
                // No need to wrap this.
                return CheckElemLow(elem,
                    [&](const SingleException &elem)
                    {
                        return elem.message == expected_message;
                    },
                    [&]
                    {
                        return CFG_TA_FMT_NAMESPACE::format("The exception message is not equal to `{}`.", expected_message);
                    },
                    flags, source_loc
                );
            }
            // Checks that the combined exception message is equal to a string.
            Ref CheckMessage(ExceptionElemsCombinedTag, std::string_view expected_message, AssertFlags flags = AssertFlags::hard, std::string_view separator = "\n", SourceLoc source_loc = SourceLoc::Current{}) const
            {
                // No need to wrap this.
                if (auto guard = MakeContextGuard(-1, flags, source_loc))
                {
                    if (CombinedMessage(separator) != expected_message)
                        TA_FAIL(source_loc, "The combined exception message is not equal to `{}`.", expected_message);
                }
            }

            // Checks that the exception message matches the regex.
            // The entire message must match, not just a part of it.
            Ref CheckMessageRegex(/* elem = top_level, */ std::string_view regex, AssertFlags flags = AssertFlags::hard, SourceLoc source_loc = SourceLoc::Current{}) const
            {
                // No need to wrap this.
                return CheckMessageRegex(ExceptionElem::top_level, regex, flags, source_loc);
            }
            // Checks that the exception message matches the regex.
            // The entire message must match, not just a part of it.
            Ref CheckMessageRegex(ExceptionElemVar elem, std::string_view regex, AssertFlags flags = AssertFlags::hard, SourceLoc source_loc = SourceLoc::Current{}) const
            {
                // No need to wrap this.
                std::regex r(text::regex::ConstructRegex(regex));
                return CheckElemLow(elem,
                    [&](const SingleException &elem)
                    {
                        return text::regex::WholeStringMatchesRegex(elem.message, r);
                    },
                    [&]
                    {
                        return CFG_TA_FMT_NAMESPACE::format("The exception message doesn't match regex `{}`.", regex);
                    },
                    flags, source_loc
                );
            }
            // Checks that the combined exception message matches the regex.
            // The entire message must match, not just a part of it.
            Ref CheckMessageRegex(ExceptionElemsCombinedTag, std::string_view regex, AssertFlags flags = AssertFlags::hard, std::string_view separator = "\n", SourceLoc source_loc = SourceLoc::Current{}) const
            {
                // No need to wrap this.
                std::regex r(text::regex::ConstructRegex(regex));
                if (auto guard = MakeContextGuard(-1, flags, source_loc))
                {
                    if (!text::regex::WholeStringMatchesRegex(CombinedMessage(separator), r))
                        TA_FAIL(source_loc, "The combined exception message doesn't match regex `{}`.", regex);
                }
            }

            // Checks that the exception type is exactly `T`.
            template <typename T>
            requires std::is_same_v<T, std::remove_cvref_t<T>>
            Ref CheckExactType(ExceptionElemVar elem = ExceptionElem::top_level, AssertFlags flags = AssertFlags::hard, SourceLoc source_loc = SourceLoc::Current{}) const
            {
                // No need to wrap this.
                return CheckElemLow(elem,
                    [&](const SingleException &elem)
                    {
                        return elem.type == typeid(T);
                    },
                    [&]
                    {
                        return CFG_TA_FMT_NAMESPACE::format("The exception type is not `{}`.", text::TypeName<T>());
                    },
                    flags,
                    source_loc
                );
            }

            // Checks that the exception type derives from `T` (or matches exactly).
            // This also permits some minor implicit conversions, like adding constness to a pointer. Anything that `catch` can do.
            template <typename T>
            requires std::is_same_v<T, std::remove_cvref_t<T>>
            Ref CheckDerivedType(ExceptionElemVar elem = ExceptionElem::top_level, AssertFlags flags = AssertFlags::hard, SourceLoc source_loc = SourceLoc::Current{}) const
            {
                // No need to wrap this.
                return CheckElemLow(elem,
                    [&](const SingleException &elem)
                    {
                        try
                        {
                            // Rethrow if the type is known.
                            if (elem.exception)
                                std::rethrow_exception(elem.exception);
                        }
                        catch (const T &)
                        {
                            return true;
                        }
                        catch (...) {}

                        // Unknown exception types also go here.
                        return false;
                    },
                    [&]
                    {
                        return CFG_TA_FMT_NAMESPACE::format("The exception type is not derived from `{}`.", text::TypeName<T>());
                    },
                    flags,
                    source_loc
                );
            }

            // Mostly for internal use. Other `Check...()` functions are implemented in terms of this one.
            // Calls `func` for one or more elements, depending on `kind`.
            // `func` is `(const SingleException &elem) -> bool`. If it returns false, we fail the test.
            // `message_func` returns the error message. It's only called on failure.
            // When you wrap this into your own function, you
            template <typename F, typename G>
            Ref CheckElemLow(ExceptionElemVar elem, F &&func, G &&message_func, AssertFlags flags = AssertFlags::hard, SourceLoc source_loc = SourceLoc::Current{}) const
            {
                // Don't really need `TA_CONTEXT` here, since we pass `source_loc` directly to `TA_FAIL`.
                if constexpr (IsWrapper)
                {
                    decltype(auto) state = State();
                    state.CheckElemLow(elem, std::forward<F>(func), std::forward<G>(message_func), flags, source_loc);
                    return ReturnedRef(state);
                }
                else
                {
                    if (elem.valueless_by_exception())
                        HardError("Invalid `ExceptionElemVar` variant.");
                    if (!State())
                        TA_FAIL(flags, source_loc, "Attempt to analyze a null `CaughtException`.");
                    if (State()->elems.empty())
                        return ReturnedRef(*this); // This was returned from a failed soft `TA_MUST_THROW`, silently pass all checks.
                    const auto &elems = State()->elems;
                    auto CheckIndex = [&](int index)
                    {
                        // This validates the index for us, and fails the test if out of range.
                        auto context = MakeContextGuard(index, flags, source_loc);
                        if (context && !bool(std::forward<F>(func)(elems[std::size_t(index)])))
                            TA_FAIL(flags, source_loc, "{}", std::forward<G>(message_func)());
                    };
                    std::visit(meta::Overload{
                        [&](ExceptionElem elem)
                        {
                            switch (elem)
                            {
                              case ExceptionElem::top_level:
                                CheckIndex(0);
                                return;
                              case ExceptionElem::most_nested:
                                CheckIndex(int(elems.size() - 1));
                                return;
                              case ExceptionElem::all:
                                for (std::size_t i = 0; i < elems.size(); i++)
                                    CheckIndex(int(i));
                                return;
                              case ExceptionElem::any:
                                {
                                    auto context = MakeContextGuard(-1, flags, source_loc);
                                    if (std::none_of(elems.begin(), elems.end(), func))
                                        TA_FAIL(flags, source_loc, "{}", std::forward<G>(message_func)());
                                }
                                return;
                            }
                            HardError("Invalid `ExceptionElem` enum.", HardErrorKind::user);
                        },
                        [&](int index)
                        {
                            CheckIndex(index);
                        },
                    }, elem);

                    return ReturnedRef(*this);
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
        using base = detail::BasicCaughtExceptionInterface<CaughtException, const CaughtException &, false>;

        // This is a `shared_ptr` to allow `MakeContextGuard()` to outlive this object without causing UB.
        std::shared_ptr<data::CaughtExceptionInfo> state;

        // For the CRTP base.
        friend base;
        const decltype(state) &_get_state() const
        {
            return state;
        }

      public:
        CaughtException() {}

        // This is primarily for internal use.
        CFG_TA_API explicit CaughtException(
            const data::MustThrowStaticInfo *static_info,
            std::weak_ptr<const data::MustThrowDynamicInfo> dynamic_info,
            const std::exception_ptr &e
        );

        // Returns false for default-constructed or moved-from instances.
        // This is not in the base class because it's impossible to call on any other class derived from it,
        //   unless you use the functional notation, which isn't a use case.
        [[nodiscard]] explicit operator bool() const {return bool(state);}

        // Expose protected members:

        using base::MakeContextGuard;
    };

    // Internals of `TA_MUST_THROW(...)`.
    namespace detail
    {
        // `TA_MUST_THROW(...)` expands to this.
        class MustThrowWrapper
        {
            // `final` removes the Clang warning about a non-virtual destructor.
            struct CFG_TA_API_CLASS Info final : data::MustThrowDynamicInfo
            {
                MustThrowWrapper &self;

                data::MustThrowInfo info;

                Info(MustThrowWrapper &self, const data::MustThrowStaticInfo *static_info)
                    : self(self)
                {
                    info.static_info = static_info;
                    info.dynamic_info = this;
                }

                CFG_TA_API std::optional<std::string_view> UserMessage() const;
            };
            // This is a `shared_ptr` to allow us to detect when the object goes out of scope.
            // After that we refuse to evaluate the user message.
            std::shared_ptr<Info> info;

            // This is called to hopefully throw the user exception.
            void (*body_func)(const void *data) = nullptr;
            const void *body_data = nullptr;

            // Call to trigger a breakpoint at the macro call site.
            void (*break_func)() = nullptr;

            // This is called to get the optional user message and flags (null if have none).
            void (*extras_func)(MustThrowWrapper &self, const void *data) = nullptr;
            const void *extras_data = nullptr;
            // The user message.
            std::optional<std::string> user_message;
            AssertFlags flags{};

            template <typename F>
            MustThrowWrapper(const F &func, void (*break_func)(), const data::MustThrowStaticInfo *static_info)
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
            // * Therefore, `TA_MUST_THROW(...)` expands to `~foo(...).DETAIL_TA_ADD_EXTRAS`, and `~` is doing all the work, regardless if there are
            //   parentheses on the right with the user message. `DETAIL_TA_ADD_EXTRAS`, when not expanded as a macro, is an instance of `class Evaluator`.
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
            Evaluator DETAIL_TA_ADD_EXTRAS = *this;

            // Makes an instance of this class.
            template <meta::ConstString File, int Line, meta::ConstString MacroName, meta::ConstString Expr, typename F>
            [[nodiscard]] static MustThrowWrapper Make(const F &func, void (*break_func)())
            {
                static const data::MustThrowStaticInfo info = []{
                    data::MustThrowStaticInfo ret;
                    ret.loc = {File.view(), Line};
                    ret.macro_name = MacroName.view();
                    ret.expr = Expr.view();
                    return ret;
                }();

                return MustThrowWrapper(func, break_func, &info);
            }

            MustThrowWrapper(const MustThrowWrapper &) = delete;
            MustThrowWrapper &operator=(const MustThrowWrapper &) = delete;

            template <typename F>
            Evaluator &AddExtras(const F &func)
            {
                extras_func = [](MustThrowWrapper &self, const void *data)
                {
                    (*static_cast<const F *>(data))(meta::Overload{
                        [&]<typename ...P>(AssertFlags flags)
                        {
                            self.flags = flags;
                        },
                        [&]<typename ...P>(CFG_TA_FMT_NAMESPACE::format_string<P...> format, P &&... args)
                        {
                            self.user_message = CFG_TA_FMT_NAMESPACE::format(std::move(format), std::forward<P>(args)...);
                        },
                        [&]<typename ...P>(AssertFlags flags, CFG_TA_FMT_NAMESPACE::format_string<P...> format, P &&... args)
                        {
                            self.flags = flags; // Do this first, in case message formatting throws.
                            self.user_message = CFG_TA_FMT_NAMESPACE::format(std::move(format), std::forward<P>(args)...);
                        },
                    });
                };
                extras_data = &func;
                return DETAIL_TA_ADD_EXTRAS;
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
        CFG_TA_API ModulePtr();
        ModulePtr(std::nullptr_t) : ModulePtr() {}

        ModulePtr(ModulePtr &&) = default;
        ModulePtr &operator=(ModulePtr &&) = default;

        CFG_TA_API ~ModulePtr();

        [[nodiscard]] explicit operator bool() const {return bool(ptr);}

        [[nodiscard]] BasicModule *get() const {return ptr.get();}
        [[nodiscard]] BasicModule &operator*() const {return *ptr;}
        [[nodiscard]] BasicModule *operator->() const {return ptr.get();}
    };

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
        // If you set `argc` and `argv` to null, does nothing.
        void ProcessFlags(int argc, const char *const *argv, bool *ok = nullptr) const
        {
            ProcessFlags([argc = argc-1, argv = argv+1]() mutable -> std::optional<std::string_view>
            {
                if (argc <= 0)
                    return {};
                argc--;
                return *argv++;
            }, ok);
        }
        // Handles command line arguments from a list of strings, e.g. `ProcessFlags({"--foo", "--blah"});`.
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
        void RemoveModule()
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
        CFG_TA_API void SetOutputStream(FILE *stream) const;

        // Configures every `BasicPrintingModule` to print to `stream`.
        CFG_TA_API void SetEnableColor(bool enable) const;

        // Sets the output stream for every module that prints stuff.
        CFG_TA_API void SetEnableUnicode(bool enable) const;

        // Calls `func` on `Terminal` of every `BasicPrintingModule`.
        CFG_TA_API void SetTerminalSettings(std::function<void(output::Terminal &terminal)> func) const;
    };

    // A simple way to run the tests.
    // Copypaste the body into your code if you need more customization.
    // `argc` and `argv` can be null.
    inline int RunSimple(int argc, const char *const *argv)
    {
        ta_test::Runner runner;
        runner.SetDefaultModules();
        runner.ProcessFlags(argc, argv);
        return runner.Run();
    }
}
