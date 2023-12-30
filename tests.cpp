#include <exception>
#include <iostream>

#include "testlib.h"

#error optional generator increment between tests (after a test in a lambda)

// The parameter is the test name regex (as in `--include`), followed by `//`, then a comma-separated list of generator overrides.
// Each test can be affected by at most one such parameter (the last matching one).
// Some examples: (here `x`,`y` are generator names as passed to `TA_GENERATE(name, ...)`)
// * -g 'foo/bar//x=42'         - generate only this value.
// * -g 'foo/bar//x=42,y=43'    - override several generators (the order matters; you can omit some of the generators).
// * -g 'foo/bar//x{=10,=20}'   - several values per generator.
// * -g 'foo/bar//x-=10         - skip specific value.
// * -g 'foo/bar//x#10'         - only generate the value at the specified index (1-based).
// * -g 'foo/bar//x#10..12'     - same, but with a range of indices (inclusive). One of the numbers can be omitted: `..10`, `10..`.
// * -g 'foo/bar//x-#10'        - skip the value at the specific index. This also accepts ranges.
// Multiple operators can be combined:
// * -g 'foo/bar//x{#..10,=42}' - generate only 10 first values, and a custom value `42`.
// Operators are applied left to right. If the first operator is `=` or `#`, all values are disabled by default. But you can reenable them manually:
// * -g 'foo/bar//x{#1..,=42}'  - generate all values, and a custom one.
// Operators `=` and `#` can be followed by a parenthesized list of generator overrides, which are used in place of the remaining string for those values:
// * -g 'foo/bar//x{#1..,#5(y=20)},y=10' - override `y=20` for 5th value of `x`, and `y=10` for all other values of `x`.
// If multiple operators match the same value, parentheses from the last match are used.
// Parentheses apply to only one operator by default. To apply them to multiple operators, separate operators with `&` instead of `,`:
// * -g 'foo/bar//x{#1..,#5&=42(y=20)},y=10' - override `y=20` for 5th value of `x` and for a custom value `x=42`, for all other values of `x` use `y=10`.
// Some notes:
// * This flag changes the generator semantics slightly, making subsequent calls to the generator lambda between the test repetitions, as opposed to
//     when the control flow reaches the `TA_GENERATE(...)` call, to avoid entering the test when all future values are disabled.
//     This shouldn't affect you, unless you're doing something really weird.
// * Not all types can be created from strings, but index-based operators will always work.
//     We support scalars, strings (with standard escape sequences), containers (as printed by `std::format()`: {...} sets, {a:b, c:d} maps, [...] other containers, and (...) tuples).
//     Custom type support can be added by specializing `ta_test::string_conv::FromStringTraits`.
// * `-=` requires overloaded `==` (and better also `<`) to work.
// * `-=` can't remove values added with `=`, because it's useless and simplifies implementation.
// * Values added with `=` have no index. We try to remove them from normal generation if the type is comparable.


namespace ta_test
{
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

