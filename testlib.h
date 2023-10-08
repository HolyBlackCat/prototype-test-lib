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

    namespace detail
    {
        [[nodiscard]] constexpr bool IsWhitespace(char ch)
        {
            return ch == ' ' || ch == '\t';
        }
        [[nodiscard]] constexpr bool IsDigit(char ch)
        {
            return ch >= '0' && ch <= '9';
        }
        // Whether `ch` can be a part of an identifier.
        [[nodiscard]] constexpr bool IsIdentifierChar(char ch)
        {
            if (ch == '$')
                return true; // Non-standard, but all modern compilers seem to support it, and we use it in our optional short macros.
            return ch == '_' || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || IsDigit(ch);
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

            // Draws a text with custom styling for each character. `func` is `(std::size_t i) -> CellInfo`.
            template <typename F>
            void DrawRichText(std::size_t line, std::size_t start, std::string_view text, F &&func)
            {
                EnsureNumLines(line + 1);
                EnsureLineSize(line, start + text.size());
                std::copy(text.begin(), text.end(), lines[line].text.begin() + start);
                for (std::size_t i = 0; i < text.size(); i++)
                    lines[line].info[start + i] = func(std::size_t(i));
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
                    this_line.text[i] = i == start ? '^' : '~';
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
                    line.text[hor_pos] = '|';

                    CellInfo &info = line.info[hor_pos];
                    info.style = style;
                    info.important = i + important_bottom_height >= line_start + height;
                }
            }
        };

        enum CharKind
        {
            normal,
            string, // A string or character literal, or a raw string.
            number, // A numeric literal.
        };

        // `emit_char` is `(char ch, CharKind kind) -> void`.
        // It's called for every character, classifying it.
        // `function_call` is `(std::string_view name, std::string_view args) -> void`.
        // It's called for every pair of parentheses. `args` is the contents of parentheses, possibly with leading and trailing whitespace.
        // `name` is the identifier preceding the `(`, without whitespace. It can be empty, or otherwise invalid.
        template <typename EmitCharFunc, typename FunctionCallFunc>
        constexpr void ParseExpr(std::string_view expr, EmitCharFunc &&emit_char, FunctionCallFunc &&function_call)
        {
            enum class State
            {
                normal,
                string_literal, // String or character literal, but not a raw string literal.
                escape_seq, // Escape sequence in a `string_literal`.
                raw_string_initial_sep, // Reading the initial separator of a raw string.
                raw_string, // In the middle of a raw string.
            };
            State state = State::normal;

            // The previous character.
            char prev_ch = '\0';
            // The current identifier. Only makes sense in `state == normal`.
            std::string_view identifier;
            // Either `"` or `'` that ends the current string/char literal.
            char string_literal_end_quote{};
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
                const State prev_state = state;

                switch (state)
                {
                  case State::normal:
                    if (ch == '"' && prev_ch == 'R')
                    {
                        state = State::raw_string_initial_sep;
                        raw_string_sep_start = &ch + 1;
                    }
                    else if (ch == '"' || ch == '\'')
                    {
                        state = State::string_literal;
                        string_literal_end_quote = ch;
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
                  case State::string_literal:
                    if (ch == string_literal_end_quote)
                        state = State::normal;
                    else if (ch == '\\')
                        state = State::escape_seq;
                    break;
                  case State::escape_seq:
                    state = State::string_literal;
                    break;
                  case State::raw_string_initial_sep:
                    if (ch == '(')
                    {
                        state = State::raw_string;
                        raw_string_sep = {raw_string_sep_start, &ch};
                    }
                    break;
                  case State::raw_string:
                    if (ch == '"')
                    {
                        std::string_view content(raw_string_sep_start, &ch);
                        if (content.size() >/*sic*/ raw_string_sep.size() && content[content.size() - raw_string_sep.size() - 1] == ')' && content.ends_with(raw_string_sep))
                            state = State::normal;
                    }
                    break;
                }

                if (prev_state != State::normal && state == State::normal)
                    identifier = {};

                CharKind kind = CharKind::normal;
                if (state != State::normal || (state == State::normal && (prev_state == State::string_literal || prev_state == State::raw_string)))
                    kind = CharKind::string;
                else if (!identifier.empty() && IsDigit(identifier.front()))
                    kind = CharKind::number;

                if constexpr (!std::is_null_pointer_v<EmitCharFunc>)
                    emit_char(ch, kind);

                prev_ch = ch;
            }
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
