#if CFG_TA_SHARED
#define CFG_TA_API __declspec(dllexport)
#endif

#include "testlib.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX 1
#endif

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

#if CFG_TA_DETECT_TERMINAL
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#define DETAIL_TA_USE_ISATTY 1
#include <unistd.h>
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

    std::fprintf(stderr, "%sta_test: %s: %.*s\n",
        output::Terminal(stderr).AnsiResetString().data(),
        kind == HardErrorKind::internal ? "Internal error" : "Error",
        int(message.size()), message.data()
    );

    // Stop.
    // Don't need to check whether the debugger is attached, a crash is fine.
    CFG_TA_BREAKPOINT();
    std::terminate();
}

bool ta_test::IsFailing()
{
    auto &thread_state = detail::ThreadState();
    return thread_state.current_test && thread_state.current_test->failed;
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
    buf_ptr = abi::__cxa_demangle(name, buf_ptr, &buf_size, &status);
    if (status != 0) // -1 = out of memory, -2 = invalid string, -3 = invalid usage
        return name;
    return buf_ptr;
    #else
    return name;
    #endif
}

bool ta_test::text::TestNameMatchesRegex(std::string_view name, const std::regex &regex)
{
    // Try matching the whole name.
    if (std::regex_match(name.begin(), name.end(), regex))
        return true;

    // Try prefixes.
    while (!name.empty())
    {
        name.remove_suffix(1);

        if (name.ends_with('/'))
        {
            // Try matching with the slash.
            if (std::regex_match(name.begin(), name.end(), regex))
                return true;

            // Try again without slash.
            name.remove_suffix(1);
            if (std::regex_match(name.begin(), name.end(), regex))
                return true;
        }
    }

    return false;
}

std::string ta_test::string_conv::DefaultToStringTraits<std::nullptr_t>::operator()(std::nullptr_t) const
{
    return "nullptr";
}

std::string ta_test::string_conv::DefaultToStringTraits<ta_test::AssertFlags>::operator()(AssertFlags value) const
{
    std::string ret;

    using type = std::underlying_type_t<AssertFlags>;
    type mask = type(1);
    do
    {
        if (AssertFlags bit = value & AssertFlags(mask); bool(bit) || value == AssertFlags{})
        {
            if (!ret.empty())
                ret += " | ";

            bool ok = false;
            switch (bit)
            {
              case AssertFlags::hard:
                ok = true;
                ret += "hard";
                break;
              case AssertFlags::soft:
                ok = true;
                ret += "soft";
                break;
              case AssertFlags::no_increment_check_counters:
                ok = true;
                ret += "no_increment_check_counters";
                break;
            }

            if (!ok)
                HardError("Unknown flag in the enum.");
        }
        mask <<= 1;
    }
    while (value != AssertFlags{});

    return ret;
}

std::string ta_test::string_conv::DefaultFallbackToStringTraits<char>::operator()(char value) const
{
    char ret[12]; // Should be at most 9: `'\x{??}'\0`, but throwing in a little extra space.
    *text::escape::EscapeString({&value, 1}, ret, false) = '\0';
    return ret;
}

std::string ta_test::string_conv::DefaultFallbackToStringTraits<std::string_view>::operator()(std::string_view value) const
{
    std::string ret;
    ret.reserve(value.size() + 2); // +2 for quotes. This assumes the happy scenario without any escapes.
    text::escape::EscapeString(value, std::back_inserter(ret), true);
    return ret;
}

std::string ta_test::string_conv::DefaultToStringTraits<std::type_index>::operator()(std::type_index value) const
{
    return text::Demangler{}(value.name());
}

std::string ta_test::string_conv::DefaultFromStringTraits<char>::operator()(char &target, const char *&string) const
{
    return text::escape::UnescapeString(string, '\'', &target, true);
}

std::string ta_test::string_conv::DefaultFromStringTraits<std::nullptr_t>::operator()(std::nullptr_t &target, const char *&string) const
{
    target = nullptr; // Just in case?

    if (string[0] == '0')
    {
        if (string[1] == 'x' && string[2] == '0')
        {
            string += 3;
            return ""; // `0x0`
        }

        string += 1;
        return ""; // `0`
    }

    if (string[0] == 'n' && string[1] == 'u' && string[2] == 'l' && string[3] == 'l' && string[4] == 'p' && string[5] == 't' && string[6] == 'r')
    {
        string += 7;
        return ""; // `nullptr`
    }

    return "Expected one of: `nullptr`, `0x0`, `0`.";
}

ta_test::context::Context ta_test::context::CurrentContext()
{
    auto &thread_state = detail::ThreadState();
    if (!thread_state.current_test)
        HardError("No test is currently running, can't access the current context.", HardErrorKind::user);
    return thread_state.context_stack;
}

ta_test::context::FrameGuard::FrameGuard(std::shared_ptr<const BasicFrame> frame) noexcept
{
    if (!frame)
        return;

    auto &thread_state = detail::ThreadState();
    if (thread_state.context_stack_set.insert(frame.get()).second)
    {
        frame_ptr = frame.get();
        thread_state.context_stack.push_back(std::move(frame));
    }

    if (thread_state.context_stack_set.size() > thread_state.context_stack.size())
        HardError("The context stack is corrupted: The set is larger than the stack.");
}

void ta_test::context::FrameGuard::Reset()
{
    if (!frame_ptr)
        return;

    auto &thread_state = detail::ThreadState();

    if (thread_state.context_stack.empty() || thread_state.context_stack.back().get() != frame_ptr)
        HardError("The context stack is corrupted: The element we're removing is not at the end of the stack.");
    thread_state.context_stack.pop_back();

    if (thread_state.context_stack_set.erase(frame_ptr) != 1)
        HardError("The context stack is corrupted: The element we're removing is in the stack, but not in the set.");

    if (thread_state.context_stack_set.size() > thread_state.context_stack.size())
        HardError("The context stack is corrupted: The set is larger than the stack.");

    frame_ptr = nullptr;
}

ta_test::context::FrameGuard::~FrameGuard()
{
    Reset();
}

std::span<const ta_test::context::LogEntry *const> ta_test::context::CurrentScopedLog()
{
    auto &thread_state = detail::ThreadState();
    if (!thread_state.current_test)
        HardError("No test is currently running, can't access the current scoped log.", HardErrorKind::user);
    return thread_state.scoped_log;
}

std::string ta_test::SingleException::GetTypeName() const
{
    if (IsTypeKnown())
        return text::Demangler{}(type.name());
    else
        return "";
}

void ta_test::AnalyzeException(const std::exception_ptr &e, const std::function<void(SingleException elem)> &func)
{
    auto &thread_state = detail::ThreadState();
    if (!thread_state.current_test)
        HardError("The current thread currently isn't running any test, can't use `AnalyzeException()`.", HardErrorKind::user);

    for (auto *m : thread_state.current_test->all_tests->modules->GetModulesImplementing<&BasicModule::OnExplainException>())
    {
        std::optional<BasicModule::ExplainedException> opt;
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
            if (opt->type == typeid(void))
                HardError("`OnExplainException()` must not return `.type == typeid(void)`, that's reserved for unknown exceptions.", HardErrorKind::user);
            func({.exception = e, .type = opt->type, .message = std::move(opt->message)});
            if (opt->nested_exception)
                AnalyzeException(opt->nested_exception, func);
            return;
        }
    }

    // Unknown exception type.
    func({});
}

std::string ta_test::BasicModule::BasicGenerator::GetTypeName() const
{
    // This can return one of the two possible forms.
    // For sufficiently simple types, we just return `[const] [volatile] Type [&[&]]`.
    // But if the type looks complex, we instead replace the type with a placeholder ("T"),
    // and follow up by `; T = ...` adding the full type name.

    TypeFlags flags = GetTypeFlags();

    std::string ret;

    if (bool(flags & TypeFlags::const_))
    {
        if (!ret.empty()) ret += ' ';
        ret += "const";
    }
    if (bool(flags & TypeFlags::volatile_))
    {
        if (!ret.empty()) ret += ' ';
        ret += "volatile";
    }

    text::Demangler demangler;
    std::string_view type_name = demangler(GetType().name());
    bool use_short_form =
        // Either this is one long template...
        type_name.ends_with('>') ||
        // Or one long type name, possibly qualified.
        std::all_of(type_name.begin(), type_name.end(), [](char ch)
        {
            return text::chars::IsIdentifierChar(ch) || ch == ':';
        });
    std::string_view long_form_placeholder = "T";

    if (!ret.empty()) ret += ' ';
    ret += use_short_form ? type_name : long_form_placeholder;

    if (bool(flags & TypeFlags::any_ref))
    {
        if (!ret.empty()) ret += ' ';
        ret += bool(flags & TypeFlags::lvalue_ref) ? "&" : "&&";
    }

    if (!use_short_form)
    {
        ret += "; ";
        ret += long_form_placeholder;
        ret += " = ";
        ret += type_name;
    }

    return ret;
}

ta_test::BasicModule::BasicGenerator::OverrideStatus ta_test::BasicModule::BasicGenerator::RunGeneratorOverride()
{
    if (overriding_module)
    {
        auto &thread_state = detail::ThreadState();
        if (!thread_state.current_test)
            HardError("Can't operate a generator when no test is running.");
        return overriding_module->OnOverrideGenerator(*thread_state.current_test, *this) ? OverrideStatus::no_more_values : OverrideStatus::success;
    }
    else
    {
        return OverrideStatus::no_override;
    }
}

ta_test::BasicModule::CaughtExceptionElemGuard::CaughtExceptionElemGuard(std::shared_ptr<const CaughtExceptionInfo> state, int active_elem, AssertFlags flags)
    : FrameGuard([&]() -> std::shared_ptr<const CaughtExceptionElemGuard>
    {
        if (!TA_CHECK(flags, $[active_elem] != -1 || ($[bool(state)] && std::size_t(active_elem) < state->elems.size())))
            return nullptr;
        if (!state)
            return nullptr;
        return std::shared_ptr<const CaughtExceptionElemGuard>(std::shared_ptr<void>{}, this);
    }()),
    state(std::move(state)), active_elem(active_elem)
{}

// Gracefully fails the current test, if not already failed.
// Call this first, before printing any messages.
void ta_test::detail::GlobalThreadState::FailCurrentTest()
{
    if (!current_test)
        HardError("Trying to fail the current test, but no test is currently running.");

    if (current_test->failed)
        return; // Already failed.

    current_test->failed = true;
    current_test->all_tests->modules->Call<&BasicModule::OnPreFailTest>(*current_test);
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
            if (text::chars::IsDigit(line[i]) && line[i] != '0')
                return true;
        }
    }
    return false;
    #else
    return false;
    #endif
}

bool ta_test::platform::IsTerminalAttached(bool is_stderr)
{
    #if CFG_TA_DETECT_TERMINAL
    // We cache the return value.
    auto lambda = []<bool IsStderr>
    {
        static bool ret = []{
            #if defined(_WIN32)
            return GetFileType(GetStdHandle(IsStderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE)) == FILE_TYPE_CHAR;
            #elif defined(DETAIL_TA_USE_ISATTY)
            return isatty(IsStderr ? STDERR_FILENO : STDOUT_FILENO) == 1;
            #else
            return false;
            #endif
        }();
        return ret;
    };
    if (is_stderr)
        return lambda.operator()<true>();
    else
        return lambda.operator()<false>();
    #else
    return false;
    #endif
}

