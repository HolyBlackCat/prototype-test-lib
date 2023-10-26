#include <exception>
#include <iostream>

#include "testlib.h"

// int sum(int a, int b, int c){return a + b + c;}
bool sum(const auto &...){return false;}

// Turn `a` and `b` into `integral_constant`s with the same __COUNTER__ value, compare them in the macro to check if we're nested or not? Could work.
#define TA_VARIANT(...) (int a = 1) if (int b = 1) {} else

bool foo()
{
    std::string first = "aaaaaaaaaaaaa", second = "baaaaaffffffffffffar", suffix = "oo\nf", extra = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    TA_CHECK( $($(first) + $(second)).ends_with($($(suffix.c_str()) + $(extra) + $("123") + $("456"))) );
    return false;
}

TA_TEST(foo/bar)
{
}

TA_TEST(test/lul/beta)
{
    TA_CHECK($(true) && $(foo()) == $(true));
}

TA_TEST( foo/baz )
{
}

TA_TEST( bar/alpha )
{
}
TA_TEST( omega )
{
}
TA_TEST(test/lel/alpha)
{

}
TA_TEST( foo/alpha )
{
}
TA_TEST( foo/alpha1 ) {}
TA_TEST( foo/alpha2 ) {}
TA_TEST( foo/alpha3 ) {}

int main()
{
    ta_test::Runner runner;
    runner.SetDefaultModules();
    runner.SetOutputStream(stdout);
    runner.SetTerminalSettings({.color = true});
    return runner.Run();
}

// Can we use `{:?}` for all types? Test separately in libfmt and std::format.

// Make it so that `$(...)` is impossible to use outside of ASSERT?

// Scoped and unscoped logging macros.

// Force remove \n from strings. Replace it with configurable character, something from Unicode by default.
// Move text around to fit into the bracket. Or maybe not?
// Allow more characters in bracket-less form: `:`, `.`, `->`?

// A second argument macro that doesn't error out when not printable. `TA_TRY_ARG`?

// Length cap on serialized values, configurable

// Take into account the terminal width? By slicing off the whole right section, and drawing it on the next lines.
// Test without RTTI. What about exception type names?

// Print to a custom fprintf-like callback instead of fprintf directly?

// Multithreading?


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

*/
