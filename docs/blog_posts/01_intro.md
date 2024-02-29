# Designing a better unit-testing library

In this post I want to introduce TODO:LINK, a new unit test framework for C++ that I've been working on for the past few months, and explain some of the design decisions.

I've came up with a new [cleaner assertion syntax](#assertion-macros), and liked it so much that I decided to build a whole unit testing library around it.

Other than the assertion syntax, I like Catch2 design, so I've copied some of the features (subcases and generators), improving them along the way. I've changed some things that I consider design mistakes, and dropped some features that I believe are unnecessary.

* [A novel assertion macro syntax](#assertion-macros) — offers better diagnostics, while being much easier to learn (just one macro, doesn't need matchers, no more writing custom predicates, etc)

* `std::format()` instead of iostreams everywhere.

  * The library can be configured to use [`libfmt`](https://fmt.dev/latest/index.html) instead.

* [Catch2-like subcases](TODO) — extended to allow cartesian products of subcases.

* [Catch2-like generators](TODO) — based on C++20 ranges (while Catch2 had a homegrown imitation of ranges that's only used for generators).

  * Can be overridden with command-line flags.

* First-class support for [nested exceptions](https://en.cppreference.com/w/cpp/error/nested_exception).

* Removed clutter to have a leaner API:

  * No BDD, Gherkin, etc.
  * No fixtures — Catch2 considered them unidiomatic but still supported them.

    TODO explan that fixtures aren't needed

## Assertion macros

TL;DR: Just `TA_CHECK( condition );`, and you can wrap any part of the condition in `$[...]` to print it.
