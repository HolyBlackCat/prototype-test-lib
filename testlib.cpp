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
        Terminal(stderr).AnsiResetString().data(),
        kind == HardErrorKind::internal ? "Internal error" : "Error",
        int(message.size()), message.data()
    );

    // Stop.
    // Don't need to check whether the debugger is attached, a crash is fine.
    CFG_TA_BREAKPOINT();
    std::terminate();
}

std::string ta_test::string_conv::DefaultFallbackToStringTraits<char>::operator()(char value) const
{
    char ret[12]; // Should be at most 9: `'\x{??}'\0`, but throwing in a little extra space.
    text::EscapeString({&value, 1}, ret, false);
    return ret;
}

std::string ta_test::string_conv::DefaultFallbackToStringTraits<std::string_view>::operator()(std::string_view value) const
{
    std::string ret;
    ret.reserve(value.size() + 2); // +2 for quotes. This assumes the happy scenario without any escapes.
    text::EscapeString(value, std::back_inserter(ret), true);
    return ret;
}

std::string ta_test::string_conv::DefaultToStringTraits<std::type_index>::operator()(std::type_index value) const
{
    return text::Demangler{}(value.name());
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

bool ta_test::platform::IsTerminalAttached(bool stderr)
{
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
    if (stderr)
        return lambda.operator()<true>();
    else
        return lambda.operator()<false>();
}

ta_test::Terminal::Terminal(FILE *stream)
{
    output_func = [stream
    #if CFG_TA_FMT_HAS_FILE_PRINT == 0 && defined(_WIN32)
    , need_init = stream == stdout || stream == stderr
    #endif
    ](std::string_view fmt, CFG_TA_FMT_NAMESPACE::format_args args) mutable
    {
        #if CFG_TA_FMT_HAS_FILE_PRINT == 2
        CFG_TA_FMT_NAMESPACE::vprint_unicode(stream, fmt, args);
        #elif CFG_TA_FMT_HAS_FILE_PRINT == 1
        CFG_TA_FMT_NAMESPACE::vprint(stream, fmt, args);
        #elif CFG_TA_FMT_HAS_FILE_PRINT == 0

        #ifdef _WIN32
        if (need_init)
        {
            need_init = false;
            if (ta_test::platform::IsTerminalAttached())
            {
                SetConsoleOutputCP(CP_UTF8);

                auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
                DWORD current_mode{};
                GetConsoleMode(handle, &current_mode);
                SetConsoleMode(handle, current_mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
        #endif

        std::string buffer = CFG_TA_FMT_NAMESPACE::vformat(fmt, args);
        std::fwrite(buffer.c_str(), buffer.size(), 1, stream);
        #else
        #error Invalid value of `CFG_TA_FMT_HAS_FILE_PRINT`.
        #endif
    };

    enable_color =
        stream == stdout ? platform::IsTerminalAttached(false) :
        stream == stderr ? platform::IsTerminalAttached(true) : false;
}

void ta_test::Terminal::PrintLow(std::string_view fmt, CFG_TA_FMT_NAMESPACE::format_args args) const
{
    if (output_func)
        output_func(fmt, args);
}

ta_test::Terminal::StyleGuard::StyleGuard(Terminal &terminal)
    : terminal(terminal)
{
    if (terminal.enable_color)
    {
        ResetStyle();
        exception_counter = std::uncaught_exceptions(); // Don't need this without color.
    }
}

ta_test::Terminal::StyleGuard::~StyleGuard()
{
    if (terminal.enable_color && exception_counter == std::uncaught_exceptions())
        ResetStyle();
}

void ta_test::Terminal::StyleGuard::ResetStyle()
{
    if (terminal.enable_color)
        terminal.Print("{}", terminal.AnsiResetString().data());
}

std::string_view ta_test::Terminal::AnsiResetString() const
{
    if (enable_color)
        return "\033[0m";
    else
        return "";
}

ta_test::Terminal::AnsiDeltaStringBuffer ta_test::Terminal::AnsiDeltaString(const StyleGuard &&cur, const TextStyle &next) const
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

void ta_test::text::TextCanvas::Print(const Terminal &terminal, Terminal::StyleGuard &cur_style) const
{
    std::string buffer;

    for (const Line &line : lines)
    {
        std::size_t segment_start = 0;

        auto FlushSegment = [&](std::size_t end_pos)
        {
            if (segment_start == end_pos)
                return;

            uni::Encode(std::u32string_view(line.text.begin() + std::ptrdiff_t(segment_start), line.text.begin() + std::ptrdiff_t(end_pos)), buffer);
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

std::size_t ta_test::text::TextCanvas::DrawString(std::size_t line, std::size_t start, std::u32string_view text, const CellInfo &info)
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

std::size_t ta_test::text::TextCanvas::DrawString(std::size_t line, std::size_t start, std::string_view text, const CellInfo &info)
{
    std::u32string decoded_text;
    uni::Decode(text, decoded_text);
    return DrawString(line, start, decoded_text, info);
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

void ta_test::text::TextCanvas::DrawOverline(std::size_t line, std::size_t column_start, std::size_t width, const CellInfo &info)
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

std::size_t ta_test::text::expr::DrawToCanvas(TextCanvas &canvas, std::size_t line, std::size_t start, std::string_view expr, const Style *style)
{
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
        if (!uni::IsFirstByte(ch))
            return;

        TextCanvas::CellInfo &info = canvas.CellInfoAt(line, start + i);
        bool is_punct = chars::IsPunct(ch);

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
            if (is_string_suffix && !chars::IsIdentifierChar(ch))
                is_string_suffix = false;
            if ((prev_kind == CharKind::string || prev_kind == CharKind::character || prev_kind == CharKind::raw_string) && chars::IsIdentifierChar(ch))
                is_string_suffix = true;

            if (is_number_suffix && !chars::IsIdentifierChar(ch))
                is_number_suffix = false;

            if (!is_number && !identifier_start && !is_string_suffix && !is_number_suffix)
            {
                if (chars::IsDigit(ch))
                {
                    is_number = true;

                    // Backtrack and make the leading `.` a number too, if it's there.
                    if (i > 0 && expr[i-1] == '.')
                        canvas.CellInfoAt(line, start + i - 1).style = style->number;
                }
                else if (chars::IsIdentifierChar(ch))
                {
                    identifier_start = &ch;
                }
            }
            else if (is_number)
            {
                if (!(chars::IsDigit(ch) || chars::IsAlpha(ch) || ch == '.' || ch == '\'' ||
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
                if (!chars::IsIdentifierChar(ch))
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
                while (j-- > 0 && (chars::IsAlpha(expr[j]) || chars::IsDigit(expr[j])))
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
    ParseExpr(expr, lambda, nullptr);
    if (identifier_start)
        FinalizeIdentifier({identifier_start, expr.data() + expr.size()});

    return expr.size();
}

void ta_test::text::PrintContext(Terminal::StyleGuard &cur_style, const context::BasicFrame *skip_last_frame, context::Context con)
{
    bool first = true;
    for (auto it = con.end(); it != con.begin();)
    {
        --it;
        if (first && skip_last_frame && skip_last_frame == it->get())
            continue;

        first = false;
        ta_test::text::PrintContextFrame(cur_style, **it);
    }
}

void ta_test::text::PrintContextFrame(Terminal::StyleGuard &cur_style, const context::BasicFrame &frame)
{
    auto &thread_state = detail::ThreadState();
    if (!thread_state.current_test)
        HardError("No test is currently running, can't print context.", HardErrorKind::user);

    for (const auto &m : thread_state.current_test->all_tests->modules->AllModules())
    {
        if (auto base = dynamic_cast<BasicPrintingModule *>(m.get()))
        {
            if (base->PrintContextFrame(cur_style, frame))
                break;
        }
    }
}

void ta_test::text::PrintLog(Terminal::StyleGuard &cur_style)
{
    auto &thread_state = detail::ThreadState();
    if (!thread_state.current_test)
        HardError("No test is currently running, can't print log.", HardErrorKind::user);

    // Refresh the messages. Only the scoped log, since the unscoped one should never be lazy.
    for (auto *entry : thread_state.scoped_log)
        entry->RefreshMessage();

    for (const auto &m : thread_state.current_test->all_tests->modules->AllModules())
    {
        if (auto base = dynamic_cast<BasicPrintingModule *>(m.get()))
        {
            if (base->PrintLogEntries(cur_style, thread_state.current_test->unscoped_log, context::CurrentScopedLog()))
                break;
        }
    }
}

void ta_test::BasicPrintingModule::PrintWarning(Terminal::StyleGuard &cur_style, std::string_view text) const
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

void ta_test::BasicPrintingModule::PrintNote(Terminal::StyleGuard &cur_style, std::string_view text) const
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

void ta_test::AnalyzeException(const std::exception_ptr &e, const std::function<void(SingleException elem)> &func)
{
    auto &thread_state = detail::ThreadState();
    if (!thread_state.current_test)
        HardError("The current thread currently isn't running any test, can't use `AnalyzeException()`.", HardErrorKind::user);

    for (auto *m : thread_state.current_test->all_tests->modules->GetModulesImplementing<BasicModule::InterfaceFunction::OnExplainException>())
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

void ta_test::detail::ArgWrapper::EnsureAssertionIsRunning()
{
    const BasicModule::BasicAssertionInfo *cur = ThreadState().current_assertion;

    while (cur)
    {
        if (cur == assertion)
            return;
        cur = cur->enclosing_assertion;
    }

    HardError("`$(...)` was evaluated when an assertion owning it already finished executing, or in a wrong thread.", HardErrorKind::user);
}

void ta_test::detail::BasicAssertWrapper::Evaluator::operator~()
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
            return;
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
    if (self.condition_value_known && !self.condition_value)
    {
        thread_state.FailCurrentTest();
        thread_state.current_test->all_tests->modules->Call<&BasicModule::OnAssertionFailed>(self);
    }

    // Break if a module callback (either on failed assertion or on exception) wants to.
    if (self.should_break)
        self.break_func();

    // Interrupt the test if the condition is false or on an exception.
    if (!self.condition_value_known || !self.condition_value)
        throw InterruptTestException{};
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
    }
    HardError("Invalid `ExceptionElem` enum.", HardErrorKind::user);
}

std::string ta_test::string_conv::DefaultToStringTraits<ta_test::ExceptionElemVar>::operator()(const ExceptionElemVar &value) const
{
    if (value.valueless_by_exception())
        HardError("Invalid `ExceptionElemVar` variant.");
    return std::visit(Overload{
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

    throw InterruptTestException{};
}

void ta_test::Runner::SetDefaultModules()
{
    modules.clear();
    // Those are ordered in a certain way to print the `--help` page in the nice order: [
    modules.push_back(MakeModule<modules::HelpPrinter>());
    modules.push_back(MakeModule<modules::TestSelector>());
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
            std::exit(int(ExitCodes::bad_command_line_arguments));
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

            if (guard.state.failed)
                any_repetition_failed = true;

            guard.state.is_last_generator_repetition = std::all_of(guard.state.generator_stack.begin(), guard.state.generator_stack.end(), [](const auto &g){return g->IsLastValue();});

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

            module_lists.Call<&BasicModule::OnPostRunSingleTest>(guard.state);

            if (guard.state.should_break)
                test->Breakpoint();

            // Prune finished generators.
            // We do this after `OnPostRunSingleTest` in case the modules want to observe the stack before pruning. Not sure how useful this is.
            [&]() noexcept
            {
                // Remove unvisited generators. Note that we show a warning/error for this condition above.
                guard.state.generator_stack.resize(guard.state.generator_index);

                while (!guard.state.generator_stack.empty() && guard.state.generator_stack.back()->IsLastValue())
                    guard.state.generator_stack.pop_back();
            }();

            next_generator_stack = std::move(guard.state.generator_stack);
        }
        while (!next_generator_stack.empty());

        if (any_repetition_failed)
            results.failed_tests.push_back(test);
    }

    module_lists.Call<&BasicModule::OnPostRunTests>(results);

    return results.failed_tests.size() > 0 ? int(ExitCodes::test_failed) : 0;
}

// --- modules::BasicExceptionContentsPrinter ---

ta_test::modules::BasicExceptionContentsPrinter::BasicExceptionContentsPrinter()
    : print_callback([](const BasicExceptionContentsPrinter &self, const Terminal &terminal, Terminal::StyleGuard &cur_style, const std::exception_ptr &e)
    {
        AnalyzeException(e, [&](const SingleException &elem)
        {
            if (elem.IsTypeKnown())
            {
                terminal.Print(cur_style, "{}{}{}{}\n{}{}{}\n",
                    self.style_exception_type,
                    self.chars_indent_type,
                    elem.GetTypeName(),
                    self.chars_type_suffix,
                    self.style_exception_message,
                    self.chars_indent_message,
                    string_conv::ToString(elem.message)
                );
            }
            else
            {
                terminal.Print(cur_style, "{}{}{}\n",
                    self.style_exception_type,
                    self.chars_indent_type,
                    self.chars_unknown_exception
                );
            }
        });
    })
{}

void ta_test::modules::BasicExceptionContentsPrinter::PrintException(const Terminal &terminal, Terminal::StyleGuard &cur_style, const std::exception_ptr &e) const
{
    print_callback(*this, terminal, cur_style, e);
}

// --- modules::HelpPrinter ---

ta_test::modules::HelpPrinter::HelpPrinter()
    : expected_flag_width(16),
    help_flag("help", "Show usage.", [](const Runner &runner, BasicModule &this_module)
    {
        std::vector<flags::BasicFlag *> flags;
        for (const auto &m : runner.modules) // Can't use `ModuleLists` here yet.
        {
            auto more_flags = m->GetFlags();
            flags.insert(flags.end(), more_flags.begin(), more_flags.end());
        }

        // The case shoudl never fail.
        HelpPrinter &self = dynamic_cast<HelpPrinter &>(this_module);

        self.terminal.Print("This is a test runner based on ta_test.\nAvailable options:\n");
        for (flags::BasicFlag *flag : flags)
            self.terminal.Print("  {:<{}} - {}\n", self.expected_flag_width, flag->HelpFlagSpelling(), flag->help_desc);

        std::exit(0);
    })
{}

std::vector<ta_test::flags::BasicFlag *> ta_test::modules::HelpPrinter::GetFlags()
{
    return {&help_flag};
}

void ta_test::modules::HelpPrinter::OnUnknownFlag(std::string_view flag, bool &abort)
{
    (void)abort;
    terminal.Print("Unknown flag `{}`, run with `{}` for usage.\n", flag, help_flag.HelpFlagSpelling());
    // Don't exit, rely on `abort`.
}

void ta_test::modules::HelpPrinter::OnMissingFlagArgument(std::string_view flag, const flags::BasicFlag &flag_obj, bool &abort)
{
    (void)flag_obj;
    (void)abort;
    terminal.Print("Flag `{}` wasn't given enough arguments, run with `{}` for usage.\n", flag, help_flag.HelpFlagSpelling());
    // Don't exit, rely on `abort`.
}

// --- modules::TestSelector ---

ta_test::modules::TestSelector::TestSelector()
    : flag_include("include", 'i',
        "Enable tests matching a pattern. The pattern must either match the whole test name, or its prefix up to and possibly including a slash. "
        "The pattern can be a regex. This flag can be repeated multiple times, and the order with respect to `--exclude` matters. "
        "If the first time this flag appears is before `--exclude`, all tests start disabled by default.",
        GetFlagCallback(false)
    ),
    flag_exclude("exclude", 'e',
        "Disable tests matching a pattern. Uses the same pattern format as `--include`.",
        GetFlagCallback(true)
    )
{}

std::vector<ta_test::flags::BasicFlag *> ta_test::modules::TestSelector::GetFlags()
{
    return {&flag_include, &flag_exclude};
}

void ta_test::modules::TestSelector::OnFilterTest(const BasicTestInfo &test, bool &enable)
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

        std::string_view name_prefix = test.Name();

        auto TryMatchPrefix = [&]() -> bool
        {
            if (std::regex_match(name_prefix.begin(), name_prefix.end(), pattern.regex))
            {
                pattern.was_used = true;
                enable = !pattern.exclude;
                return true;
            }

            return false;
        };

        // Try matching the whole name.
        if (TryMatchPrefix())
            continue;

        // Try prefixes.
        while (!name_prefix.empty())
        {
            name_prefix.remove_suffix(1);

            if (name_prefix.ends_with('/'))
            {
                // Try matching with the slash.
                if (TryMatchPrefix())
                    continue;

                // Try again without slash.
                name_prefix.remove_suffix(1);
                if (TryMatchPrefix())
                    continue;
            }
        }
    }
}

ta_test::flags::StringFlag::Callback ta_test::modules::TestSelector::GetFlagCallback(bool exclude)
{
    return [exclude](const Runner &runner, BasicModule &this_module, std::string_view pattern)
    {
        (void)runner;
        TestSelector &self = dynamic_cast<TestSelector &>(this_module); // This cast should never fail.

        Pattern new_pattern{
            .exclude = exclude,
            .regex_string = std::string(pattern),
            .regex = std::regex(new_pattern.regex_string, std::regex_constants::ECMAScript/*the default syntax*/ | std::regex_constants::optimize),
        };
        self.patterns.push_back(std::move(new_pattern));
    };
}

void ta_test::modules::TestSelector::OnPreRunTests(const RunTestsInfo &data)
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
        std::exit(int(ExitCodes::bad_test_name_pattern));
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

std::vector<ta_test::flags::BasicFlag *> ta_test::modules::PrintingConfigurator::GetFlags()
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

void ta_test::modules::ProgressPrinter::PrintContextLinePrefix(Terminal::StyleGuard &cur_style, const RunTestsProgress &all_tests, TestCounterStyle test_counter_style) const
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

void ta_test::modules::ProgressPrinter::PrintContextLineIndentation(Terminal::StyleGuard &cur_style, std::size_t depth, std::size_t skip_characters) const
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

void ta_test::modules::ProgressPrinter::PrintGeneratorInfo(Terminal::StyleGuard &cur_style, const RunSingleTestProgress &test, const BasicGenerator &generator, bool repeating_info)
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
    terminal.Print(cur_style, "{}{}{}{}{}{}{}{}{}{}",
        // The bullet point.
        st_gen.prefix,
        repeating_info ? chars_test_prefix_continuing : chars_test_prefix,
        // Generator name.
        st_gen.name,
        generator.GetName(),
        // Opening bracket.
        st_gen.index_brackets,
        chars_generator_index_prefix,
        // The index.
        st_gen.index,
        generator.NumGeneratedValues(),
        // Closing bracket.
        st_gen.index_brackets,
        chars_generator_index_suffix
    );

    // The value as a string.
    if (generator.ValueSupportsToString())
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

ta_test::modules::ProgressPrinter::ProgressPrinter()
{
    EnableUnicode(true);
}

void ta_test::modules::ProgressPrinter::EnableUnicode(bool enable)
{
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

void ta_test::modules::ProgressPrinter::OnPreRunTests(const RunTestsInfo &data)
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
            data.num_tests != 1 ? "s" : ""
        );
    }
}

void ta_test::modules::ProgressPrinter::OnPostRunTests(const RunTestsResults &data)
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

void ta_test::modules::ProgressPrinter::OnPreRunSingleTest(const RunSingleTestInfo &data)
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

void ta_test::modules::ProgressPrinter::OnPostRunSingleTest(const RunSingleTestResults &data)
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
    if (data.failed)
    {
        // Remember the failed generator stack.
        std::vector<State::PerTest::FailedGenerator> failed_generator_stack;
        failed_generator_stack.reserve(data.generator_stack.size());
        for (const auto &gen : data.generator_stack)
        {
            failed_generator_stack.push_back({
                .name = std::string(gen->GetName()),
                .index = gen->NumGeneratedValues(),
                .value = gen->ValueSupportsToString() ? std::optional(gen->ValueToString()) : std::nullopt,
                .location = gen->GetLocation(),
            });
        }
        state.per_test.failed_generator_stacks.push_back(std::move(failed_generator_stack));
    }

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
        terminal.Print(cur_style, "\n{}IN TEST {}{}{}{}{}, {}{}{}/{}{} VARIANTS FAILED:\n\n",
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
                terminal.Print(cur_style, "{}{}{}{}{}{}{}{}{}{}",
                    style_generator_failed.prefix,
                    chars_test_prefix,
                    style_generator_failed.name,
                    elem.name,
                    style_generator_failed.index_brackets,
                    chars_generator_index_prefix,
                    style_generator_failed.index,
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

void ta_test::modules::ProgressPrinter::OnPostGenerate(const GeneratorCallInfo &data)
{
    auto cur_style = terminal.MakeStyleGuard();
    if (data.geneating_new_value || state.per_test.per_repetition.prev_rep_failed)
        PrintGeneratorInfo(cur_style, *data.test, *data.generator, !data.geneating_new_value);
}

void ta_test::modules::ProgressPrinter::OnPreFailTest(const RunSingleTestProgress &data)
{
    // Avoid printing the diagnoal separator on the next progress line. It's unnecessary after a bulky failure message.
    state.per_test.last_repetition_counters_width = std::size_t(-1);

    std::string_view group;
    std::string_view name = data.test->Name();
    auto sep = name.find_last_of('/');
    if (sep != std::string_view::npos)
    {
        group = name.substr(0, sep + 1);
        name = name.substr(sep + 1);
    }

    std::size_t separator_segment_width = text::uni::CountFirstBytes(chars_test_failed_separator);
    std::size_t separator_needed_width = separator_line_width - text::uni::CountFirstBytes(chars_test_failed) - data.test->Name().size() - 1/*space before separator*/;
    std::string separator;
    for (std::size_t i = 0; i + separator_segment_width - 1 < separator_needed_width; i += separator_segment_width)
        separator += chars_test_failed_separator;

    auto cur_style = terminal.MakeStyleGuard();

    // The test failure message, and a separator after that.
    terminal.Print(cur_style, "\n{}{}:\n{}{}{}{}{}{} {}{}\n",
        common_data.style_path,
        common_data.LocationToString(data.test->Location()),
        common_data.style_error,
        chars_test_failed,
        style_failed_group_name,
        group,
        style_failed_name,
        name,
        style_test_failed_separator,
        separator
    );

    // Print the generator determinism warning, if needed.
    if (data.generator_index < data.generator_stack.size())
        PrintWarning(cur_style, "Non-deterministic failure. Previous runs didn't fail here with the same generated values. Some generators will be pruned.");

    terminal.Print("\n");
}

// --- modules::ResultsPrinter ---

void ta_test::modules::ResultsPrinter::OnPostRunTests(const RunTestsResults &data)
{
    auto cur_style = terminal.MakeStyleGuard();

    terminal.Print("\n");

    std::size_t num_passed = data.num_tests - data.failed_tests.size();
    std::size_t num_skipped = data.num_tests_with_skipped - data.num_tests;

    // The number of skipped tests.
    if (num_skipped > 0 && data.num_tests > 0)
    {
        terminal.Print(cur_style, "{}{} test{} skipped\n",
            style_num_skipped,
            num_skipped,
            num_skipped != 1 ? "s" : ""
        );
    }

    if (data.num_tests == 0)
    {
        // No tests to run.
        terminal.Print(cur_style, "{}NO TESTS TO RUN\n",
            style_no_tests
        );
    }
    else if (data.failed_tests.size() == 0)
    {
        // All passed.

        terminal.Print(cur_style, "{}{}{} TEST{} PASSED\n",
            style_all_passed,
            num_passed > 1 ? "ALL " : "",
            num_passed,
            num_passed != 1 ? "S" : ""
        );
    }
    else
    {
        // Some or all failed.

        // Passed tests, if any.
        if (num_passed > 0)
        {
            terminal.Print(cur_style, "{}{} test{} passed\n",
                style_num_passed,
                num_passed,
                num_passed != 1 ? "s" : ""
            );
        }

        // Failed tests.
        terminal.Print(cur_style, "{}{}{} TEST{} FAILED\n",
            style_num_failed,
            num_passed == 0 && data.failed_tests.size() > 1 ? "ALL " : "",
            data.failed_tests.size(),
            data.failed_tests.size() == 1 ? "" : "S"
        );
    }
}

// --- modules::AssertionPrinter ---

void ta_test::modules::AssertionPrinter::OnAssertionFailed(const BasicAssertionInfo &data)
{
    auto cur_style = terminal.MakeStyleGuard();
    text::PrintLog(cur_style);
    PrintAssertionFrameLow(cur_style, data, true);
    text::PrintContext(cur_style, &data);
}

bool ta_test::modules::AssertionPrinter::PrintContextFrame(Terminal::StyleGuard &cur_style, const context::BasicFrame &frame)
{
    if (auto assertion_frame = dynamic_cast<const BasicAssertionInfo *>(&frame))
    {
        PrintAssertionFrameLow(cur_style, *assertion_frame, false);
        return true;
    }

    return false;
}

void ta_test::modules::AssertionPrinter::PrintAssertionFrameLow(Terminal::StyleGuard &cur_style, const BasicAssertionInfo &data, bool is_most_nested) const
{
    text::TextCanvas canvas(&common_data);
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

        const text::TextCanvas::CellInfo assertion_macro_cell_info = {.style = common_data.style_failed_macro, .important = true};

        for (int i = 0;; i++)
        {
            auto var = data.GetElement(i);
            if (std::holds_alternative<std::monostate>(var))
                break;

            std::visit(Overload{
                [&](std::monostate) {},
                [&](const BasicAssertionInfo::DecoFixedString &deco)
                {
                    column += canvas.DrawString(line_counter, column, deco.string, assertion_macro_cell_info);
                },
                [&](const BasicAssertionInfo::DecoExpr &deco)
                {
                    column += text::expr::DrawToCanvas(canvas, line_counter, column, deco.string);
                },
                [&](const BasicAssertionInfo::DecoExprWithArgs &deco)
                {
                    column += common_data.spaces_in_macro_call_parentheses;
                    expr = deco.expr;
                    expr_column = column;
                    column += text::expr::DrawToCanvas(canvas, line_counter, column, deco.expr->Expr());
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
        // More than one should be impossible, but if it happens, we just combine them into a single fat one.
        int num_overline_parts = 0;

        // Incremented when we print an argument.
        std::size_t color_index = 0;

        for (std::size_t i = 0; i < expr->NumArgs(); i++)
        {
            const std::size_t arg_index = expr->ArgsInDrawOrder()[i];
            const BasicAssertionExpr::StoredArg &this_arg = expr->StoredArgs()[arg_index];
            const BasicAssertionExpr::ArgInfo &this_info = expr->ArgsInfo()[arg_index];

            bool dim_parentheses = true;

            if (this_arg.state == BasicAssertionExpr::StoredArg::State::in_progress)
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

            if (this_arg.state == BasicAssertionExpr::StoredArg::State::done)
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
                    canvas.DrawString(value_y, value_x, this_value, this_cell_info);
                    canvas.DrawColumn(common_data.bar, line_counter, center_x, value_y - line_counter, true, this_cell_info);

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

void ta_test::modules::LogPrinter::OnPreRunSingleTest(const RunSingleTestInfo &data)
{
    (void)data;
    unscoped_log_pos = 0;
}

void ta_test::modules::LogPrinter::OnPostRunSingleTest(const RunSingleTestResults &data)
{
    // Doing it in both places (before and after a test) is redundant, but doesn't hurt.
    (void)data;
    unscoped_log_pos = 0;
}

bool ta_test::modules::LogPrinter::PrintLogEntries(Terminal::StyleGuard &cur_style, std::span<const context::LogEntry> unscoped_log, std::span<const context::LogEntry *const> scoped_log)
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

void ta_test::modules::ExceptionPrinter::OnUncaughtException(const RunSingleTestInfo &test, const BasicAssertionInfo *assertion, const std::exception_ptr &e)
{
    (void)test;
    (void)assertion;

    auto cur_style = terminal.MakeStyleGuard();

    text::PrintLog(cur_style);

    terminal.Print(cur_style, "{}{}\n",
        common_data.style_error,
        chars_error
    );

    PrintException(terminal, cur_style, e);
    terminal.Print("\n");

    text::PrintContext(cur_style);
}

// --- modules::MustThrowPrinter ---

void ta_test::modules::MustThrowPrinter::OnMissingException(const MustThrowInfo &data, bool &should_break)
{
    (void)should_break;

    auto cur_style = terminal.MakeStyleGuard();

    text::PrintLog(cur_style);
    PrintFrame(cur_style, *data.static_info, data.dynamic_info, nullptr, true);
    text::PrintContext(cur_style, &data);
}

bool ta_test::modules::MustThrowPrinter::PrintContextFrame(Terminal::StyleGuard &cur_style, const context::BasicFrame &frame)
{
    if (auto ptr = dynamic_cast<const BasicModule::MustThrowInfo *>(&frame))
    {
        PrintFrame(cur_style, *ptr->static_info, ptr->dynamic_info, nullptr, false);
        return true;
    }
    if (auto ptr = dynamic_cast<const BasicModule::CaughtExceptionInfo *>(&frame))
    {
        PrintFrame(cur_style, *ptr->static_info, ptr->dynamic_info.lock().get(), ptr, false);
        return true;
    }

    return false;
}

void ta_test::modules::MustThrowPrinter::PrintFrame(
    Terminal::StyleGuard &cur_style,
    const BasicModule::MustThrowStaticInfo &static_info,
    const BasicModule::MustThrowDynamicInfo *dynamic_info,
    const BasicModule::CaughtExceptionInfo *caught,
    bool is_most_nested
)
{
    const std::string *error_message = nullptr;
    if (caught)
    {
        error_message = &chars_throw_location;

        terminal.Print(cur_style, "{}{}\n",
            common_data.style_stack_frame,
            chars_exception_contents
        );
        PrintException(terminal, cur_style, caught->elems.empty() ? nullptr : caught->elems.front().exception);
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

    text::TextCanvas canvas(&common_data);
    std::size_t column = common_data.code_indentation;
    column += canvas.DrawString(0, column, static_info.macro_name, {.style = common_data.style_failed_macro, .important = true});
    column += canvas.DrawString(0, column, "(", {.style = common_data.style_failed_macro, .important = true});
    column += common_data.spaces_in_macro_call_parentheses;
    column += text::expr::DrawToCanvas(canvas, 0, column, static_info.expr);
    column += common_data.spaces_in_macro_call_parentheses;
    column += canvas.DrawString(0, column, ")", {.style = common_data.style_failed_macro, .important = true});
    canvas.InsertLineBefore(canvas.NumLines());
    canvas.Print(terminal, cur_style);
}

// --- modules::TracePrinter ---

bool ta_test::modules::TracePrinter::PrintContextFrame(Terminal::StyleGuard &cur_style, const context::BasicFrame &frame)
{
    if (auto ptr = dynamic_cast<const BasicTrace *>(&frame))
    {
        text::TextCanvas canvas(&common_data);

        // Path.
        canvas.DrawString(0, 0, common_data.LocationToString(ptr->GetLocation()), {.style = common_data.style_path, .important = true});

        // Prefix.
        std::size_t column = 0;
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

        text::expr::DrawToCanvas(canvas, 1, column, expr);

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

std::vector<ta_test::flags::BasicFlag *> ta_test::modules::DebuggerDetector::GetFlags()
{
    return {&flag_common, &flag_break, &flag_catch};
}

bool ta_test::modules::DebuggerDetector::IsDebuggerAttached() const
{
    return platform::IsDebuggerAttached();
}

void ta_test::modules::DebuggerDetector::OnAssertionFailed(const BasicAssertionInfo &data)
{
    if (break_on_failure ? *break_on_failure : IsDebuggerAttached())
        data.should_break = true;
}

void ta_test::modules::DebuggerDetector::OnUncaughtException(const RunSingleTestInfo &test, const BasicAssertionInfo *assertion, const std::exception_ptr &e)
{
    (void)test;
    (void)e;
    if (assertion && (break_on_failure ? *break_on_failure : IsDebuggerAttached()))
        assertion->should_break = true;
}

void ta_test::modules::DebuggerDetector::OnMissingException(const MustThrowInfo &data, bool &should_break)
{
    (void)data;
    if (break_on_failure ? *break_on_failure : IsDebuggerAttached())
        should_break = true;
}

void ta_test::modules::DebuggerDetector::OnPreTryCatch(bool &should_catch)
{
    if (catch_exceptions ? !*catch_exceptions : IsDebuggerAttached())
        should_catch = false;
}

void ta_test::modules::DebuggerDetector::OnPostRunSingleTest(const RunSingleTestResults &data)
{
    if (data.failed && (break_on_failure ? *break_on_failure : IsDebuggerAttached()))
        data.should_break = true;
}

// --- modules::DebuggerStatePrinter ---

void ta_test::modules::DebuggerStatePrinter::OnPreRunTests(const RunTestsInfo &data)
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
