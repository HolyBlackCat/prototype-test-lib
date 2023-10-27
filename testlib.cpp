#if CFG_TA_SHARED
#define CFG_TA_API __declspec(dllexport)
#endif

#include "testlib.h"

#if CFG_TA_CXXABI_DEMANGLE
#include <cxxabi.h>
#endif

#if CFG_TA_DETECT_DEBUGGER
#if defined(_WIN32)
#include <windows.h> // For `IsDebuggerPresent()`.
#elif defined(__linux__)
#include <fstream> // To read `/proc/self/status` to detect the debugger.
#endif
#endif

void ta_test::HardError(std::string_view message, HardErrorKind kind)
{
    // A threadsafe once flag.
    bool once = false;
    [[maybe_unused]] static const auto once_trigger = [&]
    {
        once = true;
        return nullptr;
    }();

    if (!once)
        std::terminate(); // We've already been there.

    FILE *stream = stderr;

    std::fprintf(stream, "%s: %.*s\n", kind == HardErrorKind::internal ? "Internal error" : "Error", int(message.size()), message.data());

    // Stop.
    // Don't need to check whether the debugger is attached, a crash is fine.
    CFG_TA_BREAKPOINT();
    std::terminate();
}

ta_test::detail::GlobalThreadState &ta_test::detail::ThreadState()
{
    thread_local GlobalThreadState ret;
    return ret;
}

bool ta_test::platform::IsDebuggerAttached()
{
    #if !CFG_TA_DETECT_DEBUGGER
    return false;
    #elif defined(_WIN32)
    return bool(IsDebuggerPresent());
    #elif defined(__linux__)
    std::ifstream file("/proc/self/status");
    if (!file)
        return false;
    for (std::string line; std::getline(file, line);)
    {
        constexpr std::string_view prefix = "TracerPid:";
        if (!line.starts_with(prefix))
            continue;
        for (std::size_t i = prefix.size(); i < line.size(); i++)
        {
            if (text::IsDigit(line[i]) && line[i] != '0')
                return true;
        }
    }
    return false;
    #else
    return false;
    #endif
}

std::string_view ta_test::Terminal::AnsiResetString() const
{
    if (color)
        return "\033[0m";
    else
        return "";
}

ta_test::Terminal::AnsiDeltaStringBuffer ta_test::Terminal::AnsiDeltaString(const TextStyle &&cur, const TextStyle &next) const
{
    AnsiDeltaStringBuffer ret;
    ret[0] = '\0';

    if (!color)
        return ret;

    std::strcpy(ret.data(), "\033[");
    char *ptr = ret.data() + 2;
    if (next.color != cur.color)
    {
        if (next.color >= TextColor::extended && next.color < TextColor::extended_end)
            ptr += std::sprintf(ptr, "38;5;%d;", int(next.color) - int(TextColor::extended));
        else
            ptr += std::sprintf(ptr, "%d;", int(next.color));
    }
    if (next.bg_color != cur.bg_color)
    {
        if (next.bg_color >= TextColor::extended && next.bg_color < TextColor::extended_end)
            ptr += std::sprintf(ptr, "48;5;%d;", int(next.bg_color) - int(TextColor::extended));
        else
            ptr += std::sprintf(ptr, "%d;", int(next.bg_color) + 10);
    }
    if (next.bold != cur.bold)
        ptr += std::sprintf(ptr, "%s;", next.bold ? "1" : "22"); // Bold text is a little weird.
    if (next.italic != cur.italic)
        ptr += std::sprintf(ptr, "%s3;", next.italic ? "" : "2");
    if (next.underline != cur.underline)
        ptr += std::sprintf(ptr, "%s4;", next.underline ? "" : "2");

    if (ptr != ret.data() + 2)
    {
        // `sprintf` automatically null-terminates the buffer.
        ptr[-1] = 'm';
        return ret;
    }

    // Nothing useful in the buffer.
    ret[0] = '\0';
    return ret;
}

ta_test::text::Demangler::Demangler() {}

ta_test::text::Demangler::~Demangler()
{
    #if CFG_TA_CXXABI_DEMANGLE
    // Freeing a nullptr is a no-op.
    std::free(buf_ptr);
    #endif
}

const char *ta_test::text::Demangler::operator()(const char *name)
{
    #if CFG_TA_CXXABI_DEMANGLE
    int status = -4;
    const char *ret = abi::__cxa_demangle(name, buf_ptr, &buf_size, &status);
    if (status != 0) // -1 = out of memory, -2 = invalid string, -3 = invalid usage
        return name;
    return ret;
    #else
    return name;
    #endif
}

void ta_test::text::GetExceptionInfo(const std::exception_ptr &e, const std::function<void(const BasicModule::ExceptionInfo *info)> &func)
{
    detail::GlobalThreadState &thread_state = detail::ThreadState();
    if (!thread_state.current_test)
        HardError("The current thread currently isn't running any test, can't use `ExceptionToMessage()`.");

    for (const auto &m : thread_state.current_test->all_tests->runner->modules)
    {
        std::optional<ta_test::BasicModule::ExceptionInfoWithNested> opt;
        try
        {
            opt = m->OnExplainException(e);
        }
        catch (...)
        {
            // This means the user doesn't have to write `catch (...)` in every handler.
            // They'd likely forget that.
            continue;
        }

        if (opt)
        {
            func(&*opt);
            if (opt->nested_exception)
                GetExceptionInfo(opt->nested_exception, func);
            return;
        }
    }

    func(nullptr);
}

