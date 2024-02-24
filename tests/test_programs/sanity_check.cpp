// A minimal program using the framework,
// that is tested by the makefile itself to make sure we can run the more complicated tests.

#include <taut/taut.hpp>

#include <iostream>

enum class Mode
{
    pass,
    fail_assertion,
    throw_exception,
};
Mode mode = Mode::pass;

int num_called_tests = 0;

TA_TEST( sanity/1 )
{
    TA_CHECK( true );
    num_called_tests++;
}

TA_TEST( sanity/2 )
{
    TA_CHECK( mode != Mode::fail_assertion );

    if (mode == Mode::throw_exception)
        throw std::runtime_error("Boo!");

    num_called_tests++;
}

TA_TEST( sanity/3 )
{
    TA_CHECK( true );
    num_called_tests++;
}

int main(int argc, char **argv)
{
    if (argc == 2)
    {
        if (std::string_view(argv[1]) == "--fail-assertion")
            mode = Mode::fail_assertion;
        else if (std::string_view(argv[1]) == "--throw-exception")
            mode = Mode::throw_exception;
    }

    int ret = ta_test::RunSimple(0, nullptr);

    if (mode == Mode::pass && num_called_tests != 3)
    {
        std::cout << "The tests didn't run as expected.";
        return 1;
    }

    return ret;
}
