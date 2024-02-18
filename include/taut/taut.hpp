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
#include <functional>
#include <initializer_list>
#include <iterator>
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

// A comma-separated list of macro names defined to be equivalent to `TA_ARG`.
// `$` and `TA_ARG` are added automatically.
// NOTE! This should have the same value in all TUs to avoid ODR violations.
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

// Define a test. Must be followed by `{...}`.
// `name` is the test name without quotes and without spaces. You can use letters, digits, and `_`.
// Use `/` as a separator to make test groups: `group/sub_group/test_foo`. There must be no spaces around the slashes.
// The grouping only affects the reporting output (and sometimes the execution order, to run the entire group together).
#define TA_TEST(name) DETAIL_TA_TEST(name)

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
// * `source_loc` should rarely be used. It's an instance of `data::SourceLoc`,
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
// Stops the test immediately, not necessarily failing it.
// Equivalent to throwing `ta_test::InterruptTestException`.
#define TA_INTERRUPT_TEST DETAIL_TA_INTERRUPT_TEST

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
//     TA_MUST_THROW( throw std::runtime_error("Foo!") ).CheckMessage(".*!"); // E.g. you can validate the message with a regex. See `class CaughtException` for more.
//     TA_MUST_THROW( std::vector<int> x; x.at(0); ).CheckMessage(".*!"); // Multiple statements are allowed.
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
#define TA_LOG(...) DETAIL_TA_LOG(__VA_ARGS__)
// Creates a scoped log message. It's printed only if this line is in scope on test failure.
// Unlike `TA_LOG()`, the message can be printed multiple times, if there are multiple failures in this scope.
// The trailing `\n`, if any, is ignored.
// The code calls this a "scoped log", and "context" means something else in the code.
#define TA_CONTEXT(...) DETAIL_TA_CONTEXT(__VA_ARGS__)
// Like `TA_CONTEXT`, but only evaluates the message when needed.
// This means you need to make sure none of your variables dangle, and that they have sane values for the entire lifetime of this context.
// Can evaluate the message more than once. You can utilize this to display the current variable values.
#define TA_CONTEXT_LAZY(...) DETAIL_TA_CONTEXT_LAZY(__VA_ARGS__)

// Repeats the test for all values in the range, which is either a braced list or a C++20 range.
// Example usage: `int x = TA_GENERATE(foo, {1,2,3});`.
// `name` is the name for logging purposes, it must be a valid identifier. It's also used for controlling the generator from the command line.
// You can't use any local variables in `...`, that's a compilation error.
//   This is an artificial limitation for safety reasons, to prevent accidental dangling.
//   Use `TA_GENERATE_FUNC(...)` with `ta_test::RangeToGeneratorFunc(...)` to do that.
// Accepts an optional parameter before the range, of type `ta_test::GeneratorFlags`, same as `RangeToGeneratorFunc()`.
//   E.g. pass `ta_test::interrupt_test_if_empty` to allow empty ranges.
#define TA_GENERATE(name, ...) DETAIL_TA_GENERATE(name, __VA_ARGS__)

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
#define TA_GENERATE_FUNC(name, ...) DETAIL_TA_GENERATE_FUNC(name, __VA_ARGS__)

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
#define TA_GENERATE_PARAM(param, ...) DETAIL_TA_GENERATE_PARAM(param, __VA_ARGS__)

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
#define TA_SELECT(...) DETAIL_TA_SELECT(__VA_ARGS__)
// Marks one of the several code fragments to be executed by `TA_VARIANT(...)`. See that macro for details.
#define TA_VARIANT(name) DETAIL_TA_VARIANT(name)


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

#define DETAIL_TA_TEST(name) \
    inline void _ta_test_func(::ta_test::meta::ConstStringTag<#name>); \
    /* This must be non-inline, because we want to repeat registration for each TU, to detect source location mismatches. */\
    /* But the test body is inline to reduce bloat when tests are in headers. */\
    [[maybe_unused]] static constexpr auto _ta_test_registration_helper(::ta_test::meta::ConstStringTag<#name>) -> decltype(void(::std::integral_constant<\
        const std::nullptr_t *, &::ta_test::detail::register_test_helper<\
            ::ta_test::detail::SpecificTest<static_cast<void(*)(\
                ::ta_test::meta::ConstStringTag<#name>\
            )>(_ta_test_func),\
            []{CFG_TA_BREAKPOINT();},\
            #name, __FILE__, __LINE__>\
        >\
    >{})) {} \
    inline void _ta_test_func(::ta_test::meta::ConstStringTag<#name>)

#define DETAIL_TA_CHECK(macro_name_, str_, ...) \
    /* `~` is what actually performs the asesrtion. We need something with a high precedence. */\
    ~::ta_test::detail::AssertWrapper<macro_name_, str_, #__VA_ARGS__, __FILE__, __LINE__>(\
        [&]([[maybe_unused]]::ta_test::detail::BasicAssertWrapper &_ta_assert){_ta_assert.EvalCond(__VA_ARGS__);},\
        []{CFG_TA_BREAKPOINT();}\
    )\
    .DETAIL_TA_ADD_EXTRAS