ta_test::output::Terminal::Terminal(FILE *stream)
{
    bool is_terminal =
        stream == stdout ? platform::IsTerminalAttached(false) :
        stream == stderr ? platform::IsTerminalAttached(true) : false;

    output_func = [stream
    #if CFG_TA_FMT_HAS_FILE_VPRINT == 0 && defined(_WIN32)
    , need_init = is_terminal
    #endif
    ](std::string_view fmt, CFG_TA_FMT_NAMESPACE::format_args args) mutable
    {
        #if CFG_TA_FMT_HAS_FILE_VPRINT == 2
        CFG_TA_FMT_NAMESPACE::vprint_unicode(stream, fmt, args);
        #elif CFG_TA_FMT_HAS_FILE_VPRINT == 1
        CFG_TA_FMT_NAMESPACE::vprint(stream, fmt, args);
        #elif CFG_TA_FMT_HAS_FILE_VPRINT == 0

        #ifdef _WIN32
        if (need_init)
        {
            need_init = false;

            SetConsoleOutputCP(CP_UTF8);

            auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD current_mode{};
            GetConsoleMode(handle, &current_mode);
            SetConsoleMode(handle, current_mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
        #endif

        std::string buffer = CFG_TA_FMT_NAMESPACE::vformat(fmt, args);
        std::fwrite(buffer.c_str(), buffer.size(), 1, stream);
        #else
        #error Invalid value of `CFG_TA_FMT_HAS_FILE_VPRINT`.
        #endif
    };

    enable_color = is_terminal;
}

void ta_test::output::Terminal::PrintLow(std::string_view fmt, CFG_TA_FMT_NAMESPACE::format_args args) const
{
    if (output_func)
        output_func(fmt, args);
}

ta_test::output::Terminal::StyleGuard::StyleGuard(Terminal &terminal)
    : terminal(terminal)
{
    if (terminal.enable_color)
    {
        ResetStyle();
        exception_counter = std::uncaught_exceptions(); // Don't need this without color.
    }
}

ta_test::output::Terminal::StyleGuard::~StyleGuard()
{
    if (terminal.enable_color && exception_counter == std::uncaught_exceptions())
        ResetStyle();
}

void ta_test::output::Terminal::StyleGuard::ResetStyle()
{
    if (terminal.enable_color)
        terminal.Print("{}", terminal.AnsiResetString().data());
    cur_style = {};
}

std::string_view ta_test::output::Terminal::AnsiResetString() const
{
    if (enable_color)
        return "\033[0m";
    else
        return "";
}

ta_test::output::Terminal::AnsiDeltaStringBuffer ta_test::output::Terminal::AnsiDeltaString(const StyleGuard &&cur, const TextStyle &next) const
{
    AnsiDeltaStringBuffer ret;
    ret[0] = '\0';

    if (!enable_color)
        return ret;

    std::strcpy(ret.data(), "\033[");
    char *ptr = ret.data() + 2;
    if (next.color != cur.cur_style.color)
    {
        if (next.color >= TextColor::extended && next.color < TextColor::extended_end)
            ptr += std::sprintf(ptr, "38;5;%d;", int(next.color) - int(TextColor::extended));
        else
            ptr += std::sprintf(ptr, "%d;", int(next.color));
    }
    if (next.bg_color != cur.cur_style.bg_color)
    {
        if (next.bg_color >= TextColor::extended && next.bg_color < TextColor::extended_end)
            ptr += std::sprintf(ptr, "48;5;%d;", int(next.bg_color) - int(TextColor::extended));
        else
            ptr += std::sprintf(ptr, "%d;", int(next.bg_color) + 10);
    }
    if (next.bold != cur.cur_style.bold)
        ptr += std::sprintf(ptr, "%s;", next.bold ? "1" : "22"); // Bold text is a little weird.
    if (next.italic != cur.cur_style.italic)
        ptr += std::sprintf(ptr, "%s3;", next.italic ? "" : "2");
    if (next.underline != cur.cur_style.underline)
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

void ta_test::output::TextCanvas::Print(const Terminal &terminal, Terminal::StyleGuard &cur_style) const
{
    std::string buffer;

    for (const Line &line : lines)
    {
        std::size_t segment_start = 0;

        auto FlushSegment = [&](std::size_t end_pos)
        {
            if (segment_start == end_pos)
                return;

            text::uni::Encode(std::u32string_view(line.text.begin() + std::ptrdiff_t(segment_start), line.text.begin() + std::ptrdiff_t(end_pos)), buffer);
            terminal.Print("{}", std::string_view(buffer));
            segment_start = end_pos;
        };

        if (terminal.enable_color)
        {
            for (std::size_t i = 0; i < line.text.size(); i++)
            {
                if (line.text[i] == ' ')
                    continue;

                FlushSegment(i);
                terminal.Print("{}", terminal.AnsiDeltaString(cur_style, line.info[i].style).data());
            }
        }

        FlushSegment(line.text.size());

        terminal.Print("\n");
    }
}

std::size_t ta_test::output::TextCanvas::NumLines() const
{
    return lines.size();
}

void ta_test::output::TextCanvas::EnsureNumLines(std::size_t size)
{
    if (lines.size() < size)
        lines.resize(size);
}

void ta_test::output::TextCanvas::EnsureLineSize(std::size_t line_number, std::size_t size)
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

void ta_test::output::TextCanvas::InsertLineBefore(std::size_t line_number)
{
    if (line_number >/*sic*/ lines.size())
        HardError("Line number is out of range.");

    lines.insert(lines.begin() + std::ptrdiff_t(line_number), Line{});
}

bool ta_test::output::TextCanvas::IsCellFree(std::size_t line, std::size_t column) const
{
    if (line >= lines.size())
        return true;
    const Line &this_line = lines[line];
    if (column >= this_line.info.size())
        return true;
    return !this_line.info[column].important;
}

bool ta_test::output::TextCanvas::IsLineFree(std::size_t line, std::size_t column, std::size_t width, std::size_t gap) const
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
    if (last_column >/*sic*/ this_line.info.size())
        last_column = this_line.info.size();

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

std::size_t ta_test::output::TextCanvas::FindFreeSpace(std::size_t starting_line, std::size_t column, std::size_t height, std::size_t width, std::size_t gap, std::size_t vertical_step) const
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

char32_t &ta_test::output::TextCanvas::CharAt(std::size_t line, std::size_t pos)
{
    if (line >= lines.size())
        HardError("Line index is out of range.");

    Line &this_line = lines[line];
    if (pos >= this_line.text.size())
        HardError("Character index is out of range.");

    return this_line.text[pos];
}

// Accesses the cell info for the specified cell. The cell must exist.
ta_test::output::TextCanvas::CellInfo &ta_test::output::TextCanvas::CellInfoAt(std::size_t line, std::size_t pos)
{
    if (line >= lines.size())
        HardError("Line index is out of range.");

    Line &this_line = lines[line];
    if (pos >= this_line.info.size())
        HardError("Character index is out of range.");

    return this_line.info[pos];
}

std::size_t ta_test::output::TextCanvas::DrawString(std::size_t line, std::size_t start, std::u32string_view text, const CellInfo &info)
{
    EnsureNumLines(line + 1);
    EnsureLineSize(line, start + text.size());

    auto out = lines[line].text.begin() + (std::ptrdiff_t)start;
    for (char32_t ch : text)
    {
        // Replace control characters with their Unicode printable representations.
        if (ch >= '\0' && ch < ' ')
            ch += 0x2400;

        *out++ = ch;
    }

    for (std::size_t i = start; i < start + text.size(); i++)
        lines[line].info[i] = info;
    return text.size();
}

std::size_t ta_test::output::TextCanvas::DrawString(std::size_t line, std::size_t start, std::string_view text, const CellInfo &info)
{
    std::u32string decoded_text;
    text::uni::Decode(text, decoded_text);
    return DrawString(line, start, decoded_text, info);
}

std::size_t ta_test::output::TextCanvas::DrawRow(char32_t ch, std::size_t line, std::size_t column, std::size_t width, bool skip_important, const CellInfo &info)
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

void ta_test::output::TextCanvas::DrawColumn(char32_t ch, std::size_t line_start, std::size_t column, std::size_t height, bool skip_important, const CellInfo &info)
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

void ta_test::output::TextCanvas::DrawHorBracket(std::size_t line_start, std::size_t column_start, std::size_t height, std::size_t width, const CellInfo &info)
{
    if (width < 2 || height < 1)
        return;

    // Sides.
    if (height > 1)
    {
        DrawColumn(data->bar, line_start, column_start, height - 1, true, info);
        DrawColumn(data->bar, line_start, column_start + width - 1, height - 1, true, info);
    }

    // Bottom.
    if (width > 2)
        DrawRow(data->bracket_bottom, line_start + height - 1, column_start + 1, width - 2, false, info);

    // Corners.
    DrawRow(data->bracket_corner_bottom_left, line_start + height - 1, column_start, 1, false, info);
    DrawRow(data->bracket_corner_bottom_right, line_start + height - 1, column_start + width - 1, 1, false, info);
}

void ta_test::output::TextCanvas::DrawOverline(std::size_t line, std::size_t column_start, std::size_t width, const CellInfo &info)
{
    if (width < 2)
        return;

    // Middle part.
    if (width > 2)
        DrawRow(data->bracket_top, line, column_start + 1, width - 2, false, info);

    // Corners.
    DrawRow(data->bracket_corner_top_left, line, column_start, 1, false, info);
    DrawRow(data->bracket_corner_top_right, line, column_start + width - 1, 1, false, info);
}

std::size_t ta_test::output::expr::DrawToCanvas(output::TextCanvas &canvas, std::size_t line, std::size_t start, std::string_view expr, const Style *style)
{
    using CharKind = text::expr::CharKind;

    if (!style)
        style = &canvas.GetCommonData()->style_expr;

    canvas.DrawString(line, start, expr);

    std::size_t i = 0;
    char prev_ch = '\0';
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
        auto it = style->highlighted_keywords.find(ident);
        if (it != style->highlighted_keywords.end())
        {
            switch (it->second)
            {
              case KeywordKind::generic:
                ident_style = &style->keyword_generic;
                break;
              case KeywordKind::value:
                ident_style = &style->keyword_value;
                break;
              case KeywordKind::op:
                ident_style = &style->keyword_op;
                break;
            }
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
        if (!text::uni::IsFirstByte(ch))
            return;

        TextCanvas::CellInfo &info = canvas.CellInfoAt(line, start + i);
        bool is_punct = text::chars::IsPunct(ch);

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
                canvas.CellInfoAt(line, start + i - j - 1).style = style->raw_string_delimiters;
        }

        switch (kind)
        {
          case CharKind::normal:
            if (is_string_suffix && !text::chars::IsIdentifierChar(ch))
                is_string_suffix = false;
            if ((prev_kind == CharKind::string || prev_kind == CharKind::character || prev_kind == CharKind::raw_string) && text::chars::IsIdentifierChar(ch))
                is_string_suffix = true;

            if (is_number_suffix && !text::chars::IsIdentifierChar(ch))
                is_number_suffix = false;

            if (!is_number && !identifier_start && !is_string_suffix && !is_number_suffix)
            {
                if (text::chars::IsDigit(ch))
                {
                    is_number = true;

                    // Backtrack and make the leading `.` a number too, if it's there.
                    if (i > 0 && expr[i-1] == '.')
                        canvas.CellInfoAt(line, start + i - 1).style = style->number;
                }
                else if (text::chars::IsIdentifierChar(ch))
                {
                    identifier_start = &ch;
                }
            }
            else if (is_number)
            {
                if (!(text::chars::IsDigit(ch) || text::chars::IsAlpha(ch) || ch == '.' || ch == '\'' ||
                    ((prev_ch == 'e' || prev_ch == 'E' || prev_ch == 'p' || prev_ch == 'P') && ( ch == '-' || ch == '+'))
                ))
                {
                    is_number = false;
                    if (ch == '_')
                        is_number_suffix = true;
                }
            }
            else if (identifier_start)
            {
                if (!text::chars::IsIdentifierChar(ch))
                    identifier_start = nullptr;
            }

            if (is_string_suffix)
            {
                switch (prev_string_kind)
                {
                  case CharKind::string:
                    info.style = style->string_suffix;
                    break;
                  case CharKind::character:
                    info.style = style->character_suffix;
                    break;
                  case CharKind::raw_string:
                    info.style = style->raw_string_suffix;
                    break;
                  default:
                    HardError("Lexer error during pretty-printing.");
                    break;
                }
            }
            else if (is_number_suffix)
                info.style = style->number_suffix;
            else if (is_number)
                info.style = style->number;
            else if (is_punct)
                info.style = style->punct;
            else
                info.style = style->normal;
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
                while (j-- > 0 && (text::chars::IsAlpha(expr[j]) || text::chars::IsDigit(expr[j])))
                {
                    TextStyle &target_style = canvas.CellInfoAt(line, start + j).style;
                    switch (prev_string_kind)
                    {
                      case CharKind::string:
                        target_style = style->string_prefix;
                        break;
                      case CharKind::character:
                        target_style = style->character_prefix;
                        break;
                      case CharKind::raw_string:
                        target_style = style->raw_string_prefix;
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
                info.style = style->string;
                break;
              case CharKind::character:
                info.style = style->character;
                break;
              case CharKind::raw_string:
              case CharKind::raw_string_initial_sep:
                if (kind == CharKind::raw_string_initial_sep || prev_kind == CharKind::raw_string_initial_sep)
                    info.style = style->raw_string_delimiters;
                else
                    info.style = style->raw_string;
                break;
              default:
                HardError("Lexer error during pretty-printing.");
                break;
            }
            break;
          case CharKind::string_escape_slash:
            info.style = style->string;
            break;
          case CharKind::character_escape_slash:
            info.style = style->character;
            break;
        }

        // Finalize identifiers.
        if (prev_identifier_start && !identifier_start)
            FinalizeIdentifier({prev_identifier_start, &ch});

        prev_ch = ch;
        prev_kind = kind;

        i++;
    };
    text::expr::ParseExpr(expr, lambda, false, nullptr);
    if (identifier_start)
        FinalizeIdentifier({identifier_start, expr.data() + expr.size()});

    return expr.size();
}

void ta_test::output::PrintContext(Terminal::StyleGuard &cur_style, const context::BasicFrame *skip_last_frame, context::Context con)
{
    bool first = true;
    for (auto it = con.end(); it != con.begin();)
    {
        --it;
        if (first && skip_last_frame && skip_last_frame == it->get())
            continue;

        first = false;
        PrintContextFrame(cur_style, **it);
    }
}

void ta_test::output::PrintContextFrame(output::Terminal::StyleGuard &cur_style, const context::BasicFrame &frame)
{
    auto &thread_state = detail::ThreadState();
    if (!thread_state.current_test)
        HardError("No test is currently running, can't print context.", HardErrorKind::user);

    for (const auto &m : thread_state.current_test->all_tests->modules->GetModulesImplementing<&BasicPrintingModule::PrintContextFrame>())
    {
        if (m->PrintContextFrame(cur_style, frame))
            break;
    }
}

void ta_test::output::PrintLog(Terminal::StyleGuard &cur_style)
{
    auto &thread_state = detail::ThreadState();
    if (!thread_state.current_test)
        HardError("No test is currently running, can't print log.", HardErrorKind::user);

    // Refresh the messages. Only the scoped log, since the unscoped one should never be lazy.
    for (auto *entry : thread_state.scoped_log)
        entry->RefreshMessage();

    for (const auto &m : thread_state.current_test->all_tests->modules->GetModulesImplementing<&BasicPrintingModule::PrintLogEntries>())
    {
        if (m->PrintLogEntries(cur_style, thread_state.current_test->unscoped_log, context::CurrentScopedLog()))
            break;
    }
}

void ta_test::BasicPrintingModule::PrintWarning(output::Terminal::StyleGuard &cur_style, std::string_view text) const
{
    // Resetting the style before and after just in case.
    cur_style.ResetStyle();
    terminal.Print(cur_style, "{}{}{}\n",
        common_data.style_warning,
        common_data.warning_prefix,
        text
    );
    cur_style.ResetStyle();
}

void ta_test::BasicPrintingModule::PrintNote(output::Terminal::StyleGuard &cur_style, std::string_view text) const
{
    // Resetting the style before and after just in case.
    cur_style.ResetStyle();
    terminal.Print(cur_style, "{}{}{}\n",
        common_data.style_note,
        common_data.note_prefix,
        text
    );
    cur_style.ResetStyle();
}

void ta_test::detail::ArgWrapper::EnsureAssertionIsRunning()
{
    const BasicModule::BasicAssertionInfo *cur = ThreadState().current_assertion;

    while (cur)
    {
        if (cur == assertion)
            return;
        cur = cur->enclosing_assertion;
    }

    HardError("`$[...]` was evaluated when an assertion owning it already finished executing, or in a wrong thread.", HardErrorKind::user);
}

bool ta_test::detail::BasicAssertWrapper::Evaluator::operator~()
{
    AssertionStackGuard stack_guard(self);
    context::FrameGuard context_guard({std::shared_ptr<void>{}, &self});

    GlobalThreadState &thread_state = ThreadState();
    if (!thread_state.current_test)
        HardError("Something is wrong, the current test information disappeared while the assertion was evaluated.");
    if (thread_state.current_assertion != &self)
        HardError("The assertion being evaluated is not on the top of the assertion stack.");

    bool should_catch = true;
    thread_state.current_test->all_tests->modules->Call<&BasicModule::OnPreTryCatch>(should_catch);

    if (should_catch)
    {
        try
        {
            self.condition_func(self, self.condition_data);
        }
        catch (InterruptTestException)
        {
            // We don't want any additional errors here.
            throw;
        }
        catch (...)
        {
            thread_state.FailCurrentTest();

            auto e = std::current_exception();
            thread_state.current_test->all_tests->modules->Call<&BasicModule::OnUncaughtException>(*thread_state.current_test, &self, e);
        }
    }
    else
    {
        self.condition_func(self, self.condition_data);
    }

    // Fail if the condition is false.
    if (self.condition_value_known && !self.condition_value.value)
    {
        thread_state.FailCurrentTest();
        thread_state.current_test->all_tests->modules->Call<&BasicModule::OnAssertionFailed>(self);
    }

    // Break if a module callback (either on failed assertion or on exception) wants to.
    if (self.should_break)
        self.break_func();

    // Increment total checks counter. I'd do it before running the callback, but we need to know the flags.
    bool increment_counters = !bool(self.condition_value.flags & AssertFlags::no_increment_check_counters);
    if (increment_counters)
        const_cast<BasicModule::RunTestsProgress *>(thread_state.current_test->all_tests)->num_checks_total++;

    // Interrupt the test if the condition is false or on an exception.
    if (!self.condition_value_known || !self.condition_value.value)
    {
        // Increment failed checks counter.
        if (increment_counters)
            const_cast<BasicModule::RunTestsProgress *>(thread_state.current_test->all_tests)->num_checks_failed++;

        throw InterruptTestException{};
    }

    return self.condition_value_known && self.condition_value.value;
}

std::optional<std::string_view> ta_test::detail::BasicAssertWrapper::GetUserMessage() const
{
    if (message_func)
    {
        if (!message_cache)
            message_cache = message_func(message_data);
        return *message_cache;
    }
    else
    {
        return {};
    }
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
            {
                HardError(CFG_TA_FMT_NAMESPACE::format(
                    "Conflicting definitions for test `{}`. "
                    "One at `" DETAIL_TA_INTERNAL_ERROR_LOCATION_FORMAT "`, "
                    "another at `" DETAIL_TA_INTERNAL_ERROR_LOCATION_FORMAT "`.",
                    name, old_loc.file, old_loc.line, new_loc.file, new_loc.line
                ), HardErrorKind::user);
            }
            return; // Already registered.
        }
        else
        {
            // Make sure a test name is not also used as a group name.
            // Note, don't need to check `name.size() > it->first.size()` here, because if it was equal,
            // we wouldn't enter `else` at all, and if it was less, `.starts_with()` would return false.
            if (name.starts_with(it->first) && name[it->first.size()] == '/')
                HardError(CFG_TA_FMT_NAMESPACE::format("A test name (`{}`) can't double as a category name (`{}`). Append `/something` to the first name.", it->first, name), HardErrorKind::user);
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

std::size_t ta_test::detail::GenerateLogId()
{
    auto &thread_state = ThreadState();
    if (!thread_state.current_test)
        HardError("Can't log when no test is running.", HardErrorKind::user);
    return thread_state.log_id_counter++;
}

void ta_test::detail::AddLogEntry(std::string &&message)
{
    auto &thread_state = ThreadState();
    if (!thread_state.current_test)
        HardError("Can't log when no test is running.", HardErrorKind::user);
    thread_state.current_test->unscoped_log.emplace_back(GenerateLogId(), std::move(message));
}

ta_test::detail::BasicScopedLogGuard::BasicScopedLogGuard(context::LogEntry *entry)
    : entry(entry)
{
    auto &thread_state = ThreadState();
    if (!thread_state.current_test)
        HardError("Can't log when no test is running.", HardErrorKind::user);
    thread_state.scoped_log.push_back(entry);
}

ta_test::detail::BasicScopedLogGuard::~BasicScopedLogGuard()
{
    auto &thread_state = ThreadState();
    if (!thread_state.current_test)
        HardError("A scoped log guard somehow outlived the test.");
    if (thread_state.scoped_log.empty() || thread_state.scoped_log.back() != entry)
        HardError("The scoped log stack got corrupted.");
    thread_state.scoped_log.pop_back();
}

ta_test::detail::GenerateValueHelper::~GenerateValueHelper()
{
    if (untyped_generator)
    {
        auto &thread_state = ThreadState();

        // Check that we're still in the stack in the expected position.
        // We could've been popped from it on a generation failure.
        if (thread_state.current_test->generator_index < thread_state.current_test->generator_stack.size() &&
            thread_state.current_test->generator_stack[thread_state.current_test->generator_index].get() == untyped_generator
        )
        {
            // Post callback.
            BasicModule::GeneratorCallInfo callback_data{
                .test = thread_state.current_test,
                .generator = untyped_generator,
                .generating_new_value = generating_new_value,
            };
            thread_state.current_test->all_tests->modules->Call<&BasicModule::OnPostGenerate>(callback_data);

            thread_state.current_test->generator_index++;
        }
    }
}

void ta_test::detail::GenerateValueHelper::HandleGenerator(const BasicModule::SourceLoc &source_loc)
{
    auto &thread_state = ThreadState();

    // Need this variable, because the generator is later moved-from.
    bool creating_new_generator = bool(created_untyped_generator);

    // If creating a new generator...
    if (creating_new_generator)
    {
        if (thread_state.current_test->generator_index != thread_state.current_test->generator_stack.size())
            HardError("Something is wrong with the generator index."); // This should never happen.

        // Fail if no values.
        if (bool(untyped_generator->GetFlags() & GeneratorFlags::generate_nothing))
        {
            if (bool(untyped_generator->GetFlags() & GeneratorFlags::interrupt_test_if_empty))
            {
                // A micro-optimization. We don't need to run the destructor stuff in this case.
                untyped_generator = nullptr;

                throw InterruptTestException{};
            }
            else
            {
                HardError(CFG_TA_FMT_NAMESPACE::format(
                    "No values specified for generator at `" DETAIL_TA_INTERNAL_ERROR_LOCATION_FORMAT "`. "
                    "Must either specify them from the command line, ensure this generator isn't reached, or pass `ta_test::interrupt_test_if_empty` to interrupt the test.",
                    source_loc.file, source_loc.line
                ));
            }
        }

        // Possibly accept an override.
        for (const auto &m : thread_state.current_test->all_tests->modules->GetModulesImplementing<&BasicModule::OnRegisterGeneratorOverride>())
        {
            if (m->OnRegisterGeneratorOverride(*thread_state.current_test, *untyped_generator))
            {
                untyped_generator->overriding_module = m;
                break;
            }
        }

        thread_state.current_test->generator_stack.push_back(std::move(created_untyped_generator));
    }
    else
    {
        // Make sure this is the right generator.
        // Since the location is a part of the type, this nicely checks for the location equality.
        // This is one of the two determinism checks, the second one is in runner's `Run()` to make sure we visited all generators.
        if (!untyped_generator)
        {
            // Theoretically we can have two different generators at the same line, but I think this message is ok even in that case.
            HardError(CFG_TA_FMT_NAMESPACE::format(
                "Invalid non-deterministic use of generators. "
                "Was expecting to reach the generator at `" DETAIL_TA_INTERNAL_ERROR_LOCATION_FORMAT "`, "
                "but instead reached a different one at `" DETAIL_TA_INTERNAL_ERROR_LOCATION_FORMAT "`.",
                untyped_generator->GetLocation().file, untyped_generator->GetLocation().line,
                source_loc.file, source_loc.line
            ), HardErrorKind::user);
        }
    }

    generating_new_value = thread_state.current_test->generator_index + 1 == thread_state.current_test->generator_stack.size();

    // Advance the generator if needed.
    if (generating_new_value && (!untyped_generator->overriding_module || creating_new_generator))
    {
        switch (untyped_generator->RunGeneratorOverride())
        {
          case BasicModule::BasicGenerator::OverrideStatus::no_override:
            untyped_generator->Generate();
            break;
          case BasicModule::BasicGenerator::OverrideStatus::success:
            // Nothing.
            break;
          case BasicModule::BasicGenerator::OverrideStatus::no_more_values:
            HardError(
                CFG_TA_FMT_NAMESPACE::format(
                    "Generator `{}` was overriden to generate no values. This is not supported, you must avoid reaching the generator in the first place.",
                    untyped_generator->GetName()
                ),
                HardErrorKind::user
            );
            break;
        }
    }
}

std::string ta_test::string_conv::DefaultToStringTraits<ta_test::ExceptionElem>::operator()(const ExceptionElem &value) const
{
    switch (value)
    {
      case ExceptionElem::top_level:
        return "top_level";
      case ExceptionElem::most_nested:
        return "most_nested";
      case ExceptionElem::all:
        return "all";
      case ExceptionElem::any:
        return "any";
    }
    HardError("Invalid `ExceptionElem` enum.", HardErrorKind::user);
}

std::string ta_test::string_conv::DefaultToStringTraits<ta_test::ExceptionElemVar>::operator()(const ExceptionElemVar &value) const
{
    if (value.valueless_by_exception())
        HardError("Invalid `ExceptionElemVar` variant.");
    return std::visit(meta::Overload{
        [](ExceptionElem elem) {return (ToString)(elem);},
        [](int index) {return (ToString)(index);},
    }, value);
}

const std::vector<ta_test::SingleException> &ta_test::detail::GetEmptyExceptionListSingleton()
{
    // This is a little stupid, but probably better than a `HardError()`?
    static const std::vector<ta_test::SingleException> ret;
    return ret;
}

ta_test::CaughtException::CaughtException(
    const BasicModule::MustThrowStaticInfo *static_info,
    std::weak_ptr<const BasicModule::MustThrowDynamicInfo> dynamic_info,
    const std::exception_ptr &e
)
    : state(std::make_shared<BasicModule::CaughtExceptionInfo>())
{
    state->static_info = static_info;
    state->dynamic_info = std::move(dynamic_info);

    AnalyzeException(e, [&](SingleException elem)
    {
        state->elems.push_back(std::move(elem));
    });
}

std::optional<std::string_view> ta_test::detail::MustThrowWrapper::Info::GetUserMessage() const
{
    if (self.message_func)
    {
        if (!self.message_cache)
            self.message_cache = self.message_func(self.message_data);
        return *self.message_cache;
    }
    else
    {
        return {};
    }
}

ta_test::CaughtException ta_test::detail::MustThrowWrapper::Evaluator::operator~() const
{
    auto &thread_state = ThreadState();

    if (!ThreadState().current_test)
        HardError("Attempted to use `TA_MUST_THROW(...)`, but no test is currently running.", HardErrorKind::user);

    // Increment total checks counter.
    const_cast<BasicModule::RunTestsProgress *>(thread_state.current_test->all_tests)->num_checks_total++;

    try
    {
        context::FrameGuard guard({self.info, &self.info->info}); // A wonky owning pointer that points to a subobject.
        self.body_func(self.body_data);
    }
    catch (...)
    {
        return CaughtException(
            self.info->info.static_info,
            std::shared_ptr<const BasicModule::MustThrowDynamicInfo>{self.info, self.info->info.dynamic_info},
            std::current_exception()
        );
    }

    thread_state.FailCurrentTest();

    bool should_break = false;
    thread_state.current_test->all_tests->modules->Call<&BasicModule::OnMissingException>(self.info->info, should_break);
    if (should_break)
        self.break_func();

    // Increment failed checks counter.
    const_cast<BasicModule::RunTestsProgress *>(thread_state.current_test->all_tests)->num_checks_failed++;

    throw InterruptTestException{};
}

void ta_test::Runner::SetDefaultModules()
{
    modules.clear();
    // Those are ordered in a certain way to print the `--help` page in the nice order: [
    modules.push_back(MakeModule<modules::HelpPrinter>());
    modules.push_back(MakeModule<modules::TestSelector>());
    modules.push_back(MakeModule<modules::GeneratorOverrider>());
    modules.push_back(MakeModule<modules::PrintingConfigurator>());
    // ]
    modules.push_back(MakeModule<modules::ProgressPrinter>());
    modules.push_back(MakeModule<modules::ResultsPrinter>());
    modules.push_back(MakeModule<modules::AssertionPrinter>());
    modules.push_back(MakeModule<modules::LogPrinter>());
    modules.push_back(MakeModule<modules::DefaultExceptionAnalyzer>());
    modules.push_back(MakeModule<modules::ExceptionPrinter>());
    modules.push_back(MakeModule<modules::MustThrowPrinter>());
    modules.push_back(MakeModule<modules::TracePrinter>());
    modules.push_back(MakeModule<modules::DebuggerDetector>());
    modules.push_back(MakeModule<modules::DebuggerStatePrinter>());
}

void ta_test::Runner::ProcessFlags(std::function<std::optional<std::string_view>()> next_flag, bool *ok) const
{
    if (ok)
        *ok = true;

    auto Fail = [&]
    {
        if (ok)
            *ok = false;
        else
            std::exit(int(ExitCode::bad_command_line_arguments));
    };

    while (true)
    {
        std::optional<std::string_view> flag = next_flag();
        if (!flag)
            return;

        std::optional<std::string_view> arg;

        // Handle arguments embedded in the flag.

        // Short form.
        if (flag->size() > 2 && flag->starts_with('-') && (*flag)[1] != '-')
        {
            arg = flag->substr(2);
            flag = flag->substr(0, 2);
        }
        // Long form.
        else if (auto sep = flag->find_first_of('='); sep != std::string_view::npos)
        {
            arg = flag->substr(sep + 1);
            flag = flag->substr(0, sep);
        }

        bool unknown = true;
        for (const auto &m : modules) // `ModuleLists` doesn't exist yet.
        {
            auto flags = m->GetFlags();

            for (auto &f : flags)
            {
                bool already_used_single_arg = false;
                bool missing_arg = false;
                unknown = !f->ProcessFlag(*this, *m, *flag, [&]() -> std::optional<std::string_view>
                {
                    if (arg)
                    {
                        if (already_used_single_arg)
                        {
                            // If the argument was specified with `=`, we don't allow additional arguments after that.
                            missing_arg = true;
                            return {};
                        }
                        already_used_single_arg = true;
                        return *arg;
                    }
                    else
                    {
                        if (missing_arg)
                            return {};
                        auto ret = next_flag();
                        if (!ret)
                            missing_arg = true;
                        return ret;
                    }
                });

                // If we're missing an argument...
                if (missing_arg)
                {
                    for (const auto &m2 : modules) // `ModuleLists` doesn't exist yet.
                        m2->OnMissingFlagArgument(*flag, *f, missing_arg);
                    if (missing_arg)
                    {
                        Fail();
                        break;
                    }
                }

                if (!unknown)
                    break;
            }

            if (!unknown)
                break;
            if (ok && !*ok)
                break;
        }

        if (ok && !*ok)
            break;

        // If the argument is unknown...
        if (unknown)
        {
            for (const auto &m2 : modules) // `ModuleLists` doesn't exist yet.
                m2->OnUnknownFlag(*flag, unknown);
            if (unknown)
            {
                Fail();
                break;
            }
        }
    }
}

int ta_test::Runner::Run()
{
    auto& thread_state = detail::ThreadState();

    if (thread_state.current_test)
        HardError("This thread is already running a test.", HardErrorKind::user);

    ModuleLists module_lists(modules);

    const auto &state = detail::State();

    std::vector<std::size_t> ordered_tests;

    { // Get a list of tests to run.
        ordered_tests.reserve(state.tests.size());

        for (std::size_t i = 0; i < state.tests.size(); i++)
        {
            bool enable = true;
            module_lists.Call<&BasicModule::OnFilterTest>(*state.tests[i], enable);
            if (enable)
                ordered_tests.push_back(i);
        }
        state.SortTestListInExecutionOrder(ordered_tests);
    }

    BasicModule::RunTestsResults results;
    results.modules = &module_lists;
    results.num_tests = ordered_tests.size();
    results.num_tests_with_skipped = state.tests.size();
    module_lists.Call<&BasicModule::OnPreRunTests>(results);

    // For every non-skipped test...
    for (std::size_t test_index : ordered_tests)
    {
        const detail::BasicTest *test = state.tests[test_index];

        // This stores the generator stack between iterations.
        std::vector<std::unique_ptr<const BasicModule::BasicGenerator>> next_generator_stack;

        // Whether any of the repetitions have failed.
        bool any_repetition_failed = false;

        // Repeat to exhaust all generators...
        do
        {
            struct StateGuard
            {
                BasicModule::RunSingleTestResults state;
                StateGuard() {detail::ThreadState().current_test = &state;}
                StateGuard(const StateGuard &) = delete;
                StateGuard &operator=(const StateGuard &) = delete;
                ~StateGuard() {detail::ThreadState().current_test = nullptr;}
            };
            StateGuard guard;
            guard.state.all_tests = &results;
            guard.state.test = test;
            guard.state.generator_stack = std::move(next_generator_stack);
            guard.state.is_first_generator_repetition = guard.state.generator_stack.empty();

            module_lists.Call<&BasicModule::OnPreRunSingleTest>(guard.state);

            bool should_catch = true;
            module_lists.Call<&BasicModule::OnPreTryCatch>(should_catch);

            auto lambda = [&]
            {
                test->Run();
            };

            if (should_catch)
            {
                try
                {
                    lambda();
                }
                catch (InterruptTestException) {}
                catch (...)
                {
                    thread_state.FailCurrentTest();
                    auto e = std::current_exception();
                    module_lists.Call<&BasicModule::OnUncaughtException>(guard.state, nullptr, e);
                }
            }
            else
            {
                lambda();
            }

            // Check for non-deterministic use of generators.
            // This is one of the two determinism checks, the second one is in generator's `Generate()` to make sure we're not visiting generators out of order.
            if (!guard.state.failed && guard.state.generator_index < guard.state.generator_stack.size())
            {
                // We only emit a hard error if the test didn't fail.
                // If it did fail, we print a non-determinism warning elsewhere.

                const auto &loc = guard.state.generator_stack[guard.state.generator_index]->GetLocation();

                HardError(CFG_TA_FMT_NAMESPACE::format(
                    "Invalid non-deterministic use of generators. "
                    "Was expecting to reach the generator at `" DETAIL_TA_INTERNAL_ERROR_LOCATION_FORMAT "`, "
                    "but instead reached the end of the test.",
                    loc.file, loc.line
                ), HardErrorKind::user);
            }

            // Prune finished generators, and advance overridden generators.
            // Overridden generators are advanced here rather than in the test body, because we don't know which value is the last one beforehand,
            //   so we need to actually generate the values to decide if we should reenter the test or not.
            // We do all of this here, before calling `OnPostRunSingleTest()`,
            //   because we need this to detect the last test repetition, which the callback wants to know.
            [&]() noexcept
            {
                // Remove unvisited generators. Note that we show a warning/error for this condition above.
                // This shouldn't normally happen.
                guard.state.generator_stack.resize(guard.state.generator_index);

                { // Remove generators that threw exceptions.
                    auto iter = std::find_if(guard.state.generator_stack.begin(), guard.state.generator_stack.end(), [](const auto &g){return g->CallbackThrewException();});
                    guard.state.generator_stack.erase(iter, guard.state.generator_stack.end());

                    // Adjust the index, just in case.
                    if (guard.state.generator_index > guard.state.generator_stack.size())
                        guard.state.generator_index = guard.state.generator_stack.size();
                }

                // Prune finished generators.
                if (!guard.state.generator_stack.empty())
                {
                    // We use this as the loop counter, and also the overriding module wants to see the correct index in the callback.
                    guard.state.generator_index = guard.state.generator_stack.size() - 1;

                    // Note, we don't actually remove anything from the stack here yet.
                    // This allows us to capture the full generator stack if the test fails while we advance overridden generators here,
                    // from the on `OnPreFailTest()` callback.

                    while (true)
                    {
                        auto &this_generator = *guard.state.generator_stack[guard.state.generator_index];

                        bool should_pop = false;

                        try
                        {
                            switch (const_cast<BasicModule::BasicGenerator &>(this_generator).RunGeneratorOverride())
                            {
                              case BasicModule::BasicGenerator::OverrideStatus::no_override:
                                should_pop = this_generator.IsLastValue();
                                break;
                              case BasicModule::BasicGenerator::OverrideStatus::success:
                                // Nothing.
                                break;
                              case BasicModule::BasicGenerator::OverrideStatus::no_more_values:
                                should_pop = true;
                                break;
                            }
                        }
                        catch (...)
                        {
                            should_pop = true;
                        }

                        if (should_pop)
                        {
                            if (guard.state.generator_index == 0)
                                break;
                            guard.state.generator_index--;
                        }
                        else
                        {
                            guard.state.generator_index++;
                            break;
                        }
                    }

                    // Actually pop the generators.
                    while (guard.state.generator_stack.size() > guard.state.generator_index)
                    {
                        module_lists.Call<&BasicModule::OnPrePruneGenerator>(guard.state);
                        guard.state.generator_stack.pop_back();
                    }
                }
            }();

            // We need this to be late, since the test can fail while pruning generators.
            results.num_tests_with_repetitions_total++;
            if (guard.state.failed)
            {
                any_repetition_failed = true;
                results.num_tests_with_repetitions_failed++;
            }

            guard.state.is_last_generator_repetition = guard.state.generator_stack.empty();

            module_lists.Call<&BasicModule::OnPostRunSingleTest>(guard.state);

            if (guard.state.should_break)
                test->Breakpoint();

            next_generator_stack = std::move(guard.state.generator_stack);
        }
        while (!next_generator_stack.empty());

        if (any_repetition_failed)
            results.failed_tests.push_back(test);
    }

    module_lists.Call<&BasicModule::OnPostRunTests>(results);

    return results.failed_tests.size() > 0 ? int(ExitCode::test_failed) : 0;
}

// --- modules::BasicExceptionContentsPrinter ---

void ta_test::modules::BasicExceptionContentsPrinter::EnableUnicode(bool enable)
{
    BasicPrintingModule::EnableUnicode(enable);

    if (enable)
    {
        // This symbol has a tendency to be rendered with a half-character offset to the right, so we put it slightly to the left.
        chars_indent_type_active = " \xE2\x96\xB6  "; // U+25B6 BLACK RIGHT-POINTING TRIANGLE
    }
    else
    {
        chars_indent_type_active = "  > ";
    }
    chars_indent_message_active = chars_indent_type_active + "    ";
}

ta_test::modules::BasicExceptionContentsPrinter::BasicExceptionContentsPrinter()
{
    EnableUnicode(true);
}

void ta_test::modules::BasicExceptionContentsPrinter::PrintException(
    const output::Terminal &terminal,
    output::Terminal::StyleGuard &cur_style,
    const std::exception_ptr &e,
    int active_elem
) const
{
    int i = 0;
    AnalyzeException(e, [&](const SingleException &elem)
    {
        bool active = i++ == active_elem;

        if (elem.IsTypeKnown())
        {
            terminal.Print(cur_style, "{}{}{}{}{}\n{}{}{}{}\n",
                style_exception_active_marker,
                active ? chars_indent_type_active : chars_indent_type,
                active ? style_exception_type_active : style_exception_type,
                elem.GetTypeName(),
                chars_type_suffix,
                style_exception_active_marker,
                active ? chars_indent_message_active : chars_indent_message,
                active ? style_exception_message_active : style_exception_message,
                string_conv::ToString(elem.message)
            );
        }
        else
        {
            terminal.Print(cur_style, "{}{}{}{}\n",
                style_exception_active_marker,
                active ? chars_indent_type_active : chars_indent_type,
                active ? style_exception_type_active : style_exception_type,
                chars_unknown_exception
            );
        }
    });
}

// --- modules::HelpPrinter ---

ta_test::modules::HelpPrinter::HelpPrinter()
    : expected_flag_width(17),
    flag_help("help", "Show usage.", [](const Runner &runner, BasicModule &this_module)
    {
        std::vector<flags::BasicFlag *> flags;
        for (const auto &m : runner.modules) // Can't use `ModuleLists` here yet.
        {
            auto more_flags = m->GetFlags();
            flags.insert(flags.end(), more_flags.begin(), more_flags.end());
        }

        // The case should never fail.
        HelpPrinter &self = dynamic_cast<HelpPrinter &>(this_module);

        self.terminal.Print("This is a test runner based on ta_test.\nAvailable options:\n");
        for (flags::BasicFlag *flag : flags)
            self.terminal.Print("  {:<{}} - {}\n", flag->HelpFlagSpelling(), self.expected_flag_width, flag->help_desc);

        std::exit(int(ExitCode::ok));
    })
{}

std::vector<ta_test::flags::BasicFlag *> ta_test::modules::HelpPrinter::GetFlags() noexcept
{
    return {&flag_help};
}

void ta_test::modules::HelpPrinter::OnUnknownFlag(std::string_view flag, bool &abort) noexcept
{
    (void)abort;
    terminal.Print("Unknown flag `{}`, run with `{}` for usage.\n", flag, flag_help.HelpFlagSpelling());
    // Don't exit, rely on `abort`.
}

void ta_test::modules::HelpPrinter::OnMissingFlagArgument(std::string_view flag, const flags::BasicFlag &flag_obj, bool &abort) noexcept
{
    (void)flag_obj;
    (void)abort;
    terminal.Print("Flag `{}` wasn't given enough arguments, run with `{}` for usage.\n", flag, flag_help.HelpFlagSpelling());
    // Don't exit, rely on `abort`.
}

// --- modules::TestSelector ---

ta_test::modules::TestSelector::TestSelector()
    : flag_include("include", 'i',
        "Enable tests matching a pattern. The pattern must either match the whole test name, or its prefix up to and possibly including a slash. "
        "The pattern can be a regex. This flag can be repeated multiple times, and the order with respect to `--exclude` matters. "
        "If the first time this flag appears is before `--exclude`, all tests start disabled by default. "
        "If the pattern contains `//`, then `--include A//B` acts as a shorthand for `--include A --generate A//B`.",
        GetFlagCallback(false)
    ),
    flag_exclude("exclude", 'e',
        "Disable tests matching a pattern. Uses the same pattern format as `--include`.",
        GetFlagCallback(true)
    )
{}

std::vector<ta_test::flags::BasicFlag *> ta_test::modules::TestSelector::GetFlags() noexcept
{
    return {&flag_include, &flag_exclude};
}

void ta_test::modules::TestSelector::OnFilterTest(const BasicTestInfo &test, bool &enable) noexcept
{
    if (patterns.empty())
        return;

    // Disable by default is the first pattern is `--include`.
    if (!patterns.front().exclude)
        enable = false;

    for (Pattern &pattern : patterns)
    {
        if (enable != pattern.exclude)
            continue; // The test is already enabled/disabled.

        if (ta_test::text::TestNameMatchesRegex(test.Name(), pattern.regex))
        {
            pattern.was_used = true;
                enable = !pattern.exclude;
        }
    }
}

ta_test::flags::StringFlag::Callback ta_test::modules::TestSelector::GetFlagCallback(bool exclude)
{
    return [exclude](const Runner &runner, BasicModule &this_module, std::string_view pattern)
    {
        TestSelector &self = dynamic_cast<TestSelector &>(this_module); // This cast should never fail.

        // Check for `//` in the pattern.
        if (auto sep = pattern.find("//"); sep != std::string::npos)
        {
            if (exclude)
                HardError("Separator `//` can appear only in `--include`, not in `--exclude`.");

            bool found = runner.FindModule<GeneratorOverrider>([&](GeneratorOverrider &overrider)
            {
                overrider.flag_override.callback(runner, overrider, pattern);
                pattern = pattern.substr(0, sep);
                return true;
            });
            if (!found)
                HardError("There's no `GeneratorOverrider` module, can't process `//` in `--include`.");
        }

        Pattern new_pattern{
            .exclude = exclude,
            .regex_string = std::string(pattern),
            .regex = std::regex(new_pattern.regex_string, std::regex_constants::ECMAScript/*the default syntax*/ | std::regex_constants::optimize),
        };
        self.patterns.push_back(std::move(new_pattern));
    };
}

void ta_test::modules::TestSelector::OnPreRunTests(const RunTestsInfo &data) noexcept
{
    (void)data;

    // Make sure all patterns were used.
    bool fail = false;
    for (const Pattern &pattern : patterns)
    {
        if (!pattern.was_used)
        {
            std::fprintf(stderr, "Pattern `--%s %s` didn't match any tests.\n", pattern.exclude ? "exclude" : "include", pattern.regex_string.c_str());
            fail = true;
        }
    }
    if (fail)
        std::exit(int(ExitCode::no_test_name_match));
}

// --- modules::GeneratorOverrider ---

std::string_view ta_test::modules::GeneratorOverrider::Entry::OriginalArgument() const
{
    if (original_argument_storage.empty())
        return {};
    else
        return {original_argument_storage.data(), original_argument_storage.size() - 1};
}

ta_test::modules::GeneratorOverrider::GeneratorOverrider()
    : flag_override("generate", 'g',
        "Changes the behavior of `TA_GENERATE(...)`. The argument is a test name regex (as in `--include`), "
        "followed by `//`, then a comma-separated list of generator overrides, such as `name=value`. See `--help-generate` for a detailed explanation.",
        [](const Runner &runner, BasicModule &this_module, std::string_view input)
        {
            (void)runner;

            auto &self = dynamic_cast<GeneratorOverrider &>(this_module);

            Entry &new_entry = self.entries.emplace_back();

            // Copy the input string into a vector, which gives it a persistent address.
            // The parsed `GeneratorOverrideSeq` will point into it.
            new_entry.original_argument_storage.reserve(input.size() + 1);
            new_entry.original_argument_storage.assign(input.data(), input.data() + input.size());
            new_entry.original_argument_storage.push_back('\0');

            static constexpr std::string_view separator = "//";

            auto sep_pos = input.find(separator);
            if (sep_pos == std::string_view::npos)
            {
                self.HardErrorInFlag(
                    "Expected `//` after the test name regex.",
                    new_entry, new_entry.original_argument_storage.data() + input.size(),
                    HardErrorKind::user
                );
            }

            new_entry.test_regex = std::regex(input.begin(), input.begin() + sep_pos, std::regex_constants::ECMAScript/*the default syntax*/ | std::regex_constants::optimize);


            const char *string = new_entry.original_argument_storage.data() + sep_pos + separator.size();

            text::chars::SkipWhitespace(string);

            std::string error = self.ParseGeneratorOverrideSeq(new_entry.seq, string, false);
            if (!error.empty())
                self.HardErrorInFlag(error, new_entry, string, HardErrorKind::user);
        }
    ),
    flag_local_help("help-generate", CFG_TA_FMT_NAMESPACE::format("Show detailed help about `--{}`.", flag_override.flag),
        [](const Runner &runner, BasicModule &this_module)
        {
            (void)runner;
            (void)this_module;
            dynamic_cast<GeneratorOverrider &>(this_module).terminal.Print("{}", &R"(
The argument of `--generate` is a name regex (as in `--include`), followed by `//`, then a comma-separated list of generator overrides.
Some examples: (here `x`,`y` are generator names as passed to `TA_GENERATE(name, ...)`)
* -g 'foo/bar//x=42'         - generate only this value.
* -g 'foo/bar//x=42,y=43'    - override several generators (the order matters; you can omit some of the generators).
* -g 'foo/bar//x{=10,=20}'   - several values per generator.
* -g 'foo/bar//x-=10         - skip specific value.
* -g 'foo/bar//x#10'         - only generate the value at the specified index (1-based).
* -g 'foo/bar//x#10..12'     - same, but with a range of indices (inclusive). One of the numbers can be omitted: `..10`, `10..`.
* -g 'foo/bar//x-#10'        - skip the value at the specific index. This also accepts ranges.
Multiple operators can be combined:
* -g 'foo/bar//x{#..10,=42}' - generate only 10 first values, and a custom value `42`.
Operators are applied left to right. If the first operator is `=` or `#`, all values are disabled by default. But you can reenable them manually:
* -g 'foo/bar//x{#1..,=42}'  - generate all values, and a custom one.
Operators `=` and `#` can be followed by a parenthesized list of generator overrides, which are used in place of the remaining string for those values:
* -g 'foo/bar//x{#1..,#5(y=20)},y=10' - override `y=20` for 5th value of `x`, and `y=10` for all other values of `x`.
If multiple operators match the same value, parentheses from the last match are used.
Parentheses apply only to the single preceding operator by default. To apply them to multiple operators, separate the operators with `&` instead of `,`:
* -g 'foo/bar//x{#1..,#5&=42(y=20)},y=10' - override `y=20` for 5th value of `x` and for a custom value `x=42`, for all other values of `x` use `y=10`.
More examples:
* -g 'foo/bar//x{#1..,#5()},y=10' - override `y=10` for all values of `x` except the 5-th one.
* -g 'foo/bar//x{#1..(y=10),#5}' - same effect as above.
More than one `--generate` flag can be active in a given test at a time. They run in parallel rather than sequentially, in the sense that each flag maintains
  its own "instruction pointer". If multiple flags offer the same generator, the latest flag gets preference, and preceding flags skip that generator.
Some notes:
* This flag changes the generator semantics slightly, making subsequent calls to the generator lambda happen between the test repetitions, as opposed to
    when the control flow reaches the `TA_GENERATE(...)` call, to avoid entering the test when all future values are disabled.
    This shouldn't affect you much, unless you're doing something unusual in the generator callback, or unless you're throwing from it
    (then the repetition counters will can be slightly off, and trying to catch the resulting `InterruptTestException` stops being possible).
* Not all types can be deserialized from strings, but index-based operators will always work.
    We support scalars, strings (with standard escape sequences), containers (as printed by `std::format()`: {...} sets, {a:b, c:d} maps, [...] other containers, and (...) tuples).
    Custom type support can be added by specializing `ta_test::string_conv::FromStringTraits`.
* `-=` requires overloaded `==` to work.
* Values added with `=` have no index, so `#` and `-#` don't affect them.
)"[1]);
            std::exit(int(ExitCode::ok));
        }
    )
{}

std::vector<ta_test::flags::BasicFlag *> ta_test::modules::GeneratorOverrider::GetFlags() noexcept
{
    return {&flag_override, &flag_local_help};
}

void ta_test::modules::GeneratorOverrider::OnPreRunTests(const RunTestsInfo &data) noexcept
{
    (void)data;
    test_state = {};
}

void ta_test::modules::GeneratorOverrider::OnPostRunTests(const RunTestsResults &data) noexcept
{
    (void)data;

    for (const Entry &entry : entries)
    {
        // Whole flag is unused?
        if (!entry.was_used)
            HardErrorInFlag("This regex didn't match any tests.", entry, FlagErrorDetails{}, HardErrorKind::user);

        // Individual generators or rules are unused?
        FlagErrorDetails errors_unused;

        // Those are index ranges with upper bounds being too high.
        FlagErrorDetails errors_high_upper_bounds;
        // The first range in `errors_high_upper_bounds` should only go up to this number.
        // We only show this for the first range, for simplicity.
        std::size_t first_upper_bound = 0;

        auto lambda = [&](auto &lambda, const GeneratorOverrideSeq &seq) -> void
        {
            for (const GeneratorOverrideSeq::Entry &override_entry : seq.entries)
            {
                if (!override_entry.was_used)
                {
                    // Whole entry is unused.
                    errors_unused.elems.push_back({.marker = std::string(override_entry.total_num_characters, '~'), .location = override_entry.generator_name.data()});
                }
                else
                {
                    { // Custom values.
                        const GeneratorOverrideSeq *last_seq = nullptr;

                        for (const GeneratorOverrideSeq::Entry::CustomValue &value : override_entry.custom_values)
                        {
                            if (!value.was_used)
                            {
                                errors_unused.elems.push_back({
                                    .marker = std::string(std::size_t(value.value.data() + value.value.size() - value.operator_character), '~'),
                                    .location = value.operator_character,
                                });
                            }

                            if (value.custom_generator_seq && value.custom_generator_seq.get() != last_seq)
                            {
                                lambda(lambda, *value.custom_generator_seq);
                                last_seq = value.custom_generator_seq.get();
                            }
                        }
                    }

                    { // Other rules.
                        const GeneratorOverrideSeq *last_seq = nullptr;

                        for (const GeneratorOverrideSeq::Entry::Rule &basic_rule : override_entry.rules)
                        {
                            if (!basic_rule.was_used)
                            {
                                std::size_t num_chars = 0;

                                std::visit(meta::Overload{
                                    [&](const GeneratorOverrideSeq::Entry::RuleRemoveValue &rule)
                                    {
                                        num_chars = std::size_t(rule.value.data() + rule.value.size() - basic_rule.operator_character);
                                    },
                                    [&](const GeneratorOverrideSeq::Entry::RuleIndex &rule)
                                    {
                                        num_chars = rule.total_num_characters;
                                    },
                                }, basic_rule.var);

                                errors_unused.elems.push_back({
                                    .marker = std::string(num_chars, '~'),
                                    .location = basic_rule.operator_character,
                                });
                            }
                            else
                            {
                                // Check the range in the index rules.
                                if (auto *rule_index = std::get_if<GeneratorOverrideSeq::Entry::RuleIndex>(&basic_rule.var))
                                {
                                    if (rule_index->end != std::size_t(-1) && rule_index->max_used_end < rule_index->end)
                                    {
                                        if (errors_high_upper_bounds.elems.empty())
                                            first_upper_bound = rule_index->max_used_end;
                                        errors_high_upper_bounds.elems.push_back({.marker = "^", .location = rule_index->end_string_location});
                                    }
                                }
                            }

                            if (basic_rule.custom_generator_seq && basic_rule.custom_generator_seq.get() != last_seq)
                            {
                                lambda(lambda, *basic_rule.custom_generator_seq);
                                last_seq = basic_rule.custom_generator_seq.get();
                            }
                        }
                    }
                }
            }
        };
        lambda(lambda, entry.seq);

        if (!errors_unused.elems.empty())
            HardErrorInFlag("Those parts are unused.", entry, errors_unused, HardErrorKind::user);

        if (!errors_high_upper_bounds.elems.empty())
        {
            HardErrorInFlag(
                errors_high_upper_bounds.elems.size() == 1
                ? CFG_TA_FMT_NAMESPACE::format("This upper bound is too large, the max index was {}.", first_upper_bound)
                : CFG_TA_FMT_NAMESPACE::format("Those upper bounds are too large, e.g. max index for the first one was {}.", first_upper_bound)
                ,
                entry, errors_high_upper_bounds, HardErrorKind::user);
        }
    }
}

void ta_test::modules::GeneratorOverrider::OnPostRunSingleTest(const RunSingleTestResults &data) noexcept
{
    if (data.is_last_generator_repetition && test_state)
    {
        test_state = {};
    }
}

bool ta_test::modules::GeneratorOverrider::OnRegisterGeneratorOverride(const RunSingleTestProgress &test, const BasicGenerator &generator) noexcept
{
    // If we don't have an active `--generate` flag, try to find one.
    if (!test_state)
    {
        test_state.emplace();

        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        {
            const Entry &entry = *it;

            if (text::TestNameMatchesRegex(test.test->Name(), entry.test_regex))
            {
                entry.was_used = true;
                test_state->active_flags.push_back({
                    .entry = &entry,
                    .remaining_program = entry.seq.entries,
                });
            }
        }
    }

    // If we do have an active `--generate` flag, use it.
    if (test_state)
    {
        bool found = false;

        for (ActiveFlag &flag : test_state->active_flags)
        {
            if (flag.remaining_program.empty())
                continue;

            const GeneratorOverrideSeq::Entry &override_entry = flag.remaining_program.front();

            if (override_entry.generator_name == generator.GetName())
            {
                if (!found)
                    override_entry.was_used = true; // Only the first flag is marked as used.

                found = true;
                flag.elems.push_back({.generator_index = test.generator_index, .remaining_program = flag.remaining_program});
            }
        }
        if (found)
            return true;
    }

    return false;
}

bool ta_test::modules::GeneratorOverrider::OnOverrideGenerator(const RunSingleTestProgress &test, BasicGenerator &generator) noexcept
{
    if (!test_state)
        HardError("A generator override is requested, but we don't have an active state.");

    ActiveFlag *this_flag = nullptr;
    ActiveFlag::Elem *this_elem = nullptr;
    for (ActiveFlag &active_flag : test_state->active_flags)
    {
        auto iter = std::partition_point(active_flag.elems.begin(), active_flag.elems.end(), [&](const ActiveFlag::Elem &elem){return elem.generator_index < test.generator_index;});
        if (iter != active_flag.elems.end() && iter->generator_index == test.generator_index)
        {
            if (!this_elem)
            {
                // The first match.
                this_elem = std::to_address(iter);
                this_flag = &active_flag;
            }
            else
            {
                // In the remaining matches, we just advance the program counter.
                if (!active_flag.remaining_program.empty())
                    active_flag.remaining_program = active_flag.remaining_program.subspan(1);
            }
        }
    }
    if (!this_elem)
        HardError("A generator override is requested, but the state doesn't contain information about this generator.");


    const GeneratorOverrideSeq::Entry &command = this_elem->remaining_program.front();

    const auto default_remaining_program = this_elem->remaining_program.subspan(1);

    // Generate the values until one passes the conditions...
    while (true)
    {
        // A helper to check the result of `ReplaceValueFromString()` or `ValueEqualsToString()`. Raises a hard error on any failure.
        // `error` is the parsing error, if any, returned from the function being checked.
        // `string` is string pointer after parsing.
        // `expected_end` is where we expected the string pointer to be.
        auto CheckValueParsingResult = [&](std::string_view error, const char *string, const char *expected_end)
        {
            if (string > expected_end)
            {
                HardErrorInFlag(
                    "Parsing the value consumed more characters than expected. "
                    "Expected the parsing to end at (1), but it ended at (2).",
                    *this_flag->entry, {{
                        FlagErrorDetails::Elem{.marker = "1^", .location = expected_end - 2},
                        FlagErrorDetails::Elem{.marker = "^2", .location = string - 1},
                    }}, HardErrorKind::user
                );
            }
            if (!error.empty())
                HardErrorInFlag(error, *this_flag->entry, string, HardErrorKind::user);
            if (string < expected_end)
            {
                HardErrorInFlag(
                    "Junk characters after the value. The values ends at (1), junk ends at (2).",
                    *this_flag->entry, {{
                        FlagErrorDetails::Elem{.marker = "1^", .location = string - 2},
                        FlagErrorDetails::Elem{.marker = "^2", .location = expected_end - 1},
                    }}, HardErrorKind::user
                );
            }
        };

        // Reset the remaining program first.
        this_flag->remaining_program = default_remaining_program;

        // Insert the next custom value, if any.

        std::size_t rule_index = 0;
        bool using_custom_value = false;
        if (this_elem->num_used_custom_values < command.custom_values.size())
        {
            const auto &this_value = command.custom_values[this_elem->num_used_custom_values];

            this_value.was_used = true;

            if (!generator.ValueConvertibleFromString())
            {
                HardErrorInFlag(
                    CFG_TA_FMT_NAMESPACE::format(
                        "The generated type `{}` can't be deserialized from a string, so `=` can't be used with it. "
                        "But you can filter certain generated values by their indices using `#`, see `--help-generate` for details.",
                        generator.GetTypeName()
                    ),
                    *this_flag->entry, this_value.value.data(), HardErrorKind::user
                );
            }

            const char *string = this_value.value.data();
            std::string error = generator.ReplaceValueFromString(string);
            CheckValueParsingResult(error, string, this_value.value.data() + this_value.value.size());

            this_elem->num_used_custom_values++;

            using_custom_value = true;
            rule_index = this_value.next_rule;

            // Replace the remaining program.
            if (this_value.custom_generator_seq)
                this_flag->remaining_program = this_value.custom_generator_seq->entries;
            else
                this_flag->remaining_program = default_remaining_program;
        }

        // Generate the next natural value, if any.

        if (!using_custom_value)
        {
            if (generator.IsLastValue())
                return true; // No more values.

            try
            {
                generator.Generate();
            }
            catch (InterruptTestException)
            {
                return true; // No more values.
            }
        }

        // Outright reject natural values if they overlap with `=`.
        // But if the type isn't equality-comparable, just don't do anything.

        if (!using_custom_value && generator.ValueEqualityComparableToString())
        {
            bool skip = false;
            for (const auto &custom_value : command.custom_values)
            {
                const char *string = custom_value.value.data();
                bool equal = false;
                std::string error = generator.ValueEqualsToString(string, equal);
                CheckValueParsingResult(error, string, custom_value.value.data() + custom_value.value.size());
                if (equal)
                {
                    skip = true;
                    break;
                }
            }
            if (skip)
                continue; // Generate the next value.
        }

        // Process the rules.

        bool value_passes = using_custom_value || command.enable_values_by_default;

        for (; rule_index < command.rules.size(); rule_index++)
        {
            const GeneratorOverrideSeq::Entry::Rule &basic_rule = command.rules[rule_index];

            std::visit(meta::Overload{
                [&](const GeneratorOverrideSeq::Entry::RuleIndex &rule)
                {
                    if (using_custom_value)
                        return;

                    if (generator.NumGeneratedValues() >= rule.begin + 1 && generator.NumGeneratedValues() - 1 < rule.end)
                    {
                        if (value_passes != rule.add)
                        {
                            value_passes = rule.add;

                            basic_rule.was_used = true;

                            // Note, must use `max()` here. Even though the indices are monotonous in each test, the flag can be shared by multiple tests.
                            rule.max_used_end = std::max(rule.max_used_end, generator.NumGeneratedValues());
                        }

                        // Replace the remaining program.
                        if (rule.add)
                        {
                            if (basic_rule.custom_generator_seq)
                            {
                                if (this_flag->remaining_program.data() != basic_rule.custom_generator_seq->entries.data() ||
                                    this_flag->remaining_program.size() != basic_rule.custom_generator_seq->entries.size()
                                )
                                {
                                    basic_rule.was_used = true;
                                    this_flag->remaining_program = basic_rule.custom_generator_seq->entries;
                                }
                            }
                            else
                            {
                                if (this_flag->remaining_program.data() != default_remaining_program.data() ||
                                    this_flag->remaining_program.size() != default_remaining_program.size()
                                )
                                {
                                    basic_rule.was_used = true;
                                    this_flag->remaining_program = default_remaining_program;
                                }
                            }
                        }
                    }
                },
                [&](const GeneratorOverrideSeq::Entry::RuleRemoveValue &rule)
                {
                    if (!generator.ValueConvertibleFromString())
                    {
                        HardErrorInFlag(
                            CFG_TA_FMT_NAMESPACE::format(
                                "The generated type `{}` can't be deserialized from a string, so `-=` can't be used with it. "
                                "But you can filter certain generated values by their indices using `-#`, see `--help-generate` for details.",
                                generator.GetTypeName()
                            ),
                            *this_flag->entry, rule.value.data(), HardErrorKind::user
                        );
                    }
                    if (!generator.ValueEqualityComparableToString())
                    {
                        HardErrorInFlag(
                            CFG_TA_FMT_NAMESPACE::format(
                                "The generated type `{}` doesn't overload equality comparison, so `-=` can't be used with it. "
                                "But you can filter certain generated values by their indices using `-#`, see `--help-generate` for details.",
                                generator.GetTypeName()
                            ),
                            *this_flag->entry, rule.value.data(), HardErrorKind::user
                        );
                    }

                    const char *string = rule.value.data();
                    bool equal = false;
                    std::string error = generator.ValueEqualsToString(string, equal);
                    CheckValueParsingResult(error, string, rule.value.data() + rule.value.size());

                    if (equal && value_passes)
                    {
                        basic_rule.was_used = true;
                        value_passes = false;
                    }
                },
            }, basic_rule.var);
        }

        if (value_passes)
            return false; // Generate the value.

        // Otherwise try generating the next value.
    }
}

void ta_test::modules::GeneratorOverrider::OnPrePruneGenerator(const RunSingleTestProgress &test) noexcept
{
    if (test.generator_stack.back()->OverridingModule() == this)
    {
        if (!test_state)
            HardError("We're pruning our overridden generator, but have no state for some reason.");

        bool found = false;
        for (ActiveFlag &active_flag : test_state->active_flags)
        {
            if (active_flag.elems.empty() || active_flag.elems.back().generator_index != test.generator_stack.size() - 1)
                continue;
            found = true;

            active_flag.remaining_program = active_flag.elems.back().remaining_program;
            active_flag.elems.pop_back();

        }
        if (!found)
            HardError("We're pruning our overridden generator, but its index doesn't match what we have.");

    }
}

std::string ta_test::modules::GeneratorOverrider::ParseGeneratorOverrideSeq(GeneratorOverrideSeq &target, const char *&string, bool is_nested)
{
    bool first_generator = true;

    // For each generator.
    while (true)
    {
        if (first_generator)
        {
            first_generator = false;
        }
        else
        {
            if (*string == '\0' || (is_nested && *string == ')'))
                break;

            text::chars::SkipWhitespace(string);

            if (*string != ',')
                return "Expected `,`.";
            string++;

            text::chars::SkipWhitespace(string);
        }

        GeneratorOverrideSeq::Entry new_entry;

        { // Parse the name.
            if (!text::chars::IsNonDigitIdentifierCharStrict(*string))
                return "Expected a generator name.";

            const char *name_begin = string;

            do
            {
                string++;
            }
            while (text::chars::IsIdentifierCharStrict(*string));

            new_entry.generator_name = {name_begin, string};
        }

        bool is_first_rule = true;
        bool last_rule_is_positive = false;

        std::shared_ptr<GeneratorOverrideSeq> sub_override;

        auto ParseRule = [&] CFG_TA_NODISCARD_LAMBDA () -> std::string
        {
            auto TrimValue = [](std::string_view value) -> std::string_view
            {
                // We only trim the leading whitespace, because `TryFindUnprotectedSeparator()`
                // automatically rejects trailing whitespace.
                while (!value.empty() && text::chars::IsWhitespace(value.front()))
                    value.remove_prefix(1);
                return value;
            };

            auto BeginPositiveRule = [&]
            {
                if (is_first_rule)
                    new_entry.enable_values_by_default = false;
            };

            auto BeginNegativeRule = [&] CFG_TA_NODISCARD_LAMBDA () -> std::string
            {
                if (is_first_rule)
                    new_entry.enable_values_by_default = true;

                if (sub_override)
                    return "`&` can't appear before a negative rule, since those can't be followed by `(...)`.";

                return "";
            };

            auto FinishPositiveRule = [&] CFG_TA_NODISCARD_LAMBDA (std::shared_ptr<GeneratorOverrideSeq> &ptr) -> std::string
            {
                text::chars::SkipWhitespace(string);

                bool is_and = *string == '&';
                bool is_open = *string == '(';

                if (is_and || is_open)
                {
                    if (!sub_override)
                        sub_override = std::make_shared<GeneratorOverrideSeq>();
                    ptr = sub_override;
                }
                else
                {
                    if (sub_override)
                        return "Expected `&` or `(` after a list of `&`-separated rules.";
                }

                if (is_open)
                {
                    string++;
                    text::chars::SkipWhitespace(string);

                    if (*string == ')')
                    {
                        // Nothing. We allow empty `()`.
                    }
                    else
                    {
                        std::string error = ParseGeneratorOverrideSeq(*sub_override, string, true);
                        if (!error.empty())
                            return error;
                    }

                    sub_override = nullptr;

                    // No need to skip whitespace here, `ParseGeneratorOverrideSeq()` should do it for us.

                    if (*string != ')')
                        return "Expected closing `)`.";
                    string++;

                    text::chars::SkipWhitespace(string);
                }

                last_rule_is_positive = true;

                return "";
            };

            auto FinishNegativeRule = [&] CFG_TA_NODISCARD_LAMBDA () -> std::string
            {
                text::chars::SkipWhitespace(string);

                if (*string == '(')
                    return "`(...)` can't appear after negative rules.";
                if (*string == '&')
                    return "`&` can't appear after a negative rule, since those can't be followed by `(...)`.";;

                last_rule_is_positive = false;

                return "";
            };

            static constexpr std::string_view separators = ",&(";

            if (*string == '=')
            {
                BeginPositiveRule();

                GeneratorOverrideSeq::Entry::CustomValue new_value;
                new_value.operator_character = string;
                new_value.next_rule = new_entry.rules.size();

                string++;

                const char *value_begin = string;
                text::chars::TryFindUnprotectedSeparator(string, separators);
                new_value.value = TrimValue({value_begin, string});
                if (new_value.value.empty())
                    return "Expected a value.";

                std::string error = FinishPositiveRule(new_value.custom_generator_seq);
                if (!error.empty())
                    return error;
                new_entry.custom_values.push_back(std::move(new_value));
            }
            else if (*string == '-' && string[1] == '=')
            {
                std::string error = BeginNegativeRule();
                if (!error.empty())
                    return error;

                GeneratorOverrideSeq::Entry::Rule new_rule;
                new_rule.operator_character = string;

                string += 2;

                const char *value_begin = string;
                text::chars::TryFindUnprotectedSeparator(string, separators);

                auto &value = new_rule.var.emplace<GeneratorOverrideSeq::Entry::RuleRemoveValue>().value;
                value = TrimValue({value_begin, string});
                if (value.empty())
                    return "Expected a value.";

                new_entry.rules.push_back(std::move(new_rule));

                error = FinishNegativeRule();
                if (!error.empty())
                    return error;
            }
            else if (*string == '#' || (*string == '-' && string[1] == '#'))
            {
                GeneratorOverrideSeq::Entry::Rule new_rule;
                GeneratorOverrideSeq::Entry::RuleIndex &new_rule_index = new_rule.var.emplace<GeneratorOverrideSeq::Entry::RuleIndex>();

                new_rule.operator_character = string;

                new_rule_index.add = *string == '#';

                if (new_rule_index.add)
                {
                    BeginPositiveRule();
                    string++;
                }
                else
                {
                    std::string error = BeginNegativeRule();
                    if (!error.empty())
                        return error;

                    string += 2;
                }

                text::chars::SkipWhitespace(string);

                if (*string != '.' && !text::chars::IsDigit(*string))
                    return "Expected an integer or `..`.";

                bool have_first_number = *string != '.';

                auto CheckNumberLimits = [] CFG_TA_NODISCARD_LAMBDA (std::size_t n) -> std::string
                {
                    if (n < 1)
                        return "The index must be 1 or greater.";
                    if (n == std::size_t(-1))
                        return "The index must be less than the max value of `size_t`.";
                    return "";
                };

                // Parse the first number, if any.
                if (have_first_number)
                {
                    std::string error = string_conv::FromStringTraits<decltype(new_rule_index.begin)>{}(new_rule_index.begin, string);
                    if (!error.empty())
                        return error;
                    error = CheckNumberLimits(new_rule_index.begin);
                    if (!error.empty())
                        return error;
                    new_rule_index.begin--;
                }

                // Check for `..`.
                if (*string != '.' || string[1] != '.')
                {
                    // If we didn't have the first number, we must have `..`.
                    if (have_first_number)
                        new_rule_index.end = new_rule_index.begin + 1;
                    else
                        return "Expected `..`.";
                }
                else
                {
                    // Skip `..`.
                    string += 2;

                    // Parse the second number (optional if there was no first number).
                    if (!have_first_number || text::chars::IsDigit(*string))
                    {
                        new_rule_index.end_string_location = string;

                        std::string error = string_conv::FromStringTraits<decltype(new_rule_index.end)>{}(new_rule_index.end, string);
                        if (!error.empty())
                            return error;
                        error = CheckNumberLimits(new_rule_index.end);
                        if (!error.empty())
                            return error;
                        if (new_rule_index.end < new_rule_index.begin + 1)
                            return "The second index must be greater or equal to the first one.";
                    }
                }

                new_rule_index.total_num_characters = std::size_t(string - new_rule.operator_character);

                std::string error;
                if (new_rule_index.add)
                    error = FinishPositiveRule(new_rule.custom_generator_seq);
                else
                    error = FinishNegativeRule();
                if (!error.empty())
                    return error;

                new_entry.rules.push_back(std::move(new_rule));
            }
            else
            {
                return "Expected one of: `=`, `-=`, `#`, `-#`.";
            }

            is_first_rule = false;

            return "";
        };

        text::chars::SkipWhitespace(string);

        // Parse the rules.

        if (*string == '{')
        {
            string++;

            text::chars::SkipWhitespace(string);

            while (true)
            {
                if (!is_first_rule)
                {
                    if (*string == '}')
                    {
                        string++;
                        break;
                    }

                    if (last_rule_is_positive)
                    {
                        if (*string != ',' && *string != '&' && *string != '&')
                            return "Expected `,` or `&` or `(`.";
                    }
                    else
                    {
                        if (*string != ',')
                            return "Expected `,`.";
                    }
                    string++;

                    text::chars::SkipWhitespace(string);
                }

                // This skips the trailing whitespace.
                std::string error = ParseRule();
                if (!error.empty())
                    return error;
            }
        }
        else
        {
            std::string error = ParseRule();
            if (!error.empty())
                return error;
        }

        // Figure out the total generator length in characters.
        new_entry.total_num_characters = std::size_t(string - new_entry.generator_name.data());
        while (new_entry.total_num_characters > 0 && text::chars::IsWhitespace(new_entry.generator_name.data()[new_entry.total_num_characters-1]))
            new_entry.total_num_characters--;

        target.entries.push_back(std::move(new_entry));
    }

    return "";
}

void ta_test::modules::GeneratorOverrider::HardErrorInFlag(std::string_view message, const Entry &entry, FlagErrorDetails details, HardErrorKind kind)
{
    std::string markers;
    for (const FlagErrorDetails::Elem &elem : details.elems)
    {
        std::size_t offset = 2 + flag_override.flag.size() + 1 + std::size_t(elem.location - entry.original_argument_storage.data());
        std::size_t needed_size = offset + elem.marker.size();
        if (markers.size() < needed_size)
            markers.resize(needed_size, ' ');

        std::copy(elem.marker.begin(), elem.marker.end(), markers.begin() + std::ptrdiff_t(offset));
    }
    if (!details.elems.empty())
        markers += '\n';

    HardError(CFG_TA_FMT_NAMESPACE::format("In flag:\n--{} {}\n{}{}\n", flag_override.flag, entry.OriginalArgument(), markers, message), kind);
}

// --- modules::PrintingConfigurator ---

ta_test::modules::PrintingConfigurator::PrintingConfigurator()
    : flag_color("color", "Color output using ANSI escape sequences (by default enabled when printing to terminal).",
        [](const Runner &runner, BasicModule &this_module, bool enable)
        {
            (void)this_module;
            runner.SetEnableColor(enable);
        }
    ), flag_unicode("unicode", "Use Unicode characters for pseudographics (enabled by default).",
        [](const Runner &runner, BasicModule &this_module, bool enable)
        {
            (void)this_module;
            runner.SetEnableUnicode(enable);
        }
    )
{}

std::vector<ta_test::flags::BasicFlag *> ta_test::modules::PrintingConfigurator::GetFlags() noexcept
{
    return {&flag_color, &flag_unicode};
}

// --- modules::ProgressPrinter ---

ta_test::modules::ProgressPrinter::GeneratorValueShortener::GeneratorValueShortener(
    std::string_view value, std::string_view ellipsis, std::size_t max_prefix, std::size_t max_suffix
)
{
    // The number of unicode characters in `ellipsis`. This is initialized lazily.
    std::size_t ellipsis_size = 0;

    // Where the prefix ends, if the string is longer than `max_prefix`.
    const char *prefix_end = nullptr;

    // `prefix_end` plus the size of the `ellipsis`, if the string is longer than that.
    const char *imaginary_ellipsis_end = nullptr;

    // See if we're at least longer than `max_prefix` plus the size of the ellipsis.
    std::size_t index = 0;
    for (const char &ch : value)
    {
        if (text::uni::IsFirstByte(ch))
        {
            if (index == max_prefix)
            {
                ellipsis_size = text::uni::CountFirstBytes(ellipsis);
                prefix_end = &ch;
            }
            else if (index == max_prefix + ellipsis_size)
            {
                imaginary_ellipsis_end = &ch;
                break;
            }

            index++;
        }
    }

    // Maybe we're long, iterate backwards to find the suffix start and whether we even have enough characters for a suffix.
    if (imaginary_ellipsis_end)
    {
        const char *cur = value.data() + value.size();

        index = 0;
        while (cur != imaginary_ellipsis_end)
        {
            cur--;
            index++;
            if (index == max_suffix)
            {
                is_short = false;
                long_prefix = {value.data(), prefix_end};
                long_suffix = {cur, value.data() + value.size()};
                return;
            }
        }
    }

    is_short = true;
}

void ta_test::modules::ProgressPrinter::PrintContextLinePrefix(
    output::Terminal::StyleGuard &cur_style,
    const RunTestsProgress &all_tests,
    TestCounterStyle test_counter_style
) const
{
    // Test index, if necessary.
    if (test_counter_style != TestCounterStyle::none)
    {
        terminal.Print(cur_style, "{}{:{}}{}/{}",
            test_counter_style == TestCounterStyle::repeated ? style_index_repeated : style_index,
            state.test_counter + 1, state.num_tests_width,
            style_total_count,
            all_tests.num_tests
        );

        // Failed test count.
        if (all_tests.failed_tests.size() > 0)
        {
            terminal.Print(cur_style, "{}{}{}{}{}{}",
                style_failed_count_decorations,
                chars_failed_test_count_prefix,
                style_failed_count,
                all_tests.failed_tests.size(),
                style_failed_count_decorations,
                chars_failed_test_count_suffix
            );
        }
    }
    else
    {
        // No test index, just a gap.

        std::size_t gap_width = state.num_tests_width * 2 + 1;

        if (all_tests.failed_tests.size() > 0)
            gap_width += CFG_TA_FMT_NAMESPACE::formatted_size("{}", all_tests.failed_tests.size()) + 3;

        terminal.Print("{:{}}", "", gap_width);
    }

    // The gutter border.
    terminal.Print(cur_style, "{}{}",
        style_gutter_border,
        chars_test_counter_separator
    );
}

void ta_test::modules::ProgressPrinter::PrintContextLineIndentation(output::Terminal::StyleGuard &cur_style, std::size_t depth, std::size_t skip_characters) const
{
    // Indentation prefix.
    terminal.Print(cur_style, "{}{}",
        style_indentation_guide,
        chars_pre_indentation
    );

    std::size_t single_indentation_width = text::uni::CountFirstBytes(chars_indentation);

    if (skip_characters > single_indentation_width * depth)
        return; // Everything is skipped.

    depth -= (skip_characters + single_indentation_width - 1) / single_indentation_width;

    // Print the first incomplete indentation guide, if partially skipped.
    if (std::size_t skipped_part = skip_characters % single_indentation_width; skipped_part > 0)
    {
        std::size_t i = 0;
        for (const char &ch : chars_indentation)
        {
            if (text::uni::IsFirstByte(ch))
                i++;

            if (i > skipped_part)
            {
                terminal.Print("{}", &ch);
                break;
            }
        }
    }

    // The indentation.
    for (std::size_t i = 0; i < depth; i++)
        terminal.Print("{}", chars_indentation);
}

void ta_test::modules::ProgressPrinter::PrintGeneratorInfo(output::Terminal::StyleGuard &cur_style, const RunSingleTestProgress &test, const BasicGenerator &generator, bool repeating_info)
{
    PrintContextLinePrefix(cur_style, *test.all_tests, state.per_test.per_repetition.printed_counter ? TestCounterStyle::none : TestCounterStyle::repeated);

    std::size_t repetition_counters_width = CFG_TA_FMT_NAMESPACE::formatted_size("{}", state.per_test.repetition_counter + 1);
    if (state.per_test.failed_generator_stacks.size() > 0)
    {
        repetition_counters_width += text::uni::CountFirstBytes(chars_failed_repetition_count_prefix);
        repetition_counters_width += CFG_TA_FMT_NAMESPACE::formatted_size("{}", state.per_test.failed_generator_stacks.size());
        repetition_counters_width += text::uni::CountFirstBytes(chars_failed_repetition_count_suffix);
    }

    // Print the repetition counter, or nothing if already printed during this repetition.
    if (!state.per_test.per_repetition.printed_counter)
    {
        // The actual repetition count.

        state.per_test.per_repetition.printed_counter = true;

        terminal.Print(cur_style, "{}{}",
            style_repetition_total_count,
            state.per_test.repetition_counter + 1
        );

        // Failed repetition count, if any.
        if (!state.per_test.failed_generator_stacks.empty())
        {
            terminal.Print(cur_style, "{}{}{}{}{}{}",
                style_repetition_failed_count_decorations,
                chars_failed_repetition_count_prefix,
                style_repetition_failed_count,
                state.per_test.failed_generator_stacks.size(),
                style_repetition_failed_count_decorations,
                chars_failed_repetition_count_suffix
            );
        }
    }
    else
    {
        // Empty spacing.
        terminal.Print("{:{}}", "", repetition_counters_width);
    }

    // The ending border after the repetition counter.
    const std::string &repetition_border_string =
        repetition_counters_width <= state.per_test.last_repetition_counters_width
        ? chars_repetition_counter_separator
        : chars_repetition_counter_separator_diagonal;
    terminal.Print(cur_style, "{}{}",
        style_repetition_border,
        repetition_border_string
    );
    state.per_test.last_repetition_counters_width = repetition_counters_width;

    // Indentation.
    std::size_t num_chars_removed_from_indentation = std::min(std::size_t(repetition_counters_width) + text::uni::CountFirstBytes(repetition_border_string), text::uni::CountFirstBytes(chars_indentation) * state.stack.size());
    PrintContextLineIndentation(cur_style, state.stack.size() + test.generator_index, num_chars_removed_from_indentation);

    const auto &st_gen = repeating_info ? style_generator_repeated : style_generator;

    // The generator name and the value index.
    terminal.Print(cur_style, "{}{}{}{}{}{}{}{}{}{}{}",
        // The bullet point.
        st_gen.prefix,
        repeating_info ? chars_test_prefix_continuing : chars_test_prefix,
        // Generator name.
        st_gen.name,
        generator.GetName(),
        // Opening bracket.
        st_gen.index_brackets,
        chars_generator_index_prefix,
        // Index:
        //   Index style.
        generator.IsCustomValue() ? st_gen.index_custom : st_gen.index,
        //   Prefix for custom values, if any.
        generator.IsCustomValue() ? chars_generator_custom_index_prefix : "",
        //   The index itself.
        generator.IsCustomValue() ? generator.NumCustomValues() : generator.NumGeneratedValues(),
        // Closing bracket.
        st_gen.index_brackets,
        chars_generator_index_suffix
    );

    // The value as a string.
    if (generator.ValueConvertibleToString())
    {
        // The equals sign.
        terminal.Print(cur_style, "{}{}{}",
            st_gen.value_separator,
            chars_generator_value_separator,
            st_gen.value
        );

        // The value itself.

        std::string value = generator.ValueToString();
        GeneratorValueShortener shortener(value, chars_generator_value_ellipsis, max_generator_value_prefix_length, max_generator_value_suffix_length);

        if (shortener.is_short)
        {
            // The value is short enough to be printed completely.
            terminal.Print("{}", value);
        }
        else
        {
            // The value is too long.
            terminal.Print(cur_style, "{}{}{}{}{}",
                shortener.long_prefix,
                st_gen.value_ellipsis,
                chars_generator_value_ellipsis,
                st_gen.value,
                shortener.long_suffix
            );
        }
    }

    terminal.Print("\n");
}

std::string ta_test::modules::ProgressPrinter::MakeGeneratorSummary(const RunSingleTestProgress &test) const
{
    std::string ret;

    for (std::size_t i = 0; i < test.generator_index; i++)
    {
        const BasicGenerator &gen = *test.generator_stack[i];

        // Print the value as a string.
        if (gen.ValueConvertibleToString() && gen.ValueConvertibleFromString())
        {
            std::string value = gen.ValueToString();

            if (value.size() <= max_generator_summary_value_length)
            {
                // Check roundtrip string conversion.
                bool roundtrip_ok = false;
                {
                    const char *string = value.c_str();
                    std::string error = gen.ValueEqualsToString(string, roundtrip_ok);
                    if (!error.empty() || string != value.data() + value.size())
                        roundtrip_ok = false;
                }

                if (roundtrip_ok)
                {
                    if (!ret.empty())
                        ret += ',';

                    ret += gen.GetName();
                    ret += '=';
                    ret += value;
                    continue;
                }
            }
        }

        // Print the value index.
        if (!gen.IsCustomValue())
        {
            if (!ret.empty())
                ret += ',';

            ret += gen.GetName();
            ret += '#';
            ret += std::to_string(gen.NumGeneratedValues());
            continue;
        }

        { // Fall back to a stub.
            ret = "...";
            return ret;
        }
    }

    return ret;
}

ta_test::modules::ProgressPrinter::ProgressPrinter()
    : flag_progress("progress", "Print test names before running them (enabled by default).",
        [](const Runner &runner, BasicModule &this_module, bool enable)
        {
            (void)runner;

            // The cast should never fail.
            auto &self = dynamic_cast<ProgressPrinter &>(this_module);
            self.show_progress = enable;
        }
    )
{
    EnableUnicode(true);
}

std::vector<ta_test::flags::BasicFlag *> ta_test::modules::ProgressPrinter::GetFlags() noexcept
{
    return {&flag_progress};
}

void ta_test::modules::ProgressPrinter::EnableUnicode(bool enable)
{
    BasicPrintingModule::EnableUnicode(enable);

    if (enable)
    {
        chars_test_prefix = "\xE2\x97\x8F "; // BLACK CIRCLE, then a space.
        chars_test_prefix_continuing = "\xE2\x97\x8B "; // WHITE CIRCLE, then a space.
        chars_indentation = "\xC2\xB7   "; // MIDDLE DOT, then a space.
        chars_test_counter_separator = " \xE2\x94\x82 "; // BOX DRAWINGS LIGHT VERTICAL, with spaces around it.
        chars_repetition_counter_separator = " \xE2\x94\x82"; // BOX DRAWINGS LIGHT VERTICAL, with a space to the left.
        chars_repetition_counter_separator_diagonal = "\xE2\x95\xB0\xE2\x95\xAE"; // BOX DRAWINGS LIGHT ARC UP AND RIGHT, then BOX DRAWINGS LIGHT ARC DOWN AND LEFT
        chars_test_failed_separator = "\xE2\x94\x81"; // BOX DRAWINGS HEAVY HORIZONTAL
        chars_test_failed_ending_separator = "\xE2\x94\x80"; // BOX DRAWINGS LIGHT HORIZONTAL
        chars_summary_path_separator = "      \xE2\x94\x82 "; // BOX DRAWINGS LIGHT VERTICAL, with some spaces around it.
    }
    else
    {
        chars_test_prefix = "* ";
        chars_test_prefix_continuing = "+ ";
        chars_indentation = "    ";
        chars_test_counter_separator = " | ";
        chars_repetition_counter_separator = " |";
        chars_repetition_counter_separator_diagonal = " \\";
        chars_test_failed_separator = "#";
        chars_test_failed_ending_separator = "-";
        chars_summary_path_separator = "      | ";
    }
}

void ta_test::modules::ProgressPrinter::OnPreRunTests(const RunTestsInfo &data) noexcept
{
    state = {};

    state.num_tests_width = CFG_TA_FMT_NAMESPACE::formatted_size("{}", data.num_tests);

    // Print a message when some tests are skipped.
    if (data.num_tests < data.num_tests_with_skipped)
    {
        std::size_t num_skipped = data.num_tests_with_skipped - data.num_tests;

        auto cur_style = terminal.MakeStyleGuard();

        terminal.Print(cur_style, "{}Skipping {} test{}, will run {}/{} test{}.\n",
            style_skipped_tests,
            num_skipped,
            num_skipped != 1 ? "s" : "",
            data.num_tests,
            data.num_tests_with_skipped,
            data.num_tests_with_skipped != 1 ? "s" : ""
        );
    }
}

void ta_test::modules::ProgressPrinter::OnPostRunTests(const RunTestsResults &data) noexcept
{
    if (!data.failed_tests.empty())
    {
        auto cur_style = terminal.MakeStyleGuard();
        terminal.Print(cur_style, "\n{}{}\n\n",
            common_data.style_error,
            chars_summary_tests_failed
        );

        std::size_t max_test_name_width = 0;
        std::size_t indentation_width = text::uni::CountFirstBytes(chars_indentation);
        std::size_t prefix_width = text::uni::CountFirstBytes(chars_test_prefix);

        // Determine how much space to leave for the test name tree.
        for (const BasicTestInfo *test : data.failed_tests)
        {
            std::string_view test_name = test->Name();

            std::size_t cur_prefix_width = prefix_width;

            SplitNameToSegments(test_name, [&](std::string_view segment, bool is_last_segment)
            {
                std::size_t this_width = cur_prefix_width + (is_last_segment ? 0 : 1/*for the slash*/) + segment.size();
                if (this_width > max_test_name_width)
                    max_test_name_width = this_width;

                cur_prefix_width += indentation_width;
            });
        }

        std::vector<std::string_view> stack;
        for (const BasicTestInfo *test : data.failed_tests)
        {
            ProduceTree(stack, test->Name(), [&](std::size_t segment_index, std::string_view segment, bool is_last_segment)
            {
                (void)segment_index;

                // The indentation.
                if (!stack.empty())
                {
                    // Switch to the indentation guide color.
                    terminal.Print("{}", terminal.AnsiDeltaString(cur_style, style_indentation_guide).data());
                    // Print the required number of guides.
                    for (std::size_t repeat = 0; repeat < stack.size(); repeat++)
                        terminal.Print("{}", chars_indentation);
                }

                std::size_t gap_to_separator = max_test_name_width - (stack.size() * indentation_width + prefix_width + (is_last_segment ? 0 : 1/*for the slash*/) + segment.size());

                // Print the test name.
                terminal.Print(cur_style, "{}{}{}{}{:{}}{}{}",
                    is_last_segment ? style_summary_failed_name : style_summary_failed_group_name,
                    chars_test_prefix,
                    segment,
                    is_last_segment ? "" : "/",
                    "", int(gap_to_separator),
                    style_summary_path_separator,
                    chars_summary_path_separator
                );

                // Print the file path for the last segment.
                if (is_last_segment)
                {
                    terminal.Print(cur_style, "{}{}",
                        style_summary_path,
                        common_data.LocationToString(test->Location())
                    );
                }

                terminal.Print("\n");
            });
        }
    }

    state = {};
}

void ta_test::modules::ProgressPrinter::OnPreRunSingleTest(const RunSingleTestInfo &data) noexcept
{
    // Reset the state.
    if (data.is_first_generator_repetition)
    {
        bool prev_test_failed = state.per_test.prev_failed;
        state.per_test = {};
        state.per_test.prev_failed = prev_test_failed;
    }
    else
    {
        state.per_test.per_repetition = {};
    }

    state.per_test.per_repetition.prev_rep_failed = !state.failed_test_stack.empty();

    if (show_progress)
    {
        auto cur_style = terminal.MakeStyleGuard();

        { // Print the message when first starting tests, or when resuming from a failure.
            bool is_first_test_first_repetition = state.test_counter == 0 && state.per_test.repetition_counter == 0;

            if (is_first_test_first_repetition || state.per_test.prev_failed || state.per_test.per_repetition.prev_rep_failed)
            {
                terminal.Print(cur_style, "\n{}{}\n",
                    is_first_test_first_repetition ? style_starting_tests : style_continuing_tests,
                    is_first_test_first_repetition ? chars_starting_tests : chars_continuing_tests
                );
            }
        }

        // Print the current test name (and groups, if any).
        ProduceTree(state.stack, data.test->Name(), [&](std::size_t segment_index, std::string_view segment, bool is_last_segment)
        {

            // Whether we're reentering this group after a failed test.
            bool is_continued = segment_index < state.failed_test_stack.size() && state.failed_test_stack[segment_index] == segment;

            PrintContextLinePrefix(cur_style, *data.all_tests,
                !is_last_segment ? TestCounterStyle::none :
                is_continued     ? TestCounterStyle::repeated :
                                   TestCounterStyle::normal
            );
            PrintContextLineIndentation(cur_style, state.stack.size(), 0);

            // Print the test name.
            terminal.Print(cur_style, "{}{}{}{}{}\n",
                is_continued ? style_prefix_continuing : style_prefix,
                is_continued ? chars_test_prefix_continuing : chars_test_prefix,
                is_continued ? style_continuing_group : is_last_segment ? style_name : style_group_name,
                segment,
                is_last_segment ? "" : "/"
            );
        });
    }
}

void ta_test::modules::ProgressPrinter::OnPostRunSingleTest(const RunSingleTestResults &data) noexcept
{
    auto cur_style = terminal.MakeStyleGuard();

    // Print the ending separator.
    if (data.failed)
    {
        std::size_t separator_segment_width = text::uni::CountFirstBytes(chars_test_failed_ending_separator);
        std::string separator;
        for (std::size_t i = 0; i + separator_segment_width - 1 < separator_line_width; i += separator_segment_width)
            separator += chars_test_failed_ending_separator;

        // The test failure message, and a separator after that.
        terminal.Print(cur_style, "{}{}\n",
            style_test_failed_ending_separator,
            separator
        );
    }

    // Adjust the group name stack to show the group names again after a failed test.
    if (data.failed)
    {
        state.failed_test_stack = std::move(state.stack);
        state.stack.clear();
    }
    else
    {
        state.failed_test_stack.clear();
    }

    state.per_test.repetition_counter++;

    // Print failed repetitions summary.
    // Note the weird `!<..>.front().empty()` check. This avoid the message when there was only one repetition without any generators visited.
    if (data.is_last_generator_repetition && !state.per_test.failed_generator_stacks.empty() && !state.per_test.failed_generator_stacks.front().empty())
    {
        std::string_view test_group;
        std::string_view test_name = data.test->Name();
        auto sep = test_name.find_last_of('/');
        if (sep != std::string_view::npos)
        {
            test_group = test_name.substr(0, sep + 1);
            test_name = test_name.substr(sep + 1);
        }

        // Intentionally always using plural "VARIANTS" here, because seeing "1/N VARIANT" is confusing, because it looks kinda like "first variant".
        terminal.Print(cur_style, "\n{}{}:\n{}IN TEST {}{}{}{}{}, {}{}{}/{}{} VARIANTS FAILED:\n\n",
            common_data.style_path,
            common_data.LocationToString(data.test->Location()),
            common_data.style_error,
            style_failed_group_name,
            test_group,
            style_failed_name,
            test_name,
            common_data.style_error,
            style_repetitions_summary_failed_count,
            state.per_test.failed_generator_stacks.size(),
            style_repetitions_summary_total_count,
            state.per_test.repetition_counter,
            common_data.style_error
        );

        std::vector<const State::PerTest::FailedGenerator *> cur_stack;

        for (const auto &failed_stack : state.per_test.failed_generator_stacks)
        {
            for (std::size_t i = 0; i < failed_stack.size(); i++)
            {
                const auto &elem = failed_stack[i];

                if (i < cur_stack.size() && *cur_stack[i] == elem)
                    continue;
                cur_stack.resize(i);
                cur_stack.push_back(&elem);

                // Indent.
                for (std::size_t j = 0; j < i; j++)
                    terminal.Print(cur_style, "{}{}", style_indentation_guide, chars_indentation);

                // Generator name and index.
                terminal.Print(cur_style, "{}{}{}{}{}{}{}{}{}{}{}",
                    style_generator_failed.prefix,
                    chars_test_prefix,
                    style_generator_failed.name,
                    elem.name,
                    style_generator_failed.index_brackets,
                    chars_generator_index_prefix,
                    elem.is_custom_value ? style_generator_failed.index_custom : style_generator_failed.index,
                    elem.is_custom_value ? chars_generator_custom_index_prefix : "",
                    elem.index,
                    style_generator_failed.index_brackets,
                    chars_generator_index_suffix
                );

                // Generator value, if any.
                if (elem.value)
                {
                    // Equals sign.
                    terminal.Print(cur_style, "{}{}{}",
                        style_generator_failed.value_separator,
                        chars_generator_value_separator,
                        style_generator_failed.value
                    );

                    GeneratorValueShortener shortener(*elem.value, chars_generator_value_ellipsis, max_generator_value_prefix_length, max_generator_value_suffix_length);

                    // The value, possibly shortened.
                    if (shortener.is_short)
                    {
                        terminal.Print("{}", *elem.value);
                    }
                    else
                    {
                        terminal.Print(cur_style, "{}{}{}{}{}",
                            shortener.long_prefix,
                            style_generator_failed.value_ellipsis,
                            chars_generator_value_ellipsis,
                            style_generator_failed.value,
                            shortener.long_suffix
                        );
                    }
                }

                terminal.Print("\n");
            }
        }
    }

    // Reset per-test state after the last repetition.
    // We also do this in `OnPreRunSingleTest()`, so this isn't strictly necessary.
    if (data.is_last_generator_repetition)
    {
        state.test_counter++;

        bool prev_test_failed = !state.per_test.failed_generator_stacks.empty();
        state.per_test = {};
        state.per_test.prev_failed = prev_test_failed;
    }
    else
    {
        state.per_test.per_repetition = {};
    }
}

void ta_test::modules::ProgressPrinter::OnPostGenerate(const GeneratorCallInfo &data) noexcept
{
    if (show_progress)
    {
        auto cur_style = terminal.MakeStyleGuard();
        if (data.generating_new_value || state.per_test.per_repetition.prev_rep_failed)
            PrintGeneratorInfo(cur_style, *data.test, *data.generator, !data.generating_new_value);
    }
}

void ta_test::modules::ProgressPrinter::OnPreFailTest(const RunSingleTestProgress &data) noexcept
{
    // Remember the failed generator stack.
    std::vector<State::PerTest::FailedGenerator> failed_generator_stack;
    failed_generator_stack.reserve(data.generator_stack.size());
    for (const auto &gen : data.generator_stack)
    {
        failed_generator_stack.push_back({
            .name = std::string(gen->GetName()),
            .index = gen->IsCustomValue() ? gen->NumCustomValues() : gen->NumGeneratedValues(),
            .is_custom_value = gen->IsCustomValue(),
            .value = gen->ValueConvertibleToString() ? std::optional(gen->ValueToString()) : std::nullopt,
            .location = gen->GetLocation(),
        });
    }
    state.per_test.failed_generator_stacks.push_back(std::move(failed_generator_stack));


    // Avoid printing the diagnoal separator on the next progress line. It's unnecessary after a bulky failure message.
    state.per_test.last_repetition_counters_width = std::size_t(-1);

    // Find group and test name.
    std::string_view group;
    std::string_view name = data.test->Name();
    auto sep = name.find_last_of('/');
    if (sep != std::string_view::npos)
    {
        group = name.substr(0, sep + 1);
        name = name.substr(sep + 1);
    }

    std::string generator_summary = MakeGeneratorSummary(data);

    std::size_t separator_segment_width = text::uni::CountFirstBytes(chars_test_failed_separator);
    std::size_t separator_needed_width =
        separator_line_width
        - text::uni::CountFirstBytes(chars_test_failed)
        - data.test->Name().size()
        - generator_summary.size() - 2 * !generator_summary.empty()
        - 1/*space before separator*/;
    std::string separator;
    for (std::size_t i = 0; i + separator_segment_width - 1 < separator_needed_width; i += separator_segment_width)
        separator += chars_test_failed_separator;

    auto cur_style = terminal.MakeStyleGuard();

    // The test failure message, and a separator after that.
    terminal.Print(cur_style, "\n{}{}:\n{}{}{}{}{}{}{}{}{} {}{}\n",
        common_data.style_path,
        common_data.LocationToString(data.test->Location()),
        common_data.style_error,
        chars_test_failed,
        style_failed_group_name,
        group,
        style_failed_name,
        name,
        style_failed_generator_summary,
        !generator_summary.empty() ? "//" : "",
        generator_summary,
        style_test_failed_separator,
        separator
    );

    // Print the generator determinism warning, if needed.
    if (data.generator_index < data.generator_stack.size() &&
        // And if not all remaining generators failed due to exceptions...
        // We still want to keep the previous condition, even if it looks redundant, in case `data.generator_index` somehow ends up larger than the stack size.
        !std::all_of(data.generator_stack.begin() + std::ptrdiff_t(data.generator_index), data.generator_stack.end(), [](const auto &g){return g->CallbackThrewException();})
    )
    {
        PrintWarning(cur_style, "Non-deterministic failure. Previous runs didn't fail here with the same generated values. Some generators will be pruned.");
    }

    terminal.Print("\n");
}

// --- modules::ResultsPrinter ---

void ta_test::modules::ResultsPrinter::OnPostRunTests(const RunTestsResults &data) noexcept
{
    auto cur_style = terminal.MakeStyleGuard();

    terminal.Print("\n");

    std::size_t num_tests_skipped = data.num_tests_with_skipped - data.num_tests;
    std::size_t num_tests_passed = data.num_tests - data.failed_tests.size();
    std::size_t num_tests_failed = data.failed_tests.size();

    std::size_t num_reps_passed = data.num_tests_with_repetitions_total - data.num_tests_with_repetitions_failed;
    std::size_t num_reps_failed = data.num_tests_with_repetitions_failed;
    bool print_reps = data.num_tests_with_repetitions_total > data.num_tests;

    std::size_t num_checks_passed = data.num_checks_total - data.num_checks_failed;
    std::size_t num_checks_failed = data.num_checks_failed;

    if (num_tests_skipped == 0 && num_tests_passed == 0 && num_tests_failed == 0)
    {
        terminal.Print(cur_style, "{}{}", style_skipped_primary, chars_no_known_tests);
    }
    else
    {
        auto RowHeader = [&](const output::TextStyle &style, std::string_view value)
        {
            terminal.Print(cur_style, "{}{:<{}}", style, value, leftmost_column_width);
        };
        auto Cell = [&]<typename T>(const T &value)
        {
            bool gray_out = false;
            if constexpr (std::is_integral_v<T>)
            {
                if (value == 0)
                    gray_out = true;
            }

            auto old_style = cur_style.GetCurrentStyle(); // Need to back up the old style.
            auto new_style = gray_out ? style_zero : old_style;

            terminal.Print(cur_style, "{} {:>{}}{}", new_style, value, column_width - 1, old_style);
        };

        // The header.
        RowHeader(style_table_header, "");
        Cell(chars_col_tests);
        if (print_reps)
            Cell(chars_col_repetitions);
        Cell(chars_col_checks);
        terminal.Print("\n");

        // Num skipped.
        if (num_tests_skipped > 0)
        {
            // Num known total.
            if (num_tests_passed > 0 || num_tests_failed > 0)
            {
                RowHeader(style_total, chars_total_known);
                Cell(num_tests_skipped + num_tests_passed + num_tests_failed);
                terminal.Print("\n");
            }

            bool is_primary = num_tests_passed == 0 && num_tests_failed == 0;
            RowHeader(is_primary ? style_skipped_primary : style_skipped, is_primary ? chars_skipped_primary : chars_skipped);
            Cell(num_tests_skipped);
            terminal.Print("\n");
        }

        // Num total.
        if ((num_tests_passed > 0 && num_tests_failed > 0) || (num_checks_passed > 0 && num_checks_failed > 0))
        {
            RowHeader(style_total, chars_total_executed);
            Cell(num_tests_passed + num_tests_failed);
            if (print_reps)
                Cell(num_reps_passed + num_reps_failed);
            Cell(num_checks_passed + num_checks_failed);
            terminal.Print("\n");
        }

        // Num passed.
        if (num_tests_passed > 0 || num_checks_passed > 0)
        {
            bool is_primary = num_tests_failed == 0;
            RowHeader(is_primary ? style_passed_primary : style_passed, is_primary ? chars_passed_primary : chars_passed);
            Cell(num_tests_passed);
            if (print_reps)
                Cell(num_reps_passed);
            Cell(num_checks_passed);
            terminal.Print("\n");
        }

        // Num failed.
        if (num_tests_failed > 0)
        {
            RowHeader(style_failed_primary, chars_failed_primary);
            Cell(num_tests_failed);
            if (print_reps)
                Cell(num_reps_failed);
            Cell(num_checks_failed);
            terminal.Print("\n");
        }
    }

    terminal.Print("\n");
}

// --- modules::AssertionPrinter ---

void ta_test::modules::AssertionPrinter::OnAssertionFailed(const BasicAssertionInfo &data) noexcept
{
    auto cur_style = terminal.MakeStyleGuard();
    output::PrintLog(cur_style);
    PrintAssertionFrameLow(cur_style, data, true);
    output::PrintContext(cur_style, &data);
}

bool ta_test::modules::AssertionPrinter::PrintContextFrame(output::Terminal::StyleGuard &cur_style, const context::BasicFrame &frame) noexcept
{
    if (auto assertion_frame = dynamic_cast<const BasicAssertionInfo *>(&frame))
    {
        PrintAssertionFrameLow(cur_style, *assertion_frame, false);
        return true;
    }

    return false;
}

void ta_test::modules::AssertionPrinter::PrintAssertionFrameLow(output::Terminal::StyleGuard &cur_style, const BasicAssertionInfo &data, bool is_most_nested) const
{
    output::TextCanvas canvas(&common_data);
    std::size_t line_counter = 0;

    // The file path.
    canvas.DrawString(line_counter++, 0, common_data.LocationToString(data.SourceLocation()) + ":", {.style = common_data.style_path, .important = true});

    bool has_elems = !std::holds_alternative<std::monostate>(data.GetElement(0));

    { // The main error message.
        std::size_t column = 0;
        if (is_most_nested)
        {
            column += canvas.DrawString(line_counter, column, has_elems ? chars_assertion_failed : chars_assertion_failed_no_cond, {.style = common_data.style_error, .important = true});

            // Add a `:` or `.`.
            column += canvas.DrawString(line_counter, column, has_elems || data.GetUserMessage() ? ":" : ".", {.style = common_data.style_error, .important = true});
        }
        else
        {
            column += canvas.DrawString(line_counter, column, chars_in_assertion, {.style = common_data.style_stack_frame, .important = true});
        }

        if (auto message = data.GetUserMessage())
            canvas.DrawString(line_counter, column + 1, *message, {.style = common_data.style_user_message, .important = true});

        line_counter++;
    }

    line_counter++;

    // This is set later if we actually have an expression to print.
    const BasicModule::BasicAssertionExpr *expr = nullptr;
    std::size_t expr_line = line_counter;
    std::size_t expr_column = 0; // This is also set later.

    // The assertion call.
    if (has_elems)
    {
        std::size_t column = common_data.code_indentation;

        const output::TextCanvas::CellInfo assertion_macro_cell_info = {.style = common_data.style_failed_macro, .important = true};

        for (int i = 0;; i++)
        {
            auto var = data.GetElement(i);
            if (std::holds_alternative<std::monostate>(var))
                break;

            std::visit(meta::Overload{
                [&](std::monostate) {},
                [&](const BasicAssertionInfo::DecoFixedString &deco)
                {
                    column += canvas.DrawString(line_counter, column, deco.string, assertion_macro_cell_info);
                },
                [&](const BasicAssertionInfo::DecoExpr &deco)
                {
                    column += output::expr::DrawToCanvas(canvas, line_counter, column, deco.string);
                },
                [&](const BasicAssertionInfo::DecoExprWithArgs &deco)
                {
                    column += common_data.spaces_in_macro_call_parentheses;
                    expr = deco.expr;
                    expr_column = column;
                    column += output::expr::DrawToCanvas(canvas, line_counter, column, deco.expr->Expr());
                    column += common_data.spaces_in_macro_call_parentheses;
                },
            }, var);
        }

        line_counter++;
    }

    // The expression.
    if (expr && decompose_expression)
    {
        std::u32string this_value;

        // The bracket above the expression.
        std::size_t overline_start = 0;
        std::size_t overline_end = 0;
        // How many subexpressions want an overline.
        // This should be more than one only for nested `$[...]`, or if something really weird is going on.
        int num_overline_parts = 0;
        // This is set to true if `num_overline_parts` is larger than one, but the brackets are somehow not nested.
        bool overline_is_weird = false;

        // Incremented when we print an argument.
        std::size_t color_index = 0;

        for (std::size_t i = 0; i < expr->NumArgs(); i++)
        {
            const std::size_t arg_index = expr->ArgsInDrawOrder()[i];
            BasicAssertionExpr::ArgState this_state = expr->CurrentArgState(arg_index);
            const BasicAssertionExpr::ArgInfo &this_info = expr->ArgsInfo()[arg_index];

            bool dim_parentheses = true;

            if (this_state == BasicAssertionExpr::ArgState::in_progress)
            {
                if (num_overline_parts == 0)
                {
                    overline_start = this_info.expr_offset;
                    overline_end = this_info.expr_offset + this_info.expr_size;
                }
                else
                {
                    overline_start = std::max(overline_start, this_info.expr_offset);
                    overline_end = std::min(overline_end, this_info.expr_offset + this_info.expr_size);

                    // This shouldn't happen. This means that the two `$[...]` aren't nested in one another.
                    if (overline_end <= overline_start)
                    {
                        overline_is_weird = true;
                        overline_end = overline_start + 1;
                    }
                }
                num_overline_parts++;
            }

            if (this_state == BasicAssertionExpr::ArgState::done)
            {
                text::uni::Decode(expr->CurrentArgValue(arg_index), this_value);

                std::size_t center_x = expr_column + this_info.expr_offset + (this_info.expr_size + 1) / 2 - 1;
                std::size_t value_x = center_x - (this_value.size() + 1) / 2 + 1;
                // Make sure `value_x` didn't underflow.
                if (value_x > std::size_t(-1) / 2)
                    value_x = 0;

                const output::TextCanvas::CellInfo this_cell_info = {.style = style_arguments[color_index++ % style_arguments.size()], .important = true};

                if (!this_info.need_bracket)
                {
                    std::size_t value_y = canvas.FindFreeSpace(line_counter, value_x, 2, this_value.size(), 1, 2) + 1;
                    canvas.DrawString(value_y, value_x, this_value, this_cell_info);
                    canvas.DrawColumn(common_data.bar, line_counter, center_x, value_y - line_counter, true, this_cell_info);

                    // Color the contents.
                    for (std::size_t i = 0; i < this_info.expr_size; i++)
                    {
                        output::TextStyle &style = canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset + i).style;
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
                    canvas.DrawString(value_y, value_x, this_value, this_cell_info);

                    // Add the tail to the bracket.
                    if (center_x > bracket_left_x && center_x + 1 < bracket_right_x)
                        canvas.CharAt(bracket_y, center_x) = common_data.bracket_bottom_tail;

                    // Draw the column connecting us to the text, if it's not directly below.
                    canvas.DrawColumn(common_data.bar, bracket_y + 1, center_x, value_y - bracket_y - 1, true, this_cell_info);

                    // Color the parentheses with the argument color.
                    dim_parentheses = false;
                    canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset - 1).style.color = this_cell_info.style.color;
                    canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset + this_info.expr_size).style.color = this_cell_info.style.color;
                }
            }

            // Dim the macro name.
            for (std::size_t i = 0; i < this_info.ident_size; i++)
                canvas.CellInfoAt(line_counter - 1, expr_column + this_info.ident_offset + i).style = style_dim;

            // Dim the parentheses.
            if (dim_parentheses)
            {
                canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset - 1).style = style_dim;
                canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset + this_info.expr_size).style = style_dim;
            }
        }

        // The overline.
        if (num_overline_parts > 0)
        {
            if (overline_start > 0)
                overline_start--;
            overline_end++;

            std::u32string_view this_value = overline_is_weird ? chars_in_this_subexpr_weird : chars_in_this_subexpr;

            std::size_t center_x = expr_column + overline_start + (overline_end - overline_start) / 2;
            std::size_t value_x = center_x - this_value.size() / 2;

            canvas.InsertLineBefore(expr_line++);

            canvas.DrawOverline(expr_line - 1, expr_column + overline_start, overline_end - overline_start, {.style = style_overline, .important = true});
            canvas.DrawString(expr_line - 2, value_x, this_value, {.style = style_overline, .important = true});

            // Color the parentheses.
            canvas.CellInfoAt(expr_line, expr_column + overline_start).style.color = style_overline.color;
            canvas.CellInfoAt(expr_line, expr_column + overline_end - 1).style.color = style_overline.color;
        }
    }

    canvas.InsertLineBefore(canvas.NumLines());
    canvas.Print(terminal, cur_style);
}