    // Parses a `GeneratorOverrideSeq` object. `target` must initially be empty.
    // Returns the error on failure, or an empty string on success.
    // The `string` must remain alive, we're storing pointers into it.
    // `is_nested` should be false by default, and will be set to true when parsing nested sequences.
    // This consumes the trailing space.
    [[nodiscard]] std::string ParseGeneratorOverrideSeq(GeneratorOverrideSeq &target, const char *&string, bool is_nested)
    {
        bool first_generator = true;

        // For each generator.
        while (true)
        {
            if (first_generator)
            {
                first_generator = false;
            }
            else
            {
                if (*string == '\0' || (is_nested && *string == ')'))
                    break;

                text::chars::SkipWhitespace(string);

                if (*string != ',')
                    return "Expected `,`.";
                string++;

                text::chars::SkipWhitespace(string);
            }

            GeneratorOverrideSeq::Entry new_entry;

            { // Parse the name.
                if (!text::chars::IsIdentifierCharStrict(*string) || text::chars::IsDigit(*string))
                    return "Expected a generator name.";

                const char *name_begin = string;

                do
                {
                    string++;
                }
                while (text::chars::IsIdentifierCharStrict(*string));

                new_entry.generator_name = {name_begin, string};
            }

            bool is_first_rule = true;
            bool last_rule_is_positive = false;

            std::shared_ptr<GeneratorOverrideSeq> sub_override;

            auto ParseRule = [&] CFG_TA_NODISCARD_LAMBDA () -> std::string
            {
                auto TrimValue = [](std::string_view value) -> std::string_view
                {
                    // We only trim the leading whitespace, because `TryFindUnprotectedSeparator()`
                    // automatically rejects trailing whitespace.
                    while (!value.empty() && text::chars::IsWhitespace(value.front()))
                        value.remove_prefix(1);
                    return value;
                };

                auto BeginPositiveRule = [&]
                {
                    if (is_first_rule)
                        new_entry.enable_values_by_default = false;
                };

                auto BeginNegativeRule = [&] CFG_TA_NODISCARD_LAMBDA () -> std::string
                {
                    if (is_first_rule)
                        new_entry.enable_values_by_default = true;

                    if (sub_override)
                        return "`&` can't appear before a negative rule, since those can't be followed by `(...)`.";

                    return "";
                };

                auto FinishPositiveRule = [&] CFG_TA_NODISCARD_LAMBDA (std::shared_ptr<GeneratorOverrideSeq> &ptr) -> std::string
                {
                    text::chars::SkipWhitespace(string);

                    bool is_and = *string == '&';
                    bool is_open = *string == '(';

                    if (is_and || is_open)
                    {
                        if (!sub_override)
                            sub_override = std::make_shared<GeneratorOverrideSeq>();
                        ptr = sub_override;
                    }
                    else
                    {
                        if (sub_override)
                            return "Expected `&` or `(` after a list of `&`-separated rules.";
                    }

                    if (is_open)
                    {
                        string++;
                        text::chars::SkipWhitespace(string);
                        std::string error = ParseGeneratorOverrideSeq(*sub_override, string, true);
                        if (!error.empty())
                            return error;

                        sub_override = nullptr;

                        // No need to skip whitespace here, `ParseGeneratorOverrideSeq()` should do it for us.

                        if (*string != ')')
                            return "Expected closing `)`.";
                        string++;

                        text::chars::SkipWhitespace(string);
                    }

                    last_rule_is_positive = true;

                    return "";
                };

                auto FinishNegativeRule = [&] CFG_TA_NODISCARD_LAMBDA () -> std::string
                {
                    text::chars::SkipWhitespace(string);

                    if (*string == '(')
                        return "`(...)` can't appear after negative rules.";
                    if (*string == '&')
                        return "`&` can't appear after a negative rule, since those can't be followed by `(...)`.";;

                    last_rule_is_positive = false;

                    return "";
                };

                static constexpr std::string_view separators = ",&(";

                if (*string == '=')
                {
                    BeginPositiveRule();

                    string++;

                    const char *value_begin = string;
                    text::chars::TryFindUnprotectedSeparator(string, separators);
                    GeneratorOverrideSeq::Entry::CustomValue new_value{.value = TrimValue({value_begin, string})};

                    std::string error = FinishPositiveRule(new_value.custom_generator_seq);
                    if (!error.empty())
                        return error;
                    new_entry.custom_values.push_back(std::move(new_value));
                }
                else if (*string == '-' && string[1] == '=')
                {
                    std::string error = BeginNegativeRule();
                    if (!error.empty())
                        return error;

                    string += 2;

                    const char *value_begin = string;
                    text::chars::TryFindUnprotectedSeparator(string, separators);

                    GeneratorOverrideSeq::Entry::RuleVar new_rule;
                    new_rule.emplace<GeneratorOverrideSeq::Entry::RuleRemoveValue>().value = TrimValue({value_begin, string});
                    new_entry.rules.push_back({.var = std::move(new_rule)});

                    error = FinishNegativeRule();
                    if (!error.empty())
                        return error;
                }
                else if (*string == '#' || (*string == '-' && string[1] == '#'))
                {
                    GeneratorOverrideSeq::Entry::Rule new_rule;
                    GeneratorOverrideSeq::Entry::RuleIndex &new_rule_index = new_rule.var.emplace<GeneratorOverrideSeq::Entry::RuleIndex>();

                    new_rule_index.add = *string == '#';

                    if (new_rule_index.add)
                    {
                        BeginPositiveRule();
                        string++;
                    }
                    else
                    {
                        std::string error = BeginNegativeRule();
                        if (!error.empty())
                            return error;

                        string += 2;
                    }

                    if (*string != '.' && !text::chars::IsDigit(*string))
                        return "Expected an integer or `..`.";

                    bool have_first_number = *string != '.';

                    if (have_first_number)
                    {
                        std::string error = string_conv::FromStringTraits<decltype(new_rule_index.begin)>{}(new_rule_index.begin, string);
                        if (!error.empty())
                            return error;
                        if (new_rule_index.begin < 1)
                            return "The index must be 1 or greater.";
                        new_rule_index.begin--;
                    }

                    if (*string != '.' || string[1] != '.')
                        return "Expected `..`.";
                    string += 2;

                    if (!have_first_number || text::chars::IsDigit(*string))
                    {
                        std::string error = string_conv::FromStringTraits<decltype(new_rule_index.end)>{}(new_rule_index.end, string);
                        if (!error.empty())
                            return error;
                        if (new_rule_index.end < 1)
                            return "The index must be 1 or greater.";
                        if (new_rule_index.end < new_rule_index.begin + 1)
                            return "The second index must be greater or equal to the first one.";
                    }

                    std::string error;
                    if (new_rule_index.add)
                        error = FinishPositiveRule(new_rule.custom_generator_seq);
                    else
                        error = FinishNegativeRule();
                    if (!error.empty())
                        return error;

                    new_entry.rules.push_back(std::move(new_rule));
                }
                else
                {
                    return "Expected one of: `=`, `-=`, `#`, `-#`.";
                }

                is_first_rule = false;

                return "";
            };

            text::chars::SkipWhitespace(string);

            // Parse the rules.

            if (*string == '{')
            {
                string++;

                text::chars::SkipWhitespace(string);

                while (true)
                {
                    if (!is_first_rule)
                    {
                        if (*string == '}')
                        {
                            string++;
                            break;
                        }

                        if (last_rule_is_positive)
                        {
                            if (*string != ',' && *string != '&' && *string != '&')
                                return "Expected `,` or `&` or `(`.";
                        }
                        else
                        {
                            if (*string != ',')
                                return "Expected `,`.";
                        }
                        string++;

                        text::chars::SkipWhitespace(string);
                    }

                    // This skips the trailing whitespace.
                    std::string error = ParseRule();
                    if (!error.empty())
                        return error;
                }
            }
            else
            {
                std::string error = ParseRule();
                if (!error.empty())
                    return error;
            }

            target.entries.push_back(std::move(new_entry));
        }

        return "";
    }
}