void ta_test::text::TextCanvas::Print(const Terminal &terminal, FILE *stream) const
{
    PrintToCallback(terminal, [&](std::string_view string){fwrite(string.data(), string.size(), 1, stream);});
}

std::size_t ta_test::text::TextCanvas::NumLines() const
{
    return lines.size();
}

void ta_test::text::TextCanvas::EnsureNumLines(std::size_t size)
{
    if (lines.size() < size)
        lines.resize(size);
}

void ta_test::text::TextCanvas::EnsureLineSize(std::size_t line_number, std::size_t size)
{
    if (line_number >= lines.size())
        HardError("Line index is out of range.");

    Line &line = lines[line_number];
    if (line.text.size() < size)
    {
        line.text.resize(size, ' ');
        line.info.resize(size);
    }
}

void ta_test::text::TextCanvas::InsertLineBefore(std::size_t line_number)
{
    if (line_number >/*sic*/ lines.size())
        HardError("Line number is out of range.");

    lines.insert(lines.begin() + std::ptrdiff_t(line_number), Line{});
}

bool ta_test::text::TextCanvas::IsCellFree(std::size_t line, std::size_t column) const
{
    if (line >= lines.size())
        return true;
    const Line &this_line = lines[line];
    if (column >= this_line.info.size())
        return true;
    return !this_line.info[column].important;
}

bool ta_test::text::TextCanvas::IsLineFree(std::size_t line, std::size_t column, std::size_t width, std::size_t gap) const
{
    // Apply `gap` to `column` and `width`.
    column = gap < column ? column - gap : 0;
    width += gap * 2;

    if (line >= lines.size())
        return true; // This space is below the canvas height.

    const Line &this_line = lines[line];
    if (this_line.info.empty())
        return true; // This line is completely empty.

    std::size_t last_column = column + width;
    if (last_column >= this_line.info.size())
        last_column = this_line.info.size() - 1; // `line.info` can't be empty here.

    bool ok = true;
    for (std::size_t i = column; i < last_column; i++)
    {
        if (this_line.info[i].important)
        {
            ok = false;
            break;
        }
    }
    return ok;
}

std::size_t ta_test::text::TextCanvas::FindFreeSpace(std::size_t starting_line, std::size_t column, std::size_t height, std::size_t width, std::size_t gap, std::size_t vertical_step) const
{
    std::size_t num_free_lines = 0;
    std::size_t line = starting_line;
    while (true)
    {
        if (num_free_lines > 0 || (line - starting_line) % vertical_step == 0)
        {
            if (!IsLineFree(line, column, width, gap))
            {
                num_free_lines = 0;
            }
            else
            {
                num_free_lines++;
                if (num_free_lines >= height)
                    return line - height + 1;
            }
        }

        line++; // Try the next line.
    }
}

char32_t &ta_test::text::TextCanvas::CharAt(std::size_t line, std::size_t pos)
{
    if (line >= lines.size())
        HardError("Line index is out of range.");

    Line &this_line = lines[line];
    if (pos >= this_line.text.size())
        HardError("Character index is out of range.");

    return this_line.text[pos];
}

// Accesses the cell info for the specified cell. The cell must exist.
ta_test::text::TextCanvas::CellInfo &ta_test::text::TextCanvas::CellInfoAt(std::size_t line, std::size_t pos)
{
    if (line >= lines.size())
        HardError("Line index is out of range.");

    Line &this_line = lines[line];
    if (pos >= this_line.info.size())
        HardError("Character index is out of range.");

    return this_line.info[pos];
}

std::size_t ta_test::text::TextCanvas::DrawText(std::size_t line, std::size_t start, std::u32string_view text, const CellInfo &info)
{
    EnsureNumLines(line + 1);
    EnsureLineSize(line, start + text.size());
    std::copy(text.begin(), text.end(), lines[line].text.begin() + (std::ptrdiff_t)start);
    for (std::size_t i = start; i < start + text.size(); i++)
        lines[line].info[i] = info;
    return text.size();
}

std::size_t ta_test::text::TextCanvas::DrawText(std::size_t line, std::size_t start, std::string_view text, const CellInfo &info)
{
    std::u32string decoded_text;
    uni::Decode(text, decoded_text);
    return DrawText(line, start, decoded_text, info);
}

std::size_t ta_test::text::TextCanvas::DrawRow(char32_t ch, std::size_t line, std::size_t column, std::size_t width, bool skip_important, const CellInfo &info)
{
    EnsureNumLines(line + 1);
    EnsureLineSize(line, column + width);
    for (std::size_t i = column; i < column + width; i++)
    {
        if (skip_important && !IsCellFree(line, i))
            continue;

        lines[line].text[i] = ch;
        lines[line].info[i] = info;
    }

    return width;
}