#define DETAIL_TA_FAIL DETAIL_TA_CHECK("", "", false)

#define DETAIL_TA_INTERRUPT_TEST (throw ::ta_test::InterruptTestException{})

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
    ::ta_test::detail::AddLogEntry(CFG_TA_FMT_NAMESPACE::format(__VA_ARGS__))
#define DETAIL_TA_CONTEXT(...) \
    ::ta_test::detail::ScopedLogGuard DETAIL_TA_CAT(_ta_context,__COUNTER__)(CFG_TA_FMT_NAMESPACE::format(__VA_ARGS__))
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
        /* Another lambda to convert arguments to strings. */\
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
        test_failed = 1, // One or more tests failed.
        bad_command_line_arguments = 2, // A generic issue with command line arguments.
        no_test_name_match = 3, // `--include` or `--exclude` didn't match any tests.
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
    // You're expected to use CTAD with this. We go to lengths to ensure zero moves for the functor, which is we need.
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

            // Skips whitespace characters, if any.
            constexpr void SkipWhitespace(const char *&ch)
            {
                while (IsWhitespace(*ch))
                    ch++;
            }

            // Advances `ch` until one of the characters in `sep` is found, or until an unbalanced closing bracket (one of: `)]}`).
            // Then gives back the trailing whitespace, if any.
            // But we ignore the contents of "..." and '...' strings, and ignore matching characters inside of `(...)`, `[...]`, or `{...}`.
            // We also refuse to break on an opening bracket if it's the first non-whitespace chracter.
            // We don't check the type of brackets, treating them all as equivalent, but if we find an unbalanced closing bracket, we stop immediately.
            constexpr void TryFindUnprotectedSeparator(const char *&ch, std::string_view sep)
            {
                const char *const first_ch = ch;
                SkipWhitespace(ch);
                const char *const first_nonwhitespace_ch = ch;

                char quote_ch = '\0';
                int depth = 0;

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
                        {
                            // Found separator.

                            // Refuse to break if it's the first non-whitespace character and an opening bracket.
                            if (!(first_nonwhitespace_ch == ch && (*ch == '(' || *ch == '[' || *ch == '{')))
                                break;
                        }

                        if (*ch == '"' || *ch == '\'')
                        {
                            quote_ch = *ch;
                        }
                        else if (*ch == '(' || *ch == '[' || *ch == '{')
                        {
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
            // Escapes a string, appends the result to `output`. Includes quotes automatically.
            CFG_TA_API void EscapeString(std::string_view source, std::string &output, bool double_quotes);

            // Unescapes a string.
            // Appends the result to `output`. Returns the error message on failure, or empty string on success.
            // Tries to support all standard escapes, except for `\N{...}` named characters, because that would be stupid.
            // We also don't support the useless `\?`.
            // If `quote_char` isn't zero, we expect it before and after the string.
            // If `only_single_char` is true, will write at most one character (exactly one on success).
            CFG_TA_API std::string UnescapeString(const char *&source, std::string &output, char quote_char, bool only_single_char);
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
        // NOTE: This ignores cvref-qualifiers (because `typeid` does too).
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
            // `function_call` is `(bool exiting, std::string_view name, std::string_view args, std::size_t depth) -> void`.
            // It's called for every pair of parentheses. `args` is the contents of parentheses, possibly with leading and trailing whitespace.
            // `name` is the identifier preceding the `(`, without whitespace. It can be empty, or otherwise invalid.
            // `depth` is the parentheses nesting depth, starting at 0.
            // It's called both when entering parentheses (`exiting` == false, `args` == "") and when exiting them (`exiting` == true).
            // If `function_call_uses_brackets` is true, `function_call` expects square brackets instead of parentheses.
            template <typename EmitCharFunc, typename FunctionCallFunc>
            constexpr void ParseExpr(std::string_view expr, EmitCharFunc &&emit_char, bool function_call_uses_brackets, FunctionCallFunc &&function_call)
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
                            // to allow function calls with whitespace (and/or `)`, even) between the identifier and `(`.
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
                                if (ch == "(["[function_call_uses_brackets])
                                {
                                    if (parens_stack_pos >= std::size(parens_stack))
                                        HardError("Too many nested parentheses.");

                                    function_call(false, identifier, {}, parens_stack_pos);

                                    parens_stack[parens_stack_pos++] = {
                                        .ident = identifier,
                                        .args = &ch + 1,
                                    };
                                    identifier = {};
                                }
                                else if (ch == ")]"[function_call_uses_brackets] && parens_stack_pos > 0)
                                {
                                    parens_stack_pos--;
                                    function_call(true, parens_stack[parens_stack_pos].ident, std::string_view(parens_stack[parens_stack_pos].args, &ch), parens_stack_pos);
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
        requires std::is_same_v<T, std::remove_cvref_t<T>>
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
        // We throw in some other formatters just for consistency.
        // Bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=112832
        template <> struct DefaultToStringTraits<std::string > : DefaultToStringTraits<std::string_view> {};
        template <> struct DefaultToStringTraits<      char *> : DefaultToStringTraits<std::string_view> {};
        template <> struct DefaultToStringTraits<const char *> : DefaultToStringTraits<std::string_view> {};
        // Somehow this catches const arrays too.
        template <std::size_t N> struct DefaultToStringTraits<char[N]> : DefaultToStringTraits<std::string_view> {};

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
        template <typename T> requires std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>
        struct DefaultMaybeLazyToString<T>
        {
            std::string operator()(const T &source) const {return std::string(source);}
        };


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
            [[nodiscard]] CFG_TA_API std::string operator()(char &target, const char *&string) const;
        };

        // Single character.
        template <>
        struct DefaultFromStringTraits<std::nullptr_t>
        {
            [[nodiscard]] CFG_TA_API std::string operator()(std::nullptr_t &target, const char *&string) const;
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
                    return text::escape::UnescapeString(string, target, '"', false);
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
    // Normally runs the callback at least once. The last element may or may not have `IsTypeKnown() == false`, and other elements will always be known.
    // If `e` is null, does nothing (doesn't not run the callback).
    CFG_TA_API void AnalyzeException(const std::exception_ptr &e, const std::function<void(SingleException elem)> &func);


    // --- DATA TYPES ---

    // Various runtime data types. Should only be useful if you're writing custom modules.
    namespace data
    {
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

        // A compile-time description of a single `TA_TEST(...)`.
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
            std::vector<const BasicTestInfo *> failed_tests;

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

        // Information about the expression argument of `TA_CHECK(...)`, both compile-time and runtime.
        struct BasicAssertionExpr
        {
          protected:
            ~BasicAssertionExpr() = default;

          public:
            // The exact code passed to the assertion macro, as a string. Before macro expansion.
            [[nodiscard]] virtual std::string_view Expr() const = 0;

            // How many `$[...]` arguments are in this assertion.
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
                // This should be automatically true for all arguments with nested arguments inside of them.
                bool need_bracket = false;
            };

            // The current runtime state of the argument.
            enum class ArgState
            {
                not_started, // No value yet.
                in_progress, // Started calculating, but no value yet.
                done, // Has value.
            };

            // Information about each argument. The size of this is `NumArgs()`.
            [[nodiscard]] virtual std::span<const ArgInfo> ArgsInfo() const = 0;
            // Indices of the arguments (0..N-1), sorted in the preferred draw order. The size of this is `NumArgs()`.
            [[nodiscard]] virtual std::span<const std::size_t> ArgsInDrawOrder() const = 0;
            // The current state of an argument.
            // Causes a hard error if the index is out of range.
            [[nodiscard]] virtual ArgState CurrentArgState(std::size_t index) const = 0;
            // Returns the string representation of an argument.
            // Causes a hard error if the index is out of range, if the argument state isn't equal to `done`.
            // For some types this is lazy, and computes the string the first time it's called.
            [[nodiscard]] virtual const std::string &CurrentArgValue(std::size_t index) const = 0;
        };
        // Information about a single `TA_CHECK(...)` call, both compile-time and runtime.
        struct BasicAssertionInfo : context::BasicFrame
        {
            // You can set this to true to trigger a breakpoint.
            mutable bool should_break = false;

            // The enclosing assertion, if any.
            const BasicAssertionInfo *enclosing_assertion = nullptr;

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
            struct DecoExprWithArgs {const BasicAssertionExpr *expr = nullptr;};
            // `std::monostate` indicates that there is no more elements.
            using DecoVar = std::variant<std::monostate, DecoFixedString, DecoExpr, DecoExprWithArgs>;
            // Returns one of the elements to be printed.
            [[nodiscard]] virtual DecoVar GetElement(int index) const = 0;
        };

        // A compile-time information about a single `TA_MUST_THROW(...)` call.
        struct MustThrowStaticInfo
        {
            // The source location of `TA_MUST_THROW(...)`.
            data::SourceLoc loc;
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
            CFG_TA_API CaughtExceptionContext(std::shared_ptr<const CaughtExceptionInfo> state, int active_elem, AssertFlags flags, data::SourceLoc source_loc);
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
            [[nodiscard]] virtual const data::SourceLocWithCounter &GetLocation() const = 0;
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

            // The generator flags.
            [[nodiscard]] virtual GeneratorFlags GetFlags() const = 0;

            // Whether the last generated value is the last one for this generator.
            // Note that when the generator is operating under a module override, the system doesn't respect this variable (see below).
            [[nodiscard]] bool IsLastValue() const {return !repeat || callback_threw_exception;}

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
            const BasicTestInfo *test = nullptr;

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
        // The global per-thread state.
        struct GlobalThreadState
        {
            data::RunSingleTestResults *current_test = nullptr;
            data::BasicAssertionInfo *current_assertion = nullptr;

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
    }

    namespace platform
    {
        // Whether the debugger is currently attached.
        // `false` if unknown or disabled with `CFG_TA_DETECT_DEBUGGER`
        CFG_TA_API bool IsDebuggerAttached();

        // Whether stdout (or stderr, depending on the argument) is attached to a terminal.
        CFG_TA_API bool IsTerminalAttached(bool is_stderr);
    }


    // --- STACK TRACE HELPERS ---

    // Don't use this, use `Trace<...>` defined below.
    class BasicTrace : public context::BasicFrame, public context::FrameGuard
    {
        bool accept_args = false;
        data::SourceLoc loc;
        std::string_view func;
        struct Args
        {
            std::vector<std::string> func_args;
            std::vector<std::string> template_args;
        };
        Args arg_storage;
        // If specified, we don't use `arg_storage` and write everything to this.
        Args *arg_override = nullptr;

        Args &GetArgs() {return arg_override ? *arg_override : arg_storage;}
        const Args &GetArgs() const {return const_cast<BasicTrace &>(*this).GetArgs();}

      protected:
        BasicTrace() : FrameGuard(nullptr) {}

        BasicTrace(bool push_to_stack, bool accept_args, Args *write_args_to, std::string_view file, int line, std::string_view func)
            : FrameGuard(push_to_stack ? std::shared_ptr<const BasicFrame>(std::shared_ptr<void>{}, this) : nullptr), // A non-owning shared pointer.
            accept_args(accept_args), loc{file, line}, func(func), arg_override(write_args_to)
        {}

        BasicTrace(std::string_view file, int line, std::string_view func)
            : BasicTrace(true, true, nullptr, file, line, func)
        {}

        struct HideParams
        {
            const BasicTrace *target = nullptr;
        };

        BasicTrace(HideParams hide)
            : BasicTrace(false, false, nullptr, hide.target->loc.file, hide.target->loc.line, hide.target->func)
        {}

        struct ExtractArgsParams
        {
            BasicTrace *target = nullptr;
        };

        BasicTrace(ExtractArgsParams extract)
            : BasicTrace(false, extract.target->accept_args, &extract.target->GetArgs(), extract.target->loc.file, extract.target->loc.line, extract.target->func)
        {}

      public:
        BasicTrace &operator=(const BasicTrace &) = delete;
        BasicTrace &operator=(BasicTrace &&) = delete;

        // `operator bool` is inherited. It returns false when constructed from `.Hide()` or `.ExtractArgs()`.

        [[nodiscard]] const data::SourceLoc &GetLocation() const {return loc;}
        [[nodiscard]] const std::vector<std::string> &GetFuncArgs() const {return GetArgs().func_args;}
        [[nodiscard]] const std::vector<std::string> &GetTemplateArgs() const {return GetArgs().template_args;}
        [[nodiscard]] std::string_view GetFuncName() const {return func;}

        // Pass this to the constructor of `Trace` to disable tracing the callee.
        // This also copies the location information to the callee.
        [[nodiscard]] HideParams Hide() const
        {
            return {this};
        }

        // Pass this to the constructor of `Trace` to disable the tracing of the callee and make it write its arguments into the current object.
        // This also copies the location information to the callee.
        [[nodiscard]] ExtractArgsParams ExtractArgs()
        {
            return {this};
        }

        template <typename ...P>
        BasicTrace &AddArgs(const P &... args)
        {
            if (accept_args)
                (void(GetArgs().func_args.push_back(string_conv::ToString(args))), ...);
            return *this;
        }

        template <typename ...P>
        BasicTrace &AddTemplateTypes()
        {
            if (accept_args)
                (void(GetArgs().template_args.push_back(text::TypeName<P>())), ...);
            return *this;
        }
        template <typename ...P>
        BasicTrace &AddTemplateValues(const P &... args)
        {
            if (accept_args)
                (void(GetArgs().template_args.push_back(string_conv::ToString(args))), ...);
            return *this;
        }
    };
    // Add this as the last function parameter: `Trace<"MyFunc"> trace = {}` to show this function call as the context when a test fails.
    // You can optionally do `trace.AddArgs/AddTemplateTypes/AddTemplateValues()` to also display information about the function arguments.
    // Those functions are not lazy, and calculate the new strings immediately, so don't overuse them.
    // You can construct from a different trace in several different ways:
    // * From `outer_trace.Hide()`. This will disable tracing the callee.
    // * From `outer_trace.ExtractArgs()`. This will disable tracing the callee, and will redirect all arguments from it to `outer_trace`.
    // You can call the inherited `Reset()` to disarm a `Trace` to remove it from the stack.
    template <meta::ConstString FuncName>
    class Trace : public BasicTrace
    {
      public:
        // Default constructor - traces normally.
        // Don't pass any arguments manually.
        Trace(std::string_view file = __builtin_FILE(), int line = __builtin_LINE(), std::string_view func = FuncName.view())
            : BasicTrace(file, line, func)
        {}
        // This doesn't trace, but copies the location information to the callee.
        // Usage: `foo(..., /*.trace=*/trace.ExtractArgs())`.
        Trace(HideParams hide) : BasicTrace(hide) {}
        // This doesn't trace, but writes function arguments to the specified instace of `Trace`. Also copies the location information to the callee.
        // Usage: `foo(..., /*.trace=*/trace.ExtractArgs())`.
        Trace(ExtractArgsParams extract) : BasicTrace(extract) {}
    };


    // Macro internals, except `TA_MUST_THROW(...)`.
    namespace detail
    {
        // --- ASSERTIONS ---

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

        struct ArgMetadata
        {
            data::BasicAssertionExpr::ArgState state = data::BasicAssertionExpr::ArgState::not_started;

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

        // `TA_ARG` ultimately expands to this.
        // Stores a pointer to a `StoredArg` in an `AssertWrapper` where it will write the argument as a string.
        struct ArgWrapper
        {
            data::BasicAssertionInfo *assertion = nullptr;
            ArgBuffer *target_buffer = nullptr;
            ArgMetadata *target_metadata = nullptr;

            // Raises a hard error if the assertion owning this argument isn't currently running in this thread.
            CFG_TA_API void EnsureAssertionIsRunning();

            ArgWrapper(data::BasicAssertionInfo &assertion, ArgBuffer &target_buffer, ArgMetadata &target_metadata)
                : assertion(&assertion), target_buffer(&target_buffer), target_metadata(&target_metadata)
            {
                EnsureAssertionIsRunning();
                target_metadata.state = data::BasicAssertionExpr::ArgState::in_progress;
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
                    target_metadata->StoreValue(*target_buffer, std::as_const(arg));
                    target_metadata->to_string_func = [](ArgMetadata &self, ArgBuffer &buffer) -> const std::string &
                    {
                        // Convert to a string.
                        std::string string = string_conv::ToString(*std::launder(reinterpret_cast<type *>(buffer.buffer)));

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

                target_metadata->state = data::BasicAssertionExpr::ArgState::done;
                return std::forward<T>(arg);
            }
        };

        // An intermediate base class that `AssertWrapper<T>` inherits from.
        // You can also inherit custom assertion classes from this, if they don't need the expression decomposition provided by `AssertWrapper<T>`.
        class BasicAssertWrapper : public data::BasicAssertionInfo
        {
            bool condition_value = false;

            // User condition was evaluated to completion.
            bool condition_value_known = false;

            // This is called to get the optional user message, flags, etc (null if none of that).
            void (*extras_func)(BasicAssertWrapper &self, const void *data) = nullptr;
            const void *extras_data = nullptr;
            // The user message (if any) is written here on failure.
            std::optional<std::string> user_message;

            // This is only set on failure.
            AssertFlags flags{};

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

                    thread_state.current_assertion = const_cast<data::BasicAssertionInfo *>(self.enclosing_assertion);
                }
            };

            // This is invoked when the assertion finishes evaluating.
            struct Evaluator
            {
                BasicAssertWrapper &self;
                CFG_TA_API bool operator~();
            };

          protected:
            // This is called to evaluate the user condition.
            void (*condition_func)(BasicAssertWrapper &self, const void *data) = nullptr;
            const void *condition_data = nullptr;

            // Call to trigger a breakpoint at the macro call site.
            void (*break_func)() = nullptr;

            // This can be overridden on failure, but not necessarily. Otherwise (and by defualt) points to the actual location.
            data::SourceLoc source_loc;

          public:
            // Note the weird variable name, it helps with our macro syntax that adds optional messages.
            Evaluator DETAIL_TA_ADD_EXTRAS{*this};

            BasicAssertWrapper() {}

            BasicAssertWrapper(const BasicAssertWrapper &) = delete;
            BasicAssertWrapper &operator=(const BasicAssertWrapper &) = delete;

            template <typename T>
            void EvalCond(T &&value)
            {
                condition_value = (std::forward<T>(value) ? true : false); // Using `? :` to force a contextual bool conversion.
                condition_value_known = true;
            }

            template <typename F>
            Evaluator &AddExtras(const F &func)
            {
                extras_func = [](BasicAssertWrapper &self, const void *data)
                {
                    (*static_cast<const F *>(data))(meta::Overload{
                        [&]<typename ...P>(AssertFlags flags)
                        {
                            self.flags = flags;
                        },
                        [&]<typename ...P>(AssertFlags flags, data::SourceLoc loc)
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
                        [&]<typename ...P>(AssertFlags flags, data::SourceLoc loc, CFG_TA_FMT_NAMESPACE::format_string<P...> format, P &&... args)
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

            CFG_TA_API const data::SourceLoc &SourceLocation() const override;
            CFG_TA_API std::optional<std::string_view> UserMessage() const override;

            virtual ArgWrapper _ta_arg_(int counter) = 0;
        };

        template <meta::ConstString MacroName, meta::ConstString RawString, meta::ConstString ExpandedString, meta::ConstString FileName, int LineNumber>
        requires
            // Check (at compile-time) that `$[...]` weren't expanded too early by another macro.
            (RawString.view().find("_ta_arg_(") == std::string_view::npos)
        struct AssertWrapper : BasicAssertWrapper, data::BasicAssertionExpr
        {
            template <typename F>
            AssertWrapper(const F &func, void (*breakpoint_func)())
            {
                condition_func = [](BasicAssertWrapper &self, const void *data)
                {
                    return (*static_cast<const F *>(data))(self);
                };
                condition_data = &func;

                break_func = breakpoint_func;

                // This constructor is not in the parents because of this line. We don't know the source location in the parent.
                source_loc = {.file = FileName.view(), .line = LineNumber};
            }

            // The number of arguments.
            static constexpr std::size_t num_args = []{
                std::size_t ret = 0;
                text::expr::ParseExpr(RawString.view(), nullptr, true, [&](bool exiting, std::string_view name, std::string_view args, std::size_t depth)
                {
                    (void)args;
                    (void)depth;
                    if (!exiting && text::chars::IsArgMacroName(name))
                        ret++;
                });
                return ret;
            }();

            // The values of the arguments.
            // Mutable because those sometimes use lazy to-string conversion.
            mutable std::array<ArgBuffer, num_args> stored_arg_buffers;
            mutable std::array<ArgMetadata, num_args> stored_arg_metadata;

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
                    // Below we parse the expression twice. The first parse triggers the callback at the beginning of `$[...]`,
                    // and the second triggers it at the end of `$[...]`. This creates more work for us, but can't be fixed without
                    // wrapping the whole argument of `$[...]` in a macro call (which is incompatible with using `[...]`, which we want because they look better).

                    // Parse expanded string.
                    std::size_t pos = 0;
                    text::expr::ParseExpr(ExpandedString.view(), nullptr, false, [&](bool exiting, std::string_view name, std::string_view args, std::size_t depth)
                    {
                        (void)depth;

                        if (!exiting || name != "_ta_arg_")
                            return;

                        if (pos >= num_args)
                            HardError("More `$[...]`s than expected.");

                        ArgInfo &new_info = ret.info[pos];

                        // Note: Can't fill `new_info.depth` here, because the parentheses only container the counter, and not the actual `$[...]` argument.
                        // We fill the depth during the next pass.

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
                        HardError("Less `$[...]`s than expected.");

                    // This stack maps bracket depth to the element index, so that the second pass processes things in the same order as the first pass.
                    // This only matters for nested brackets.
                    std::size_t bracket_stack[num_args]{};

                    // Parse raw string.
                    pos = 0;
                    std::size_t stack_depth = 0;
                    text::expr::ParseExpr(RawString.view(), nullptr, true, [&](bool exiting, std::string_view name, std::string_view args, std::size_t depth)
                    {
                        // This `depth` is useless to us, because it also counts the user parentheses.
                        (void)depth;

                        if (!text::chars::IsArgMacroName(name))
                            return;

                        if (!exiting)
                        {
                            if (pos >= num_args)
                                HardError("More `$[...]`s than expected.");

                            bracket_stack[stack_depth++] = pos++;
                            return;
                        }

                        ArgInfo &this_info = ret.info[bracket_stack[--stack_depth]];

                        this_info.depth = stack_depth;

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
                    });
                    if (pos != num_args)
                        HardError("Less `$[...]`s than expected.");

                    // Sort `counter_to_arg_index` by counter, to allow binary search.
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
                        // if (auto d = ret.info[a].counter <=> ret.info[b].counter; d != 0)
                        //     return d < 0;
                        return false;
                    });
                }

                return ret;
            }();

            ~AssertWrapper() noexcept
            {
                // Destroy the stored arguments.
                for (std::size_t i = num_args; i-- > 0;)
                    stored_arg_metadata[i].Destroy(stored_arg_buffers[i]);
            }

            std::string_view Expr() const override {return RawString.view();}
            std::span<const ArgInfo> ArgsInfo() const override {return arg_data.info;}
            std::span<const std::size_t> ArgsInDrawOrder() const override {return arg_data.args_in_draw_order;}

            ArgState CurrentArgState(std::size_t index) const override
            {
                if (index >= num_args)
                    HardError("Assertion argument index is out of range.");
                return stored_arg_metadata[index].state;
            }

            const std::string &CurrentArgValue(std::size_t index) const override
            {
                // Check if this argument was actually computed. This also validates the index for us.
                if (CurrentArgState(index) != ArgState::done)
                    HardError("This argument wasn't computed yet.");
                return stored_arg_metadata[index].to_string_func(stored_arg_metadata[index], stored_arg_buffers[index]);
            }

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

            [[nodiscard]] ArgWrapper _ta_arg_(int counter) override
            {
                auto it = std::partition_point(arg_data.counter_to_arg_index.begin(), arg_data.counter_to_arg_index.end(),
                    [&](const CounterIndexPair &pair){return pair.counter < counter;}
                );
                if (it == arg_data.counter_to_arg_index.end() || it->counter != counter)
                    HardError("`TA_CHECK` isn't aware of this `$[...]`.");

                return {*this, stored_arg_buffers[it->index], stored_arg_metadata[it->index]};
            }
        };

        // --- TESTS ---

        struct BasicTest : data::BasicTestInfo
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
            data::SourceLoc Location() const override
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
            meta::ConstString Name, meta::ConstString LocFile, int LocLine, int LocCounter, typename F,
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

            static constexpr data::SourceLocWithCounter location = {
                data::SourceLoc{
                    .file = LocFile.view(),
                    .line = LocLine,
                },
                LocCounter,
            };

            const data::SourceLocWithCounter &GetLocation() const override
            {
                return location;
            }

            std::string_view GetName() const override
            {
                return Name.view();
            }

            [[nodiscard]] GeneratorFlags GetFlags() const override final
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
            data::SourceLocWithCounter source_loc;

            // The caller places the current generator here.
            data::BasicGenerator *untyped_generator = nullptr;

            // The caller sets this if a new generator is being created. In that case it must have the same value as `untyped_generator`.
            // Note that `HandleGenerator()` moves from this variable, so it's always null in the destructor.
            std::unique_ptr<data::BasicGenerator> created_untyped_generator;

            CFG_TA_API GenerateValueHelper(data::SourceLocWithCounter source_loc);

            GenerateValueHelper(const GenerateValueHelper &) = delete;
            GenerateValueHelper &operator=(const GenerateValueHelper &) = delete;

            CFG_TA_API ~GenerateValueHelper();
            CFG_TA_API void HandleGenerator();
        };

        // `TA_GENERATE_FUNC(...)` expands to this.
        // `func` returns the user lambda (it's not the user lambda itself).
        template <
            // Manually specified:
            meta::ConstString Name, meta::ConstString LocFile, int LocLine, int LocCounter,
            // Deduced:
            typename F
        >
        requires(text::chars::IsIdentifierStrict(Name.view()))
        [[nodiscard]] auto GenerateValue(F &&func)
            -> const typename SpecificGenerator<Name, LocFile, LocLine, LocCounter, F>::return_type &
        {
            using GeneratorType = SpecificGenerator<Name, LocFile, LocLine, LocCounter, F>;

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
                if (!bool(new_generator->GetFlags() & GeneratorFlags::new_value_when_revisiting))
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
            std::string operator()(meta::PreferenceTagA) const
            {
                // Prefer `ToString()` if supported.
                // Otherwise MSVC prints integers in hex, which is stupid.
                std::string value = string_conv::ToString(_ta_test_NameOf);

                // Prepend the type if necessary.
                // Note, we don't need to remove constness here, it seems `decltype()` can never return const-qualified types here.
                if constexpr (string_conv::ClarifyTypeInMixedTypeContexts<decltype(_ta_test_NameOf)>::value)
                    return CFG_TA_FMT_NAMESPACE::format("({}){}", text::TypeName<decltype(_ta_test_NameOf)>(), value);
                else
                    return value;
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
        template <meta::ConstString LocFile, int LocLine, int LocCounter, meta::ConstString Name, auto ListLambda, typename NameLambda>
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

                auto index = (GenerateValue<Name, LocFile, LocLine, LocCounter>)(
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
        template <meta::ConstString LocFile, int LocLine, int LocCounter, meta::ConstString Name>
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
        template <meta::ConstString LocFile, int LocLine, int LocCounter, meta::ConstString Name>
        requires(text::chars::IsIdentifierStrict(Name.view()))
        class VariantGenerator
        {
            using IndexType = VariantIndex<LocFile, LocLine, LocCounter, Name>;

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
                    && bool(dynamic_cast<const SpecificGenerator<Name, LocFile, LocLine, LocCounter, GeneratorFunctor> *>(
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
                    return Enum(GenerateValue<Name, LocFile, LocLine, LocCounter>(GeneratorFunctor{*this}).value);
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
                // Prepare a list of names. Stable-sort by decreasing length, to give longer strings priority.
                static const std::vector<std::pair<std::string_view, std::size_t>> list = []{
                    std::vector<std::pair<std::string_view, std::size_t>> ret;
                    ret.reserve(N);
                    for (std::size_t i = 0; i < N; i++)
                        ret.emplace_back(NameLambda{}(i), i);
                    std::stable_sort(ret.begin(), ret.end(), [](const auto &a, const auto &b){return a.first.size() > b.first.size();});
                    return ret;
                }();

                auto iter = std::find_if(list.begin(), list.end(), [&](const auto &p)
                {
                    for (std::size_t i = 0; i < p.first.size(); i++)
                    {
                        if (p.first[i] != string[i] || string[i] == '\0')
                            return false;
                    }
                    return true;
                });

                if (iter == list.end())
                {
                    std::string error = "Expected one of: ";

                    // To make sure we don't print the same element twice.
                    std::set<std::string_view> elems;

                    for (std::size_t i = 0; i < N; i++)
                    {
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
            [[nodiscard]] data::CaughtExceptionContext MakeContextGuard(int index = -1, AssertFlags flags = {}, data::SourceLoc source_loc = {__builtin_FILE(), __builtin_LINE()}) const
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

            // Checks that the exception message matches the regex.
            // The entire message must match, not just a part of it.
            Ref CheckMessage(/* elem = top_level, */ std::string_view regex, AssertFlags flags = AssertFlags::hard, Trace<"CheckMessage"> trace = {}) const
            {
                // No need to wrap this.
                trace.AddArgs(regex, flags);
                return CheckMessage(ExceptionElem::top_level, regex, flags, trace.Hide());
            }
            // Checks that the exception message matches the regex.
            // The entire message must match, not just a part of it.
            Ref CheckMessage(ExceptionElemVar elem, std::string_view regex, AssertFlags flags = AssertFlags::hard, Trace<"CheckMessage"> trace = {}) const
            {
                // No need to wrap this.
                trace.AddArgs(elem, regex, flags);
                std::regex r(regex.begin(), regex.end());
                return CheckElemLow(elem,
                    [&](const SingleException &elem)
                    {
                        return std::regex_match(elem.message, r);
                    },
                    [&]
                    {
                        return CFG_TA_FMT_NAMESPACE::format("The exception message doesn't match the regex `{}`.", regex);
                    },
                    flags, trace.Hide()
                );
            }

            // Checks that the exception type is exactly `T`.
            template <typename T>
            requires std::is_same_v<T, std::remove_cvref_t<T>>
            Ref CheckExactType(ExceptionElemVar elem = ExceptionElem::top_level, AssertFlags flags = AssertFlags::hard, Trace<"CheckExactType"> trace = {}) const
            {
                // No need to wrap this.
                trace.AddTemplateTypes<T>();
                trace.AddArgs(elem, flags);
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
                    trace.Hide()
                );
            }

            // Checks that the exception type derives from `T` (or matches exactly).
            // This also permits some minor implicit conversions, like adding constness to a pointer. Anything that `catch` can do.
            template <typename T>
            requires std::is_same_v<T, std::remove_cvref_t<T>>
            Ref CheckDerivedType(ExceptionElemVar elem = ExceptionElem::top_level, AssertFlags flags = AssertFlags::hard, Trace<"CheckDerivedType"> trace = {}) const
            {
                // No need to wrap this.
                trace.AddTemplateTypes<T>();
                trace.AddArgs(elem, flags);
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
                    trace.Hide()
                );
            }

            // Mostly for internal use. Other `Check...()` functions are implemented in terms of this one.
            // Calls `func` for one or more elements, depending on `kind`.
            // `func` is `(const SingleException &elem) -> bool`. If it returns false, we fail the test.
            // `message_func` returns the error message. It's only called on failure.
            // When you wrap this into your own function, you
            template <typename F, typename G>
            Ref CheckElemLow(ExceptionElemVar elem, F &&func, G &&message_func, AssertFlags flags = AssertFlags::hard, Trace<"CheckElemLow"> trace = {}) const
            {
                if constexpr (IsWrapper)
                {
                    decltype(auto) state = State();
                    state.CheckElemLow(elem, std::forward<F>(func), std::forward<G>(message_func), flags, trace.ExtractArgs());
                    return ReturnedRef(state);
                }
                else
                {
                    if (elem.valueless_by_exception())
                        HardError("Invalid `ExceptionElemVar` variant.");
                    if (!State())
                        TA_FAIL(flags, trace.GetLocation(), "Attempt to analyze a null `CaughtException`.");
                    if (State()->elems.empty())
                        return ReturnedRef(*this); // This was returned from a failed soft `TA_MUST_THROW`, silently pass all checks.
                    const auto &elems = State()->elems;
                    auto CheckIndex = [&](int index)
                    {
                        // This validates the index for us, and fails the test if out of range.
                        auto context = MakeContextGuard(index, flags);
                        if (context && !bool(std::forward<F>(func)(elems[std::size_t(index)])))
                            TA_FAIL(flags, trace.GetLocation(), "{}", std::forward<G>(message_func)());
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
                                    auto context = MakeContextGuard(-1, flags);
                                    if (std::none_of(elems.begin(), elems.end(), func))
                                        TA_FAIL(flags, trace.GetLocation(), "{}", std::forward<G>(message_func)());
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
            struct Info final : data::MustThrowDynamicInfo
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

            // This is called to get the optional user message (null if no message).
            std::string (*extras_func)(MustThrowWrapper &self, const void *data) = nullptr;
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
                    ret.loc = {.file = File.view(), .line = Line};
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
                    std::string ret;
                    (*static_cast<const F *>(data))(meta::Overload{
                        [&]<typename ...P>(AssertFlags flags)
                        {
                            self.flags = flags;
                        },
                        [&]<typename ...P>(CFG_TA_FMT_NAMESPACE::format_string<P...> format, P &&... args)
                        {
                            ret = CFG_TA_FMT_NAMESPACE::format(std::move(format), std::forward<P>(args)...);
                        },
                        [&]<typename ...P>(AssertFlags flags, CFG_TA_FMT_NAMESPACE::format_string<P...> format, P &&... args)
                        {
                            self.flags = flags; // Do this first, in case message formatting throws.
                            ret = CFG_TA_FMT_NAMESPACE::format(std::move(format), std::forward<P>(args)...);
                        },
                    });
                    return ret;
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
    inline int RunSimple(int argc, char **argv)
    {
        ta_test::Runner runner;
        runner.SetDefaultModules();
        runner.ProcessFlags(argc, argv);
        return runner.Run();
    }
}
