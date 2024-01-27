#include <exception>
#include <iostream>
#include <deque>

#include "testlib.h"
#if 0

TA_CHECK( $[($[a] - $[b]).length()] < 42 )

            Tests  Variants    Checks
Skipped         1         1         1
Passed          1         1         1
FAILED          1         1         1

            Tests    Checks
Skipped         1         1
Passed          1         1
FAILED          1         1

bool fof()
{
    TA_CONTEXT("Hello {}", []{std::cout << "A\n"; return 42;}());
    TA_LOG("Hello!");
    TA_CONTEXT_LAZY("Hello {}", []{std::cout << "B\n"; return 43;}());
    try{
        TA_CHECK($[1] == $[2]);
    }
    catch (ta_test::InterruptTestException) {}
        TA_CHECK($[1] == $[2]);
    // TA_CHECK($[1] == $[2])("x = {}", 42);
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

TA_TEST(foo/test)
{
    // std::cout << TA_GENERATE(x, {nullptr}) << '\n';

    TA_GENERATE_PARAM(auto T, nullptr, 42, 'A')
    {
        // std::cout << T << '\n';
    };

    TA_GENERATE_PARAM(typename U, ta_test::expand, std::tuple<int, float>)
    {
        // std::cout << ta_test::text::TypeName<T>() << '\n';
    };
}

TA_TEST(foo/test2)
{
    int a = 1, b = 2, c = 3;
    TA_CHECK($[$[a] + $[b] + $[c]] == 7);
}

int main(int argc, char **argv)
{
    return ta_test::RunSimple(argc, argv);
}

// Try supporting $[...] instead of $[...] - looks cleaner to me (internally make it easier to switch to (...) if desired.

// TA_VARIANT (should be scoped?)

// Optimize the calls to the `BasicPrintingModule` with the module lists too.

// Split the runner (with all modules) into a separate header? Including most utility functions too.

// Better CaughtException interface?
//     single function to check combined message
//     or
//     THIS: expand ForEach to allow "any" elem to be checked; expand context to allow pointing to element

// Test without exceptions.
// Test without RTTI. What about exception type names?

// Subsections

// Not now? -- Move `mutable bool should_break` to a saner location, don't keep it in the context? Review it in all locations (TA_CHECK, TA_MUST_THROW, etc).

// Check that paths are clickable in Visual Studio

// Later:
//     Should we transition to __PRETTY_FUNCTION__/__FUNCSIG__-based type names?
//     Soft TA_MUST_THROW? (accept AssertFlags somehow?)
//     Multithreading? Thread inheritance system.
//         The thread identity object should be just copyable around. Also record source location in copy constructor to identify the thread later.
//     What's the deal with SEH? Do we need to do anything?
//     Signal handling?

// Maybe not?
//     Soft TA_MUST_THROW?
//     Allow more characters in bracket-less form: `:`, `.`, `->`?
// Maybe not...
//     Get terminal width, and limit separator length to that value (but still not make them longer than they currently are)
//     Try to enforce relative paths, and try printing errors on the same line as paths.
//     Decorate line breaks in logs with `//` as well?
//     A second argument macro that doesn't error out when not printable. `TA_TRY_ARG`?
// Probably not:
//     Short macros that can be disabled in the config.
//     After file paths, print `error: ` (on MSVC `error :` ? Check that.), and some error messages for the parsers.
//     $[...] should tolerate non-printable arguments, but only in non-dependent context - stops being possible when we transition to `$[...]` spelling.
//     In, $[...] for really long lines, do just [1], then a reference at the bottom: `[1]: ...`. (Decide on the exact spelling, for this to not be confused with a single-element vector, or whatever)
//         What's the point? $[...] isn't lazy, so you shouldn't have long lines in it anyway. Use the user message, which is lazy.
//     Do we force-open the console on Windows if there's none? That's when `GetFileType(GetStdHandle(STD_OUTPUT_HANDLE))` returns 0.
//         Maybe not? If this is a release build, then we'll also not have argc/argv, and if the user goes out of their way to construct them,
//         they can also open the console themselves.

// Unclear how:
//     Draw a fat bracket while explaining each test failure?
//     `$[...]` could be useful to provide context, but what if the function returns void or non-printable type?
//     -g messes up repetition counter a bit if a generator throws (while in the after-test generator update block)

// Selling points:
//     "DESIGNING A SUPERIOR UNIT TEST FRAMEWORK"
//     * Expression unwrapping
//     * Sections that are not broken (can do a cross product)
//     * First-class nested exceptions support out of the box
//     * Lazy message evaluation
//         * Point out that you can't do proper lazyness with <<, because operands are still evaluated.
//     * No comma weirdness
//     * Clickthrough everywhere (i.e. file paths everywhere, that should be clickable in an IDE)
//     * Tests in headers = bad practice, but we support it without code duplication, but still check if the test names clash (different source locations)


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
    nullptr

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
ta_test::text::expr::DrawToCanvas(canv, line++, 3, "($ ( foo42bar bitand static_cast<int>(0) && __COUNTER__ ) && $[foo(]) && $[false])");
// Unicode: (make sure unicode chars are not highlighted as punctuation)
ta_test::text::expr::DrawToCanvas(canv, line++, 3, "[мур] int");
canv.Print(ta_test::Terminal{});

--- Colors

TA_CHECK($["foo"] && $["foo"] && $["foo"] && $["foo"] && $["foo"] && $["foo"] && $["foo"] && $["foo"] && $["foo"] && $["foo"] && $["foo"] && $["foo"] && false);

--- Werror on everything?

Results printer:
    A custom message when no tests are registered
        but exit code 0

    Checks counter:
        TA_CHECK
            check failure on both false and throw, but NOT on InterruptTestExceptions
            total is incremented BEFORE entering
        FAIL
            increments both total and failure
        TA_MUST_THROW
            total is incremented BEFORE entering
            increments fail if no exception

        TA_GENERATE - MUST NOT increment check counters (check on exception and without it)

    Variants
        Only show the column when have variants

    All combinations of zero/nonzero counts of: skipped, failed, passed (2^3)

Non-zero exit code when all tests are skipped, or none are registered

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
    Duplicate names in the same file = compile error
    Duplicate names in different files = either no error (if source locations match = in header) or a runtime error otherwise

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
    TA_CHECK(((((((((((((($[(((42)))])))))))))))))) // There's some internal limit on the number of parens, but this is way below it.

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
        What if the generator lambda constructor throws?
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

    On test failure, print the generator summary after the test name, to simplify reproduction.
        If the roundtrip passes AND the string is short (<= 20 chars, since that's max for [u]uint64_t), THEN printed as `=...`.
        Otherwise if the value doesn't come from `=...` flag, THEN print the index `#...`.
        Otherwise the whole summary is replaced with `...`.

    Overriding values:
        =
            (...)
            Repeating = adds the same value multiple times.
            REMOVES this value from the normal generation
                BUT ONLY IF it's equality-comparable
        -=
            -= can negate the = values, but only if -= comes after =.
        #
            (...)
        -#

        Empty {} are illegal.

        Empty () are legal.

        Test types convertible from string but not eq-comparable (and not convertible to string)
        Test types both convertible from string and eq-comparable (and not convertible to string)

        Common tests:
            (#1,=42) - # disables values by default, we don't want it to disable =42
            =42,-=42 - no values
            -=42,=42 - generates 42

        What if inserting a user value throws?
        What if generating outside of a test throws?
        User indices:
            out of range
            overlapping
        Removing all values = error

        As usual, try everything without spaces and with spaces everywhere
            Except no whitespaces between .. and numbers (which is good?)

        & before a negative rule
        & after a negative rule
        (...) after a negative rule
        & with no (...) after it
            with a rule after it
            as the last rule

        Bad indices:
            #-1
            #0
            #..
            #0..
            #..0
            #2..1
            #18446744073709551615
            #18446744073709551615..
            #..18446744073709551615
        Good indices:
            #1
            #1..1
            #2..3
            #1..
            #..1

        Make sure the upper bound isn't too big (at least one repetition needs to reach the max value for it to be ok)

        Outright invalid character after a generator name.

        Empty string as value = early error
        Whitespaces as value = early error

        Various containers and tuples as values, including (a,b,c), (a,b,c)(override).

        zero rules per generator = error

        Accepting trailing whitespace after different kinds of rules.

        Nice error on a bad test regex.

        Allow --include to introduce a generator override

        LAST matching -g is used.


        Check overriding a nested generator (the same override should be reused multiple times)

        Empty () must be legal

        Jank parsed types that land before and after (test both) the expected value bounds.
            Try 1-char difference to make sure the markers don't overlap.
            Test for both = and -=

        Error if a regex matched no tests at the end of program.

        Unconsumed program at the end of any test run = NOT an error
        But any unused thing in the flag is an error at the end of the program
            Rules are "used" when they match something, OR when they provide a (...), OR when they negate a previous ().
                But if they negate a program that'a already zeroed, they are NOT used.
        * Whole flag is unused?
            * If not, for each generator:
                * Whole generator is unused? Don't highlight (...) after a generator.
                    * If not, individual rules are unused?
                        DO highlight (...) after a rule.
                        Don't highlight whitespace before and after a rule - check each rule type.
                * Also check `(...)`, regardless of anything.


        Last matching positive rule provides the `(...)`.
            !!! If the last matching positive rule has no `(...)`, it undoes the previous ones.
                Test this for both = and #

            * -g 'foo/bar//x{#1..,#5()},y=10' - override `y=10` for all values of `x` except the 5-th one.
            * -g 'foo/bar//x{#1..(y=10),#5}' - same effect as above.

        Specifying an outright wrong generator name

        Parallel overrides:
            A=1,B=2 + A=3,C=4 === A=3, then {B=2,C=4}(in any order)

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

--- TA_GENERATE_PARAM
    Work with -g
        For duplicate elements, -g must return the first one.
        In the error message, print the list of allowed strings, but don't repeat them, and don't sort them.
    When generating a value, don't break when only some values are printable (non-printable shouldn't show `= ...` in the log)
    The user can return values from the lambda:
        Forward the result (The return type is `auto`, not `decltype(auto)`)
        Check for [[nodiscard]]
    Not adding (...) on complex a initial parameter should give a pretty static_assert, not a cryptic error (preferably)
    What should work without (...)
        typename
        class
        auto
    (...) can only be followed by a single identifier
    Check for crap like `(std::)integral` by declaring a dummy lambda without a parameter name

    Default return type is auto (check that it's not a reference).
    User can -> override return type (check that references are forwarded correctly)

    Single arg = ok
    0 args = nice error

    `auto` uses `string_conv::ToString()` to print stuff (because MSVC prints template param `int`s in hex otherwise, which is lame),
        but falling back to native printing
    But `typename` uses __PRETTY_FUNCTION__ printing, because that's generally nicer (check this while being inside of various templates,
        to make sure they don't mess with our name parsing).

    Test NTTPs: `(int) X, 10, 20`.
    Test `nullptr`.

    For non-char integrals prepend the type: `(int)42`.
        Make sure -g understands this syntax.

    ta_test::expand (with types, values, and templates - use std::tuple for types?)
        (auto A, ta_test::expand) = build error, don't generate `expand`.
        (auto A, ta_test::expand, 42) = build error
        (auto A, ta_test::expand, ValueTag<42>, 42) = build error

--- All the macros nicely no-op when tests are disabled
    TA_CHECK validates the arguments (crash on call?)
    $[...] only works inside of TA_CHECK
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
    $[...] lexically outside of ASSERT().
    No error when ASSERT() is lexically outside of a test.
    Unformattable $[...] type

--- Runtime errors when:
    Same test name registered from two different places
    But no error when it's in header, and two TUs register it with the same location
    One test name is a prefix of the other (a test can't double as a group)

--- Evaluation order weirdness
    ASSERT evaluated when no test is running.
    ASSERT evaluated in a wrong thread.
    $[...] evaluation delayed until after the assertion.
    $[...] evaluation
    If $[...] is evaluated more than once, should keep the latest value.
    Try duplicating $[...] with a macro, what then?

--- Formatting errors shouldn't compile.

--- Various flag styles:
    --include foo
    --include=foo
    -ifoo
    -i foo
    But not -i=foo

--- Command line flags
    Nice error on bad --include regex, same for `--generate` regex

    --help shouldn't crash.

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