void ta_test::text::TextCanvas::DrawColumn(char32_t ch, std::size_t line_start, std::size_t column, std::size_t height, bool skip_important, const CellInfo &info)
{
    if (height == 0)
        return;

    EnsureNumLines(line_start + height);

    for (std::size_t i = line_start; i < line_start + height; i++)
    {
        if (skip_important && !IsCellFree(i, column))
            continue;

        EnsureLineSize(i, column + 1);

        Line &line = lines[i];
        line.text[column] = ch;
        line.info[column] = info;
    }
}

void ta_test::text::TextCanvas::DrawHorBracket(std::size_t line_start, std::size_t column_start, std::size_t height, std::size_t width, const CellInfo &info)
{
    if (width < 2 || height < 1)
        return;

    // Sides.
    if (height > 1)
    {
        DrawColumn(chars->bar, line_start, column_start, height - 1, true, info);
        DrawColumn(chars->bar, line_start, column_start + width - 1, height - 1, true, info);
    }

    // Bottom.
    if (width > 2)
        DrawRow(chars->bracket_bottom, line_start + height - 1, column_start + 1, width - 2, false, info);

    // Corners.
    DrawRow(chars->bracket_corner_bottom_left, line_start + height - 1, column_start, 1, false, info);
    DrawRow(chars->bracket_corner_bottom_right, line_start + height - 1, column_start + width - 1, 1, false, info);
}

void ta_test::text::TextCanvas::DrawOverline(std::size_t line, std::size_t column_start, std::size_t width, const CellInfo &info)
{
    if (width < 2)
        return;

    // Middle part.
    if (width > 2)
        DrawRow(chars->bracket_top, line, column_start + 1, width - 2, false, info);

    // Corners.
    DrawRow(chars->bracket_corner_top_left, line, column_start, 1, false, info);
    DrawRow(chars->bracket_corner_top_right, line, column_start + width - 1, 1, false, info);
}

std::size_t ta_test::text::expr::DrawExprToCanvas(TextCanvas &canvas, const Style &style, std::size_t line, std::size_t start, std::string_view expr)
{
    canvas.DrawText(line, start, expr);
    std::size_t i = 0;
    CharKind prev_kind = CharKind::normal;
    bool is_number = false;
    const char *identifier_start = nullptr;
    bool is_number_suffix = false;
    bool is_string_suffix = false;
    std::size_t raw_string_separator_len = 0;

    CharKind prev_string_kind{}; // One of: `string`, `character`, `raw_string`.

    auto FinalizeIdentifier = [&](std::string_view ident)
    {
        const TextStyle *ident_style = nullptr;

        // Check if this is a keyword.
        auto it = style.highlighted_keywords.find(ident);
        if (it != style.highlighted_keywords.end())
        {
            switch (it->second)
            {
              case KeywordKind::generic:
                ident_style = &style.keyword_generic;
                break;
              case KeywordKind::value:
                ident_style = &style.keyword_value;
                break;
              case KeywordKind::op:
                ident_style = &style.keyword_op;
                break;
            }
        }
        else if (std::all_of(ident.begin(), ident.end(), [](char ch){return IsIdentifierChar(ch) && !IsAlphaLowercase(ch);}))
        {
            ident_style = &style.all_caps;
        }

        // If this identifier needs a custom style...
        if (ident_style)
        {
            for (std::size_t j = 0; j < ident.size(); j++)
                canvas.CellInfoAt(line, start + i - j - 1).style = *ident_style;
        }
    };

    auto lambda = [&](const char &ch, CharKind kind)
    {
        TextCanvas::CellInfo &info = canvas.CellInfoAt(line, start + i);
        bool is_punct = !IsIdentifierChar(ch);

        const char *const prev_identifier_start = identifier_start;

        if (kind != CharKind::normal)
        {
            is_number = false;
            identifier_start = nullptr;
            is_number_suffix = false;
            is_string_suffix = false;
        }

        // When exiting raw string, backtrack and color the closing sequence.
        if (prev_kind == CharKind::raw_string && kind != CharKind::raw_string)
        {
            for (std::size_t j = 0; j < raw_string_separator_len; j++)
                canvas.CellInfoAt(line, start + i - j - 1).style = style.raw_string_delimiters;
        }

        switch (kind)
        {
          case CharKind::normal:
            if (is_string_suffix && !IsIdentifierChar(ch))
                is_string_suffix = false;
            if ((prev_kind == CharKind::string || prev_kind == CharKind::character || prev_kind == CharKind::raw_string) && IsIdentifierChar(ch))
                is_string_suffix = true;

            if (is_number_suffix && !IsIdentifierChar(ch))
                is_number_suffix = false;

            if (!is_number && !identifier_start && !is_string_suffix && !is_number_suffix)
            {
                if (IsDigit(ch))
                {
                    is_number = true;

                    // Backtrack and make the leading `.` a number too, if it's there.
                    if (i > 0 && expr[i-1] == '.')
                        canvas.CellInfoAt(line, start + i - 1).style = style.number;
                }
                else if (IsIdentifierChar(ch))
                {
                    identifier_start = &ch;
                }
            }
            else if (is_number)
            {
                if (!(IsDigit(ch) || IsAlpha(ch) || ch == '.' || ch == '-' || ch == '+' || ch == '\''))
                {
                    is_number = false;
                    if (ch == '_')
                        is_number_suffix = true;
                }
            }
            else if (identifier_start)
            {
                if (!IsIdentifierChar(ch))
                    identifier_start = nullptr;
            }

            if (is_string_suffix)
            {
                switch (prev_string_kind)
                {
                  case CharKind::string:
                    info.style = style.string_suffix;
                    break;
                  case CharKind::character:
                    info.style = style.character_suffix;
                    break;
                  case CharKind::raw_string:
                    info.style = style.raw_string_suffix;
                    break;
                  default:
                    HardError("Lexer error during pretty-printing.");
                    break;
                }
            }
            else if (is_number_suffix)
                info.style = style.number_suffix;
            else if (is_number)
                info.style = style.number;
            else if (is_punct)
                info.style = style.punct;
            else
                info.style = style.normal;
            break;
          case CharKind::string:
          case CharKind::character:
          case CharKind::raw_string:
          case CharKind::raw_string_initial_sep:
            if (prev_kind != kind && prev_kind != CharKind::raw_string_initial_sep)
            {
                if (kind == CharKind::raw_string_initial_sep)
                    prev_string_kind = CharKind::raw_string;
                else
                    prev_string_kind = kind;

                // Backtrack and color the prefix.
                std::size_t j = i;
                while (j-- > 0 && (IsAlpha(expr[j]) || IsDigit(expr[j])))
                {
                    TextStyle &target_style = canvas.CellInfoAt(line, start + j).style;
                    switch (prev_string_kind)
                    {
                      case CharKind::string:
                        target_style = style.string_prefix;
                        break;
                      case CharKind::character:
                        target_style = style.character_prefix;
                        break;
                      case CharKind::raw_string:
                        target_style = style.raw_string_prefix;
                        break;
                      default:
                        HardError("Lexer error during pretty-printing.");
                        break;
                    }
                }
            }

            if (kind == CharKind::raw_string_initial_sep)
            {
                if (prev_kind != CharKind::raw_string_initial_sep)
                    raw_string_separator_len = 1;
                raw_string_separator_len++;
            }

            switch (kind)
            {
              case CharKind::string:
                info.style = style.string;
                break;
              case CharKind::character:
                info.style = style.character;
                break;
              case CharKind::raw_string:
              case CharKind::raw_string_initial_sep:
                if (kind == CharKind::raw_string_initial_sep || prev_kind == CharKind::raw_string_initial_sep)
                    info.style = style.raw_string_delimiters;
                else
                    info.style = style.raw_string;
                break;
              default:
                HardError("Lexer error during pretty-printing.");
                break;
            }
            break;
          case CharKind::string_escape_slash:
            info.style = style.string;
            break;
          case CharKind::character_escape_slash:
            info.style = style.character;
            break;
        }

        // Finalize identifiers.
        if (prev_identifier_start && !identifier_start)
            FinalizeIdentifier({prev_identifier_start, &ch});

        prev_kind = kind;

        i++;
    };
    ParseExpr(expr, lambda, nullptr);
    if (identifier_start)
        FinalizeIdentifier({identifier_start, expr.data() + expr.size()});

    return expr.size();
}