#if 0
foo=42,bar[2],baz=56
    |      |      |
   [1]     42    [3]

-i foo/bar//x=1 // Double slash indicates start of generator overrides
== -i foo/bar -s foo/bar//x=1

-s foo/bar//x=1

foo=42(bar=43(baz=44))
foo{=42&=43(foo=42),}



foo#1
foo#1..2
foo-#1
foo-#1..2
foo-=42
foo+=42 // Adds to a set of skipped values. Not cancelable by -= or anything else, trying to do so will error -= as unused.


// Only specific indices:
foo{=42,#4..5,-#1..2}
foo=1..10,-5,{42,43}
foo#1..
foo#..3
foo#.. // Only all.


foo=5 // Set value
foo+=5 // Add value // = and += conflict with each other

foo?#4 // Check index
foo?-#4 // Check not index
foo?=42 // Check value
foo?-=42 // Check value

foo#1,bar=2,baz#3..5
foo
foo+=2 // not combinable with = nor #
foo{#1,=2}
foo={1,2}
foo#{1,3-5,7}
foo{#{1,3..5,7},=42}

foo=42
foo={42,43,44}
foo[1,3..5,10]
foo[1,3..5,10],=42
foo[1,3..5,10],={42,43,44}
// ={ is always a list, if your value starts with {, use double braces for a single-element list

Running tests...
    │  ● foo/
1/2 │  ·   ● bar
    │ 1 │  ·   ● x[1] = 42
    │   │  ·   ·   ● y[1] = 42
    │ 2 │  ·   ·   ● y[2] = 42
    │ 3 [1] │  ● x[2]
    │       │  ·   ● z[1]
    │ 4 [2] │  ·   ● z[2]


bool fof()
{
    TA_CONTEXT("Hello {}", []{std::cout << "A\n"; return 42;}());
    TA_LOG("Hello!");
    TA_CONTEXT_LAZY("Hello {}", []{std::cout << "B\n"; return 43;}());
    try{
        TA_CHECK($(1) == $(2));
    }
    catch (ta_test::InterruptTestException) {}
        TA_CHECK($(1) == $(2));
    // TA_CHECK($(1) == $(2))("x = {}", 42);
    // TA_FAIL("stuff {}", 42);

    // auto x = TA_MUST_THROW(throw std::runtime_error("123"))("z={}",43);
    // x.CheckMessage("123").CheckMessage("1234");

    TA_STOP;

    TA_GENERATE(i, std::views::iota(10,20))
    TA_GENERATE(i, {1,2,3})

    TA_GENERATE_FUNC(i, [i = 0](bool &repeat) mutable {repeat = i < 3; return i++})


    TA_FOR_TYPES(T, int, float, double) {...};
    TA_FOR_TYPE_LIST(T, type_list<int, float, double>) {...};
    TA_FOR_VALUES(T, 1, 1.f, 1.) {...};
    TA_FOR_VALUE_LIST(T, value_list<1, 1.f, 1.>) {...};

    if TA_VARIANT(foo) // expands more or less:  if (TA_GENERATE(foo, {true, false}))
    // VARIANT also has a customized logging:
    //     if we get a `true` variant, and
    //     the preceding variant was just switched to false,
    //     and we haven't seen this true variant on the previous run
    //     // not needed: and if we're currently out of scope of the preceding varaint IF
    //          // this would require expanding to like `(var; cond)`,
    //          // and would prevent usage outside of conditions - should be do it regardless? I guess not?
    // then the preceding variant is marked as non-printing as long as it remains false (aka the rest of its lifetime)
    //     because it seems to be the sibling of the current variant

    // ^ Now do we handle command-line overrides of variants and this sibling system?

    // We will have to print an "otherwise" branch when running it (decide how to name it?)



    for (int i : {1,2,3})
    {
        // T i = 1;
        // T i = 2;
        // T i = 3;
    }


    for (ta_test::RepeatTestFor<int> i = 1; i <= 3; i++)
    {
        // T i = 1;
        // i = 2;
        // i = 3;
        //
    }

    return true;
}
#endif

TA_TEST(foof/generators)
{
    // static int i = 0;
    // std::cout << "### " << i++ << '\n';

    [[maybe_unused]] auto a = TA_GENERATE_FUNC(foo, [i = 0](bool &repeat) mutable {if (i == 3) repeat = false; return i++;});
    // std::cout << a << '\n';

    [[maybe_unused]] auto b = TA_GENERATE_FUNC(blahh, [i = 20](bool &repeat) mutable {if (i == 23) repeat = false; return i++;});
    // std::cout << b << '\n';

    static int i = 0;
    if (i++ == 10)
        TA_FAIL;

    [[maybe_unused]] int &c = TA_GENERATE(hah, {1,2,3,4});
}

TA_TEST(foo/baz)
{
    auto e = TA_MUST_THROW(
        try
        {
            throw std::runtime_error("Hello!");
        }
        catch (...)
        {
            std::throw_with_nested(std::runtime_error("Nested!"));
        }
    );

    // e.CheckMessage("Hello!");
    TA_CHECK(R"123(foof)123" && false);
}

int main(int argc, char **argv)
{
    ta_test::GeneratorOverrideSeq seq;

    const char *str = "foo  {  =  42  &  =  43  &  =  44  (  foo  =  42  )  ,  #1..2  ,  #..1  ,  -#1..  }  ,  bar  =  43  ", *old_str = str;

    std::string error = ta_test::ParseGeneratorOverrideSeq(seq, str, false);
    if (error.empty())
        std::cout << "ok\n";
    else
        std::cout << "error: " << error << '\n';
    std::cout << std::format("{}\n{:{}}^", old_str, "", str - old_str);

    // return ta_test::RunSimple(argc, argv);
}


// Roundtrip check when printing a reproduction string.

// Report how many repetitions per test failed/passed/total.
// Different global summary style: (3 columns? tests, repetitions(?), asserts)
//     Skipped: 42
//     Passed:  42
//     FAILED:  42

// Overriding generator values?

// Soft assertions?

// TA_FOR_TYPES, sane something for values

// TA_VARIANT (should be scoped?)

// Better CaughtException interface?
//     single function to check combined message
//     or
//     THIS: expand ForEach to allow "any" elem to be checked; expand context to allow pointing to element

// Throw away excessive use of `size_t`, switch to `int` or `ptrdiff_t`?

// Quiet flag: --no-progress
// Failing a test should print the values of generators? Or not?

// Test without exceptions.
// Test without RTTI. What about exception type names?

// Subsections, for_types, and for_values (for_values optional?)

// Move `mutable bool should_break` to a saner location, don't keep it in the context?
// Review it in all locations (TA_CHECK, TA_MUST_THROW, etc).

// Short macros that can be disabled in the config.

// Split the runner (with all modules) into a separate header? Including most utility functions too.

// Rebrand using this regex: `(?<![a-z])ta(?![a-z])` (not case-sensitive, not whole word).
// Sort declarations, then sort definitions.

// Check that paths are clickable in Visual Studio

// Later:
//     Somehow don't evaluate the message if the assertion passes? Perhaps change the syntax to `CHECK(cond, LOG("{}", 42))`, reusing one of the log macros?
//         Same for the arguments passed to `Trace`?
//     Multithreading? Thread inheritance system.
//         The thread identity object should be just copyable around. Also record source location in copy constructor to identify the thread later.
//     What's the deal with SEH? Do we need to do anything?
//     Do we force-open the console on Windows if there's none? That's when `GetFileType(GetStdHandle(STD_OUTPUT_HANDLE))` returns 0.
//     Take into account the terminal width? By slicing off the whole right section, and drawing it on the next lines.
//     Length cap on serialized values, configurable.
//     Signal and SEH handling?

// Maybe not?
//     Soft TA_MUST_THROW?
//     Allow more characters in bracket-less form: `:`, `.`, `->`?
//     A second argument macro that doesn't error out when not printable. `TA_TRY_ARG`?
//     After file paths, print `error: ` (on MSVC `error :` ? Check that.), and some error messages for the parsers.
//     Deduplicate assertions in stacks? Both when an assertion fails and when an exception is triggered.
//     Get terminal width, and limit separator length to that value (but still not make them longer than they currently are)
//     Try to enforce relative paths, and try printing errors on the same line as paths.
//     $(...) should tolerate non-printable arguments, but only in non-dependent context.
//     In, $(...) for really long lines, do just [1], then a reference at the bottom: `[1]: ...`. (Decide on the exact spelling, for this to not be confused with a single-element vector, or whatever)
//         What's the point? $(...) isn't lazy, so you shouldn't have long lines in it anyway. Use the user message, which is lazy.
//     Decorate line breaks in logs with `//` as well?

// Unclear how:
//     Draw a fat bracket while explaining each test failure?
//     Print user messages from assertions that didn't execute to completion.
//     Don't show the assertion user message the second time when printing the expression
//     Optimize the calls to the `BasicPrintingModule` with the module lists too.
//     `$(...)` could be useful to provide context, but what if the function returns void or non-printable type?

// Selling points:
//     "DESIGNING A SUPERIOR UNIT TEST FRAMEWORK"
//     * Expression unwrapping
//     * Sections that are not broken (can do a cross product)
//     * First-class nested exceptions support out of the box
//     * Lazy message evaluation
//         * Point out that you can't do proper lazyness with <<, because operands are still evaluated.
//     * No comma weirdness
//     * Clickthrough everywhere (i.e. file paths everywhere, that should be clickable in an IDE)


/* Pending tests:

--- ToString-ability of the common types:
    char *, const char *
    char[N], const char[N]
    std::string_view
    std::string
    char
    ??? wchar_t, wstring, wstring_view? See what libfmt does, but probably later.
    ?????? char16_t, char32_t
    int, short, long, float, double, long double
    std::vector<int> std::set<int>, std::map<int, int>
    tuple (including empty), pair

--- FromString
    Same types as in ToString
    Reject duplicate keys in sets and maps
    Reject spaces before and after:
        scalars
        strings
        containers
    Containers:
        empty containers
        allow spaces everywhere (except before and after)
    tuples
        empty tuples
    Unescaping strings - ugh
        how many chars we consume in the escape sequences
        reject large escapes (note that \0xx and \x should not produce multibyte chars?)
        uppercase and lowercase hex
        `char` should reject multibyte characters. And even characters with codes > 0x7f.
        `char` should reject empty literals: ''.

--- Expression colorizer

ta_test::text::CommonData common;
ta_test::text::TextCanvas canv(&common);
ta_test::text::expr::DrawToCanvas(canv, line++, 3, "foo(42, .5f,.5f, 5.f, 5.4f, 42_lit, 42lit, 42_foo42_bar, +42,-42, 123'456'789, 0x123'456, 0123'456)");
ta_test::text::expr::DrawToCanvas(canv, line++, 3, "foo(12e5,12e+5,12e-5,12.3e5,12.3e+5,12.3e-5,0x1p2,0x1p+2,0x1p-2,0x12.34p2)");
ta_test::text::expr::DrawToCanvas(canv, line++, 3, "1+1"); // `+` must not be highlighted as a number
ta_test::text::expr::DrawToCanvas(canv, line++, 3, "foo(\"meow\",foo42foo\"meow\"bar42bar,\"meow\"_bar42bar,\"foo\\\"bar\")");
ta_test::text::expr::DrawToCanvas(canv, line++, 3, "foo('a','\\n','meow',foo42foo'meow'bar42bar,'meow'_bar42bar,'foo\\'bar')");
ta_test::text::expr::DrawToCanvas(canv, line++, 3, "foo(R\"(meow)\",foo42fooR\"(meow)\"bar42bar,u8R\"(meow)\"_bar42bar,R\"(foo\"bar)\",R\"ab(foo\"f)\"g)a\"bar)ab\")");
// Different identifier/keyword categories:
ta_test::text::expr::DrawToCanvas(canv, line++, 3, "($ ( foo42bar bitand static_cast<int>(0) && __COUNTER__ ) && $(foo()) && $(false))");
// Unicode: (make sure unicode chars are not highlighted as punctuation)
ta_test::text::expr::DrawToCanvas(canv, line++, 3, "[мур] int");
canv.Print(ta_test::Terminal{});

--- Colors

TA_CHECK($("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && false);

--- Werror on everything?

Test both shared and static library builds.
    Static library only once, and prefer shared for all tests.
    On Linux also add `-fvisibility-hidden` to catch missing `CFG_TA_API` calls.

TA_TEST
    Name validation
        Allow a-z A-Z 0-9 _
        No $
        Allow digits at the beginning of test names
        No /foo
        No foo/
        No foo//bar
        No other stuff: - .

TA_CHECK:
    return type is void
    local variable capture actually works, and the values are correct
    `TA_CHECK(true, true)` shouldn't compile because of a comma
    make sure that two values can't be printed side-by-side
    hard errors on unprintable stuff only in non-dependent contexts
    When doing a oneliner: `TA_CHECK(true), TA_CHECK(false)` - the first one shouldn't extend its scope into the next one.
    We're performing a proper contextual bool conversion.
    A non-const format string is a compilation error.
    A bad format is a compilation error.
    Check that on libfmt, the check for `{:?}` being supported actually passes.
    correctly breaks on the call site
    usable without (...) in fold expressions
    Gracefully fail the test if the lazy message throws?
    Error if outlives the test. Error if destroyed out of order?
    ta_test::ExactString - control characters should be printed as unicode replacements
    Compilation error on comma

--- TA_FAIL
    With and without the message.

--- TA_STOP

--- TA_MUST_THROW:
    local variable capture actually works, and the values are correct
    Doesn't warn on unused value.
    Doesn't warn on nodiscard violation.
    Doesn't warn on `;` at the end.
    Opening two same context frames deduplicates them.
    When doing a oneliner: `TA_MUST_THROW(...).Check...()`, make sure that the frame guard from the macro doesn't extend into the check.
    Element index out of range is a test fail, not a hard error.
    correctly breaks on the call site
    usable without (...) in fold expressions
    Gracefully fail the test if the lazy message throws?
    Error if outlives the test. Error if destroyed out of order?
    Error if context guard is destroyed out of order?

    call ABSOLUTELY ALL methods in three contexts: inline, inline after another method, out of line
        We must check all this due to the weird way TA_MUST_THROW is written.
        Make sure the stacks are printed correctly, including the function argments.

--- TA_LOG
    \n suffix is silently stripped, but at most once.
    Don't break if an argument throws.
    Usable in fold expressions without parentheses.

--- TA_CONTEXT
    \n suffix is silently stripped, but at most once (check both lazy and non-lazy versions)
    Don't break if an argument in the non-lazy version throws.
    What happens if the lazy version throws? We should probably gracefully stop the test.
    Usable in fold expressions without parentheses.
    Error if it outlives the test.

--- TA_GENERATE_FUNC
    Name validation to be a valid identifier.
        a-z A-Z 0-9 _
        No $
        Can't start with a digit.
    What if the generator throws? Advance the current generator, destroy the future ones?
    Hard error when called outside of a test.
    The return type, as documented.
    The lambda shouldn't be evaluated at all (capturing might have side effects) when visited repeatedly.
    The lambda should be destroyed at the end of the test, not earlier.
    Catch non-deterministic use:
        Reaching wrong TA_GENERATE - hard error
        Reaching end of test without reaching a TA_GENERATE
            Normally - a hard reror
            But not when the test failed - that's a warning - check that we correctly prune the generators and advance the last one.
    Type requirements:
        Absolutely no requirements (non-default-constructable, non-movable)
    Sanity check:
        failing 2/N repetitions fails the whole test, and the counters don't break
    When repeating the very first test after a failure, make sure it says `Continuing...` and not `Running tests...`.
    How long values are printed
        Ellipsis in a different color, both in progress report and in test failures.
        Check cutoff lengths (test all sizes: < prefix size, < prefix size + ellipsis size, < prefix size + ellipsis size + suffix size, or a long string)
    Printing the counter should neatly expand the right border when the number of digits increases.
        But not after a failure!
    Partial trimming of the indentation (but only the part that belongs to the test name, not to the generators themselves - test the cutoff)
    Reentering a test after a failure shoulnd't print the test index in bright color.
    Summary after all repetitions
        Check that we're not printing more than N repetitions.
    Overall try two scenarios: failing last repetition and failing some other ones. Make sure everything prints sanely.

    Overriding values:
        What if inserting a user value throws?
        What if generating outside of a test throws?
        User indices:
            out of range
            overlapping
        Removing all values = error

        As usual, try everything without spaces and with spaces everywhere

        & before a negative rule
        & after a negative rule
        (...) after a negative rule
        & with no (...) after it
            with a rule after it
            as the last rule

        Bad indices:
            #..
            #0..
            #..0
            #2..1
        Good indices:
            #1..1
            #2..3
            #1..
            #..1

        Outright invalid character after a generator name.

        Empty string as value.
        Whitespaces as value.
        Various containers and tuples as values, including (a,b,c)(override).

        zero rules per generator = error

        Accepting trailing whitespace after different kinds of rules.

--- TA_GENERATE
    Braced lists:
        lvalue
        rvalue
        mixed
        empty = build error
    C arrays
        lvalue and rvalue, both const and non-const
    Some dumb ranges: non-default-constructible, non-movable, of non-movable elements
    Empty range handling
        hard error by default
        interrupt-test-exception with the flag
    Build error on using a local variable

--- All the macros nicely no-op when tests are disabled
    TA_CHECK validates the arguments (crash on call?)
    $(...) only works inside of TA_CHECK
    TA_TEST generates a dummy function to validate stuff
    TA_LOG and TA_CONTEXT - crash on call??? (disable crash with a macro?)

--- `Trace` type
    All the ten thousand constructor overloads.
    Reset()
    NoTrace{}

--- Exception printer
Known and unknown exception types.
Nested exceptions (containing known and unknown types in them).

--- Assertion stack
Due to failed assertion.
Due to exception.

User messages in assertions.
    But not in assertion stack, since it's hard to do with our current implementation.

--- Test name validation
Bad test names:
    Empty name
    Starts with slash
    Ends with slash
    Two consecutive slashes
    Bad characters
    Dollar in name
Good test names:
    a-zA-Z0-9_/

--- Build errors when:
    $(...) lexically outside of ASSERT().
    No error when ASSERT() is lexically outside of a test.
    Unformattable $(...) type

--- Runtime errors when:
    Same test name registered from two different places
    But no error when it's in header, and two TUs register it with the same location
    One test name is a prefix of the other (a test can't double as a group)

--- Evaluation order weirdness
    ASSERT evaluated when no test is running.
    ASSERT evaluated in a wrong thread.
    $(...) evaluation delayed until after the assertion.
    $(...) evaluation
    If $(...) is evaluated more than once, should keep the latest value.
    Try duplicating $(...) with a macro, what then?

--- Formatting errors shouldn't compile.
    formatting containers might not be supported in the standard library yet, check it in libfmt for strings and numbers.

--- Various flag styles:
    --include foo
    --include=foo
    -ifoo
    -i foo
    But not -i=foo

--- Control characters are replaced with their symbolic representations in:
    Stringified arguments
    User assertion messages (in all macros)

--- Test name width for the results is calculated correctly.
    In particular, group names can be longer than (test names + indentation), so try with a really long group name.
        Also observe that `/` after the group name is included in the calculation.

--- Unicode
    A run with all features, and --no-unicode, automatically test that no unicode crap is printed.

--- Try to start the tests again while they are already running. Should crash with an error.

Absolutely no `{}:{}` in the code, all error locations must use

---------------

DOCUMENTATION:

* Document that you can catch InterruptTestException to ensure softness (or come up with a better looking macro)

*/
