#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Override to change what we call to terminate the application.
#ifndef DETAIL_TA_FAIL
#define DETAIL_TA_FAIL() std::terminate()
#endif

// Whether to define `$(...)` as an alias for `TA_ARG(...)`.
#ifndef DETAIL_TA_USE_DOLLAR
#define DETAIL_TA_USE_DOLLAR 1
#endif

#if DETAIL_TA_USE_DOLLAR
#ifdef __clang__
// We can't `push` and `pop` this, since it has to extend to the user code. And inline `_Pragma` in the macro doesn't work too.
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#endif
#endif

// A comma-separated list of macro names defined to be equivalent to `TA_ARG`.
#ifndef DETAIL_TA_ARG_MACROS
#if DETAIL_TA_USE_DOLLAR
#define DETAIL_TA_ARG_MACROS "TA_ARG", "$"
#else
#define DETAIL_TA_ARG_MACROS "TA_ARG"
#endif
#endif

#if 1
#include <format>
#define DETAIL_TL_FORMAT(...) ::std::format(__VA_ARGS__)
#endif

#define TA_CHECK(...) DETAIL_TA_CHECK(#__VA_ARGS__, __VA_ARGS__)
#define DETAIL_TA_CHECK(x, ...) \
    /* Using `? :` to force the contextual conversion to `bool`. */\
    if (::ta_test::detail::AssertWrapper<x, #__VA_ARGS__, __FILE__, __LINE__>{}((__VA_ARGS__) ? true : false)) {} else {DETAIL_TA_FAIL();}

#define TA_ARG(...) DETAIL_TA_ARG(__COUNTER__, __VA_ARGS__)
#if DETAIL_TA_USE_DOLLAR
#define $(...) TA_ARG(__VA_ARGS__)
#endif
#define DETAIL_TA_ARG(counter, ...) \
    /* Passing `counter` the second time is redundant, but helps with our parsing. */\
    ::ta_test::detail::ArgWrapper(__FILE__, __LINE__, counter)._ta_handle_arg_(counter, __VA_ARGS__)

namespace ta_test
{
    template <typename T, typename = void>
    struct ToString
    {
        std::string operator()(const T &value) const
        {
            return DETAIL_TL_FORMAT("{}", value);
        }
    };

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

        // Printing this string resets the styles. It's always null-terminated.
        [[nodiscard]] static std::string_view ResetString()
        {
            return "\033[0m";
        }

        // If `prev` differs from `*this`, calls `func`, which is `(std::string_view string) -> void`,
        //   with a string printing which performs the requested style change. The string is always null-terminated.
        template <typename F>
        void DeltaString(const TextStyle &prev, F &&func) const
        {
            // Should be large enough.
            char buffer[100];
            std::strcpy(buffer, "\033[");
            char *cur = buffer + 2;
            if (color != prev.color)
                cur += std::sprintf(cur, "%d;", int(color));
            if (bg_color != prev.bg_color)
                cur += std::sprintf(cur, "%d;", int(bg_color) + 10);
            if (bold != prev.bold)
                cur += std::sprintf(cur, "%s;", bold ? "1" : "22"); // Bold text is a little weird.
            if (italic != prev.italic)
                cur += std::sprintf(cur, "%s3;", italic ? "" : "2");
            if (underline != prev.underline)
                cur += std::sprintf(cur, "%s4;", underline ? "" : "2");
            if (cur != buffer + 2)
            {
                // `sprintf` automatically null-terminates the buffer.
                cur[-1] = 'm';
                std::forward<F>(func)(std::string_view(buffer, cur));
            }
        }
    };

    struct GlobalConfig
    {
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

        char ch_underline = '~';
        char ch_underline_start = '^';
        char ch_bar = '|';
    };
    inline GlobalConfig config;

    namespace detail
    {
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
            for (std::string_view alias : std::initializer_list<std::string_view>{DETAIL_TA_ARG_MACROS})
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
                std::string text;
                std::vector<CellInfo> info;
            };
            std::vector<Line> lines;

          public:
            TextCanvas() {}

            // Prints the canvas to a callback `func`, which is `(std::string_view string) -> void`.
            template <typename F>
            void PrintToCallback(bool enable_style, F &&func) const
            {
                TextStyle cur_style;

                for (const Line &line : lines)
                {
                    if (!enable_style)
                    {
                        func(std::string_view(line.text));
                        func(std::string_view("\n"));
                        continue;
                    }

                    std::size_t segment_start = 0;
                    for (std::size_t i = 0; i < line.text.size(); i++)
                    {
                        if (line.text[i] == ' ')
                            continue;

                        line.info[i].style.DeltaString(cur_style, [&](std::string_view escape)
                        {
                            if (i != segment_start)
                            {
                                func(std::string_view(line.text.c_str() + segment_start, i - segment_start));
                                segment_start = i;
                            }
                            func(escape);
                            cur_style = line.info[i].style;
                        });
                    }

                    if (segment_start != line.text.size())
                        func(std::string_view(line.text.c_str() + segment_start, line.text.size() - segment_start));

                    func(std::string_view("\n"));
                }

                if (cur_style != TextStyle{})
                    func(TextStyle::ResetString());
            }

            // Prints to a C stream.
            void Print(bool enable_style, FILE *file) const
            {
                PrintToCallback(enable_style, [&](std::string_view string){fwrite(string.data(), string.size(), 1, file);});
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
                    std::terminate(); // Line index is out of range.

                Line &line = lines[line_number];
                if (line.text.size() < size)
                {
                    line.text.resize(size, ' ');
                    line.info.resize(size);
                }
            }

            // Draws a text.
            void DrawText(std::size_t line, std::size_t start, std::string_view text, const CellInfo &info = {.style = {}, .important = true})
            {
                EnsureNumLines(line + 1);
                EnsureLineSize(line, start + text.size());
                std::copy(text.begin(), text.end(), lines[line].text.begin() + start);
                for (std::size_t i = start; i < start + text.size(); i++)
                    lines[line].info[i] = info;
            }

            // Accesses the cell info for the specified cell. The cell must exist.
            [[nodiscard]] CellInfo &CellInfoAt(std::size_t line, std::size_t pos)
            {
                if (line >= lines.size())
                    std::terminate(); // Out of range.

                Line &this_line = lines[line];
                if (pos >= this_line.info.size())
                    std::terminate(); // Out of range.

                return this_line.info[pos];
            }

            // Draws a `^~~~` underline, starting at `(line, start)` of length `size`.
            void DrawUnderline(std::size_t line, std::size_t start, std::size_t size, const CellInfo &info = {.style = {}, .important = true})
            {
                if (size == 0)
                    return;

                EnsureNumLines(line + 1);
                EnsureLineSize(line, start + size);

                for (std::size_t i = start; i < start + size; i++)
                {
                    Line &this_line = lines[line];
                    this_line.text[i] = i == start ? config.ch_underline_start : config.ch_underline;
                    this_line.info[i] = info;
                }
            }

            // Draws a vertical `|` column, starting at `(hor_pos, line_start)`, of height `height`.
            // The bottom `important_bottom_height` characters will have `.important == true`. If it's larger than the height, all characters will have it.
            void DrawColumn(std::size_t line_start, std::size_t hor_pos, std::size_t height, std::size_t important_bottom_height = std::size_t(-1), const TextStyle &style = {})
            {
                if (height == 0)
                    return;
                if (important_bottom_height > height)
                    important_bottom_height = height;

                EnsureNumLines(line_start + height);

                for (std::size_t i = line_start; i < line_start + height; i++)
                {
                    EnsureLineSize(i, hor_pos + 1);

                    Line &line = lines[i];
                    line.text[hor_pos] = config.ch_bar;

                    CellInfo &info = line.info[hor_pos];
                    info.style = style;
                    info.important = i + important_bottom_height >= line_start + height;
                }
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
        // `function_call` is `(std::string_view name, std::string_view args) -> void`.
        // It's called for every pair of parentheses. `args` is the contents of parentheses, possibly with leading and trailing whitespace.
        // `name` is the identifier preceding the `(`, without whitespace. It can be empty, or otherwise invalid.
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
            std::vector<Entry> parens_stack;

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
                                parens_stack.push_back({
                                    .ident = identifier,
                                    .args = &ch + 1,
                                });
                            }
                            else if (ch == ')' && !parens_stack.empty())
                            {
                                function_call(parens_stack.back().ident, {parens_stack.back().args, &ch});
                                parens_stack.pop_back();
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
        inline void DrawExprToCanvas(TextCanvas &canvas, std::size_t line, std::size_t start, std::string_view expr)
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
                            std::terminate(); // Internal lexer error during pretty-printing.
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
                                std::terminate(); // Internal lexer error during pretty-printing.
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
                        std::terminate(); // Internal lexer error during pretty-printing.
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
                    std::terminate(); // The input string must be null-terminated.
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

                std::terminate(); // Unknown argument.
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
                {
                    int pos = 0;
                    ParseExpr(RawString.view(), nullptr, [&](std::string_view name, std::string_view args)
                    {
                        if (!IsArgMacroName(name))
                            return;

                        std::printf("[%.*s] -> ", int(args.size()), args.data());
                        if (stored_args[pos].state == StoredArg::State::not_started)
                            std::printf("not computed\n");
                        else if (stored_args[pos].state == StoredArg::State::in_progress)
                            std::printf("...\n");
                        else
                            std::printf("`%s`\n", stored_args[pos].value.c_str());

                        pos++;
                    });
                }

                auto &stack = thread_state.assert_stack;
                if (stack.empty() || stack.back() != this)
                {
                    // Something is wrong. Did you `co_await` inside of an assertion?
                    std::terminate();
                }
                stack.pop_back();

                return value;
            }

            // The number of arguments.
            static constexpr int num_args = []{
                int ret = 0;
                ParseExpr(RawString.view(), nullptr, [&](std::string_view name, std::string_view args)
                {
                    (void)args;
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
                int index = 0;
            };
            // Maps `__COUNTER__` values to argument indices.
            static constexpr std::array<CounterIndexPair, num_args> counter_to_arg_index = []{
                std::array<CounterIndexPair, num_args> ret;

                int pos = 0;
                ParseExpr(ExpandedString.view(), nullptr, [&](std::string_view name, std::string_view args)
                {
                    if (name != "_ta_handle_arg_")
                        return;

                    CounterIndexPair &new_pair = ret[pos];
                    new_pair.index = pos++;

                    for (const char &ch : args)
                    {
                        if (IsDigit(ch))
                            new_pair.counter = new_pair.counter * 10 + (ch - '0');
                        else if (ch == ',')
                            break;
                        else
                            std::terminate(); // Parsing error, unexpected character.
                    }
                });

                // Sorting is necessary when the arguments are nested.
                std::sort(ret.begin(), ret.end(), [](const CounterIndexPair &a, const CounterIndexPair &b){return a.counter < b.counter;});

                return ret;
            }();

            StoredArg *GetArgumentDataPtr(const ArgLocation &loc)
            {
                if (loc.line != LineNumber || loc.file != FileName.view())
                    return nullptr;

                auto it = std::partition_point(counter_to_arg_index.begin(), counter_to_arg_index.end(), [&](const CounterIndexPair &pair){return pair.counter < loc.counter;});
                if (it == counter_to_arg_index.end() || it->counter != loc.counter)
                    return nullptr;

                return &stored_args[it->index];
            }
        };
    }
}