void ta_test::BasicPrintingModule::PrintNote(std::string_view text) const
{
    std::fprintf(output_stream, "%s%s%s%.*s%s\n",
        terminal.AnsiResetString().data(),
        terminal.AnsiDeltaString({}, common_styles.note).data(),
        common_chars.note_prefix.c_str(),
        int(text.size()), text.data(),
        terminal.AnsiResetString().data()
    );
}

ta_test::detail::BasicAssertWrapper::BasicAssertWrapper()
{
    auto &cur = ThreadState().current_assertion;
    enclosing_assertion = cur;
    cur = this;
}

// Checks `value`, reports an error if it's false. In any case, returns `value` unchanged.
bool ta_test::detail::BasicAssertWrapper::operator()(bool value)
{
    if (finished)
        HardError("Invalid usage, `operator()` called more than once on an `AssertWrapper`.");

    GlobalThreadState &thread_state = ThreadState();
    if (!thread_state.current_test)
        HardError("This thread doesn't have a test currently running, yet it tries to use an assertion.");
    if (thread_state.current_assertion != this)
        HardError("The assertion being evaluated is not on the top of the assertion stack.");

    if (!value)
    {
        for (const auto &m : thread_state.current_test->all_tests->runner->modules)
            m->OnAssertionFailed(*this);
        thread_state.current_test->failed = true;
    }

    thread_state.current_assertion = const_cast<BasicModule::BasicAssertionInfo *>(enclosing_assertion);

    finished = true;
    return value;
}

ta_test::detail::BasicAssertWrapper::~BasicAssertWrapper()
{
    // We don't check `finished` here. It can be false when a nested assertion fails.
}