// --- modules::LogPrinter

void ta_test::modules::LogPrinter::OnPreRunSingleTest(const RunSingleTestInfo &data) noexcept
{
    (void)data;
    unscoped_log_pos = 0;
}

void ta_test::modules::LogPrinter::OnPostRunSingleTest(const RunSingleTestResults &data) noexcept
{
    // Doing it in both places (before and after a test) is redundant, but doesn't hurt.
    (void)data;
    unscoped_log_pos = 0;
}

bool ta_test::modules::LogPrinter::PrintLogEntries(output::Terminal::StyleGuard &cur_style, std::span<const context::LogEntry> unscoped_log, std::span<const context::LogEntry *const> scoped_log) noexcept
{
    if (unscoped_log_pos > unscoped_log.size())
        HardError("Less entires in the unscoped log than expected.");

    // Remove the already printed unscoped log messages.
    unscoped_log = unscoped_log.last(unscoped_log.size() - unscoped_log_pos);
    unscoped_log_pos += unscoped_log.size();

    if (!unscoped_log.empty() || !scoped_log.empty())
    {
        do
        {
            bool use_unscoped = false;
            if (unscoped_log.empty())
                use_unscoped = false;
            else if (scoped_log.empty())
                use_unscoped = true;
            else
                use_unscoped = unscoped_log.front().IncrementalId() < scoped_log.front()->IncrementalId();

            // Since this is lazy, this can throw.
            std::string_view message = (use_unscoped ? unscoped_log.front() : *scoped_log.front()).Message();

            if (use_unscoped)
                unscoped_log = unscoped_log.last(unscoped_log.size() - 1);
            else
                scoped_log = scoped_log.last(scoped_log.size() - 1);

            // We reset the style every time in case the `Message()` is lazy and ends up throwing.
            cur_style.ResetStyle();
            terminal.Print(cur_style, "{}{}{}\n",
                style_message,
                chars_message_prefix,
                message
            );
            cur_style.ResetStyle();
        }
        while (!unscoped_log.empty() || !scoped_log.empty());

        terminal.Print("\n");
    }

    return false; // Shrug.
}

