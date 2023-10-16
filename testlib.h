#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <numeric>
#include <optional>
#include <string_view>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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
// This can be empty, we follow up by a forced termination anyway.
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

// Whether to use libfmt.
#ifndef CFG_TA_USE_LIBFMT
#define CFG_TA_USE_LIBFMT 0
#endif

// Which formatting function to use.
#ifndef CFG_TA_FORMAT
#if CFG_TA_USE_LIBFMT
#include <fmt/format.h>
#define CFG_TA_FORMAT(...) ::fmt::format(__VA_ARGS__)
#else
#include <format>
#define CFG_TA_FORMAT(...) ::std::format(__VA_ARGS__)
#endif
#endif

// Whether `{:?}` is a valid format string for strings and characters.
#ifndef CFG_TA_FORMAT_SUPPORTS_QUOTED
#if CFG_TA_CXX_STANDARD >= 23 || CFG_TA_USE_LIBFMT
#define CFG_TA_FORMAT_SUPPORTS_QUOTED 1
#else
#define CFG_TA_FORMAT_SUPPORTS_QUOTED 0
#endif
#endif

#define TA_CHECK(...) DETAIL_TA_CHECK(#__VA_ARGS__, __VA_ARGS__)
#define DETAIL_TA_CHECK(x, ...) \
    /* Using `? :` to force the contextual conversion to `bool`. */\
    if (::ta_test::detail::AssertWrapper<x, #__VA_ARGS__, __FILE__, __LINE__>{}((__VA_ARGS__) ? true : false)) {} else {CFG_TA_BREAKPOINT(); std::terminate();}

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
    inline void _ta_test_func(::ta_test::detail::ConstStringTag<#name>); \
    constexpr auto _ta_registration_helper(::ta_test::detail::ConstStringTag<#name>) -> decltype(void(::std::integral_constant<\
        const std::nullptr_t *, &::ta_test::detail::register_test_helper<\
            ::ta_test::detail::SpecificTest<static_cast<void(*)(::ta_test::detail::ConstStringTag<#name>)>(_ta_test_func), #name, __FILE__, __LINE__>\
        >\
    >{})) {} \
    inline void _ta_test_func(::ta_test::detail::ConstStringTag<#name>)

namespace ta_test
{
    // String conversions.

    template <typename T, typename = void>
    struct ToString
    {
        std::string operator()(const T &value) const
        {
            #if CFG_TA_FORMAT_SUPPORTS_QUOTED
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

    // Styling text.

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

    // Configuration.

    // C++ keyword classification for highlighting.
    enum class KeywordKind {generic, value, op};

    struct GlobalConfig
    {
        // The output stream for all our messages.
        // If null, starting tests initializes it to `stdout`.
        FILE *output_stream = nullptr;

        // Use ANSI sequences to change the text style.
        // If empty, initializing the tests will try to guess it.
        std::optional<bool> text_color;

        // See below.
        CFG_TA_API static std::optional<std::string> DefaultExceptionToMessage(const std::exception_ptr &e);
        // A list of functions to convert exceptions to strings.
        // The first function that returns non-null is used. Throwing from a function is same as returning null.
        std::vector<std::function<std::optional<std::string>(const std::exception_ptr &e)>> exception_to_message = {DefaultExceptionToMessage};

        // --- Visual options ---

        // Text colors and styles.
        struct Style
        {
            // The message when a test starts.
            TextStyle test_started = {.color = TextColor::light_green, .bold = true};
            // The indentation guides for nested test starts.
            TextStyle test_started_indentation = {.color = TextColorGrayscale24(8), .bold = true};
            // The test index.
            TextStyle test_started_index = {.color = TextColor::light_green, .bold = true};
            // The total test count printed after each test index.
            TextStyle test_started_total_count = {.color = TextColor::dark_green};
            // The line that separates the test counter from the test names/groups.
            TextStyle test_started_gutter_border = {.color = TextColorGrayscale24(10), .bold = true};

            // The argument colors. They are cycled in this order.
            std::vector<TextStyle> arguments = {
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
            TextStyle overline = {.color = TextColor::light_magenta, .bold = true};
            // This is used to dim the unwanted parts of expressions.
            TextColor color_dim = TextColor::light_black;

            // Error messages.
            TextStyle assertion_failed = {.color = TextColor::light_red, .bold = true};
            TextStyle test_failed = {.color = TextColor::light_red, .bold = true};
            TextStyle test_failed_exception_message = {.color = TextColor::light_yellow, .bold = true};

            // Error messages in the stack, after the first one.
            TextStyle stack_error = {.color = TextColor::light_magenta, .bold = true};
            // Paths in the stack traces.
            TextStyle stack_path = {.color = TextColor::light_black};
            // The color of `filename_prefix`.
            TextStyle stack_path_prefix = {.color = TextColor::light_black, .bold = true};

            // When printing an assertion macro failure, the macro name itself (and parentheses) will use this style.
            TextStyle expr_assertion_macro = {.color = TextColor::light_red, .bold = true};
            // A piece of an expression that doesn't fit into the categories below.
            TextStyle expr_normal;
            // Punctuation.
            TextStyle expr_punct = {.bold = true};
            // Keywords.
            TextStyle expr_keyword_generic = {.color = TextColor::light_blue, .bold = true};
            TextStyle expr_keyword_value = {.color = TextColor::dark_magenta, .bold = true};
            TextStyle expr_keyword_op = {.color = TextColor::light_white, .bold = true};
            // Identifiers written in all caps, probably macros.
            TextStyle expr_all_caps = {.color = TextColor::dark_red};
            // Numbers.
            TextStyle expr_number = {.color = TextColor::dark_green, .bold = true};
            // User-defined literal on a number, starting with `_`. For my sanity, literals not starting with `_` are colored like the rest of the number.
            TextStyle expr_number_suffix = {.color = TextColor::dark_green};
            // A string literal; everything between the quotes inclusive.
            TextStyle expr_string = {.color = TextColor::dark_cyan, .bold = true};
            // Stuff before the opening `"`.
            TextStyle expr_string_prefix = {.color = TextColor::dark_cyan};
            // Stuff after the closing `"`.
            TextStyle expr_string_suffix = {.color = TextColor::dark_cyan};
            // A character literal.
            TextStyle expr_char = {.color = TextColor::dark_yellow, .bold = true};
            TextStyle expr_char_prefix = {.color = TextColor::dark_yellow};
            TextStyle expr_char_suffix = {.color = TextColor::dark_yellow};
            // A raw string literal; everything between the parentheses exclusive.
            TextStyle expr_raw_string = {.color = TextColor::light_blue, .bold = true};
            // Stuff before the opening `"`.
            TextStyle expr_raw_string_prefix = {.color = TextColor::dark_magenta};
            // Stuff after the closing `"`.
            TextStyle expr_raw_string_suffix = {.color = TextColor::dark_magenta};
            // Quotes, parentheses, and everything between them.
            TextStyle expr_raw_string_delimiters = {.color = TextColor::dark_magenta, .bold = true};
            // Internal error messages.
            TextStyle internal_error = {.color = TextColor::light_white, .bg_color = TextColor::dark_red, .bold = true};
        };
        Style style;

        // Printed characters and strings.
        struct Visual
        {
            // Using narrow strings for strings printed as text, and u32 strings for ASCII graphics things.

            // This goes right before each test/group name.
            std::string starting_test_prefix = "\xE2\x97\x8F "; // BLACK CIRCLE, then a space.
            // The is used for indenting test names/groups.
            std::string starting_test_indent = "\xC2\xB7   "; // MIDDLE DOT, then a space.
            // This is printed after the test counter and before the test names/groups (and before their indentation guides).
            std::string starting_test_counter_separator = " \xE2\x94\x82  "; // BOX DRAWINGS LIGHT VERTICAL, with some spaces around it.

            // A test failed because of an exception.
            std::string test_failed_exception = "TEST FAILED WITH EXCEPTION:\n    ";
            // An assertion failed.
            std::u32string assertion_failed = U"ASSERTION FAILED:";
            // Something happened while evaluating an assertion.
            std::u32string while_checking_assertion = U"WHILE CHECKING ASSERTION:";

            // When printing an assertion macro with all the argument values, it's indented by this amount of spaces.
            std::size_t assertion_macro_indentation = 4;

            // How we call the assertion macro when printing it. You can redefine those if you rename the macro.
            std::u32string assertion_macro_prefix = U"TA_CHECK( ";
            std::u32string assertion_macro_suffix = U" )";

            // When printing a path in a stack, this comes before the path.
            std::u32string filename_prefix = U"  at:  ";
            // When printing a path, separates it from the line number.
            std::u32string filename_linenumber_separator =
            #if CFG_TA_MSVC_STYLE_ERRORS
                U"(";
            #else
                U":";
            #endif
            // When printing a path with a line number, this comes after the line number.
            std::u32string filename_linenumber_suffix =
            #if CFG_TA_MSVC_STYLE_ERRORS
                U") :"; // Huh.
            #else
                U":";
            #endif

            // Vertical bars, either standalone or in brackets.
            char32_t bar = 0x2502; // BOX DRAWINGS LIGHT VERTICAL
            // Bottom brackets.
            char32_t bracket_bottom = 0x2500; // BOX DRAWINGS LIGHT HORIZONTAL
            char32_t bracket_corner_bottom_left = 0x2570; // BOX DRAWINGS LIGHT ARC UP AND RIGHT
            char32_t bracket_corner_bottom_right = 0x256f; // BOX DRAWINGS LIGHT ARC UP AND LEFT
            char32_t bracket_bottom_tail = 0x252c; // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
            // Top brackets.
            char32_t bracket_top = 0x2500; // BOX DRAWINGS LIGHT HORIZONTAL
            char32_t bracket_corner_top_left = 0x256d; // BOX DRAWINGS LIGHT ARC DOWN AND RIGHT
            char32_t bracket_corner_top_right = 0x256e; // BOX DRAWINGS LIGHT ARC DOWN AND LEFT

            // Labels a subexpression that had a nested assertion failure in it.
            std::u32string in_this_subexpr = U"in here";
            // Same, but when there's more than one subexpression. This should never happen.
            std::u32string in_this_subexpr_inexact = U"in here?";

            void SetAscii()
            {
                starting_test_prefix = "* ";
                starting_test_indent = "  ";
                starting_test_counter_separator = " |  ";

                bar = '|';
                bracket_bottom = '_';
                bracket_corner_bottom_left = '|';
                bracket_corner_bottom_right = '|';
                bracket_bottom_tail = '.';
                bracket_top = '-';
                bracket_corner_top_left = '|';
                bracket_corner_top_right = '|';
            }
        };
        Visual vis;

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
    // Returns the config singleton.
    [[nodiscard]] CFG_TA_API GlobalConfig &Config();

    // Misc.

    namespace detail
    {
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

    namespace detail
    {
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
            [[nodiscard]] inline bool IsFirstByte(char byte)
            {
                return (byte & 0b11000000) != 0b10000000;
            }

            // Given the first byte of a multibyte character (or a single-byte character), returns the amount of bytes occupied by the character.
            // Returns 0 if this is not a valid first byte, or not a first byte at all.
            [[nodiscard]] inline std::size_t FirstByteToCharacterLength(char first_byte)
            {
                if ((first_byte & 0b10000000) == 0b00000000) return 1; // Note the different bit pattern in this one.
                if ((first_byte & 0b11100000) == 0b11000000) return 2;
                if ((first_byte & 0b11110000) == 0b11100000) return 3;
                if ((first_byte & 0b11111000) == 0b11110000) return 4;
                return 0;
            }

            // Returns true if `ch` is a valid unicode ch (aka 'codepoint').
            [[nodiscard]] inline bool IsValidCharacterCode(char32_t ch)
            {
                return ch <= 0x10ffff;
            }

            // Returns the amount of bytes needed to represent a character.
            // If the character is invalid (use `IsValidCharacterCode` to check for validity) returns 4, which is the maximum possible length
            [[nodiscard]] inline std::size_t CharacterCodeToLength(char32_t ch)
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
            inline std::size_t Encode(char32_t ch, char *buffer)
            {
                if (!IsValidCharacterCode(ch))
                    return Encode(default_char, buffer);

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

            // Same as `std::size_t Encode(Char ch, char *buffer)`, but appends the data to a string.
            inline std::size_t Encode(char32_t ch, std::string &str)
            {
                char buf[max_char_len];
                std::size_t len = Encode(ch, buf);
                str.append(buf, len);
                return len;
            }

            // Encodes one string into another.
            inline void Encode(std::u32string_view view, std::string &str)
            {
                str.clear();
                str.reserve(view.size() * max_char_len);
                for (char32_t ch : view)
                    Encode(ch, str);
            }

            // Decodes a UTF8 character.
            // Returns a pointer to the first byte of the next character.
            // If `end` is not null, it'll stop reading at `end`. In this case `end` will be returned.
            [[nodiscard]] inline const char *FindNextCharacter(const char *data, const char *end = nullptr)
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
            inline char32_t Decode(const char *data, const char *end = nullptr, const char **next_char = nullptr)
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
            inline void Decode(std::string_view view, std::u32string &str)
            {
                str.clear();
                str.reserve(view.size());
                for (const char *cur = view.data(); cur - view.data() < std::ptrdiff_t(view.size());)
                    str += uni::Decode(cur, view.data() + view.size(), &cur);
            }
            [[nodiscard]] inline std::u32string Decode(std::string_view view)
            {
                std::u32string ret;
                uni::Decode(view, ret);
                return ret;
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
            [[nodiscard]] CFG_TA_API const char *operator()(const char *name);
        };


        // Printing this string resets the text styles. It's always null-terminated.
        [[nodiscard]] inline std::string_view AnsiResetString()
        {
            if (Config().text_color.value())
                return "\033[0m";
            else
                return "";
        }

        // Produces a string to switch between text styles, from `prev` to `cur`.
        // If the styles are the same, does nothing.
        // Otherwise calls `func`, which is `(std::string_view string) -> void`,
        //   with a string printing which performs the requested style change. The string is always null-terminated.
        // In any case, returns `cur`.
        template <typename F>
        const TextStyle &AnsiDeltaString(const TextStyle &prev, const TextStyle &cur, F &&func)
        {
            if (!Config().text_color.value())
                return cur;

            // Should be large enough.
            char buffer[100];
            std::strcpy(buffer, "\033[");
            char *ptr = buffer + 2;
            if (cur.color != prev.color)
            {
                if (cur.color >= TextColor::extended && cur.color < TextColor::extended_end)
                    ptr += std::sprintf(ptr, "38;5;%d;", int(cur.color) - int(TextColor::extended));
                else
                    ptr += std::sprintf(ptr, "%d;", int(cur.color));
            }
            if (cur.bg_color != prev.bg_color)
            {
                if (cur.bg_color >= TextColor::extended && cur.bg_color < TextColor::extended_end)
                    ptr += std::sprintf(ptr, "48;5;%d;", int(cur.bg_color) - int(TextColor::extended));
                else
                    ptr += std::sprintf(ptr, "%d;", int(cur.bg_color) + 10);
            }
            if (cur.bold != prev.bold)
                ptr += std::sprintf(ptr, "%s;", cur.bold ? "1" : "22"); // Bold text is a little weird.
            if (cur.italic != prev.italic)
                ptr += std::sprintf(ptr, "%s3;", cur.italic ? "" : "2");
            if (cur.underline != prev.underline)
                ptr += std::sprintf(ptr, "%s4;", cur.underline ? "" : "2");

            if (ptr != buffer + 2)
            {
                // `sprintf` automatically null-terminates the buffer.
                ptr[-1] = 'm';
                std::forward<F>(func)(std::string_view(buffer, ptr));
            }

            return cur;
        }

        // Prints an ANSI sequence to set `style`, assuming `prev_style` is currently set.
        // Returns `cur`.
        inline const TextStyle &PrintAnsiDeltaString(const TextStyle &prev_style, const TextStyle &style)
        {
            return AnsiDeltaString(prev_style, style, [&](std::string_view str)
            {
                std::fprintf(Config().output_stream, "%s", str.data());
            });
        }

        // Prints an ANSI sequence to set `style`.
        // Returns `cur`.
        inline const TextStyle &PrintAnsiForceString(const TextStyle &style)
        {
            return AnsiDeltaString({}, style, [&](std::string_view str)
            {
                std::fprintf(Config().output_stream, "%s%s", AnsiResetString().data(), str.data());
            });
        }


        enum class HardErrorKind {internal, user};

        // Aborts the application with an internal error.
        [[noreturn]] CFG_TA_API void HardError(std::string_view message, HardErrorKind kind = HardErrorKind::internal);

        // Given an exception, tries to get an error message from it.
        [[nodiscard]] CFG_TA_API std::string ExceptionToMessage(const std::exception_ptr &e);


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

        // Describes a cell in a 2D canvas.
        struct CellInfo
        {
            TextStyle style;
            bool important = false; // If this is true, will avoid overwriting this cell.
        };

        // A class for composing 2D ASCII graphics.
        class TextCanvas
        {
            struct Line
            {
                std::u32string text;
                std::vector<CellInfo> info;
            };
            std::vector<Line> lines;

          public:
            TextCanvas() {}

            // Prints the canvas to a callback `func`, which is `(std::string_view string) -> void`.
            template <typename F>
            void PrintToCallback(F &&func) const
            {
                const bool enable_style = Config().text_color.value();

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

                    if (enable_style)
                    {
                        for (std::size_t i = 0; i < line.text.size(); i++)
                        {
                            if (line.text[i] == ' ')
                                continue;

                            AnsiDeltaString(cur_style, line.info[i].style, [&](std::string_view escape)
                            {
                                FlushSegment(i);
                                func(escape);
                                cur_style = line.info[i].style;
                            });
                        }
                    }

                    FlushSegment(line.text.size());

                    // Reset the style after the last line.
                    // Must do it before the line feed, otherwise the "core dumped" message also gets colored.
                    if (enable_style && &line == &lines.back() && cur_style != TextStyle{})
                        func(AnsiResetString());

                    func(std::string_view("\n"));
                }
            }

            // Prints to the current output stream.
            CFG_TA_API void Print() const;

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

        // Pretty-prints an expression to a canvas.
        // Returns `expr.size()`.
        CFG_TA_API std::size_t DrawExprToCanvas(TextCanvas &canvas, std::size_t line, std::size_t start, std::string_view expr);

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

        // Describes the location of `TA_ARG(...)` in the code.
        struct ArgLocation
        {
            std::string_view file;
            int line = 0;
            int counter = 0;

            friend auto operator<=>(const ArgLocation &, const ArgLocation &) = default;
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

        // Misc information about an argument.
        struct ArgInfo
        {
            // The value of `__COUNTER__`.
            int counter = 0;
            // Parentheses nesting depth.
            std::size_t depth = 0;

            // Where this argument is located in the raw expression string.
            std::size_t expr_offset = 0;
            std::size_t expr_size = 0;
            // This is where the argument macro name is located in the raw expression string.
            std::size_t ident_offset = 0;
            std::size_t ident_size = 0;

            // Whether this argument has a complex enough spelling to require drawing a horizontal bracket.
            // This should be automatically true for all arguments with nested arguments.
            bool need_bracket = false;
        };

        // A common base for `AssertWrapper<T>` instantiations.
        // Describes an assertion that's currently being computed.
        struct BasicAssert
        {
          protected:
            ~BasicAssert() = default;

          public:
            // If `loc` is known, returns a pointer to the respective argument data.
            // Otherwise returns null.
            virtual StoredArg *GetArgumentDataPtr(const ArgLocation &loc) = 0;
            // Print the assertion failure. `depth` starts at 0 and increases as we go deeper into the stack.
            virtual void PrintFailure(std::size_t depth) const = 0;
        };

        // The global per-thread state.
        struct GlobalThreadState
        {
            // The stack of the currently running assertions.
            std::vector<BasicAssert *> assert_stack;

            // Finds an argument in the currently running assertions.
            StoredArg &GetArgumentData(const ArgLocation &loc)
            {
                auto it = assert_stack.end();
                while (true)
                {
                    if (it == assert_stack.begin())
                        break;
                    --it;
                    if (StoredArg *ptr = (*it)->GetArgumentDataPtr(loc))
                        return *ptr;
                }

                HardError("Unknown argument.");
            }

            // Prints the assertion failure.
            // `top` must be the last element of the stack, otherwise an internal error is triggered.
            void PrintAssertionFailure(const BasicAssert &top)
            {
                if (assert_stack.empty() || assert_stack.back() != &top)
                    HardError("The failed assertion is not the top element of the stack.");

                std::size_t depth = 0;
                auto it = assert_stack.end();
                while (it != assert_stack.begin())
                {
                    --it;
                    (*it)->PrintFailure(depth++);
                }
            }
        };
        [[nodiscard]] CFG_TA_API GlobalThreadState &ThreadState();

        // `TA_ARG` expands to this.
        // Stores a pointer into an `AssertWrapper` where it will write the argument as a string.
        struct ArgWrapper
        {
            StoredArg *target = nullptr;

            ArgWrapper(std::string_view file, int line, int counter)
                : target(&ThreadState().GetArgumentData(ArgLocation{file, line, counter}))
            {
                target->state = StoredArg::State::in_progress;
            }
            ArgWrapper(const ArgWrapper &) = default;
            ArgWrapper &operator=(const ArgWrapper &) = default;

            // The method name is wonky to assist with our parsing.
            template <typename T>
            T &&_ta_handle_arg_(int counter, T &&arg) &&
            {
                (void)counter; // Unused, but passing it helps with parsing.
                target->value = ToString<std::remove_cvref_t<T>>{}(arg);
                target->state = StoredArg::State::done;
                return std::forward<T>(arg);
            }
        };

        // Fails an assertion. `AssertWrapper` uses this.
        CFG_TA_API void PrintAssertionFailure(
            std::string_view raw_expr,
            std::size_t num_args,
            const ArgInfo *arg_info,               // Array of size `num_args`.
            const std::size_t *args_in_draw_order, // Array of size `num_args`.
            const StoredArg *stored_args,          // Array of size `num_args`.
            std::string_view file_name,
            int line_number,
            std::size_t depth // Starts at 0, increments when we go deeper into the assertion stack.
        );

        // Returns `value` as is. But before that, prints an error message if it's false.
        template <ConstString RawString, ConstString ExpandedString, ConstString FileName, int LineNumber>
        struct AssertWrapper : BasicAssert
        {
            AssertWrapper()
            {
                ThreadState().assert_stack.push_back(this);
            }
            AssertWrapper(const AssertWrapper &) = delete;
            AssertWrapper &operator=(const AssertWrapper &) = delete;

            // Checks `value`, reports an error if it's false. In any case, returns `value` unchanged.
            bool operator()(bool value) &&
            {
                if (!value)
                    ThreadState().PrintAssertionFailure(*this);

                auto &stack = ThreadState().assert_stack;
                if (stack.empty() || stack.back() != this)
                    HardError("Something is wrong with `TA_CHECK`. Did you `co_await` inside of an assertion?");

                stack.pop_back();

                return value;
            }

            // The number of arguments.
            static constexpr std::size_t num_args = []{
                std::size_t ret = 0;
                ParseExpr(RawString.view(), nullptr, [&](std::string_view name, std::string_view args, std::size_t depth)
                {
                    (void)args;
                    (void)depth;
                    if (IsArgMacroName(name))
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
                std::array<ArgInfo, num_args> info;

                // Maps `__COUNTER__` values to argument indices. Sorted by counter, but the values might not be consecutive.
                std::array<CounterIndexPair, num_args> counter_to_arg_index;

                // Arguments in the order they should be printed. Which is: highest depth first, then smaller counter values first.
                std::array<std::size_t, num_args> args_in_draw_order{};
            };

            static constexpr ArgData arg_data = []{
                ArgData ret;

                // Parse expanded string.
                std::size_t pos = 0;
                ParseExpr(ExpandedString.view(), nullptr, [&](std::string_view name, std::string_view args, std::size_t depth)
                {
                    if (name != "_ta_handle_arg_")
                        return;

                    if (pos >= num_args)
                        HardError("More `TA_ARG`s than expected.");

                    ArgInfo &new_info = ret.info[pos];
                    new_info.depth = depth;

                    for (const char &ch : args)
                    {
                        if (IsDigit(ch))
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
                    HardError("Less `TA_ARG`s than expected.");

                // Parse raw string.
                pos = 0;
                ParseExpr(RawString.view(), nullptr, [&](std::string_view name, std::string_view args, std::size_t depth)
                {
                    (void)depth;

                    if (!IsArgMacroName(name))
                        return;

                    if (pos >= num_args)
                        HardError("More `TA_ARG`s than expected.");

                    ArgInfo &this_info = ret.info[pos];

                    this_info.expr_offset = std::size_t(args.data() - RawString.view().data());
                    this_info.expr_size = args.size();

                    this_info.ident_offset = std::size_t(name.data() - RawString.view().data());
                    this_info.ident_size = name.size();

                    // Trim side whitespace from `args`.
                    std::string_view trimmed_args = args;
                    while (!trimmed_args.empty() && IsWhitespace(trimmed_args.front()))
                        trimmed_args.remove_prefix(1);
                    while (!trimmed_args.empty() && IsWhitespace(trimmed_args.back()))
                        trimmed_args.remove_suffix(1);

                    // Decide if we should draw a bracket for this argument.
                    for (char ch : trimmed_args)
                    {
                        // Whatever the condition is, it should trigger for all arguments with nested arguments.
                        if (!IsIdentifierChar(ch))
                        {
                            this_info.need_bracket = true;
                            break;
                        }
                    }

                    pos++;
                });
                if (pos != num_args)
                    HardError("Less `TA_ARG`s than expected.");

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

                return ret;
            }();

            StoredArg *GetArgumentDataPtr(const ArgLocation &loc) override
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

            void PrintFailure(std::size_t depth) const override
            {
                PrintAssertionFailure(RawString.view(), num_args, arg_data.info.data(), arg_data.args_in_draw_order.data(), stored_args.data(), FileName.view(), LineNumber, depth);
            }
        };


        struct TestLocation
        {
            std::string_view file;
            int line = 0;
            friend auto operator<=>(const TestLocation &, const TestLocation &) = default;
        };

        struct BasicTest
        {
            virtual ~BasicTest() = default;

            // The name passed to the test macro.
            [[nodiscard]] virtual std::string_view Name() const = 0;
            [[nodiscard]] virtual TestLocation Location() const = 0;
            virtual void Run() const = 0;
        };
        // Stores singletons derived from `BasicTest`.
        template <typename T>
        requires std::is_base_of_v<BasicTest, T>
        inline const T test_singleton{};

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

            // Examines `State()` and lists test indices in the preferred execution order.
            [[nodiscard]] CFG_TA_API std::vector<std::size_t> GetTestListInExecutionOrder() const;
        };
        [[nodiscard]] CFG_TA_API GlobalState &State();

        // Registers a test. Pass a pointer to an instance of `test_singleton<??>`.
        CFG_TA_API void RegisterTest(const BasicTest *singleton);

        // An implementation of `BasicTest` for a specific test.
        // `P` is a pointer to the test function, see `DETAIL_TA_TEST()` for details.
        template <auto P, ConstString TestName, ConstString LocFile, int LocLine>
        struct SpecificTest : BasicTest
        {
            static constexpr bool test_name_is_valid = []{
                if (!std::all_of(TestName.view().begin(), TestName.view().end(), [](char ch){return IsIdentifierCharStrict(ch) || ch == '/';}))
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
            TestLocation Location() const override
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

    CFG_TA_API void RunTests();
}

// Manually support quoting strings if the formatting library can't do that.
#if !CFG_TA_FORMAT_SUPPORTS_QUOTED
namespace ta_test
{
    namespace detail::formatting
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
    }

    template <typename Void>
    struct ToString<char, Void>
    {
        std::string operator()(char value) const
        {
            char ret[12]; // Should be at most 9: `'\x??'\0`, but throwing in a little extra space.
            detail::formatting::EscapeString({&value, 1}, ret, false);
            return ret;
        }
    };
    template <typename Void>
    struct ToString<std::string, Void>
    {
        std::string operator()(const std::string &value) const
        {
            std::string ret;
            ret.reserve(value.size() + 2); // +2 for quotes. This assumes the happy scenario without any escapes.
            detail::formatting::EscapeString(value, std::back_inserter(ret), true);
            return ret;
        }
    };
    template <typename Void>
    struct ToString<std::string_view, Void>
    {
        std::string operator()(const std::string_view &value) const
        {
            std::string ret;
            ret.reserve(value.size() + 2); // +2 for quotes. This assumes the happy scenario without any escapes.
            detail::formatting::EscapeString(value, std::back_inserter(ret), true);
            return ret;
        }
    };
    template <typename Void> struct ToString<      char *, Void> {std::string operator()(const char *value) const {return ToString<std::string_view>{}(value);}};
    template <typename Void> struct ToString<const char *, Void> {std::string operator()(const char *value) const {return ToString<std::string_view>{}(value);}};
    // Somehow this catches const arrays too:
    template <std::size_t N, typename Void> struct ToString<char[N], Void> {std::string operator()(const char *value) const {return ToString<std::string_view>{}(value);}};
}
#endif
