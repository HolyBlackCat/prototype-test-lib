#include <exception>
#include <iostream>
#include <deque>

#include <taut/taut.hpp>

// reset && LANG= make -C tests COMPILERS=clang++ SKIP=clang++_libstdc++_c++2b -j12

TA_TEST(foo/test)
{
    TA_GENERATE_PARAM(auto T, 1, 2u, 3ll)
    {
        std::cout << T << '\n';
    };
}

int main(int argc, char **argv)
{
    return ta_test::RunSimple(argc, argv);
}

// TESTS!!

// Support all three big compilers!

// Check that paths are clickable in Visual Studio (especially when not at line start)

// Add CMakeLists.txt!
//     pkg-config files!!

// Introduction and license in headers? Maybe not the license? A short introduction in modules.hpp too.

// Documentation!
//     Document configuring different IDEs/debuggers.

// v0.2:
//     Optionally no exceptions
//     Optionally no RTTI
//     Multithreading? Thread inheritance system.
//         The thread identity object should be just copyable around. Also record source location in copy constructor to identify the thread later.
//     Experiment with filesystem::path and wstring and wchar_t (on all compilers and on libfmt), and u8string and u16string and u32string and char{16,32,8}_t
//         If good, don't forget to enable "lazy copy for printing" for them
//     Add explicit instantiation declarations/definitions for BasicCaughtExceptionInterface (this requires moving derived classes to namespace scope, which is sad)
//     Experiment with function attributes ("artificial"?) for better debugging
//     Analyzing TA_MUST_THROW must immediately point to the user code first, and not to the library internals
//         Add an argument to AssertParam to customize the source location, and stop baking it into template parameters
//     In TA_GENERATE_PARAM, move the fat lambdas to a template IF the parameter kind is unparenthesized.

// Later:
//     Column indicators in paths, and transition to source_location entirely?

// Maybe not?
//     Allow more characters in bracket-less form: `:`, `.`, `->`?
//     What's the deal with SEH? Do we need to do anything?
//     Signal handling?
// Maybe not...
//     Get terminal width, and limit separator length to that value (but still not make them longer than they currently are)
//     Decorate line breaks in logs with `//` as well?
//     Lazy string conversion for ranges:
//         Forward iterator or stronger, the element is also lazy copyable (similar range or `CopyForLazyStringConversion == true`)
//         Copy everything to a single flat heap buffer (separate pass to calculate the buffer length)
//             Think about range of ranges (std::filesystem::path)
// Probably not:
//     Try to enforce relative paths, and try printing errors on the same line as paths.
//     A second argument macro that doesn't error out when not printable. `TA_TRY_ARG`?
//     Short macros that can be disabled in the config.
//     After file paths, print `error: ` (on MSVC `error :` ? Check that.), and some error messages for the parsers.
//     $[...] should tolerate non-printable arguments, but only in non-dependent context - stops being possible when we transition to `$[...]` spelling.
//     In, $[...] for really long lines, do just [1], then a reference at the bottom: `[1]: ...`. (Decide on the exact spelling, for this to not be confused with a single-element vector, or whatever)
//         What's the point? $[...] isn't lazy, so you shouldn't have long lines in it anyway. Use the user message, which is lazy.
//     Do we force-open the console on Windows if there's none? That's when `GetFileType(GetStdHandle(STD_OUTPUT_HANDLE))` returns 0.
//         Maybe not? If this is a release build, then we'll also not have argc/argv, and if the user goes out of their way to construct them,
//         they can also open the console themselves.

// Unclear how:
//     To actually read the `new_value_when_revisiting` flag, the lambda needs to be constructed, which sucks dick. But there's probably no workaround?
//     `noop_if_empty` flag for generators. This will work for `TA_GENERATE_PARAM` and `TA_SELECT`, but what to do with others to have feature parity?
//     In `TA_GENERATE_PARAM`, an extra list can only runs on demand from the command line.
//     Draw a fat bracket while explaining each test failure?
//     `$[...]` could be useful to provide context for non-printable function calls (including void).
//     -g messes up repetition counter a bit if a generator throws (while in the after-test generator update block)

// Selling points:
//     * Expression unwrapping
//     * Better sections/subcases (can do a cross product, always at least one executes) (controllable from the command line, but so are catch2's ones)
//     * Generators
//         * Understand C++20 ranges
//         * Fully controllable from the command line (though catch2's subsections are too; check that their generators aren't)
//         * ERGONOMICS
//             * fully logged (check if catch2 can enable this; if yes say "by default")
//             * can copypaste the reproducer string from the output
//     * Quality of life:
//         * Using std::format/libfmt everywhere, no iostreams.
//         * First-class nested exceptions support out of the box
//             * Also rich exception analysis: Can check derived type and exact match (check what gtest and catch2 can do)
//         * True lazy message evaluation
//             * Point out that you can't do proper lazyness with <<, because operands are still evaluated.
//         * No comma weirdness in macros
//         * Strong assertions throw rather than using `return`.
//         * hard/soft enum is passed to assertions at runtime.
//         * Good automatic breakpoints! Do other frameworks do that?
//     * Clickthrough everywhere (i.e. file paths everywhere, that should be clickable in an IDE)
//     * Tests in headers = bad practice, but we support it without code duplication, but still check if the test names clash (different source locations)
//         * Compare with what gtest and catch2 do.
//
// Do gtest and catch2 support non-default-constructible type parameters? Check.


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
    std::filesystem::path - don't crash with unicode symbols on non-unicode locales? Need to experiment.

