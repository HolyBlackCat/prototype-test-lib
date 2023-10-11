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

int main()
{
    ta_test::config.output_stream = stdout;
    ta_test::config.text_color = true;

    // int a = 1, b = 2, c = 3;
    // TA_CHECK($(sum($(a), $(b), $(c))) == 7);

    // std::string a = "мур", b = "4\n\0333";
    // char c = 'e';
    // TA_CHECK($ (sum($(a), $(b), $( c )) ) == true);

    // TA_CHECK( (first + second).ends_with(suffix) );
    TA_CHECK($(true) && $(foo()) && $(false));

    // TA_FOR_TYPES(int, float, double)
    // {
    //     using T = TA_TYPE;
    // };
    // TA_FOR_VALUES(1, 2.3f, "53")
    // {
    //     using T = TA_VALUE;
    // };

    // if TA_VARIANT(foo)
    // {

    // }
    // else if TA_VARIANT(bar)
    // {

    // }




    // TA_FOR(x, vec)
    // {

    // }
}

// Force remove \n from strings. Replace it with configurable character, something from Unicode by default.
// Move text around to fit into the bracket. Or maybe not?
// Allow more characters in bracket-less form: `:`, `.`, `->`?
// Test that we can handle spaces in `$ ( x )`.

// A second argument macro that doesn't error out when not printable. `TA_TRY_ARG`?

// Length cap on serialized values, configurable

// Color the keywords. Separately color `true`, `false`, `nullptr` (same as numbers?).


/* Pending tests:

--- Expression colorizer

ta_test::detail::TextCanvas canv;
ta_test::detail::DrawExprToCanvas(canv, 1, 3, "foo(42, .5f,.5f, 5.f, 5.4f, 42_lit, 42lit, 42_foo42_bar, +42,-42, 123'456'789, 0x123'456, 0123'456)");
ta_test::detail::DrawExprToCanvas(canv, 2, 3, "foo(12e5,12e+5,12e-5,12.3e5,12.3e+5,12.3e-5,0x1p2,0x1p+2,0x1p-2,0x12.34p2)");
ta_test::detail::DrawExprToCanvas(canv, 3, 3, "foo(\"meow\",foo42foo\"meow\"bar42bar,\"meow\"_bar42bar,\"foo\\\"bar\")");
ta_test::detail::DrawExprToCanvas(canv, 4, 3, "foo(\'meow\',foo42foo\'meow\'bar42bar,\'meow\'_bar42bar,\'foo\\\'bar\')");
ta_test::detail::DrawExprToCanvas(canv, 5, 3, "foo(R\"(meow)\",foo42fooR\"(meow)\"bar42bar,u8R\"(meow)\"_bar42bar,R\"(foo\"bar)\",R\"ab(foo\"f)\"g)a\"bar)ab\")");
canv.Print(true, stdout);

*/