void ta_test::detail::GlobalState::SortTestListInExecutionOrder(std::span<std::size_t> indices) const
{
    std::sort(indices.begin(), indices.end(), [&](std::size_t a, std::size_t b)
    {
        std::string_view name_a = tests[a]->Name();
        std::string_view name_b = tests[b]->Name();

        std::string_view::iterator it_a = name_a.begin();
        std::string_view::iterator it_b = name_b.begin();

        while (true)
        {
            auto new_it_a = std::find(it_a, name_a.end(), '/');
            auto new_it_b = std::find(it_b, name_b.end(), '/');

            if (std::string_view(it_a, new_it_a) == std::string_view(it_b, new_it_b))
            {
                if ((new_it_a == name_a.end()) != (new_it_b == name_b.end()))
                    HardError("This shouldn't happen. One test name can't be a prefix of another?");
                if (new_it_a == name_a.end())
                    return false; // Equal.

                it_a = new_it_a + 1;
                it_b = new_it_b + 1;
                continue;
            }

            return name_prefixes_to_order.at(std::string_view(name_a.begin(), new_it_a)) <
                name_prefixes_to_order.at(std::string_view(name_b.begin(), new_it_b));
        }
    });
}

ta_test::detail::GlobalState &ta_test::detail::State()
{
    static GlobalState ret;
    return ret;
}

void ta_test::detail::RegisterTest(const BasicTest *singleton)
{
    GlobalState &state = State();

    auto name = singleton->Name();
    auto it = state.name_to_test_index.lower_bound(name);

    if (it != state.name_to_test_index.end())
    {
        if (it->first == name)
        {
            // This test is already registered. Make sure it comes from the same source file and line, then stop.
            BasicModule::SourceLoc old_loc = state.tests[it->second]->Location();
            BasicModule::SourceLoc new_loc = singleton->Location();
            if (new_loc != old_loc)
                HardError(CFG_TA_FORMAT("Conflicting definitions for test `{}`. One at `{}:{}`, another at `{}:{}`.", name, old_loc.file, old_loc.line, new_loc.file, new_loc.line), HardErrorKind::user);
            return; // Already registered.
        }
        else
        {
            // Make sure a test name is not also used as a group name.
            // Note, don't need to check `name.size() > it->first.size()` here, because if it was equal,
            // we wouldn't enter `else` at all, and if it was less, `.starts_with()` would return false.
            if (name.starts_with(it->first) && name[it->first.size()] == '/')
                HardError(CFG_TA_FORMAT("A test name (`{}`) can't double as a category name (`{}`). Append `/something` to the first name.", it->first, name), HardErrorKind::user);
        }
    }

    state.name_to_test_index.try_emplace(name, state.tests.size());
    state.tests.push_back(singleton);

    // Fill `state.name_prefixes_to_order` with all prefixes of this test.
    for (const char &ch : name)
    {
        if (ch == '/')
            state.name_prefixes_to_order.try_emplace(std::string_view(name.data(), &ch), state.name_prefixes_to_order.size());
    }
    state.name_prefixes_to_order.try_emplace(name, state.name_prefixes_to_order.size());
}

void ta_test::Runner::SetDefaultModules()
{
    modules.clear();
    modules.push_back(MakeModule<modules::ProgressPrinter>());
    modules.push_back(MakeModule<modules::ResultsPrinter>());
    modules.push_back(MakeModule<modules::AssertionPrinter>());
    modules.push_back(MakeModule<modules::DefaultExceptionAnalyzer>());
    modules.push_back(MakeModule<modules::ExceptionPrinter>());
    modules.push_back(MakeModule<modules::DebuggerDetector>());
    modules.push_back(MakeModule<modules::DebuggerStatePrinter>());
}

int ta_test::Runner::Run()
{
    if (detail::ThreadState().current_test)
        HardError("This thread is already running a test.", HardErrorKind::user);

    const auto &state = detail::State();

    std::vector<std::size_t> ordered_tests;

    { // Get a list of tests to run.
        ordered_tests.reserve(state.tests.size());

        for (std::size_t i = 0; i < state.tests.size(); i++)
        {
            bool enable = true;
            for (const auto &m : modules)
                m->OnFilterTest(*state.tests[i], enable);
            if (enable)
                ordered_tests.push_back(i);
        }
        state.SortTestListInExecutionOrder(ordered_tests);
    }

    BasicModule::RunTestsResults results;
    results.runner = this;
    results.num_tests = ordered_tests.size();
    for (const auto &m : modules)
        m->OnPreRunTests(results);

    for (std::size_t test_index : ordered_tests)
    {
        const detail::BasicTest *test = state.tests[test_index];

        struct StateGuard
        {
            BasicModule::SingleTestResults state;
            StateGuard() {detail::ThreadState().current_test = &state;}
            ~StateGuard() {detail::ThreadState().current_test = nullptr;}
        };
        StateGuard guard;
        guard.state.all_tests = &results;
        guard.state.test = test;

        for (const auto &m : modules)
            m->OnPreRunSingleTest(guard.state);


        try
        {
            test->Run();
        }
        catch (InterruptTestException) {}
        catch (...)
        {
            guard.state.failed = true;
            for (const auto &m : modules)
                m->OnUncaughtException(std::current_exception());
        }

        for (const auto &m : modules)
            m->OnPostRunSingleTest(guard.state);

        if (guard.state.failed)
            results.num_failed_tests++;
    }

    for (const auto &m : modules)
        m->OnPostRunTests(results);

    return results.num_failed_tests != 0;
}

// --- modules::ProgressPrinter ---

