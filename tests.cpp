#include <exception>
#include <iostream>

#include "testlib.h"

// int sum(int a, int b, int c){return a + b + c;}
bool sum(const auto &...){return false;}

// Turn `a` and `b` into `integral_constant`s with the same __COUNTER__ value, compare them in the macro to check if we're nested or not? Could work.
#define TA_VARIANT(...) (int a = 1) if (int b = 1) {} else

bool foo()
{
    // throw std::runtime_error("Blah!");

    std::string first = "aaaaaaaaaaaaa", second = "baaaaaffffffffffffar", suffix = "oof", extra = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    // TA_SOFT_CHECK( false, "Hello {}!", first);
    TA_CHECK( $($(first) + $(second)).ends_with($($(suffix.c_str()) + $(extra) + $("123") + $("456"))) );
    return false;
}

TA_TEST(foo/bar)
{
}

TA_TEST(test/lul/beta)
{
    TA_CHECK($(true) && $(foo()) == $(true), "huh");
}

TA_TEST( foo/baz )
{
}

TA_TEST( bar/alpha )
{
}
TA_TEST( omega )
{
    // throw std::runtime_error("123");
}
TA_TEST(test/lel/alpha)
{
    TA_CHECK(false);
}
TA_TEST( foo/alpha )
{
}
TA_TEST( foo/alpha1 ) {}
TA_TEST( foo/alpha2 ) {}
TA_TEST( foo/alpha3 ) {}

int main(int argc, char **argv)
{
    return ta_test::RunSimple(argc, argv);
}

// Some form of expect_throw.

// Generate module lists for faster calls.

// Scoped and unscoped logging macros.

// Review token styles. In particular, string literals are ugly bright cyan.

// Length cap on serialized values, configurable. Maybe libfmt can do the clipping for us?

// Take into account the terminal width? By slicing off the whole right section, and drawing it on the next lines.

// Test without exceptions.
// Test without RTTI. What about exception type names?

// Rebrand using this regex: `(?<![a-z])ta(?![a-z])` (not case-sensitive, not whole word).

// Subsections, for_types, and for_values (for_values optional?)

// Later:
//     Multithreading? Thread inheritance system.
//         The thread identity object should be just copyable around. Also record source location in copy constructor to identify the thread later.
//     What's the deal with SEH? Do we need to do anything?

// Maybe not?
//     Allow more characters in bracket-less form: `:`, `.`, `->`?
//     A second argument macro that doesn't error out when not printable. `TA_TRY_ARG`?
//     After file paths, print `error: ` (on MSVC `error :` ? Check that.), and some error messages for the parsers.
//     Deduplicate assertions in stacks? Both when an assertion fails and when an exception is triggered.
//     Get terminal width, and limit separator length to that value (but still not make them longer than they currently are)

// Unclear how:
//     Print user messages from assertions that didn't execute to completion.
//     Don't show the assertion user message the second time when printing the expression

// Selling points:
//     * Expression unwrapping
//     * Printing nested exceptions out of the box


/* Pending tests:

--- Expression colorizer

ta_test::detail::TextCanvas canv;
ta_test::detail::DrawExprToCanvas(canv, 1, 3, "foo(42, .5f,.5f, 5.f, 5.4f, 42_lit, 42lit, 42_foo42_bar, +42,-42, 123'456'789, 0x123'456, 0123'456)");
ta_test::detail::DrawExprToCanvas(canv, 2, 3, "foo(12e5,12e+5,12e-5,12.3e5,12.3e+5,12.3e-5,0x1p2,0x1p+2,0x1p-2,0x12.34p2)");
ta_test::detail::DrawExprToCanvas(canv, 3, 3, "foo(\"meow\",foo42foo\"meow\"bar42bar,\"meow\"_bar42bar,\"foo\\\"bar\")");
ta_test::detail::DrawExprToCanvas(canv, 4, 3, "foo(\'meow\',foo42foo\'meow\'bar42bar,\'meow\'_bar42bar,\'foo\\\'bar\')");
ta_test::detail::DrawExprToCanvas(canv, 5, 3, "foo(R\"(meow)\",foo42fooR\"(meow)\"bar42bar,u8R\"(meow)\"_bar42bar,R\"(foo\"bar)\",R\"ab(foo\"f)\"g)a\"bar)ab\")");
// Different identifier/keyword categories:
int foo42bar = 42;
TA_CHECK($ ( foo42bar bitand static_cast<int>(0) && __COUNTER__ ) && $(foo()) && $(false));
// Unicode:
TA_CHECK($("мур"));
canv.Print(true, stdout);

--- Colors

TA_CHECK($("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && $("foo") && false);

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

*/