// --- modules::ExceptionPrinter ---

void ta_test::modules::ExceptionPrinter::OnUncaughtException(const RunSingleTestInfo &test, const BasicAssertionInfo *assertion, const std::exception_ptr &e) noexcept
{
    (void)test;
    (void)assertion;

    auto cur_style = terminal.MakeStyleGuard();

    output::PrintLog(cur_style);

    terminal.Print(cur_style, "{}{}\n",
        common_data.style_error,
        chars_error
    );

    PrintException(terminal, cur_style, e, -1);
    terminal.Print("\n");

    output::PrintContext(cur_style);
}

// --- modules::MustThrowPrinter ---

void ta_test::modules::MustThrowPrinter::OnMissingException(const MustThrowInfo &data, bool &should_break) noexcept
{
    (void)should_break;

    auto cur_style = terminal.MakeStyleGuard();

    output::PrintLog(cur_style);
    PrintFrame(cur_style, *data.static_info, data.dynamic_info, nullptr, true);
    output::PrintContext(cur_style, &data);
}

bool ta_test::modules::MustThrowPrinter::PrintContextFrame(output::Terminal::StyleGuard &cur_style, const context::BasicFrame &frame) noexcept
{
    if (auto ptr = dynamic_cast<const BasicModule::MustThrowInfo *>(&frame))
    {
        PrintFrame(cur_style, *ptr->static_info, ptr->dynamic_info, nullptr, false);
        return true;
    }
    if (auto ptr = dynamic_cast<const BasicModule::CaughtExceptionElemGuard *>(&frame))
    {
        PrintFrame(cur_style, *ptr->state->static_info, ptr->state->dynamic_info.lock().get(), ptr, false);
        return true;
    }

    return false;
}