void ta_test::modules::ProgressPrinter::EnableUnicode(bool enable)
{
    if (enable)
    {
        chars_test_prefix = "\xE2\x97\x8F "; // BLACK CIRCLE, then a space.
        chars_indentation_guide = "\xC2\xB7   "; // MIDDLE DOT, then a space.
        chars_test_counter_separator = " \xE2\x94\x82  "; // BOX DRAWINGS LIGHT VERTICAL, with some spaces around it.
    }
    else
    {
        chars_test_prefix = "* ";
        chars_indentation_guide = "    ";
        chars_test_counter_separator = " |  ";
    }
}

void ta_test::modules::ProgressPrinter::OnPreRunTests(const RunTestsInfo &data)
{
    (void)data;
    *this = {};
}

void ta_test::modules::ProgressPrinter::OnPreRunSingleTest(const SingleTestInfo &data)
{
    // How much characters in the test counter.
    const int test_counter_width = std::snprintf(nullptr, 0, "%zu", data.all_tests->num_tests);

    std::string_view test_name = data.test->Name();

    auto it = test_name.begin();
    std::size_t segment_index = 0;
    while (true)
    {
        auto new_it = std::find(it, test_name.end(), '/');

        std::string_view segment(it, new_it);

        // Pop the tail off the stack.
        if (segment_index < stack.size() && stack[segment_index] != segment)
            stack.resize(segment_index);

        // Push the segment into the stack, and print a line.
        if (segment_index >= stack.size())
        {
            TextStyle cur_style;

            // Test index (if this is the last segment).
            if (new_it == test_name.end())
            {
                auto style_a = terminal.AnsiDeltaString(cur_style, style_index);
                auto style_b = terminal.AnsiDeltaString(cur_style, style_total_count);

                std::fprintf(output_stream, "%s%*zu%s/%zu",
                    style_a.data(),
                    test_counter_width, test_counter + 1,
                    style_b.data(),
                    data.all_tests->num_tests
                );
            }
            else
            {
                // No test index, just a gap.
                std::fprintf(output_stream, "%*s", test_counter_width * 2 + 1, "");
            }

            // The gutter border.
            std::fprintf(output_stream, "%s%s",
                terminal.AnsiDeltaString(cur_style, style_gutter_border).data(),
                chars_test_counter_separator.c_str()
            );

            // The indentation.
            if (!stack.empty())
            {
                // Switch to the indentation guide color.
                std::fprintf(output_stream, "%s", terminal.AnsiDeltaString(cur_style, style_indentation_guide).data());
                // Print the required number of guides.
                for (std::size_t repeat = 0; repeat < stack.size(); repeat++)
                    std::fprintf(output_stream, "%s", chars_indentation_guide.c_str());
            }

            // Whether we're reentering this group after a failed test.
            bool is_continued = segment_index < failed_test_stack.size() && failed_test_stack[segment_index] == segment;

            // Print the test name.
            std::fprintf(output_stream, "%s%s%.*s%s%s\n",
                terminal.AnsiDeltaString(cur_style, is_continued ? style_continuing_group : new_it == test_name.end() ? style_name : style_group_name).data(),
                is_continued ? chars_test_prefix_continuing_group.c_str() : chars_test_prefix.c_str(),
                int(segment.size()), segment.data(),
                new_it == test_name.end() ? "" : "/",
                terminal.AnsiResetString().data()
            );

            // Push to the stack.
            stack.push_back(segment);
        }

        if (new_it == test_name.end())
            break;

        segment_index++;
        it = new_it + 1;
    }

    test_counter++;
}

void ta_test::modules::ProgressPrinter::OnPostRunSingleTest(const SingleTestResults &data)
{
    // Print the error message.
    if (data.failed)
    {
        TextStyle cur_style;

        auto style_message = terminal.AnsiDeltaString(cur_style, common_styles.error);
        auto style_group = terminal.AnsiDeltaString(cur_style, style_failed_group_name);
        auto style_name = terminal.AnsiDeltaString(cur_style, style_failed_name);

        std::string_view group;
        std::string_view name = data.test->Name();
        auto sep = name.find_last_of('/');
        if (sep != std::string_view::npos)
        {
            group = name.substr(0, sep + 1);
            name = name.substr(sep + 1);
        }

        fprintf(output_stream, "%s%s\n%s%s%s%.*s%s%.*s%s\n",
            terminal.AnsiResetString().data(),
            common_chars.LocationToString(data.test->Location()).c_str(),
            style_message.data(),
            chars_test_failed.c_str(),
            style_group.data(),
            int(group.size()), group.data(),
            style_name.data(),
            int(name.size()), name.data(),
            terminal.AnsiResetString().data()
        );
    }

    // Adjust the group name stack to show the group names again after a failed test.
    if (data.failed)
    {
        failed_test_stack = std::move(stack);
        stack.clear();
    }
    else
    {
        failed_test_stack.clear();
    }
}

// --- modules::ResultsPrinter ---