--- FromString
    Same types as in ToString
    Reject duplicate keys in sets and maps
    Reject spaces before and after:
        scalars
        strings, chars
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
        single `char`:
            reject multibyte characters. And even characters with codes > 0x7f.
            reject empty literals: ''.

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

Big-ass test:
    with different macro values
        CFG_TA_FMT_HAS_FILE_VPRINT=0
        CFG_TA_FMT_HAS_RANGE_FORMAT=0

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
    returns the condition value (casted to bool)
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

    Check all overloads of the second (...)
        flags
        flags, location
        message...
        flags, message...
        flags, location, message...

    Does `$[...]` has top-level (...) in expansion? I think not. Add a negative test.

    Try passing an rvalue to $[...] that has CopyForLazyStringConversion specialzied to true. It must not be moved by the `$[...]` itself.

    Challenge the parsing:
        strings, char literals, raw strings - all containing opening/closing brackets, whole $[...]

    TA_CHECK( ((((((((((((($[(((42)))]))))))))))))) ) // There's some internal limit on the number of parens, but this is way below it.

    TA_CHECK( $ [ 42 ] ) - spaces must work

    TA_CHECK( ($)[42] ), TA_CHECK( ( $ ) [42] ) - should work. $ must be grayed out, but not its enclosing `(`,`)`

    Build error if $ is already expanded (nested in another macro)

    "in here" context for non-nested brackets.
    "in here" context for nested brackets (should show the most nested one)

    What if wrong thread evaluates the $[...]? That should be a hard error.

    Multiline user messages.

--- TA_FAIL
    With and without the message.

--- TA_INTERRUPT_TEST

--- TA_MUST_THROW:
    local variable capture actually works, and the values are correct
    Doesn't warn on unused value.
    Doesn't warn on unused [[nodiscard]] value??? (if possible at all)
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

    Hard and soft flags
        also check how they are printed on failures.

    Checking unknown exceptions. All functions must fail.

    Specifying wrong index for ANY function (or MakeContextGuard()) fails the test
        unless in soft mode

    CaughtException should be default-constructible

    Empty CaughtException should pass all checks.
        MakeContextGuard for it should be a no-op

    Moved-from CaughtException should fail all checks.

    Multiline user messages.

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
    Lazy version should always re-evaluate to print the up-to-date variable values.

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
            Check that if we have both to_string and from_string conversions, BUT not ==, then the index gets printed.
        Otherwise the whole summary is replaced with `...`.

    Default value of `repeat` is true.

    Passing an lvalue functor.

    When printing a generator, don't print = before an empty string, even if the type is convertible to string.

    How exceptions are handled:
        when constructing the lambda
        when calling it
            for the first time
            for the second+ time

    [[nodiscard]] on the result

    Ban nested calls (generating from a generator callback)

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

    [[nodiscard]] on the result

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

    (typename T) = build error
    (typename T,) = build error
    (typename T, int) = ok
    (typename T, (int)) = ok
    (typename T, ()) = OK
    (typename T, (int), ta_test::GeneratorFlags{}) = ok
    (typename T, (), ta_test::GeneratorFlags{}) = ok
    (typename T, (int),) = ok? (doesn't look good, but doesn't worth the effort checking for?)
    (typename T, (),) = ok? (same)

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

    Pastable reproducer strings should print the proper values (if short enough), not # indices

--- TA_SELECT and TA_VARIANT
    No registered variants = runtime error
        Must mention the source location
        The flags parameter to allow no variants.
    break and continue
        inside TA_VARIANT
        just inside TA_SELECT
    nested variants (for same TA_SELECT) = build error
    duplicate variant name = build error

    Pastable reproducer strings should print the proper values (if short enough), not # indices

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

ta_test::IsFailing()

strictness of identifiers:
    No $ in any names
    Names shouldn't start with digits (except text names can)
    No empty names, can only contain alphanumeric chars and _

--- Interactive tests for Clangd!!
    Must immediately report bad names:
        test names
        all generator macros

breakpoints?
    TA_CHECK
        if condition is false
        if exception falls out (only visible in debugger with `--catch`)
    TA_MUST_THROW
        if exception is missing
    TA_TEST
        if exception falls out (only visible in debugger with `--catch`)
        after any test failure (after continuing after a failure)


---------------

DOCUMENTATION:

* Document that you can catch InterruptTestException to ensure softness (or come up with a better looking macro)

*/