void ta_test::modules::MustThrowPrinter::PrintFrame(
    output::Terminal::StyleGuard &cur_style,
    const BasicModule::MustThrowStaticInfo &static_info,
    const BasicModule::MustThrowDynamicInfo *dynamic_info,
    const BasicModule::CaughtExceptionElemGuard *caught,
    bool is_most_nested
) const
{
    const std::string *error_message = nullptr;
    if (caught)
    {
        error_message = &chars_throw_location;

        terminal.Print(cur_style, "{}{}\n",
            common_data.style_stack_frame,
            chars_exception_contents
        );
        PrintException(terminal, cur_style, caught->state->elems.empty() ? nullptr : caught->state->elems.front().exception, caught->active_elem);
        terminal.Print("\n");
    }
    else if (is_most_nested)
    {
        error_message = &chars_expected_exception;
    }
    else
    {
        error_message = &chars_while_expecting_exception;
    }

    terminal.Print(cur_style, "{}{}:\n{}{}",
        common_data.style_path,
        common_data.LocationToString(static_info.loc),
        is_most_nested && !caught ? common_data.style_error : common_data.style_stack_frame,
        *error_message
    );
    if (dynamic_info)
    {
        if (auto message = dynamic_info->GetUserMessage())
        {
            terminal.Print(cur_style, " {}{}",
                common_data.style_user_message,
                *message
            );
        }
    }
    terminal.Print("\n\n");

    output::TextCanvas canvas(&common_data);
    std::size_t column = common_data.code_indentation;
    column += canvas.DrawString(0, column, static_info.macro_name, {.style = common_data.style_failed_macro, .important = true});
    column += canvas.DrawString(0, column, "(", {.style = common_data.style_failed_macro, .important = true});
    column += common_data.spaces_in_macro_call_parentheses;
    column += output::expr::DrawToCanvas(canvas, 0, column, static_info.expr);
    column += common_data.spaces_in_macro_call_parentheses;
    column += canvas.DrawString(0, column, ")", {.style = common_data.style_failed_macro, .important = true});
    canvas.InsertLineBefore(canvas.NumLines());
    canvas.Print(terminal, cur_style);
}