void ta_test::modules::ResultsPrinter::OnPostRunTests(const RunTestsResults &data)
{
    // Reset color.
    std::fprintf(output_stream, "%s\n", terminal.AnsiResetString().data());

    if (data.num_tests == 0)
    {
        // No tests to run.
        std::fprintf(output_stream, "%sNO TESTS TO RUN%s\n",
            terminal.AnsiDeltaString({}, style_no_tests).data(),
            terminal.AnsiResetString().data()
        );
    }
    else
    {
        std::size_t num_passed = data.num_tests - data.num_failed_tests;
        if (num_passed > 0)
        {
            std::fprintf(output_stream, "%s%s%zu TEST%s PASSED%s\n",
                terminal.AnsiDeltaString({}, data.num_failed_tests == 0 ? style_all_passed : style_num_passed).data(),
                data.num_failed_tests == 0 && data.num_tests > 1 ? "ALL " : "",
                num_passed,
                num_passed == 1 ? "" : "S",
                terminal.AnsiResetString().data()
            );
        }

        if (data.num_failed_tests > 0)
        {
            std::fprintf(output_stream, "%s%s%zu TEST%s FAILED%s\n",
                terminal.AnsiDeltaString({}, style_num_failed).data(),
                num_passed == 0 && data.num_failed_tests > 1 ? "ALL " : "",
                data.num_failed_tests,
                data.num_failed_tests == 1 ? "" : "S",
                terminal.AnsiResetString().data()
            );
        }
    }
}

// --- modules::AssertionPrinter ---

void ta_test::modules::AssertionPrinter::OnAssertionFailed(const BasicAssertionInfo &data)
{
    auto lambda = [this](auto &lambda, const BasicAssertionInfo &data, int depth) -> void
    {
        text::TextCanvas canvas(&common_chars);
        std::size_t line_counter = 0;

        // The file path.
        canvas.DrawText(line_counter++, 0, common_chars.LocationToString(data.SourceLocation()), {.style = style_filename, .important = true});

        // The main error message.
        if (depth == 0)
            canvas.DrawText(line_counter++, 0, chars_assertion_failed, {.style = common_styles.error, .important = true});
        else
            canvas.DrawText(line_counter++, 0, chars_in_assertion, {.style = style_in_assertion, .important = true});

        line_counter++;

        std::size_t expr_line = line_counter;

        { // The assertion call.
            std::size_t column = printed_code_indentation;

            const text::TextCanvas::CellInfo assertion_macro_cell_info = {.style = style_assertion_macro, .important = true};
            column += canvas.DrawText(line_counter, column, chars_assertion_macro_prefix, assertion_macro_cell_info);
            column += text::expr::DrawExprToCanvas(canvas, style_expr, line_counter, column, data.Expr());
            column += canvas.DrawText(line_counter, column, chars_assertion_macro_suffix, assertion_macro_cell_info);
            line_counter++;
        }

        // The expression.
        if (decompose_expression)
        {
            std::size_t expr_column = printed_code_indentation + chars_assertion_macro_prefix.size();

            std::u32string this_value;

            // The bracket above the expression.
            std::size_t overline_start = 0;
            std::size_t overline_end = 0;
            // How many subexpressions want an overline.
            // More than one should be impossible, but if it happens, we just combine them into a single fat one.
            int num_overline_parts = 0;

            // Incremented when we print an argument.
            std::size_t color_index = 0;

            for (std::size_t i = 0; i < data.NumArgs(); i++)
            {
                const std::size_t arg_index = data.ArgsInDrawOrder()[i];
                const BasicAssertionInfo::StoredArg &this_arg = data.StoredArgs()[arg_index];
                const BasicAssertionInfo::ArgInfo &this_info = data.ArgsInfo()[arg_index];

                bool dim_parentheses = true;

                if (this_arg.state == BasicAssertionInfo::StoredArg::State::in_progress)
                {
                    if (num_overline_parts == 0)
                    {
                        overline_start = this_info.expr_offset;
                        overline_end = this_info.expr_offset + this_info.expr_size;
                    }
                    else
                    {
                        overline_start = std::min(overline_start, this_info.expr_offset);
                        overline_end = std::max(overline_end, this_info.expr_offset + this_info.expr_size);
                    }
                    num_overline_parts++;
                }

                if (this_arg.state == BasicAssertionInfo::StoredArg::State::done)
                {
                    text::uni::Decode(this_arg.value, this_value);

                    std::size_t center_x = expr_column + this_info.expr_offset + (this_info.expr_size + 1) / 2 - 1;
                    std::size_t value_x = center_x - (this_value.size() + 1) / 2 + 1;
                    // Make sure `value_x` didn't underflow.
                    if (value_x > std::size_t(-1) / 2)
                        value_x = 0;

                    const text::TextCanvas::CellInfo this_cell_info = {.style = style_arguments[color_index++ % style_arguments.size()], .important = true};

                    if (!this_info.need_bracket)
                    {
                        std::size_t value_y = canvas.FindFreeSpace(line_counter, value_x, 2, this_value.size(), 1, 2) + 1;
                        canvas.DrawText(value_y, value_x, this_value, this_cell_info);
                        canvas.DrawColumn(common_chars.bar, line_counter, center_x, value_y - line_counter, true, this_cell_info);

                        // Color the contents.
                        for (std::size_t i = 0; i < this_info.expr_size; i++)
                        {
                            TextStyle &style = canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset + i).style;
                            style.color = this_cell_info.style.color;
                            style.bold = true;
                        }
                    }
                    else
                    {
                        std::size_t bracket_left_x = expr_column + this_info.expr_offset;
                        std::size_t bracket_right_x = bracket_left_x + this_info.expr_size + 1;
                        if (bracket_left_x > 0)
                            bracket_left_x--;

                        std::size_t bracket_y = canvas.FindFreeSpace(line_counter, bracket_left_x, 2, bracket_right_x - bracket_left_x, 0, 2);
                        std::size_t value_y = canvas.FindFreeSpace(bracket_y + 1, value_x, 1, this_value.size(), 1, 2);

                        canvas.DrawHorBracket(line_counter, bracket_left_x, bracket_y - line_counter + 1, bracket_right_x - bracket_left_x, this_cell_info);
                        canvas.DrawText(value_y, value_x, this_value, this_cell_info);

                        // Add the tail to the bracket.
                        if (center_x > bracket_left_x && center_x + 1 < bracket_right_x)
                            canvas.CharAt(bracket_y, center_x) = common_chars.bracket_bottom_tail;

                        // Draw the column connecting us to the text, if it's not directly below.
                        canvas.DrawColumn(common_chars.bar, bracket_y + 1, center_x, value_y - bracket_y - 1, true, this_cell_info);

                        // Color the parentheses with the argument color.
                        dim_parentheses = false;
                        canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset - 1).style.color = this_cell_info.style.color;
                        canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset + this_info.expr_size).style.color = this_cell_info.style.color;
                    }
                }

                // Dim the macro name.
                for (std::size_t i = 0; i < this_info.ident_size; i++)
                    canvas.CellInfoAt(line_counter - 1, expr_column + this_info.ident_offset + i).style.color = color_dim;

                // Dim the parentheses.
                if (dim_parentheses)
                {
                    canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset - 1).style.color = color_dim;
                    canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset + this_info.expr_size).style.color = color_dim;
                }
            }

            // The overline.
            if (num_overline_parts > 0)
            {
                if (overline_start > 0)
                    overline_start--;
                overline_end++;

                std::u32string_view this_value = num_overline_parts > 1 ? chars_in_this_subexpr_inexact : chars_in_this_subexpr;

                std::size_t center_x = expr_column + overline_start + (overline_end - overline_start) / 2;
                std::size_t value_x = center_x - this_value.size() / 2;

                canvas.InsertLineBefore(expr_line++);

                canvas.DrawOverline(expr_line - 1, expr_column + overline_start, overline_end - overline_start, {.style = style_overline, .important = true});
                canvas.DrawText(expr_line - 2, value_x, this_value, {.style = style_overline, .important = true});

                // Color the parentheses.
                canvas.CellInfoAt(expr_line, expr_column + overline_start).style.color = style_overline.color;
                canvas.CellInfoAt(expr_line, expr_column + overline_end - 1).style.color = style_overline.color;
            }
        }

        canvas.InsertLineBefore(canvas.NumLines());

        canvas.Print(terminal, output_stream);

        if (print_assertion_stack && data.enclosing_assertion)
            lambda(lambda, *data.enclosing_assertion, depth + 1);
    };

    lambda(lambda, data, 0);
}

