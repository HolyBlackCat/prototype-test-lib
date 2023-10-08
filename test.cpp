#include <exception>
#include <iostream>

#include "testlib.h"

int sum(int a, int b, int c){return a + b + c;}

// Turn `a` and `b` into `integral_constant`s with the same __COUNTER__ value, compare them in the macro to check if we're nested or not? Could work.
#define TA_VARIANT(...) (int a = 1) if (int b = 1) {} else

int main()
{
    // TA_CHECK($(1 + $(2) ) + TA_ARG(3) == 7);
    // int a = 1, b = 2, c = 3;
    // TA_CHECK($(sum($(a), $(b), $(c))) == 7);

    ta_test::detail::TextCanvas canv;
    canv.DrawText(0, 0, "Hello!");
    canv.DrawText(0, 7, "World!", {.style = {.color = ta_test::detail::TextColor::dark_blue}});
    canv.DrawUnderline(1, 7, 6, {.style = {.color = ta_test::detail::TextColor::light_green, .bold = true}});
    canv.DrawColumn(2, 12, 3, 1, {.color = ta_test::detail::TextColor::dark_red});
    canv.Print(true, stdout);

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


// * Replace `emit_char` to emit blocks of characters. Or not?
// * Replace `std::terminate` calls with something saner.