// --- modules::TracePrinter ---

bool ta_test::modules::TracePrinter::PrintContextFrame(output::Terminal::StyleGuard &cur_style, const context::BasicFrame &frame) noexcept
{
    if (auto ptr = dynamic_cast<const BasicTrace *>(&frame))
    {
        output::TextCanvas canvas(&common_data);

        std::size_t column = 0;

        // Path.
        column += canvas.DrawString(0, column, common_data.LocationToString(ptr->GetLocation()), {.style = common_data.style_path, .important = true});
        column += canvas.DrawString(0, column, ":", {.style = common_data.style_path, .important = true});

        // Prefix.
        column = 0;
        column += canvas.DrawString(1, column, chars_func_name_prefix, {.style = common_data.style_stack_frame, .important = true});

        std::string expr;
        { // Generate the function call string.
            expr += ptr->GetFuncName();

            // Template arguments.
            if (!ptr->GetTemplateArgs().empty())
            {
                expr += "<";
                for (bool first = true; const auto &elem : ptr->GetTemplateArgs())
                {
                    if (first)
                        first = false;
                    else
                        expr += common_data.spaced_comma;
                    expr += elem;
                }
                expr += ">";
            }

            // Function arguments.
            expr += '(';
            if (!ptr->GetFuncArgs().empty())
            {
                if (common_data.spaces_in_func_call_parentheses)
                    expr += ' ';

                for (bool first = true; const auto &elem : ptr->GetFuncArgs())
                {
                    if (first)
                        first = false;
                    else
                        expr += common_data.spaced_comma;
                    expr += elem;
                }

                if (common_data.spaces_in_func_call_parentheses)
                    expr += ' ';
            }
            expr += ')';
        }

        output::expr::DrawToCanvas(canvas, 1, column, expr);

        canvas.InsertLineBefore(canvas.NumLines());
        canvas.Print(terminal, cur_style);

        return true;
    }

    return false;
}