// --- modules::ExceptionPrinter ---

void ta_test::modules::ExceptionPrinter::OnUncaughtException(const std::exception_ptr &e)
{
    TextStyle cur_style;

    std::fprintf(output_stream, "%s%s%s\n",
        terminal.AnsiResetString().data(),
        terminal.AnsiDeltaString(cur_style, common_styles.error).data(),
        chars_error.c_str()
    );

    text::GetExceptionInfo(e, [&](const ExceptionInfo* info)
    {
        if (info)
        {
            auto style_a = terminal.AnsiDeltaString(cur_style, style_exception_type);
            auto style_b = terminal.AnsiDeltaString(cur_style, style_exception_message);
            std::fprintf(output_stream, "%s%s%s%s\n%s%s%s\n",
                style_a.data(),
                chars_indent_type.c_str(),
                info->type_name.c_str(),
                chars_type_suffix.c_str(),
                style_b.data(),
                chars_indent_message.c_str(),
                info->message.c_str()
            );
        }
        else
        {
            std::fprintf(output_stream, "%s%s%s\n",
                terminal.AnsiDeltaString(cur_style, style_exception_type).data(),
                chars_indent_type.c_str(),
                chars_unknown_exception.c_str()
            );
        }
    });

    std::fprintf(output_stream, "%s\n", terminal.AnsiResetString().data());
}

// --- modules::DebuggerDetector ---

bool ta_test::modules::DebuggerDetector::IsDebuggerAttached() const
{
    return platform::IsDebuggerAttached();
}

void ta_test::modules::DebuggerDetector::OnAssertionFailed(const BasicAssertionInfo &data)
{
    if (IsDebuggerAttached())
        data.should_break = true;
}

// --- modules::DebuggerStatePrinter ---

void ta_test::modules::DebuggerStatePrinter::OnPreRunTests(const RunTestsInfo &data)
{
    data.runner->FindModule<DebuggerDetector>([this](DebuggerDetector &detector)
    {
        if (detector.break_on_failure && *detector.break_on_failure)
            PrintNote("Will break on failure.");
        else if (!detector.break_on_failure && detector.IsDebuggerAttached())
            PrintNote("A debugger is attached, will break on failure.");
    });
}
