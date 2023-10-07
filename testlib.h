#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdio>
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