// --- modules::DebuggerDetector ---

ta_test::modules::DebuggerDetector::DebuggerDetector()
    : flag_common("debug", "Act as if a debugger was or wasn't attached, bypassing debugger detection. Enabling this is a shorthand for `--break --no-catch`, and vice versa.",
        [](const Runner &runner, BasicModule &this_module, bool enable)
        {
            (void)runner;

            // The cast should never fail.
            auto &self = dynamic_cast<DebuggerDetector &>(this_module);
            self.break_on_failure = enable;
            self.catch_exceptions = !enable;
        }
    ),
    flag_break("break", "Trigger a breakpoint on any failure, this will crash if no debugger is attached (by default enabled if a debugger is attached).",
        [](const Runner &runner, BasicModule &this_module, bool enable)
        {
            (void)runner;

            // The cast should never fail.
            dynamic_cast<DebuggerDetector &>(this_module).break_on_failure = enable;
        }
    ),
    flag_catch("catch", "Catch exceptions. Disabling this means that the application will terminate on the first exception, "
        "but this improves debugging experience by letting you configure your debugger to only break on uncaught exceptions, "
        "which means you don't need to manually skip the ones that are thrown and successfully caught. Enabling this while debugging "
        "will give you only approximate exception locations (the innermost enclosing assertion or test), rather than precise ones. (By default enabled if a debugger is not attached.)",
        [](const Runner &runner, BasicModule &this_module, bool enable)
        {
            (void)runner;

            // The cast should never fail.
            dynamic_cast<DebuggerDetector &>(this_module).catch_exceptions = enable;
        }
    )
{}

