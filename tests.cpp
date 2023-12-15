#include <exception>
#include <iostream>

#include "testlib.h"

// Print summary after all repetitions finish.

// #error 2. Handle interruptions (failed test or IntteruptException = prune remaining generators, otherwise complain about non-determinism)


#if 0
foo=42,bar[2],baz=56
    |      |      |
   [1]     42    [3]

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

TA_TEST(foof/1)
{
    // static int i = 0;
    // std::cout << "### " << i++ << '\n';

    [[maybe_unused]] auto a = TA_GENERATE_FUNC(foo, [i = 0](bool &repeat) mutable {if (i == 20) repeat = false; return std::string(std::size_t(i++), 'f');});
    // std::cout << a << '\n';

    [[maybe_unused]] auto b = TA_GENERATE_FUNC(blahh, [i = 20](bool &repeat) mutable {if (i == 21) repeat = false; return i++;});
    // std::cout << b << '\n';

    if (a.size() == 5)
        TA_FAIL;


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
    return ta_test::RunSimple(argc, argv);
}

//

// Roundtrip check when printing a reproduction string.

// TA_GENERATE (non-_FUNC)

// Report failed values in test summary? (this one only during the run?) And also how many repetitions per test failed/passed/total.

// Different global summary style: (3 columns? tests, repetitions(?), asserts)
//     Skipped: 42
//     Passed:  42
//     FAILED:  42

// Overriding generator values?

// TA_VARIANT

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
    ??? wchar_t, char16_t, char32_t
    int, short, long, float, double, long double
    std::vector<int>
    std::set<int>, std::map<int, int> - format will differ in old `std::format`?

--- Expression colorizer

ta_test::text::CommonData common;
ta_test::text::TextCanvas canv(&common);
ta_test::text::expr::DrawToCanvas(canv, 1, 3, "foo(42, .5f,.5f, 5.f, 5.4f, 42_lit, 42lit, 42_foo42_bar, +42,-42, 123'456'789, 0x123'456, 0123'456)");
ta_test::text::expr::DrawToCanvas(canv, 2, 3, "foo(12e5,12e+5,12e-5,12.3e5,12.3e+5,12.3e-5,0x1p2,0x1p+2,0x1p-2,0x12.34p2)");
ta_test::text::expr::DrawToCanvas(canv, 5, 3, "1+1"); // `+` must not be highlighted as a number
ta_test::text::expr::DrawToCanvas(canv, 3, 3, "foo(\"meow\",foo42foo\"meow\"bar42bar,\"meow\"_bar42bar,\"foo\\\"bar\")");
ta_test::text::expr::DrawToCanvas(canv, 4, 3, "foo('a','\\n','meow',foo42foo'meow'bar42bar,'meow'_bar42bar,'foo\\'bar')");
ta_test::text::expr::DrawToCanvas(canv, 5, 3, "foo(R\"(meow)\",foo42fooR\"(meow)\"bar42bar,u8R\"(meow)\"_bar42bar,R\"(foo\"bar)\",R\"ab(foo\"f)\"g)a\"bar)ab\")");
// Different identifier/keyword categories:
ta_test::text::expr::DrawToCanvas(canv, 6, 3, "($ ( foo42bar bitand static_cast<int>(0) && __COUNTER__ ) && $(foo()) && $(false))");
// Unicode: (make sure unicode chars are not highlighted as punctuation)
ta_test::text::expr::DrawToCanvas(canv, 7, 3, "[мур] int");
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
        Reaching wrong TA_GENERATE
        Reaching end of test without reaching a TA_GENERATE
            But not when InterruptTestException was thrown
            And not when the test fails
            ... in those case we should remove the remaining generators, and advance the last one
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
    User assertion messages

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
