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

#define TA_CHECK(...) CFG_TA_CHECK(#__VA_ARGS__, __VA_ARGS__)
#define CFG_TA_CHECK(x, ...) \
    /* Using `? :` to force the contextual conversion to `bool`. */\
    if (::ta_test::detail::AssertWrapper<x, #__VA_ARGS__, __FILE__, __LINE__>{}((__VA_ARGS__) ? true : false)) {} else {CFG_TA_BREAKPOINT(); std::terminate();}

#define TA_ARG(...) CFG_TA_ARG(__COUNTER__, __VA_ARGS__)
#if CFG_TA_USE_DOLLAR
#define $(...) TA_ARG(__VA_ARGS__)
#endif
#define CFG_TA_ARG(counter, ...) \
    /* Passing `counter` the second time is redundant, but helps with our parsing. */\
    ::ta_test::detail::ArgWrapper(__FILE__, __LINE__, counter)._ta_handle_arg_(counter, __VA_ARGS__)

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

    // Manually support quoting strings if the formatting library can't do that.
    #if !CFG_TA_FORMAT_SUPPORTS_QUOTED
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
    struct ToString<char, Void>
    {
        std::string operator()(char value) const
        {
            char ret[12]; // Should be at most 9: `'\x??'\0`, but throwing in a little extra space.
            detail::formatting::EscapeString({&value, 1}, ret, false);
            return ret;
        }
    };
    #endif

    // Text color.
    // The values are the foreground text colors. Add 10 to make background colors.
    enum class TextColor
    {
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
    };
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

        // Error messages.
        TextStyle style_error = {.color = TextColor::light_red, .bold = true};
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

        struct Chars
        {
            // Vertical bars, either standalone or in brackets.
            char32_t bar = '|';
            // Other parts of brackets.
            char32_t bracket_bottom = '_';
            char32_t bracket_corner_left = '|';
            char32_t bracket_corner_right = '|';

            void SetAscii()
            {
                bar = '|';
                bracket_bottom = '_';
                bracket_corner_left = bracket_corner_right = '|';
            }
        };
        Chars chars;
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
        template <typename F>
        void AnsiDeltaString(const TextStyle &prev, const TextStyle &cur, F &&func)
        {
            if (!config.text_color.value())
                return;

            // Should be large enough.
            char buffer[100];
            std::strcpy(buffer, "\033[");
            char *ptr = buffer + 2;
            if (cur.color != prev.color)
                ptr += std::sprintf(ptr, "%d;", int(cur.color));
            if (cur.bg_color != prev.bg_color)
                ptr += std::sprintf(ptr, "%d;", int(cur.bg_color) + 10);
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
        }

        // Aborts the application with an internal error.
        [[noreturn]] inline void InternalError(std::string_view message)
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
            std::fprintf(stream, " Internal error: %.*s ", int(message.size()), message.data());
            // Reset style.
            std::fprintf(stream, "%s\n", AnsiResetString().data());

            // Stop.
            CFG_TA_BREAKPOINT();
            std::terminate();
        }

        [[nodiscard]] constexpr bool IsWhitespace(char ch)
        {
            return ch == ' ' || ch == '\t';
        }
        [[nodiscard]] constexpr bool IsAlpha(char ch)
        {
            return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
        }
        // Whether `ch` is a letter or an other non-digit identifier character.
        [[nodiscard]] constexpr bool IsNonDigitIdentifierChar(char ch)
        {
            if (ch == '$')
                return true; // Non-standard, but all modern compilers seem to support it, and we use it in our optional short macros.
            return ch == '_' || IsAlpha(ch);
        }
        [[nodiscard]] constexpr bool IsDigit(char ch)
        {
            return ch >= '0' && ch <= '9';
        }
        // Whether `ch` can be a part of an identifier.
        [[nodiscard]] constexpr bool IsIdentifierChar(char ch)
        {
            return IsNonDigitIdentifierChar(ch) || IsDigit(ch);
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
                    InternalError("Line index is out of range.");

                Line &line = lines[line_number];
                if (line.text.size() < size)
                {
                    line.text.resize(size, ' ');
                    line.info.resize(size);
                }
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

            #if 0
            // Applies `IsLineFree` to several lines.
            [[nodiscard]] bool IsRectFree(std::size_t line, std::size_t column, std::size_t height, std::size_t width, std::size_t gap)
            {
                if (line >= lines.size())
                    return true; // Completely below the canvas.

                // Fix `height` to remove redundant comparisons below the canvas.
                if (line + height >/*sic*/ lines.size())
                    height = lines.size() - line;

                for (std::size_t y = line; y < line + height; y++)
                {
                    if (!IsLineFree(y, column, width, gap))
                        return false;
                }
                return true;
            }
            #endif

            // Looks for a free space in the canvas.
            // Searches for `width + gap*2` consecutive cells with `.important == false`.
            // Starts looking at `(column - gap, starting_line)`, and proceeds downwards until it finds the free space,
            // which could be below the canvas.
            [[nodiscard]] std::size_t FindFreeSpace(std::size_t starting_line, std::size_t column, std::size_t width, std::size_t gap) const
            {
                while (true)
                {
                    if (IsLineFree(starting_line, column, width, gap))
                        return starting_line;

                    starting_line++; // Try the next line.
                }
            }

            // Accesses the cell info for the specified cell. The cell must exist.
            [[nodiscard]] CellInfo &CellInfoAt(std::size_t line, std::size_t pos)
            {
                if (line >= lines.size())
                    InternalError("Line index is out of range.");

                Line &this_line = lines[line];
                if (pos >= this_line.info.size())
                    InternalError("Character index is out of range.");

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
                DrawRow(config.chars.bracket_corner_left, line_start + height - 1, column_start, 1, false, info);
                DrawRow(config.chars.bracket_corner_right, line_start + height - 1, column_start + width - 1, 1, false, info);
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

        // `emit_char` is `(char ch, CharKind kind) -> void`.
        // It's called for every character, classifying it.
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
                                    InternalError("Too many nested parentheses.");
                                parens_stack[parens_stack_pos++] = {
                                    .ident = identifier,
                                    .args = &ch + 1,
                                };
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
            bool is_number_suffix = false;
            bool is_string_suffix = false;
            std::size_t raw_string_separator_len = 0;

            CharKind prev_string_kind{}; // One of: `string`, `character`, `raw_string`.

            auto lambda = [&](char ch, CharKind kind)
            {
                CellInfo &info = canvas.CellInfoAt(line, start + i);
                bool is_punct = !IsIdentifierChar(ch);

                if (kind != CharKind::normal)
                {
                    is_number = false;
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

                    if (!is_number)
                    {
                        if (!is_string_suffix && !is_number_suffix && IsDigit(ch))
                        {
                            is_number = true;

                            // Backtrack and make the leading `.` a number too, if it's there.
                            if (i > 0 && expr[i-1] == '.')
                                canvas.CellInfoAt(line, start + i - 1).style = config.style_expr_number;
                        }
                    }
                    else
                    {
                        if (!(IsDigit(ch) || IsAlpha(ch) || ch == '.' || ch == '-' || ch == '+' || ch == '\''))
                        {
                            is_number = false;
                            if (ch == '_')
                                is_number_suffix = true;
                        }
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
                            InternalError("Internal lexer error during pretty-printing.");
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
                                InternalError("Internal lexer error during pretty-printing.");
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
                        InternalError("Internal lexer error during pretty-printing.");
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
                    InternalError("The input string must be null-terminated.");
                std::copy_n(new_str, size, str);
            }

            [[nodiscard]] constexpr std::string_view view() const
            {
                return {str, str + size};
            }
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

                InternalError("Unknown argument.");
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
            const ArgInfo *arg_info,
            const std::size_t *args_in_draw_order,
            const StoredArg *stored_args,
            std::string_view file_name,
            int line_number
        )
        {
            TextCanvas canvas;
            std::size_t line_counter = 0;

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

            canvas.DrawText(line_counter++, 0, "ASSERTION FAILED:", {.style = config.style_error, .important = true});
            line_counter++;

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

            for (std::size_t i = 0; i < num_args; i++)
            {
                const std::size_t arg_index = args_in_draw_order[i];
                const StoredArg &this_arg = stored_args[arg_index];
                const ArgInfo &this_info = arg_info[arg_index];

                if (this_arg.state == StoredArg::State::not_started)
                    return; // This argument wasn't computed.

                bool incomplete = this_arg.state == StoredArg::State::in_progress;
                if (incomplete) // Trying to use a `? :` here leads to a dangling `std::string_view`.
                    this_value = U"...";
                else
                    this_value = uni::Decode(this_arg.value);

                std::size_t value_x = expr_column + this_info.expr_offset + this_info.expr_size / 2 - this_value.size() / 2;
                // Make sure `value_x` didn't underflow.
                if (value_x > std::size_t(-1) / 2)
                    value_x = 0;

                if (!this_info.need_bracket)
                {
                    std::size_t column_x = expr_column + this_info.expr_offset + this_info.expr_size / 2;

                    std::size_t value_y = canvas.FindFreeSpace(line_counter + 1, value_x, this_value.size(), 1);
                    canvas.DrawText(value_y, value_x, this_value);
                    canvas.DrawColumn(config.chars.bar, line_counter, column_x, value_y - line_counter, true);
                }
                else
                {
                    std::size_t bracket_left_x = expr_column + this_info.expr_offset;
                    std::size_t bracket_right_x = bracket_left_x + this_info.expr_size;
                    if (bracket_left_x > 0)
                        bracket_left_x--;

                    std::size_t bracket_y = canvas.FindFreeSpace(line_counter + 1, bracket_left_x, bracket_right_x - bracket_left_x, 0);

                    canvas.DrawHorBracket(line_counter, bracket_left_x, bracket_y - line_counter + 1, bracket_right_x - bracket_left_x);
                    canvas.DrawText(bracket_y + 1, value_x, this_value);
                }
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
                    PrintAssertionFailure(RawString.view(), num_args, arg_data.info.data(), arg_data.args_in_draw_order.data(), stored_args.data(), FileName.view(), LineNumber);

                auto &stack = thread_state.assert_stack;
                if (stack.empty() || stack.back() != this)
                    InternalError("Something is wrong with `TA_CHECK`. Did you `co_await` inside of an assertion?");

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

                // Arguments in the order they should be printed. Which is: highest depth first, then larger counter values first.
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
                        InternalError("More `TA_ARG`s than expected.");

                    ArgInfo &new_info = ret.info[pos];
                    new_info.depth = depth;

                    for (const char &ch : args)
                    {
                        if (IsDigit(ch))
                            new_info.counter = new_info.counter * 10 + (ch - '0');
                        else if (ch == ',')
                            break;
                        else
                            InternalError("Lexer error: Unexpected character after the counter macro.");
                    }

                    CounterIndexPair &new_pair = ret.counter_to_arg_index[pos];
                    new_pair.index = pos;
                    new_pair.counter = new_info.counter;

                    pos++;
                });

                // Parse raw string.
                pos = 0;
                ParseExpr(RawString.view(), nullptr, [&](std::string_view name, std::string_view args, std::size_t depth)
                {
                    (void)depth;

                    if (!IsArgMacroName(name))
                        return;

                    if (pos >= num_args)
                        InternalError("More `TA_ARG`s than expected.");

                    ArgInfo &this_info = ret.info[pos];

                    this_info.expr_offset = std::size_t(args.data() - RawString.view().data());
                    this_info.expr_size = args.size();

                    for (char ch : args)
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
                        return d > 0;
                    return false;
                });

                return ret;
            }();

            StoredArg *GetArgumentDataPtr(const ArgLocation &loc)
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
    }
}