std::vector<ta_test::flags::BasicFlag *> ta_test::modules::DebuggerDetector::GetFlags() noexcept
{
    return {&flag_common, &flag_break, &flag_catch};
}

bool ta_test::modules::DebuggerDetector::IsDebuggerAttached() const
{
    return platform::IsDebuggerAttached();
}

void ta_test::modules::DebuggerDetector::OnAssertionFailed(const BasicAssertionInfo &data) noexcept
{
    if (break_on_failure ? *break_on_failure : IsDebuggerAttached())
        data.should_break = true;
}

void ta_test::modules::DebuggerDetector::OnUncaughtException(const RunSingleTestInfo &test, const BasicAssertionInfo *assertion, const std::exception_ptr &e) noexcept
{
    (void)test;
    (void)e;
    if (assertion && (break_on_failure ? *break_on_failure : IsDebuggerAttached()))
        assertion->should_break = true;
}

void ta_test::modules::DebuggerDetector::OnMissingException(const MustThrowInfo &data, bool &should_break) noexcept
{
    (void)data;
    if (break_on_failure ? *break_on_failure : IsDebuggerAttached())
        should_break = true;
}

void ta_test::modules::DebuggerDetector::OnPreTryCatch(bool &should_catch) noexcept
{
    if (catch_exceptions ? !*catch_exceptions : IsDebuggerAttached())
        should_catch = false;
}

void ta_test::modules::DebuggerDetector::OnPostRunSingleTest(const RunSingleTestResults &data) noexcept
{
    if (data.failed && (break_on_failure ? *break_on_failure : IsDebuggerAttached()))
        data.should_break = true;
}

// --- modules::DebuggerStatePrinter ---

void ta_test::modules::DebuggerStatePrinter::OnPreRunTests(const RunTestsInfo &data) noexcept
{
    data.modules->FindModule<DebuggerDetector>([this](DebuggerDetector &detector)
    {
        auto cur_style = terminal.MakeStyleGuard();

        if (detector.break_on_failure && *detector.break_on_failure)
            PrintNote(cur_style, "Will break on failure.");
        else if (!detector.break_on_failure && detector.IsDebuggerAttached())
            PrintNote(cur_style, "Will break on failure (because a debugger is attached, `--catch` to override).");

        if (detector.catch_exceptions && !*detector.catch_exceptions)
            PrintNote(cur_style, "Will not catch exceptions.");
        else if (!detector.catch_exceptions && detector.IsDebuggerAttached())
            PrintNote(cur_style, "Will not catch exceptions (because a debugger is attached, `--no-break` to override).");
        return true;
    });
}
