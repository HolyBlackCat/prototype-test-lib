#pragma once

#include <taut/taut.hpp>

#include <any>

// You only need to include this header if you want to access the individual modules, or write your own ones.

namespace ta_test
{
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
            char short_flag = '\0'; // Zero if none.

            using Callback = std::function<void(const Runner &runner, BasicModule &this_module)>;
            Callback callback;

            // `short_flag` can be zero if none.
            SimpleFlag(std::string flag, char short_flag, std::string help_desc, Callback callback)
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
                return ret + "--" + flag;
            }

            bool ProcessFlag(const Runner &runner, BasicModule &this_module, std::string_view input, std::function<std::optional<std::string_view>()> request_arg) override
            {
                (void)request_arg;

                // Short form.
                if (short_flag && input.size() == 2 && input[0] == '-' && input[1] == short_flag)
                {
                    callback(runner, this_module);
                    return true;
                }

                // Long form.
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
        [[nodiscard]] virtual std::vector<flags::BasicFlag *> GetFlags() noexcept {return {};}
        // This is called when an unknown flag is passed to the command line.
        // `abort` defaults to true. If it remains true after this is called on all modules, the application is terminated.
        virtual void OnUnknownFlag(std::string_view flag, bool &abort) noexcept {(void)flag; (void)abort;}
        // Same, but for when a flag lacks an argument.
        virtual void OnMissingFlagArgument(std::string_view flag, const flags::BasicFlag &flag_obj, bool &abort) noexcept {(void)flag; (void)flag_obj; (void)abort;}

        // --- RUNNING TESTS ---

        enum class TestFilterState
        {
            enabled,
            disabled,
            disabled_in_source, // The test is disabled in the source code with the `disabled` flag.
        };

        // Whether the test should run.
        // This is called once for every test, with `state` initially set to `enabled` (or `disabled_in_source` is the test has the `disabled` flag).
        // If `state` ends up as `enabled`, the test will run.
        virtual void OnFilterTest(const data::BasicTest &test, TestFilterState &state) noexcept {(void)test; (void)state;}

        // This is called first, before any tests run.
        virtual void OnPreRunTests(const data::RunTestsInfo &data) noexcept {(void)data;}
        // This is called after all tests run.
        virtual void OnPostRunTests(const data::RunTestsResults &data) noexcept {(void)data;}

        // This is called before every single test runs.
        virtual void OnPreRunSingleTest(const data::RunSingleTestInfo &data) noexcept {(void)data;}
        // This is called after every single test runs.
        // The generators can be in weird state at this point. Interact with them in `OnPostGenerate()` and in `OnPreFailTest()` instead.
        virtual void OnPostRunSingleTest(const data::RunSingleTestResults &data) noexcept {(void)data;}

        // This is called after every `TA_GENERATE(...)`.
        virtual void OnPostGenerate(const data::GeneratorCallInfo &data) noexcept {(void)data;}

        // Return true if you want this module to have special control over this generator.
        // If you do this, you must override `OnGeneratorOverride()`, see below.
        // This also changes the behavior of `TA_GENERATE(...)` slightly, it will generate new values between tests and
        //   not when the control flow reaches it (except for the first time it's reached).
        virtual bool OnRegisterGeneratorOverride(const data::RunSingleTestProgress &test, const data::BasicGenerator &generator) noexcept {(void)test; (void)generator; return false;}
        // If you returned true from `OnRegisterGeneratorOverride()`, this function will be called instead of `generator.Generate()`.
        // You must call `generator.Generate()` (possibly several times to skip values) or `generator.ReplaceValueFromString()`.
        // Returning true from this means that there's no more values (unlike non-overridden generators, we can back out from a generation without knowing
        //   which value is the last one beforehand).
        // You must return true from this when the generator is exhausted, `IsLastValue()` is ignored when an override is active.
        virtual bool OnOverrideGenerator(const data::RunSingleTestProgress &test, data::BasicGenerator &generator) noexcept {(void)test; (void)generator; return false;}
        // This is called right before a generator is popped from the stack, because it has no more values.
        virtual void OnPrePruneGenerator(const data::RunSingleTestProgress &test) noexcept {(void)test;}

        // --- FAILING TESTS ---

        // This is called when a test fails for any reason, followed by a more specific callback (see below).
        // Note that the test can continue to run after this, if this is a delayed (soft) failure.
        // Note that this is called at most once per test, even if after a soft failure something else fails.
        virtual void OnPreFailTest(const data::RunSingleTestProgress &data) noexcept {(void)data;}

        // Called when an assertion fails.
        virtual void OnAssertionFailed(const data::BasicAssertion &data) noexcept {(void)data;}

        // Called when an exception falls out of an assertion or out of the entire test (in the latter case `assertion` will be null).
        // `assertion` is provided solely to allow you to do `assertion->should_break = true`. If you just want to print the failure context,
        // use `namespace context` instead, it will give you the same assertion and more.
        virtual void OnUncaughtException(const data::RunSingleTestInfo &test, const data::BasicAssertion *assertion, const std::exception_ptr &e) noexcept {(void)test; (void)assertion; (void)e;}

        // This is called when `TA_MUST_THROW` doesn't throw an exception.
        virtual void OnMissingException(const data::MustThrowInfo &data) noexcept {(void)data;}

        // --- MISC ---

        // This is called when an exception needs to be converted to a string.
        // Return the information on your custom exception type, if they don't inherit from `std::exception`.
        // Do nothing (or throw) to let some other module handle this.
        [[nodiscard]] virtual std::optional<data::ExplainedException> OnExplainException(const std::exception_ptr &e) const {(void)e; return {};}

        // This is called before entering try/catch blocks, so you can choose between that and just executing directly. (See `--catch`.)
        // `should_catch` defaults to true.
        // This is NOT called by `TA_MUST_THROW(...)`.
        virtual void OnPreTryCatch(bool &should_catch) noexcept {(void)should_catch;}


        // All virtual functions of this interface must be listed here.
        // See `class ModuleLists` for how this list is used.
        #define DETAIL_TA_MODULE_FUNCS_X(x) \
            x(BasicModule, GetFlags) \
            x(BasicModule, OnUnknownFlag) \
            x(BasicModule, OnMissingFlagArgument) \
            x(BasicModule, OnFilterTest) \
            x(BasicModule, OnPreRunTests) \
            x(BasicModule, OnPostRunTests) \
            x(BasicModule, OnPreRunSingleTest) \
            x(BasicModule, OnPostRunSingleTest) \
            x(BasicModule, OnPostGenerate) \
            x(BasicModule, OnRegisterGeneratorOverride) \
            /* `OnOverrideGenerator` isn't needed. */ \
            x(BasicModule, OnPrePruneGenerator) \
            x(BasicModule, OnPreFailTest) \
            x(BasicModule, OnAssertionFailed) \
            x(BasicModule, OnUncaughtException) \
            x(BasicModule, OnMissingException) \
            x(BasicModule, OnExplainException) \
            x(BasicModule, OnPreTryCatch) \
            x(BasicPrintingModule, EnableUnicode) /* Not needed, but could be useful later. */ \
            x(BasicPrintingModule, PrintContextFrame) \
            x(BasicPrintingModule, PrintLogEntries) \

        // A list of module bases that appear in `DETAIL_TA_MODULE_FUNCS_X`.
        #define DETAIL_TA_MODULE_KINDS_X(x) \
            x(BasicModule) \
            x(BasicPrintingModule) \

        enum class InterfaceFunc
        {
            #define DETAIL_TA_X(base_, func_) func_,
            DETAIL_TA_MODULE_FUNCS_X(DETAIL_TA_X)
            #undef DETAIL_TA_X
            _count [[maybe_unused]],
        };
        // For internal use, don't use and don't override. Returns the mask of functions implemented by this class.
        [[nodiscard]] virtual unsigned int Detail_ImplementedFunctionsMask() const noexcept = 0;
        // For internal use. Returns true if the specified function is overridden in the derived class.
        [[nodiscard]] bool ImplementsFunction(InterfaceFunc func) const noexcept
        {
            using MaskType = decltype(Detail_ImplementedFunctionsMask());
            static_assert(int(InterfaceFunc::_count) < sizeof(MaskType) * 8, "You're out of bits in the mask.");
            return Detail_ImplementedFunctionsMask() & (MaskType(1) << int(func));
        }
    };

    namespace text
    {
        // Extra character manipulation functions.
        namespace chars
        {
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

            // A list of separators for `TryFindUnprotectedSeparator` for generator values in `--generate`.
            inline constexpr std::string_view generator_override_separators = ",&(";

            // Splits the string at a separator.
            // `func` is `(std::string_view segment, bool last) -> bool`.
            // If it returns true, the function stops and also returns true.
            template <typename F>
            constexpr bool Split(std::string_view str, char separator, F &&func)
            {
                auto it = str.begin();

                while (true)
                {
                    auto new_it = std::find(it, str.end(), separator);

                    if (func(std::string_view(it, new_it), new_it == str.end()))
                        return true;

                    if (new_it == str.end())
                        break;
                    it = new_it + 1;
                }

                return false;
            }
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
            void ParseExpr(std::string_view expr, EmitCharFunc &&emit_char, bool function_call_uses_brackets, FunctionCallFunc &&function_call)
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
                                    function_call(false, identifier, {}, parens_stack.size());

                                    parens_stack.push_back({
                                        .ident = identifier,
                                        .args = &ch + 1,
                                    });
                                    identifier = {};
                                }
                                else if (ch == ")]"[function_call_uses_brackets] && !parens_stack.empty())
                                {
                                    function_call(true, parens_stack.back().ident, std::string_view(parens_stack.back().args, &ch), parens_stack.size() - 1);
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
        }
    }

    // Terminal output.
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
            std::function<void(std::string_view fmt, CFG_TA_FMT_NAMESPACE::format_args args)> output_func;

            // Default to stdout.
            Terminal() : Terminal(stdout) {}

            // Sets `output_func` to print to `stream`.
            // Also guesses `enable_color` (always false when `stream` is neither `stdout` nor `stderr`).
            CFG_TA_API Terminal(FILE *stream);

            // Prints a message using `output_func`. Unlike `Print`, doesn't accept `TextStyle`s directly.
            // Prefer `Print()`.
            CFG_TA_API void PrintLow(std::string_view fmt, CFG_TA_FMT_NAMESPACE::format_args args) const;

            // Stores the current text style. Resets the text style when constructed and when destructed.
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

                // Pokes the terminal to reset the style. This is called automatically in the constructor and in the destructor.
                CFG_TA_API void ResetStyle();

                [[nodiscard]] const TextStyle &GetCurrentStyle() const {return cur_style;}
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
                PrintLow(
                    #if CFG_TA_FMT_USES_CUSTOM_STRING_VIEW
                    {fmt.get().data(), fmt.get().size()},
                    #else
                    fmt.get(),
                    #endif
                    // It seems we don't need to forward `args...`.
                    CFG_TA_FMT_NAMESPACE::make_format_args(args...)
                );
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

            // Converts rvalues to lvalues, because in new C++ `make_format_args` no longer accepts rvalues.
            // It's safe to do in our case, since the format args are used immediately.
            template <typename T>
            static T &UnmoveFormatArg(T &&value)
            {
                // Need the cast to work around the simplified implicit move in C++23.
                return static_cast<T &>(value);
            }

          public:
            // Prints all arguments using `output_func`. This overload supports text styles.
            template <typename ...P>
            void Print(StyleGuard &cur_style, CFG_TA_FMT_NAMESPACE::format_string<WrapStyleTypeForFormatString<P>...> fmt, P &&... args) const
            {
                // It seems we don't need to forward `args...`.
                PrintLow(
                    #if CFG_TA_FMT_USES_CUSTOM_STRING_VIEW
                    {fmt.get().data(), fmt.get().size()},
                    #else
                    fmt.get(),
                    #endif
                    CFG_TA_FMT_NAMESPACE::make_format_args(UnmoveFormatArg(WrapStyleForFormatString(*this, cur_style, args))...)
                );
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
            // Function names.
            TextStyle style_func_name = {.color = TextColor::dark_magenta};
            // The offending macro call.
            TextStyle style_failed_macro = {.color = TextColor::none, .bold = true};
            // Highlighted expressions.
            expr::Style style_expr;
            // The custom messages that can be optionally passed to `TA_CHECK` and `TA_MUST_THROW`.
            TextStyle style_user_message = {.color = TextColor::none, .bold = true};

            // Characters:

            std::string warning_prefix = "WARNING: ";
            std::string note_prefix = ""; // Used to be "NOTE: ", but an empty string looks better to me.

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

            // Other:

            // When we print a macro call, it's indented by this many spaces.
            std::size_t code_indentation = 4;

            // Whether to pad the argum&commonent of `TA_CHECK()` and other macros with a space on each side.
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
            [[nodiscard]] std::string LocationToString(const SourceLoc &loc) const
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

        using ContextFrameState = std::map<std::type_index, std::any>;
        // Same, but only prints a single context frame.
        // `state` is arbitrary, it's preserved between frames when printing a stack, and modules can interpret it in whatever way they want.
        CFG_TA_API void PrintContextFrame(Terminal::StyleGuard &cur_style, const context::BasicFrame &frame, ContextFrameState &state);

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
        virtual bool PrintContextFrame(output::Terminal::StyleGuard &cur_style, const context::BasicFrame &frame, output::ContextFrameState &state) noexcept {(void)cur_style; (void)frame; (void)state; return false;}
        // This is called to print the log.
        // Return true to prevent other modules from receiving this call.
        // `unscoped_log` can alternatively be obtained from `BasicModule::RunSingleTestResults`.
        // `scoped_log` can alternatively be obtained from `context::CurrentScopedLog()`.
        virtual bool PrintLogEntries(output::Terminal::StyleGuard &cur_style, std::span<const context::LogEntry> unscoped_log, std::span<const context::LogEntry *const> scoped_log) noexcept {(void)cur_style; (void)unscoped_log; (void)scoped_log; return false;}

      protected:
        CFG_TA_API void PrintWarning(output::Terminal::StyleGuard &cur_style, std::string_view text) const;
        CFG_TA_API void PrintNote(output::Terminal::StyleGuard &cur_style, std::string_view text) const;
    };

    // A non-owning wrapper on top of a module list.
    // Additionally stores lists of modules implemeting certain functions, to optimize the calls to them.
    // It's constructed once we start running tests, since that's when the module becomes frozen,
    // and then becomes the only thing modules can use to interact with the test runner, since there's no way for them to obtain a runner reference.
    class ModuleLists
    {
        // How many interface functions per module base.
        static constexpr auto function_counts = []{
            struct FunctionCounts
            {
                #define DETAIL_TA_X(base_) std::size_t base_ = 0;
                DETAIL_TA_MODULE_KINDS_X(DETAIL_TA_X)
                #undef DETAIL_TA_X
            };
            FunctionCounts ret{};
            #define DETAIL_TA_X(base_, func_) ret.base_++;
            DETAIL_TA_MODULE_FUNCS_X(DETAIL_TA_X)
            #undef DETAIL_TA_X
            return ret;
        }();

        std::span<const ModulePtr> all_modules;

        // Lists of modules implementing interface functions, per base.
        #define DETAIL_TA_X(base_) std::array<std::vector<base_ *>, function_counts.base_> DETAIL_TA_CAT(lists_, base_);
        DETAIL_TA_MODULE_KINDS_X(DETAIL_TA_X)
        #undef DETAIL_TA_X

      public:
        ModuleLists() {}
        ModuleLists(std::span<const ModulePtr> all_modules)
            : all_modules(all_modules)
        {
            for (const auto &m : all_modules)
            {
                // Counters per module base type.
                #define DETAIL_TA_X(base_) std::size_t DETAIL_TA_CAT(i_, base_) = 0;
                DETAIL_TA_MODULE_KINDS_X(DETAIL_TA_X)
                #undef DETAIL_TA_X

                #define DETAIL_TA_X(base_, func_) \
                    if (m->ImplementsFunction(BasicModule::InterfaceFunc::func_)) \
                        DETAIL_TA_CAT(lists_, base_)[DETAIL_TA_CAT(i_, base_)].push_back(&dynamic_cast<base_ &>(*m)); /* This cast should never fail. */ \
                    DETAIL_TA_CAT(i_, base_)++;
                DETAIL_TA_MODULE_FUNCS_X(DETAIL_TA_X)
                #undef DETAIL_TA_X
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
        template <auto F>
        [[nodiscard]] std::span<typename meta::MemberPointerClass<decltype(F)>::type *const> GetModulesImplementing() const
        {
            // Counters per module base type.
            #define DETAIL_TA_X(base_) std::size_t DETAIL_TA_CAT(i_, base_) = 0;
            DETAIL_TA_MODULE_KINDS_X(DETAIL_TA_X)
            #undef DETAIL_TA_X

            #define DETAIL_TA_X(base_, func_) \
                if constexpr (meta::ValuesAreEqual<F, &base_::func_>::value) \
                    return DETAIL_TA_CAT(lists_, base_)[DETAIL_TA_CAT(i_, base_)]; \
                else if (DETAIL_TA_CAT(i_, base_)++, false) {} else
            DETAIL_TA_MODULE_FUNCS_X(DETAIL_TA_X)
            #undef DETAIL_TA_X
            static_assert(meta::AlwaysFalse<meta::ValueTag<F>>::value, "Bad member function pointer.");
        }

        // Calls a specific function for every module.
        // The return values are ignored. If you need them, call manually using `GetModulesImplementing()`.
        template <auto F, typename ...P>
        requires std::is_member_function_pointer_v<decltype(F)>
        void Call(P &&... params) const
        {

            for (auto *m : GetModulesImplementing<F>())
                (m->*F)(params...); // No forwarding because there's more than one call.
        }
    };


    namespace detail
    {
        // Inherits from a user module, and checks which virtual functions were overridden.
        template <typename T>
        struct ModuleWrapper final : T
        {
            using T::T;

            unsigned int Detail_ImplementedFunctionsMask() const noexcept override final
            {
                using MaskType = decltype(Detail_ImplementedFunctionsMask());
                constexpr MaskType ret = []{
                    MaskType value = 0;
                    #define DETAIL_TA_X(base_, func_) \
                        if constexpr (std::is_base_of_v<base_, T>) \
                            if constexpr (!meta::ValuesAreEqual<&T::func_, &base_::func_>::value) \
                                value |= MaskType(1) << int(BasicModule::InterfaceFunc::func_);
                    DETAIL_TA_MODULE_FUNCS_X(DETAIL_TA_X)
                    #undef DETAIL_TA_X
                    return value;
                }();
                return ret;
            }
        };
    }

    // Allocates a new module as a `ModulePtr`.
    template <std::derived_from<BasicModule> T, typename ...P>
    requires std::constructible_from<detail::ModuleWrapper<T>, P &&...>
    [[nodiscard]] ModulePtr MakeModule(P &&... params)
    {
        ModulePtr ret;
        ret.ptr = std::make_unique<detail::ModuleWrapper<T>>(std::forward<P>(params)...);
        return ret;
    }


    // --- BUILT-IN MODULES ---

    namespace modules
    {
        // --- BASES ---

        // Inherit modules from this when they need to print exception contents.
        // We use inheritance instead of composition to allow mass customization of all modules using this.
        struct BasicExceptionContentsPrinter : virtual BasicPrintingModule
        {
          public:
            output::TextStyle style_exception_type = {.color = output::TextColor::light_blue};
            output::TextStyle style_exception_message = {.color = output::TextColor::light_white};
            output::TextStyle style_exception_type_active = {.color = output::TextColor::light_blue, .bold = true};
            output::TextStyle style_exception_message_active = {.color = output::TextColor::light_white, .bold = true};
            output::TextStyle style_exception_active_marker = {.color = output::TextColor::light_magenta, .bold = true};

            std::string chars_unknown_exception = "Unknown exception.";
            std::string chars_indent_type = "    ";
            std::string chars_indent_message = "        ";
            std::string chars_indent_type_active;
            std::string chars_indent_message_active;
            std::string chars_type_suffix = ":";

            // Due to virtual inheritance, this can do double assignment to `BasicPrintingModule` stuff. Annoying, but whatever?
            CFG_TA_API void EnableUnicode(bool enable) override;

          protected:
            CFG_TA_API BasicExceptionContentsPrinter();

            // If `active_elem` is not -1, it's the index of the nested exception that should be highlighted.
            // If `only_one_element` is true, the `active_elem` highlight is modified with the assumption that there's only one element.
            CFG_TA_API virtual void PrintException(
                const output::Terminal &terminal, output::Terminal::StyleGuard &cur_style,
                const std::exception_ptr &e, int active_elem, bool only_one_element
            ) const;
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
            std::vector<flags::BasicFlag *> GetFlags() noexcept override;
            void OnUnknownFlag(std::string_view flag, bool &abort) noexcept override;
            void OnMissingFlagArgument(std::string_view flag, const flags::BasicFlag &flag_obj, bool &abort) noexcept override;
        };

        // Responds to `--include` and `--exclude` to select which tests to run.
        struct TestSelector : BasicModule
        {
            flags::StringFlag flag_include;
            flags::StringFlag flag_exclude;
            flags::StringFlag flag_force_include;

            struct Pattern
            {
                bool exclude = false;
                bool force = false; // Only for `exclude = false`.

                std::string regex_string;
                std::regex regex;

                bool was_used = false;
            };
            std::vector<Pattern> patterns;

            CFG_TA_API TestSelector();
            std::vector<flags::BasicFlag *> GetFlags() noexcept override;
            void OnFilterTest(const data::BasicTest &test, TestFilterState &state) noexcept override;
            void OnPreRunTests(const data::RunTestsInfo &data) noexcept override;

            CFG_TA_API static flags::StringFlag::Callback GetFlagCallback(bool exclude, bool force);
        };

        // Responds to `--generate` to override the generated values.
        struct GeneratorOverrider : BasicPrintingModule
        {
            // A sequence of generator overrides coming from a `--generate`.
            struct GeneratorOverrideSeq
            {
                struct Entry
                {
                    mutable bool was_used = false;

                    std::string_view generator_name;
                    // How many characters this flag occupies starting from `generator_name.data()`.
                    std::size_t total_num_characters = 0;

                    // If false, don't generate anything by default unless explicitly enabled.
                    bool enable_values_by_default = true;

                    struct CustomValue
                    {
                        mutable bool was_used = false;

                        std::string_view value;

                        std::shared_ptr<GeneratorOverrideSeq> custom_generator_seq;

                        // Next rule index in `rules` (or its size if no next rule).
                        std::size_t next_rule = 0;

                        // This points to the `=` before the value.
                        const char *operator_character = nullptr;
                    };

                    // Custom values provided by the user, using the `=...`syntax.
                    // Anything listed here is skipped during natural generation, and none of the rules below apply to those.
                    std::vector<CustomValue> custom_values;

                    // Add or remove a certain index range.
                    // This corresponds to `#...` and `-#...` syntax.
                    struct RuleIndex
                    {
                        // Max index that was affected by this rule (plus one). We use this to detect upper bounds being too large.
                        mutable std::size_t max_used_end = 0;

                        bool add = true;

                        // 0-based, half-open range.
                        std::size_t begin = 0;
                        std::size_t end = std::size_t(-1);

                        // This is where `end` begins in the flag, if it's specified at all.
                        const char *end_string_location = nullptr;

                        // How many characters this flag occupies starting from `operator_character` of the enclosing rule.
                        // Only index rules have this, because in value rules we can look at the end of the value string.
                        std::size_t total_num_characters = 0;

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
                        mutable bool was_used = false;

                        RuleVar var;

                        // If not null, this replaces the rest of the program for those values.
                        std::shared_ptr<GeneratorOverrideSeq> custom_generator_seq;

                        // This points to the symbol before the value (one of: `-=`, `#`, `-#`).
                        const char *operator_character = nullptr;
                    };

                    std::vector<Rule> rules;
                };

                std::vector<Entry> entries;
            };

            struct Entry
            {
                mutable bool was_used = false;

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

            struct ActiveFlag
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
            };

            struct TestState
            {
                // Those are ordered in the order they should be applied, which is in reverse compared to the flag order.
                std::vector<ActiveFlag> active_flags;

                // Need this for `std::optional<TestState>` to register the default-constructibility.
                TestState() {}
            };
            std::optional<TestState> test_state;

            CFG_TA_API GeneratorOverrider();

            std::vector<flags::BasicFlag *> GetFlags() noexcept override;

            void OnPreRunTests(const data::RunTestsInfo &data) noexcept override;
            void OnPostRunTests(const data::RunTestsResults &data) noexcept override;
            void OnPostRunSingleTest(const data::RunSingleTestResults &data) noexcept override;
            bool OnRegisterGeneratorOverride(const data::RunSingleTestProgress &test, const data::BasicGenerator &generator) noexcept override;
            bool OnOverrideGenerator(const data::RunSingleTestProgress &test, data::BasicGenerator &generator) noexcept override;
            void OnPrePruneGenerator(const data::RunSingleTestProgress &test) noexcept override;

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
            std::vector<flags::BasicFlag *> GetFlags() noexcept override;
        };

        // Prints the test names as they're being run.
        struct ProgressPrinter : BasicPrintingModule
        {
            // When printing a generator summary for a failed test, how many characters max will be printed per generator value.
            // If this limit is exceeded (of if the type can't be serialized), the `#` index is printed instead.
            // Or, if it's exceeded with custom values (that have no index), we don't print anything at all.
            std::size_t max_generator_summary_value_length = 20; // 20 characters is enough to print 2^64, or 2^63 with sign.

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
            // The generator summary for a failed test.
            output::TextStyle style_failed_generator_summary = {.color = output::TextColor::dark_yellow};
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

            // Whether to print the progress.
            bool show_progress = true;
            flags::BoolFlag flag_progress;

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
                        bool is_custom_value = false;
                        std::optional<std::string> value;
                        SourceLocWithCounter location;

                        // This should be unique enough.
                        [[nodiscard]] friend bool operator==(const FailedGenerator &a, const FailedGenerator &b)
                        {
                            return a.location == b.location && a.index == b.index && a.is_custom_value == b.is_custom_value;
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

            // This is used to convert a sequence of test names to what looks like a tree.
            // `stack` must start empty before calling this the first time, and is left in an unspecified state after the last call.
            // `push_segment` is called every time we're entering a new tree node.
            // `push_segment` is `(std::size_t segment_index, std::string_view segment, bool is_last_segment) -> void`.
            // `segment` is one of the `/`-separated parts of the `name`. `segment_index` is the index of that part.
            // `is_last_segment` is whether this is the last segment in `name`.
            static void ProduceTree(std::vector<std::string_view> &stack, std::string_view name, auto &&push_segment)
            {
                std::size_t segment_index = 0;

                text::chars::Split(name, '/', [&](std::string_view segment, bool is_last_segment)
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

                    return false;
                });
            }

            enum class TestCounterStyle {none, normal, repeated};
            CFG_TA_API void PrintContextLinePrefix(output::Terminal::StyleGuard &cur_style, const data::RunTestsProgress &all_tests, TestCounterStyle test_counter_style) const;
            CFG_TA_API void PrintContextLineIndentation(output::Terminal::StyleGuard &cur_style, std::size_t depth, std::size_t skip_characters) const;

            // Prints the entire line describing a generator.
            // `repeating_info == true` means that we're printing this not because a new value got generated,
            // but because we're providing the context again after an error.
            CFG_TA_API void PrintGeneratorInfo(output::Terminal::StyleGuard &cur_style, const data::RunSingleTestProgress &test, const data::BasicGenerator &generator, bool repeating_info);

            // Returns a string describing the current generators, that's suitable for passing to `--generate` (after `test//`).
            // Returns an empty string if no generators are active.
            [[nodiscard]] CFG_TA_API std::string MakeGeneratorSummary(const data::RunSingleTestProgress &test) const;

          public:
            CFG_TA_API ProgressPrinter();

            std::vector<flags::BasicFlag *> GetFlags() noexcept override;

            void EnableUnicode(bool enable) override;
            void OnPreRunTests(const data::RunTestsInfo &data) noexcept override;
            void OnPostRunTests(const data::RunTestsResults &data) noexcept override;
            void OnPreRunSingleTest(const data::RunSingleTestInfo &data) noexcept override;
            void OnPostRunSingleTest(const data::RunSingleTestResults &data) noexcept override;
            void OnPostGenerate(const data::GeneratorCallInfo &data) noexcept override;
            void OnPreFailTest(const data::RunSingleTestProgress &data) noexcept override;
        };

        // Prints the results of a run.
        struct ResultsPrinter : BasicPrintingModule
        {
            // The table header.
            output::TextStyle style_table_header = {.color = output::TextColor::light_white};
            // Total number of tests (with and without skipped).
            output::TextStyle style_total = {.color = output::TextColor::light_black};

            // Zero numbers use this style instead of their normal styles.
            output::TextStyle style_zero = {.color = output::TextColor::light_black};

            // The number of skipped tests.
            output::TextStyle style_skipped = {.color = output::TextColor::light_blue};
            // The number of skipped tests, when all tests were skipped.
            output::TextStyle style_skipped_primary = {.color = output::TextColor::light_blue, .bold = true};
            // The number of passed tests.
            output::TextStyle style_passed = {.color = output::TextColor::light_green};
            // The number of passed tests, when all tests passed (or were skipped).
            output::TextStyle style_passed_primary = {.color = output::TextColor::light_green, .bold = true};
            // The number of failed tests.
            output::TextStyle style_failed_primary = {.color = output::TextColor::light_red, .bold = true};

            // Rows. `..._primary` is used for the last (most important) row.
            std::string chars_skipped           = "Skipped";
            std::string chars_passed            = "Passed";
            std::string chars_skipped_primary   = "SKIPPED";
            std::string chars_passed_primary    = "PASSED";
            std::string chars_failed_primary    = "FAILED";
            std::string chars_total_known       = "Known";
            std::string chars_total_executed = "Executed";

            // Columns.
            std::string chars_col_tests = "Tests";
            std::string chars_col_repetitions = "Variants";
            std::string chars_col_checks = "Checks"; // This covers assertions, `TA_MUST_THROW`, `TA_FAIL`.

            // No tests are registered at all.
            std::string chars_no_known_tests = "NO TESTS ARE REGISTERED";

            int column_width = 10;
            int leftmost_column_width = 8; // This is used for the column with the row names. Should be equal to max row header length.

            void OnPostRunTests(const data::RunTestsResults &data) noexcept override;
        };

        // Prints failed assertions.
        struct AssertionPrinter : BasicPrintingModule
        {
            // Whether we should print the values of `$[...]` in the expression.
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
            output::TextStyle style_dim = {.color = output::TextColor::light_black};

            // Labels a subexpression that had a nested assertion failure in it.
            std::u32string chars_in_this_subexpr = U"in here";
            // Same, but when there's something wrong internally with determining the location. This shouldn't happen.
            std::u32string chars_in_this_subexpr_weird = U"in here?";

            void OnAssertionFailed(const data::BasicAssertion &data) noexcept override;
            bool PrintContextFrame(output::Terminal::StyleGuard &cur_style, const context::BasicFrame &frame, output::ContextFrameState &state) noexcept override;

            CFG_TA_API void PrintAssertionFrameLow(output::Terminal::StyleGuard &cur_style, const data::BasicAssertion &data, bool is_most_nested) const;
        };

        // Responds to `text::PrintLog()` to print the current log.
        // Does nothing by itself, is only used by the other modules.
        struct LogPrinter : BasicPrintingModule
        {
            output::TextStyle style_message = {.color = output::TextColor::dark_cyan};

            // Prefix for user messages.
            std::string chars_message_prefix = "// ";
            // Prefix for source locations passed to `TA_LOG`.
            std::string chars_loc_reached_prefix = "Reached ";
            // Prefix for source locations passed to `TA_CONTEXT`.
            std::string chars_loc_context_prefix = "At ";
            // A separator between the source location passed to `TA_CONTEXT` and the callee function name.
            std::string chars_loc_context_callee = "\nIn function: ";

            // The current position in the unscoped log vector, to avoid printing the same stuff twice. We reset this when we start a new test.
            // We intentionally re-print the scoped logs every time they're needed.
            std::size_t unscoped_log_pos = 0;

            void OnPreRunSingleTest(const data::RunSingleTestInfo &data) noexcept override;
            void OnPostRunSingleTest(const data::RunSingleTestResults &data) noexcept override;
            bool PrintLogEntries(output::Terminal::StyleGuard &cur_style, std::span<const context::LogEntry> unscoped_log, std::span<const context::LogEntry *const> scoped_log) noexcept override;
        };

        // A generic module to analyze exceptions.
        // `E` is the exception base class.
        // `F` is a functor to get the error string from an exception, defaults to `e.what()`.
        template <typename E, typename F = void>
        struct GenericExceptionAnalyzer : BasicModule
        {
            std::optional<data::ExplainedException> OnExplainException(const std::exception_ptr &e) const override
            {
                try
                {
                    std::rethrow_exception(e);
                }
                catch (E &e)
                {
                    data::ExplainedException ret;
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
        struct ExceptionPrinter : virtual BasicPrintingModule, BasicExceptionContentsPrinter
        {
            std::string chars_error = "Uncaught exception:";

            void OnUncaughtException(const data::RunSingleTestInfo &test, const data::BasicAssertion *assertion, const std::exception_ptr &e) noexcept override;
        };

        // Prints things related to `TA_MUST_THROW()`.
        struct MustThrowPrinter : virtual BasicPrintingModule, BasicExceptionContentsPrinter
        {
            std::string chars_expected_exception = "Expected exception:";
            std::string chars_while_expecting_exception = "While expecting exception here:";
            std::string chars_exception_contents = "While analyzing exception:";
            std::string chars_throw_location = "Thrown here:";

            void OnMissingException(const data::MustThrowInfo &data) noexcept override;
            bool PrintContextFrame(output::Terminal::StyleGuard &cur_style, const context::BasicFrame &frame, output::ContextFrameState &state) noexcept override;

            CFG_TA_API void PrintFrame(
                output::Terminal::StyleGuard &cur_style,
                const data::MustThrowStaticInfo &static_info,
                const data::MustThrowDynamicInfo *dynamic_info, // Optional.
                const data::CaughtExceptionContext *caught, // Optional. If set, we're analyzing a caught exception. If null, we're looking at a macro call.
                bool is_most_nested // Must be false if `caught` is set.
            ) const;
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

            std::vector<flags::BasicFlag *> GetFlags() noexcept override;

            CFG_TA_API bool IsDebuggerAttached() const;
            void OnAssertionFailed(const data::BasicAssertion &data) noexcept override;
            void OnUncaughtException(const data::RunSingleTestInfo &test, const data::BasicAssertion *assertion, const std::exception_ptr &e) noexcept override;
            void OnMissingException(const data::MustThrowInfo &data) noexcept override;
            void OnPreTryCatch(bool &should_catch) noexcept override;
            void OnPostRunSingleTest(const data::RunSingleTestResults &data) noexcept override;
        };

        // A little module that examines `DebuggerDetector` and notifies you when it detected a debugger.
        struct DebuggerStatePrinter : BasicPrintingModule
        {
            void OnPreRunTests(const data::RunTestsInfo &data) noexcept override;
        };
    }
}

template <>
struct CFG_TA_FMT_NAMESPACE::formatter<ta_test::output::Terminal::PrintableAnsiDelta, char>
{
    constexpr auto parse(CFG_TA_FMT_NAMESPACE::basic_format_parse_context<char> &parse_ctx)
    {
        return parse_ctx.begin();
    }

    template <typename OutputIt>
    constexpr auto format(const ta_test::output::Terminal::PrintableAnsiDelta &arg, CFG_TA_FMT_NAMESPACE::basic_format_context<OutputIt, char> &format_ctx) const
    {
        return CFG_TA_FMT_NAMESPACE::format_to(format_ctx.out(), "{}", arg.terminal.AnsiDeltaString(arg.cur_style, arg.new_style).data());
    }
};
