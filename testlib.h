#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
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


        // --- Visual options ---

        // When printing an assertion macro with all the argument values, it's indented by this amount of spaces.
        std::size_t assertion_macro_indentation = 4;

        // How we call the assertion macro when printing it. You can redefine those if you rename the macro.
        std::u32string assertion_macro_prefix = U"TA_CHECK( ";
        std::u32string assertion_macro_suffix = U" )";

        // When printing a path in a stack, this comes before the path.
        std::u32string filename_prefix = U"  at:  ";
        // When printing a path, separates it from the line number.
        std::u32string filename_linenumber_separator =
        #ifdef _MSC_VER
            U"(";
        #else
            U":";
        #endif
        // When printing a path with a line number, this comes after the line number.
        std::u32string filename_linenumber_suffix =
        #ifdef _MSC_VER
            U") :"; // Huh.
        #else
            U":";
        #endif

        // The message when a test starts.
        TextStyle style_test_started = {.color = TextColor::light_green, .bold = true};
        // The indentation guides for nested test starts.
        TextStyle style_test_started_indentation = {.color = TextColorGrayscale24(8), .bold = true};
        // The test index.
        TextStyle style_test_started_index = {.color = TextColor::light_green, .bold = true};
        // The total test count printed after each test index.
        TextStyle style_test_started_total_count = {.color = TextColor::dark_green};
        // The line that separates the test counter from the test names/groups.
        TextStyle style_test_started_gutter_border = {.color = TextColorGrayscale24(10), .bold = true};

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

        // Error messages.
        TextStyle style_error = {.color = TextColor::light_red, .bold = true};
        // Error messages in the stack, after the first one.
        TextStyle style_stack_error = {.color = TextColor::light_magenta, .bold = true};
        // Paths in the stack traces.
        TextStyle style_stack_path = {.color = TextColor::light_black};
        // The color of `filename_prefix`.
        TextStyle style_stack_path_prefix = {.color = TextColor::light_black, .bold = true};

        // When printing an assertion macro failure, the macro name itself (and parentheses) will use this style.
        TextStyle style_expr_assertion_macro = {.color = TextColor::light_red, .bold = true};
        // A piece of an expression that doesn't fit into the categories below.
        TextStyle style_expr_normal;
        // Punctuation.
        TextStyle style_expr_punct = {.bold = true};
        // Keywords.
        TextStyle style_expr_keyword_generic = {.color = TextColor::light_blue, .bold = true};
        TextStyle style_expr_keyword_value = {.color = TextColor::dark_magenta, .bold = true};
        TextStyle style_expr_keyword_op = {.color = TextColor::light_white, .bold = true};
        // Identifiers written in all caps, probably macros.
        TextStyle style_expr_all_caps = {.color = TextColor::dark_red};
        // Numbers.
        TextStyle style_expr_number = {.color = TextColor::dark_green, .bold = true};
        // User-defined literal on a number, starting with `_`. For my sanity, literals not starting with `_` are colored like the rest of the number.
        TextStyle style_expr_number_suffix = {.color = TextColor::dark_green};
        // A string literal; everything between the quotes inclusive.
        TextStyle style_expr_string = {.color = TextColor::dark_cyan, .bold = true};
        // Stuff before the opening `"`.
        TextStyle style_expr_string_prefix = {.color = TextColor::dark_cyan};
        // Stuff after the closing `"`.
        TextStyle style_expr_string_suffix = {.color = TextColor::dark_cyan};
        // A character literal.
        TextStyle style_expr_char = {.color = TextColor::dark_yellow, .bold = true};
        TextStyle style_expr_char_prefix = {.color = TextColor::dark_yellow};
        TextStyle style_expr_char_suffix = {.color = TextColor::dark_yellow};
        // A raw string literal; everything between the parentheses exclusive.
        TextStyle style_expr_raw_string = {.color = TextColor::light_blue, .bold = true};
        // Stuff before the opening `"`.
        TextStyle style_expr_raw_string_prefix = {.color = TextColor::dark_magenta};
        // Stuff after the closing `"`.
        TextStyle style_expr_raw_string_suffix = {.color = TextColor::dark_magenta};
        // Quotes, parentheses, and everything between them.
        TextStyle style_expr_raw_string_delimiters = {.color = TextColor::dark_magenta, .bold = true};
        // Internal error messages.
        TextStyle style_internal_error = {.color = TextColor::light_white, .bg_color = TextColor::dark_red, .bold = true};

        // Printed characters and strings.
        struct Chars
        {
            // Using narrow strings for strings printed as text, and u32 strings for ASCII graphics things.

            // This goes right before each test/group name.
            std::string starting_test_prefix = "\xE2\x97\x8F "; // BLACK CIRCLE, then a space.
            // The is used for indenting test names/groups.
            std::string starting_test_indent = "\xC2\xB7   "; // MIDDLE DOT, then a space.
            // This is printed after the test counter and before the test names/groups (and before their indentation guides).
            std::string starting_test_counter_separator = " \xE2\x94\x82  "; // BOX DRAWINGS LIGHT VERTICAL, with some spaces around it.

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
        Chars chars;

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
    inline GlobalConfig config;

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

        // Printing this string resets the text styles. It's always null-terminated.
        [[nodiscard]] inline std::string_view AnsiResetString()
        {
            if (config.text_color.value())
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
            if (!config.text_color.value())
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

        // Prints an ANSI sequence to set `style`.
        // Assumes no state was set before.
        // Returns `cur`.
        inline const TextStyle &PrintAnsiDeltaString(const TextStyle &prev_style, const TextStyle &style)
        {
            return AnsiDeltaString(prev_style, style, [&](std::string_view str)
            {
                std::fprintf(config.output_stream, "%s", str.data());
            });
        }

        enum class HardErrorKind {internal, user};

        // Aborts the application with an internal error.
        [[noreturn]] inline void HardError(std::string_view message, HardErrorKind kind = HardErrorKind::internal)
        {
            // A threadsafe once flag.
            bool once = false;
            [[maybe_unused]] static const auto once_trigger = [&]
            {
                once = true;
                return nullptr;
            }();

            if (!once)
                std::terminate(); // We've already been there.

            FILE *stream = config.output_stream;
            if (!stream)
                stream = stdout;
            if (!config.text_color)
                config.text_color = false;

            // Set style.
            AnsiDeltaString({}, config.style_internal_error, [&](std::string_view str)
            {
                std::fprintf(stream, "%s%s", AnsiResetString().data(), str.data());
            });
            // Write message.
            std::fprintf(stream, " %s: %.*s ", kind == HardErrorKind::internal ? "Internal error" : "Error", int(message.size()), message.data());
            // Reset style. Must do it before the newline, otherwise the "core dumped" message also gets colored.
            std::fprintf(stream, "%s\n", AnsiResetString().data());

            // Stop.
            CFG_TA_BREAKPOINT();
            std::terminate();
        }

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
                const bool enable_style = config.text_color.value();

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
            void Print() const
            {
                FILE *stream = config.output_stream;
                PrintToCallback([&](std::string_view string){fwrite(string.data(), string.size(), 1, stream);});
            }

            // Resize the canvas to have at least the specified number of lines.
            void EnsureNumLines(std::size_t size)
            {
                if (lines.size() < size)
                    lines.resize(size);
            }

            // Resize the line to have at least the specified number of characters.
            void EnsureLineSize(std::size_t line_number, std::size_t size)
            {
                if (line_number >= lines.size())
                    HardError("Line index is out of range.");

                Line &line = lines[line_number];
                if (line.text.size() < size)
                {
                    line.text.resize(size, ' ');
                    line.info.resize(size);
                }
            }

            // Inserts the line before the specified line index (or at the bottom of the canvas if given the number of lines).
            void InsertLineBefore(std::size_t line_number)
            {
                if (line_number >/*sic*/ lines.size())
                    HardError("Line number is out of range.");

                lines.insert(lines.begin() + std::ptrdiff_t(line_number), Line{});
            }

            // Whether a cell is free, aka has `.important == false`.
            [[nodiscard]] bool IsCellFree(std::size_t line, std::size_t column) const
            {
                if (line >= lines.size())
                    return true;
                const Line &this_line = lines[line];
                if (column >= this_line.info.size())
                    return true;
                return !this_line.info[column].important;
            }

            // Checks if the space is free in the canvas.
            // Examines a single line (at number `line`), starting at `column - gap`, checking `width + gap*2` characters.
            // Returns false if at least one character has `.important == true`.
            [[nodiscard]] bool IsLineFree(std::size_t line, std::size_t column, std::size_t width, std::size_t gap) const
            {
                // Apply `gap` to `column` and `width`.
                column = gap < column ? column - gap : 0;
                width += gap * 2;

                if (line >= lines.size())
                    return true; // This space is below the canvas height.

                const Line &this_line = lines[line];
                if (this_line.info.empty())
                    return true; // This line is completely empty.

                std::size_t last_column = column + width;
                if (last_column >= this_line.info.size())
                    last_column = this_line.info.size() - 1; // `line.info` can't be empty here.

                bool ok = true;
                for (std::size_t i = column; i < last_column; i++)
                {
                    if (this_line.info[i].important)
                    {
                        ok = false;
                        break;
                    }
                }
                return ok;
            }

            // Looks for a free space in the canvas.
            // Searches for `width + gap*2` consecutive cells with `.important == false`.
            // Starts looking at `(column - gap, starting_line)`, and proceeds downwards until it finds the free space,
            // which could be below the canvas.
            // Moves down in increments of `vertical_step`.
            [[nodiscard]] std::size_t FindFreeSpace(std::size_t starting_line, std::size_t column, std::size_t height, std::size_t width, std::size_t gap, std::size_t vertical_step) const
            {
                std::size_t num_free_lines = 0;
                std::size_t line = starting_line;
                while (true)
                {
                    if (num_free_lines > 0 || (line - starting_line) % vertical_step == 0)
                    {
                        if (!IsLineFree(line, column, width, gap))
                        {
                            num_free_lines = 0;
                        }
                        else
                        {
                            num_free_lines++;
                            if (num_free_lines >= height)
                                return line - height + 1;
                        }
                    }

                    line++; // Try the next line.
                }
            }

            // Accesses the character for the specified cell. The cell must exist.
            [[nodiscard]] char32_t &CharAt(std::size_t line, std::size_t pos)
            {
                if (line >= lines.size())
                    HardError("Line index is out of range.");

                Line &this_line = lines[line];
                if (pos >= this_line.text.size())
                    HardError("Character index is out of range.");

                return this_line.text[pos];
            }

            // Accesses the cell info for the specified cell. The cell must exist.
            [[nodiscard]] CellInfo &CellInfoAt(std::size_t line, std::size_t pos)
            {
                if (line >= lines.size())
                    HardError("Line index is out of range.");

                Line &this_line = lines[line];
                if (pos >= this_line.info.size())
                    HardError("Character index is out of range.");

                return this_line.info[pos];
            }

            // Draws a text.
            // Returns `text.size()`.
            std::size_t DrawText(std::size_t line, std::size_t start, std::u32string_view text, const CellInfo &info = {.style = {}, .important = true})
            {
                EnsureNumLines(line + 1);
                EnsureLineSize(line, start + text.size());
                std::copy(text.begin(), text.end(), lines[line].text.begin() + (std::ptrdiff_t)start);
                for (std::size_t i = start; i < start + text.size(); i++)
                    lines[line].info[i] = info;
                return text.size();
            }
            // Draws a UTF8 text. Returns the text size after converting to UTF32.
            std::size_t DrawText(std::size_t line, std::size_t start, std::string_view text, const CellInfo &info = {.style = {}, .important = true})
            {
                std::u32string decoded_text = uni::Decode(text);
                return DrawText(line, start, decoded_text, info);
            }

            // Draws a horizontal row of `ch`, starting at `(column, line_start)`, of width `width`.
            // If `skip_important == true`, don't overwrite important cells.
            // Returns `width`.
            std::size_t DrawRow(char32_t ch, std::size_t line, std::size_t column, std::size_t width, bool skip_important, const CellInfo &info = {.style = {}, .important = true})
            {
                EnsureNumLines(line + 1);
                EnsureLineSize(line, column + width);
                for (std::size_t i = column; i < column + width; i++)
                {
                    if (skip_important && !IsCellFree(line, i))
                        continue;

                    lines[line].text[i] = ch;
                    lines[line].info[i] = info;
                }

                return width;
            }

            // Draws a vertical column of `ch`, starting at `(column, line_start)`, of height `height`.
            // If `skip_important == true`, don't overwrite important cells.
            void DrawColumn(char32_t ch, std::size_t line_start, std::size_t column, std::size_t height, bool skip_important, const CellInfo &info = {.style = {}, .important = true})
            {
                if (height == 0)
                    return;

                EnsureNumLines(line_start + height);

                for (std::size_t i = line_start; i < line_start + height; i++)
                {
                    if (skip_important && !IsCellFree(i, column))
                        continue;

                    EnsureLineSize(i, column + 1);

                    Line &line = lines[i];
                    line.text[column] = ch;
                    line.info[column] = info;
                }
            }

            // Draws a horziontal bracket: `|___|`. Vertical columns skip important cells, but the bottom bar doesn't.
            // The left column is on `column_start`, and the right one is on `column_start + width - 1`.
            void DrawHorBracket(std::size_t line_start, std::size_t column_start, std::size_t height, std::size_t width, const CellInfo &info = {.style = {}, .important = true})
            {
                if (width < 2 || height < 1)
                    return;

                // Sides.
                if (height > 1)
                {
                    DrawColumn(config.chars.bar, line_start, column_start, height - 1, true, info);
                    DrawColumn(config.chars.bar, line_start, column_start + width - 1, height - 1, true, info);
                }

                // Bottom.
                if (width > 2)
                    DrawRow(config.chars.bracket_bottom, line_start + height - 1, column_start + 1, width - 2, false, info);

                // Corners.
                DrawRow(config.chars.bracket_corner_bottom_left, line_start + height - 1, column_start, 1, false, info);
                DrawRow(config.chars.bracket_corner_bottom_right, line_start + height - 1, column_start + width - 1, 1, false, info);
            }

            // Draws a little 1-high top bracket.
            void DrawOverline(std::size_t line, std::size_t column_start, std::size_t width, const CellInfo &info = {.style = {}, .important = true})
            {
                if (width < 2)
                    return;

                // Middle part.
                if (width > 2)
                    DrawRow(config.chars.bracket_top, line, column_start + 1, width - 2, false, info);

                // Corners.
                DrawRow(config.chars.bracket_corner_top_left, line, column_start, 1, false, info);
                DrawRow(config.chars.bracket_corner_top_right, line, column_start + width - 1, 1, false, info);
            }
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
        inline std::size_t DrawExprToCanvas(TextCanvas &canvas, std::size_t line, std::size_t start, std::string_view expr)
        {
            canvas.DrawText(line, start, expr);
            std::size_t i = 0;
            CharKind prev_kind = CharKind::normal;
            bool is_number = false;
            const char *identifier_start = nullptr;
            bool is_number_suffix = false;
            bool is_string_suffix = false;
            std::size_t raw_string_separator_len = 0;

            CharKind prev_string_kind{}; // One of: `string`, `character`, `raw_string`.

            auto lambda = [&](const char &ch, CharKind kind)
            {
                CellInfo &info = canvas.CellInfoAt(line, start + i);
                bool is_punct = !IsIdentifierChar(ch);

                const char *const prev_identifier_start = identifier_start;

                if (kind != CharKind::normal)
                {
                    is_number = false;
                    identifier_start = nullptr;
                    is_number_suffix = false;
                    is_string_suffix = false;
                }

                // When exiting raw string, backtrack and color the closing sequence.
                if (prev_kind == CharKind::raw_string && kind != CharKind::raw_string)
                {
                    for (std::size_t j = 0; j < raw_string_separator_len; j++)
                        canvas.CellInfoAt(line, start + i - j - 1).style = config.style_expr_raw_string_delimiters;
                }

                switch (kind)
                {
                  case CharKind::normal:
                    if (is_string_suffix && !IsIdentifierChar(ch))
                        is_string_suffix = false;
                    if ((prev_kind == CharKind::string || prev_kind == CharKind::character || prev_kind == CharKind::raw_string) && IsIdentifierChar(ch))
                        is_string_suffix = true;

                    if (is_number_suffix && !IsIdentifierChar(ch))
                        is_number_suffix = false;

                    if (!is_number && !identifier_start && !is_string_suffix && !is_number_suffix)
                    {
                        if (IsDigit(ch))
                        {
                            is_number = true;

                            // Backtrack and make the leading `.` a number too, if it's there.
                            if (i > 0 && expr[i-1] == '.')
                                canvas.CellInfoAt(line, start + i - 1).style = config.style_expr_number;
                        }
                        else if (IsIdentifierChar(ch))
                        {
                            identifier_start = &ch;
                        }
                    }
                    else if (is_number)
                    {
                        if (!(IsDigit(ch) || IsAlpha(ch) || ch == '.' || ch == '-' || ch == '+' || ch == '\''))
                        {
                            is_number = false;
                            if (ch == '_')
                                is_number_suffix = true;
                        }
                    }
                    else if (identifier_start)
                    {
                        if (!IsIdentifierChar(ch))
                            identifier_start = nullptr;
                    }

                    if (is_string_suffix)
                    {
                        switch (prev_string_kind)
                        {
                          case CharKind::string:
                            info.style = config.style_expr_string_suffix;
                            break;
                          case CharKind::character:
                            info.style = config.style_expr_char_suffix;
                            break;
                          case CharKind::raw_string:
                            info.style = config.style_expr_raw_string_suffix;
                            break;
                          default:
                            HardError("Lexer error during pretty-printing.");
                            break;
                        }
                    }
                    else if (is_number_suffix)
                        info.style = config.style_expr_number_suffix;
                    else if (is_number)
                        info.style = config.style_expr_number;
                    else if (is_punct)
                        info.style = config.style_expr_punct;
                    else
                        info.style = config.style_expr_normal;
                    break;
                  case CharKind::string:
                  case CharKind::character:
                  case CharKind::raw_string:
                  case CharKind::raw_string_initial_sep:
                    if (prev_kind != kind && prev_kind != CharKind::raw_string_initial_sep)
                    {
                        if (kind == CharKind::raw_string_initial_sep)
                            prev_string_kind = CharKind::raw_string;
                        else
                            prev_string_kind = kind;

                        // Backtrack and color the prefix.
                        std::size_t j = i;
                        while (j-- > 0 && (IsAlpha(expr[j]) || IsDigit(expr[j])))
                        {
                            TextStyle &target_style = canvas.CellInfoAt(line, start + j).style;
                            switch (prev_string_kind)
                            {
                              case CharKind::string:
                                target_style = config.style_expr_string_prefix;
                                break;
                              case CharKind::character:
                                target_style = config.style_expr_char_prefix;
                                break;
                              case CharKind::raw_string:
                                target_style = config.style_expr_raw_string_prefix;
                                break;
                              default:
                                HardError("Lexer error during pretty-printing.");
                                break;
                            }
                        }
                    }

                    if (kind == CharKind::raw_string_initial_sep)
                    {
                        if (prev_kind != CharKind::raw_string_initial_sep)
                            raw_string_separator_len = 1;
                        raw_string_separator_len++;
                    }

                    switch (kind)
                    {
                      case CharKind::string:
                        info.style = config.style_expr_string;
                        break;
                      case CharKind::character:
                        info.style = config.style_expr_char;
                        break;
                      case CharKind::raw_string:
                      case CharKind::raw_string_initial_sep:
                        if (kind == CharKind::raw_string_initial_sep || prev_kind == CharKind::raw_string_initial_sep)
                            info.style = config.style_expr_raw_string_delimiters;
                        else
                            info.style = config.style_expr_raw_string;
                        break;
                      default:
                        HardError("Lexer error during pretty-printing.");
                        break;
                    }
                    break;
                  case CharKind::string_escape_slash:
                    info.style = config.style_expr_string;
                    break;
                  case CharKind::character_escape_slash:
                    info.style = config.style_expr_char;
                    break;
                }

                // Finalize identifiers.
                if (prev_identifier_start && !identifier_start)
                {
                    const TextStyle *style = nullptr;

                    // Check if this is a keyword.
                    std::string_view ident(prev_identifier_start, &ch);
                    auto it = config.highlighted_keywords.find(ident);
                    if (it != config.highlighted_keywords.end())
                    {
                        switch (it->second)
                        {
                          case KeywordKind::generic:
                            style = &config.style_expr_keyword_generic;
                            break;
                          case KeywordKind::value:
                            style = &config.style_expr_keyword_value;
                            break;
                          case KeywordKind::op:
                            style = &config.style_expr_keyword_op;
                            break;
                        }
                    }
                    else if (std::all_of(ident.begin(), ident.end(), [](char ch){return IsIdentifierChar(ch) && !IsAlphaLowercase(ch);}))
                    {
                        style = &config.style_expr_all_caps;
                    }

                    // If this identifier needs a custom style...
                    if (style)
                    {
                        for (std::size_t j = 0; j < ident.size(); j++)
                            canvas.CellInfoAt(line, start + i - j - 1).style = *style;
                    }
                }

                prev_kind = kind;

                i++;
            };
            ParseExpr(expr, lambda, nullptr);

            return expr.size();
        }

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
        struct ThreadState
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
        inline thread_local ThreadState thread_state;

        // `TA_ARG` expands to this.
        // Stores a pointer into an `AssertWrapper` where it will write the argument as a string.
        struct ArgWrapper
        {
            StoredArg *target = nullptr;

            ArgWrapper(std::string_view file, int line, int counter)
                : target(&thread_state.GetArgumentData(ArgLocation{file, line, counter}))
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
        inline void PrintAssertionFailure(
            std::string_view raw_expr,
            std::size_t num_args,
            const ArgInfo *arg_info,               // Array of size `num_args`.
            const std::size_t *args_in_draw_order, // Array of size `num_args`.
            const StoredArg *stored_args,          // Array of size `num_args`.
            std::string_view file_name,
            int line_number,
            std::size_t depth // Starts at 0, increments when we go deeper into the assertion stack.
        )
        {
            TextCanvas canvas;
            std::size_t line_counter = 0;

            line_counter++;

            if (depth == 0)
                canvas.DrawText(line_counter++, 0, "ASSERTION FAILED:", {.style = config.style_error, .important = true});
            else
                canvas.DrawText(line_counter++, 0, "WHILE CHECKING ASSERTION:", {.style = config.style_stack_error, .important = true});

            { // The file path.
                CellInfo cell_info = {.style = config.style_stack_path, .important = true};
                std::size_t column = 0;
                column += canvas.DrawText(line_counter, column, config.filename_prefix, {.style = config.style_stack_path_prefix, .important = true});
                column += canvas.DrawText(line_counter, column, file_name, cell_info);
                column += canvas.DrawText(line_counter, column, config.filename_linenumber_separator, cell_info);
                column += canvas.DrawText(line_counter, column, std::to_string(line_number), cell_info);
                column += canvas.DrawText(line_counter, column, config.filename_linenumber_suffix, cell_info);
                line_counter++;
            }

            line_counter++;

            std::size_t expr_line = line_counter;

            { // The assertion call.
                std::size_t column = config.assertion_macro_indentation;

                const CellInfo assertion_macro_cell_info = {.style = config.style_expr_assertion_macro, .important = true};
                column += canvas.DrawText(line_counter, column, config.assertion_macro_prefix, assertion_macro_cell_info);
                column += DrawExprToCanvas(canvas, line_counter, column, raw_expr);
                column += canvas.DrawText(line_counter, column, config.assertion_macro_suffix, assertion_macro_cell_info);
                line_counter++;
            }

            std::size_t expr_column = config.assertion_macro_indentation + config.assertion_macro_prefix.size();

            std::u32string this_value;

            // The bracket above the expression.
            std::size_t overline_start = 0;
            std::size_t overline_end = 0;
            // How many subexpressions want an overline.
            // More than one should be impossible, but if it happens, we just combine them into a single fat one.
            int num_overline_parts = 0;

            // Incremented when we print an argument.
            std::size_t color_index = 0;

            for (std::size_t i = 0; i < num_args; i++)
            {
                const std::size_t arg_index = args_in_draw_order[i];
                const StoredArg &this_arg = stored_args[arg_index];
                const ArgInfo &this_info = arg_info[arg_index];

                bool dim_parentheses = true;

                if (this_arg.state == StoredArg::State::in_progress)
                {
                    if (num_overline_parts == 0)
                    {
                        overline_start = this_info.expr_offset;
                        overline_end = this_info.expr_offset + this_info.expr_size;
                    }
                    else
                    {
                        overline_start = std::min(overline_start, this_info.expr_offset);
                        overline_end = std::max(overline_end, this_info.expr_offset + this_info.expr_size);
                    }
                    num_overline_parts++;
                }

                if (this_arg.state == StoredArg::State::done)
                {

                    this_value = uni::Decode(this_arg.value);

                    std::size_t center_x = expr_column + this_info.expr_offset + (this_info.expr_size + 1) / 2 - 1;
                    std::size_t value_x = center_x - (this_value.size() + 1) / 2 + 1;
                    // Make sure `value_x` didn't underflow.
                    if (value_x > std::size_t(-1) / 2)
                        value_x = 0;

                    const CellInfo this_cell_info = {.style = config.style_arguments[color_index++ % config.style_arguments.size()], .important = true};

                    if (!this_info.need_bracket)
                    {
                        std::size_t value_y = canvas.FindFreeSpace(line_counter, value_x, 2, this_value.size(), 1, 2) + 1;
                        canvas.DrawText(value_y, value_x, this_value, this_cell_info);
                        canvas.DrawColumn(config.chars.bar, line_counter, center_x, value_y - line_counter, true, this_cell_info);

                        // Color the contents.
                        for (std::size_t i = 0; i < this_info.expr_size; i++)
                        {
                            TextStyle &style = canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset + i).style;
                            style.color = this_cell_info.style.color;
                            style.bold = true;
                        }
                    }
                    else
                    {
                        std::size_t bracket_left_x = expr_column + this_info.expr_offset;
                        std::size_t bracket_right_x = bracket_left_x + this_info.expr_size + 1;
                        if (bracket_left_x > 0)
                            bracket_left_x--;

                        std::size_t bracket_y = canvas.FindFreeSpace(line_counter, bracket_left_x, 2, bracket_right_x - bracket_left_x, 0, 2);
                        std::size_t value_y = canvas.FindFreeSpace(bracket_y + 1, value_x, 1, this_value.size(), 1, 2);

                        canvas.DrawHorBracket(line_counter, bracket_left_x, bracket_y - line_counter + 1, bracket_right_x - bracket_left_x, this_cell_info);
                        canvas.DrawText(value_y, value_x, this_value, this_cell_info);

                        // Add the tail to the bracket.
                        if (center_x > bracket_left_x && center_x + 1 < bracket_right_x)
                            canvas.CharAt(bracket_y, center_x) = config.chars.bracket_bottom_tail;

                        // Draw the column connecting us to the text, if it's not directly below.
                        canvas.DrawColumn(config.chars.bar, bracket_y + 1, center_x, value_y - bracket_y - 1, true, this_cell_info);

                        // Color the parentheses with the argument color.
                        dim_parentheses = false;
                        canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset - 1).style.color = this_cell_info.style.color;
                        canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset + this_info.expr_size).style.color = this_cell_info.style.color;
                    }
                }

                // Dim the macro name.
                for (std::size_t i = 0; i < this_info.ident_size; i++)
                    canvas.CellInfoAt(line_counter - 1, expr_column + this_info.ident_offset + i).style.color = config.color_dim;

                // Dim the parentheses.
                if (dim_parentheses)
                {
                    canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset - 1).style.color = config.color_dim;
                    canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset + this_info.expr_size).style.color = config.color_dim;
                }
            }

            // The overline.
            if (num_overline_parts > 0)
            {
                if (overline_start > 0)
                    overline_start--;
                overline_end++;

                std::u32string_view this_value = num_overline_parts > 1 ? config.chars.in_this_subexpr_inexact : config.chars.in_this_subexpr;

                std::size_t center_x = expr_column + overline_start + (overline_end - overline_start) / 2;
                std::size_t value_x = center_x - this_value.size() / 2;

                canvas.InsertLineBefore(expr_line++);

                canvas.DrawOverline(expr_line - 1, expr_column + overline_start, overline_end - overline_start, {.style = config.style_overline, .important = true});
                canvas.DrawText(expr_line - 2, value_x, this_value, {.style = config.style_overline, .important = true});

                // Color the parentheses.
                canvas.CellInfoAt(expr_line, expr_column + overline_start).style.color = config.style_overline.color;
                canvas.CellInfoAt(expr_line, expr_column + overline_end - 1).style.color = config.style_overline.color;
            }

            canvas.Print();
        }

        // Returns `value` as is. But before that, prints an error message if it's false.
        template <ConstString RawString, ConstString ExpandedString, ConstString FileName, int LineNumber>
        struct AssertWrapper : BasicAssert
        {
            AssertWrapper()
            {
                thread_state.assert_stack.push_back(this);
            }
            AssertWrapper(const AssertWrapper &) = delete;
            AssertWrapper &operator=(const AssertWrapper &) = delete;

            // Checks `value`, reports an error if it's false. In any case, returns `value` unchanged.
            bool operator()(bool value) &&
            {
                if (!value)
                    thread_state.PrintAssertionFailure(*this);

                auto &stack = thread_state.assert_stack;
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

            // Examines `GetState()` and lists test indices in the preferred execution order.
            [[nodiscard]] std::vector<std::size_t> GetTestListInExecutionOrder() const
            {
                std::vector<std::size_t> ret(tests.size());
                std::iota(ret.begin(), ret.end(), std::size_t(0));

                std::sort(ret.begin(), ret.end(), [&](std::size_t a, std::size_t b)
                {
                    std::string_view name_a = tests[a]->Name();
                    std::string_view name_b = tests[b]->Name();

                    std::string_view::iterator it_a = name_a.begin();
                    std::string_view::iterator it_b = name_b.begin();

                    while (true)
                    {
                        auto new_it_a = std::find(it_a, name_a.end(), '/');
                        auto new_it_b = std::find(it_b, name_b.end(), '/');

                        if (std::string_view(it_a, new_it_a) == std::string_view(it_b, new_it_b))
                        {
                            if ((new_it_a == name_a.end()) != (new_it_b == name_b.end()))
                                HardError("This shouldn't happen. One test name can't be a prefix of another?");
                            if (new_it_a == name_a.end())
                                return false; // Equal.

                            it_a = new_it_a + 1;
                            it_b = new_it_b + 1;
                            continue;
                        }

                        return name_prefixes_to_order.at(std::string_view(name_a.begin(), new_it_a)) <
                            name_prefixes_to_order.at(std::string_view(name_b.begin(), new_it_b));
                    }
                });

                return ret;
            }
        };
        [[nodiscard]] inline GlobalState &GetState()
        {
            static GlobalState ret;
            return ret;
        }

        // Registers a test. Pass a pointer to an instance of `test_singleton<??>`.
        inline void RegisterTest(const BasicTest *singleton)
        {
            GlobalState &state = GetState();

            auto name = singleton->Name();
            auto it = state.name_to_test_index.lower_bound(name);

            if (it != state.name_to_test_index.end())
            {
                if (it->first == name)
                {
                    // This test is already registered. Make sure it comes from the same source file and line, then stop.
                    TestLocation old_loc = state.tests[it->second]->Location();
                    TestLocation new_loc = singleton->Location();
                    if (new_loc != old_loc)
                        HardError(CFG_TA_FORMAT("Conflicting definitions for test `{}`. One at `{}:{}`, another at `{}:{}`.", name, old_loc.file, old_loc.line, new_loc.file, new_loc.line), HardErrorKind::user);
                    return; // Already registered.
                }
                else
                {
                    // Make sure a test name is not also used as a group name.
                    // Note, don't need to check `name.size() > it->first.size()` here, because if it was equal,
                    // we wouldn't enter `else` at all, and if it was less, `.starts_with()` would return false.
                    if (name.starts_with(it->first) && name[it->first.size()] == '/')
                        HardError(CFG_TA_FORMAT("A test name (`{}`) can't double as a category name (`{}`). Append `/something` to the first name.", it->first, name), HardErrorKind::user);
                }
            }

            state.name_to_test_index.try_emplace(name, state.tests.size());
            state.tests.push_back(singleton);

            // Fill `state.name_prefixes_to_order` with all prefixes of this test.
            for (const char &ch : name)
            {
                if (ch == '/')
                    state.name_prefixes_to_order.try_emplace(std::string_view(name.data(), &ch), state.name_prefixes_to_order.size());
            }
            state.name_prefixes_to_order.try_emplace(name, state.name_prefixes_to_order.size());
        }

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

    inline void RunTests()
    {
        const auto &state = detail::GetState();

        auto ordered_tests = state.GetTestListInExecutionOrder();

        std::vector<std::string_view> stack;

        // How much characters in the test counter.
        const int test_counter_width = std::snprintf(nullptr, 0, "%zu", ordered_tests.size());

        std::size_t test_counter = 0;

        for (std::size_t test_index : ordered_tests)
        {
            const detail::BasicTest *test = state.tests[test_index];
            std::string_view test_name = test->Name();

            { // Print the test name, and any prefixes if we're starting a new group.
                auto it = test_name.begin();
                std::size_t segment_index = 0;
                while (true)
                {
                    auto new_it = std::find(it, test_name.end(), '/');

                    std::string_view segment(it, new_it);

                    // Pop the tail off the stack.
                    if (segment_index < stack.size() && stack[segment_index] != segment)
                        stack.resize(segment_index);

                    // Push the segment into the stack, and print a line.
                    if (segment_index >= stack.size())
                    {
                        TextStyle cur_style;

                        // Test index (if this is the last segment).
                        if (new_it == test_name.end())
                        {
                            cur_style = detail::PrintAnsiDeltaString(cur_style, config.style_test_started_index);
                            std::fprintf(config.output_stream, "%*zu", test_counter_width, test_counter + 1);
                            cur_style = detail::PrintAnsiDeltaString(cur_style, config.style_test_started_total_count);
                            std::fprintf(config.output_stream, "/%zu", ordered_tests.size());
                        }
                        else
                        {
                            // No test index, just a gap.
                            std::fprintf(config.output_stream, "%*s", test_counter_width * 2 + 1, "");

                        }

                        // The gutter border.
                        cur_style = detail::PrintAnsiDeltaString(cur_style, config.style_test_started_gutter_border);
                        std::fprintf(config.output_stream, "%.*s", int(config.chars.starting_test_counter_separator.size()), config.chars.starting_test_counter_separator.data());

                        // The indentation.
                        if (!stack.empty())
                        {
                            // Switch to the indentation guide color.
                            cur_style = detail::PrintAnsiDeltaString(cur_style, config.style_test_started_indentation);
                            // Print the required number of guides.
                            for (std::size_t repeat = 0; repeat < stack.size(); repeat++)
                                std::fprintf(config.output_stream, "%s", config.chars.starting_test_indent.c_str());
                        }
                        // Switch to the test name color.
                        cur_style = detail::PrintAnsiDeltaString(cur_style, config.style_test_started);\
                        // Print the test name, and reset the color.
                        std::fprintf(config.output_stream, "%s%.*s%s\n", config.chars.starting_test_prefix.c_str(), int(segment.size()), segment.data(), detail::AnsiResetString().data());

                        // Push to the stack.
                        stack.push_back(segment);
                    }

                    if (new_it == test_name.end())
                        break;

                    segment_index++;
                    it = new_it + 1;
                }
            }

            test->Run();
            test_counter++;
        }
    }
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
