#include <exception>
#include <iostream>

#include "testlib.h"

// int sum(int a, int b, int c){return a + b + c;}
bool sum(const auto &...){return false;}

// Turn `a` and `b` into `integral_constant`s with the same __COUNTER__ value, compare them in the macro to check if we're nested or not? Could work.
#define TA_VARIANT(...) (int a = 1) if (int b = 1) {} else

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
    return true;
}
TA_TEST(foo/bar)
{
    TA_CHECK($(std::string("1\n2")).size() > 0 && $(fof()))("y = {}", 43);
}

int main(int argc, char **argv)
{
    return ta_test::RunSimple(argc, argv);
}

// Review styles:
//     string literals are ugly bright cyan

// Throw away excessive use of `size_t`, switch to `int` or `ptrdiff_t`?

// Test without exceptions.
// Test without RTTI. What about exception type names?

// Subsections, for_types, and for_values (for_values optional?)

// Move `mutable bool should_break` to a saner location, don't keep it in the context?
// Review it in all locations (TA_CHECK, TA_MUST_THROW, etc).

// Do we need `__visibility__("default")` when exporting from a shared library on Linux? And also test that somehow...

// Short macros that can be disabled in the config.

// Rebrand using this regex: `(?<![a-z])ta(?![a-z])` (not case-sensitive, not whole word).
// Sort declarations, then sort definitions.

// Later:
//     Somehow don't evaluate the message if the assertion passes? Perhaps change the syntax to `CHECK(cond, LOG("{}", 42))`, reusing one of the log macros?
//         Same for the arguments passed to `Trace`?
//     Multithreading? Thread inheritance system.
//         The thread identity object should be just copyable around. Also record source location in copy constructor to identify the thread later.
//     What's the deal with SEH? Do we need to do anything?
//     Do we force-open the console on Windows if there's none? That's when `GetFileType(GetStdHandle(STD_OUTPUT_HANDLE))` returns 0.
//     Take into account the terminal width? By slicing off the whole right section, and drawing it on the next lines.
//     Length cap on serialized values, configurable.

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
//     * Expression unwrapping
//     * First-class nested exceptions support out of the box
//     * Lazy message evaluation
//     * No comma weirdness


/* Pending tests:

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

---------------

DOCUMENTATION:

* Document that you can catch InterruptTestException to ensure softness (or come up with a better looking macro)

*/
