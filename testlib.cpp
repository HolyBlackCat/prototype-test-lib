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

ta_test::context::Context ta_test::context::CurrentContext()
{
    return detail::ThreadState().context_stack;
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

ta_test::context::FrameGuard::~FrameGuard()
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
}

std::string ta_test::SingleException::GetTypeName() const
{
    if (IsTypeKnown())
        return text::Demangler{}(type.name());
    else
        return "";
}

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

    std::fprintf(stderr, "%s: %.*s\n", kind == HardErrorKind::internal ? "Internal error" : "Error", int(message.size()), message.data());

    // Stop.
    // Don't need to check whether the debugger is attached, a crash is fine.
    CFG_TA_BREAKPOINT();
    std::terminate();
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

ta_test::CaughtException::CaughtException(const BasicModule::MustThrowInfo *must_throw_call, const std::exception_ptr &e)
    : state(std::make_shared<BasicModule::CaughtExceptionInfo>())
{
    state->must_throw_call = must_throw_call;

    AnalyzeException(e, [&](SingleException elem)
    {
        state->elems.push_back(std::move(elem));
    });
}

const std::vector<ta_test::SingleException> &ta_test::CaughtException::GetElems() const
{
    if (!state)
    {
        // This is a little stupid, but probably better than a `HardError()`?
        static const std::vector<ta_test::SingleException> ret;
        return ret;
    }
    else
    {
        return state->elems;
    }
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

bool ta_test::platform::IsTerminalAttached()
{
    // We cache the return value.
    static bool ret = []{
        #if defined(_WIN32)
        return GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_CHAR;
        #elif defined(DETAIL_TA_USE_ISATTY)
        return isatty(STDOUT_FILENO) == 1;
        #else
        return false;
        #endif
    }();
    return ret;
}

ta_test::Terminal::Terminal()
{
    SetFileOutput(stdout);
}

void ta_test::Terminal::SetFileOutput(FILE *stream)
{
    output_func = [stream](const char *format, std::va_list args)
    {
        std::vfprintf(stream, format, args);
    };
}

void ta_test::Terminal::Print(const char *format, ...) const
{
    std::va_list list;
    va_start(list, format);
    struct Guard
    {
        std::va_list *list = nullptr;
        ~Guard()
        {
            // Ok, here's the thing. We formally need to call this even if the callback throws,
            //   but formally it must be called by the same function that called `va_start()`.
            // Also it seems that on modern implementations it's a no-op.
            // Adding a `catch(...)` just for this would interfere with crash dumps, so I'm not doing that.
            // Simply ignoring exceptions doesn't sound right.
            // Calling it from a scope guard, while formally undefined, at least has the same behavior regardless of exceptions.
            // And if we even find ourselves on a platform where we can't call it from a scope guard, we'll know about it immediately,
            //   without testing throwing exceptions.
            va_end(*list);
        }
    };
    Guard guard{.list = &list};
    output_func(format, list);
}

static void ConfigureTerminalIfNeeded()
{
    #ifdef _WIN32
    if (ta_test::platform::IsTerminalAttached() )
    {
        SetConsoleOutputCP(CP_UTF8);

        auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD current_mode{};
        GetConsoleMode(handle, &current_mode);
        SetConsoleMode(handle, current_mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    #endif
}

std::string_view ta_test::Terminal::AnsiResetString() const
{
    if (enable_color)
    {
        ConfigureTerminalIfNeeded();
        return "\033[0m";
    }
    else
    {
        return "";
    }
}

ta_test::Terminal::AnsiDeltaStringBuffer ta_test::Terminal::AnsiDeltaString(const TextStyle &&cur, const TextStyle &next) const
{
    AnsiDeltaStringBuffer ret;
    ret[0] = '\0';

    if (!enable_color)
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

        ConfigureTerminalIfNeeded();
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

void ta_test::text::TextCanvas::Print(const Terminal &terminal) const
{
    PrintToCallback(terminal, [&](std::string_view string){terminal.Print("%.*s", int(string.size()), string.data());});
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
        else if (std::all_of(ident.begin(), ident.end(), [](char ch){return chars::IsIdentifierChar(ch) && !chars::IsAlphaLowercase(ch);}))
        {
            ident_style = &style->all_caps;
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
        bool is_punct = !chars::IsIdentifierChar(ch);

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

void ta_test::text::PrintContext(const context::BasicFrame *skip_last_frame, context::Context con)
{
    bool first = true;
    for (auto it = con.end(); it != con.begin();)
    {
        --it;
        if (first && skip_last_frame && skip_last_frame == it->get())
            continue;

        first = false;
        ta_test::text::PrintContextFrame(**it);
    }
}

void ta_test::text::PrintContextFrame(const context::BasicFrame &frame)
{
    auto &thread_state = detail::ThreadState();
    if (!thread_state.current_test)
        HardError("No test is currently running, can't print context.", HardErrorKind::user);

    for (const auto &m : thread_state.current_test->all_tests->modules->AllModules())
    {
        if (auto base = dynamic_cast<BasicPrintingModule *>(m.get()))
        {
            if (base->PrintContextFrame(frame))
                break;
        }
    }
}

void ta_test::BasicPrintingModule::PrintNote(std::string_view text) const
{
    terminal.Print("%s%s%s%.*s%s\n",
        terminal.AnsiResetString().data(),
        terminal.AnsiDeltaString({}, common_data.style_note).data(),
        common_data.note_prefix.c_str(),
        int(text.size()), text.data(),
        terminal.AnsiResetString().data()
    );
}

void ta_test::AnalyzeException(const std::exception_ptr &e, const std::function<void(SingleException elem)> &func)
{
    detail::GlobalThreadState &thread_state = detail::ThreadState();
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

ta_test::detail::BasicAssertWrapper::BasicAssertWrapper()
    : FrameGuard({std::shared_ptr<void>{}, this}) // A non-owning shared pointer.
{
    GlobalThreadState &thread_state = ThreadState();
    if (!thread_state.current_test)
        HardError("This thread doesn't have a test currently running, yet it tries to use an assertion.");

    auto &cur = thread_state.current_assertion;
    enclosing_assertion = cur;
    cur = this;
}

ta_test::detail::BasicAssertWrapper::~BasicAssertWrapper()
{
    // We don't check `finished` here. It can be false when a nested assertion fails.

    GlobalThreadState &thread_state = ThreadState();
    if (thread_state.current_assertion != this)
        HardError("Something is wrong. Are we in a coroutine that was transfered to a different thread in the middle on an assertion?");

    thread_state.current_assertion = const_cast<BasicModule::BasicAssertionInfo *>(enclosing_assertion);
}

bool ta_test::detail::BasicAssertWrapper::PreEvaluate()
{
    if (finished)
        HardError("Invalid usage, `operator()` called more than once on an `AssertWrapper`.");

    GlobalThreadState &thread_state = ThreadState();
    if (!thread_state.current_test)
        HardError("Something is wrong, the current test information disappeared while the assertion was evaluated.");
    if (thread_state.current_assertion != this)
        HardError("The assertion being evaluated is not on the top of the assertion stack.");

    bool should_catch = true;
    thread_state.current_test->all_tests->modules->Call<&BasicModule::OnPreTryCatch>(should_catch);
    return should_catch;
}

void ta_test::detail::BasicAssertWrapper::UncaughtException()
{
    GlobalThreadState &thread_state = ThreadState();
    thread_state.FailCurrentTest();

    auto e = std::current_exception();
    thread_state.current_test->all_tests->modules->Call<&BasicModule::OnUncaughtException>(*thread_state.current_test, e);

    finished = true;
}

void ta_test::detail::BasicAssertWrapper::PostEvaluate(bool value, const std::optional<std::string> &fail_message)
{
    if (!value)
    {
        GlobalThreadState &thread_state = ThreadState();
        thread_state.FailCurrentTest();

        std::optional<std::string_view> fail_message_view;
        if (fail_message)
            fail_message_view = *fail_message;

        thread_state.current_test->all_tests->modules->Call<&BasicModule::OnAssertionFailed>(*this, fail_message_view);
    }

    finished = true;
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
                HardError(CFG_TA_FMT_NAMESPACE::format("Conflicting definitions for test `{}`. One at `{}:{}`, another at `{}:{}`.", name, old_loc.file, old_loc.line, new_loc.file, new_loc.line), HardErrorKind::user);
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

void ta_test::detail::MustThrowWrapper::MissingException()
{
    auto &thread_state = ThreadState();

    // This check should be redundant, since `operator()` already checks the same thing.
    // But it's better to have it, in case something changes in the future.
    if (!thread_state.current_test)
        HardError("Attempted to use `TA_MUST_THROW(...)`, but no test is currently running.", HardErrorKind::user);

    thread_state.FailCurrentTest();
    thread_state.current_test->all_tests->modules->Call<&BasicModule::OnMissingException>(*info);

    throw InterruptTestException{};
}

ta_test::detail::MustThrowWrapper::MustThrowWrapper(const BasicModule::MustThrowInfo *info)
    : FrameGuard({std::shared_ptr<void>{}, info}), info(info) // A non-owning shared pointer.
{}

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
    if (detail::ThreadState().current_test)
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

    for (std::size_t test_index : ordered_tests)
    {
        const detail::BasicTest *test = state.tests[test_index];

        struct StateGuard
        {
            BasicModule::RunSingleTestResults state;
            StateGuard() {detail::ThreadState().current_test = &state;}
            ~StateGuard() {detail::ThreadState().current_test = nullptr;}
        };
        StateGuard guard;
        guard.state.all_tests = &results;
        guard.state.test = test;

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
                detail::ThreadState().FailCurrentTest();
                auto e = std::current_exception();
                module_lists.Call<&BasicModule::OnUncaughtException>(guard.state, e);
            }
        }
        else
        {
            lambda();
        }

        module_lists.Call<&BasicModule::OnPostRunSingleTest>(guard.state);

        if (guard.state.failed)
            results.failed_tests.push_back(test);

        if (guard.state.should_break)
            test->Breakpoint();
    }

    module_lists.Call<&BasicModule::OnPostRunTests>(results);

    return results.failed_tests.size() > 0 ? int(ExitCodes::test_failed) : 0;
}

// --- modules::BasicExceptionContentsPrinter ---

ta_test::modules::BasicExceptionContentsPrinter::BasicExceptionContentsPrinter()
    : print_callback([](const BasicExceptionContentsPrinter &self, const Terminal &terminal, const std::exception_ptr &e)
    {
        TextStyle cur_style;

        AnalyzeException(e, [&](const SingleException &elem)
        {
            if (elem.IsTypeKnown())
            {
                auto st_type = terminal.AnsiDeltaString(cur_style, self.style_exception_type);
                auto st_message = terminal.AnsiDeltaString(cur_style, self.style_exception_message);
                terminal.Print("%s%s%s%s\n%s%s%s\n",
                    st_type.data(),
                    self.chars_indent_type.c_str(),
                    elem.GetTypeName().c_str(),
                    self.chars_type_suffix.c_str(),
                    st_message.data(),
                    self.chars_indent_message.c_str(),
                    ToString<std::string_view>{}(elem.message).c_str()
                );
            }
            else
            {
                terminal.Print("%s%s%s\n",
                    terminal.AnsiDeltaString(cur_style, self.style_exception_type).data(),
                    self.chars_indent_type.c_str(),
                    self.chars_unknown_exception.c_str()
                );
            }
        });

        terminal.Print("%s", terminal.AnsiResetString().data());
    })
{}

void ta_test::modules::BasicExceptionContentsPrinter::PrintException(const Terminal &terminal, const std::exception_ptr &e) const
{
    print_callback(*this, terminal, e);
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
            self.terminal.Print("  %-*s - %s\n", self.expected_flag_width, flag->HelpFlagSpelling().c_str(), flag->help_desc.c_str());

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
    terminal.Print("Unknown flag `%.*s`, run with `%s` for usage.\n", int(flag.size()), flag.data(), help_flag.HelpFlagSpelling().c_str());
    // Don't exit, rely on `abort`.
}

void ta_test::modules::HelpPrinter::OnMissingFlagArgument(std::string_view flag, const flags::BasicFlag &flag_obj, bool &abort)
{
    (void)flag_obj;
    (void)abort;
    terminal.Print("Flag `%.*s` wasn't given enough arguments, run with `%s` for usage.\n", int(flag.size()), flag.data(), help_flag.HelpFlagSpelling().c_str());
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

ta_test::modules::ProgressPrinter::ProgressPrinter()
{
    EnableUnicode(true);
}

void ta_test::modules::ProgressPrinter::EnableUnicode(bool enable)
{
    if (enable)
    {
        chars_test_prefix = "\xE2\x97\x8F "; // BLACK CIRCLE, then a space.
        chars_test_prefix_continuing_group = "\xE2\x97\x8B "; // WHITE CIRCLE, then a space.
        chars_indentation = "\xC2\xB7   "; // MIDDLE DOT, then a space.
        chars_test_counter_separator = " \xE2\x94\x82  "; // BOX DRAWINGS LIGHT VERTICAL, with some spaces around it.
        chars_test_failed_separator = "\xE2\x94\x81"; // BOX DRAWINGS HEAVY HORIZONTAL
        chars_test_failed_ending_separator = "\xE2\x94\x80"; // BOX DRAWINGS LIGHT HORIZONTAL
        chars_summary_path_separator = "      \xE2\x94\x82 "; // BOX DRAWINGS LIGHT VERTICAL, with some spaces around it.
    }
    else
    {
        chars_test_prefix = "* ";
        chars_test_prefix_continuing_group = ". ";
        chars_indentation = "    ";
        chars_test_counter_separator = " |  ";
        chars_test_failed_separator = "#";
        chars_test_failed_ending_separator = "-";
        chars_summary_path_separator = "      | ";
    }
}

void ta_test::modules::ProgressPrinter::OnPreRunTests(const RunTestsInfo &data)
{
    state = {};

    // Print a message when some tests are skipped.
    if (data.num_tests < data.num_tests_with_skipped)
    {
        std::size_t num_skipped = data.num_tests_with_skipped - data.num_tests;

        terminal.Print("%s%sSkipping %zu test%s, will run %zu/%zu test%s.%s\n",
            terminal.AnsiResetString().data(),
            terminal.AnsiDeltaString({}, style_skipped_tests).data(),
            num_skipped,
            num_skipped != 1 ? "s" : "",
            data.num_tests,
            data.num_tests_with_skipped,
            data.num_tests != 1 ? "s" : "",
            terminal.AnsiResetString().data()
        );
    }
}

void ta_test::modules::ProgressPrinter::OnPostRunTests(const RunTestsResults &data)
{
    if (!data.failed_tests.empty())
    {
        TextStyle cur_style;
        terminal.Print("%s\n%s%s\n\n",
            terminal.AnsiResetString().data(),
            terminal.AnsiDeltaString(cur_style, common_data.style_error).data(),
            chars_summary_tests_failed.c_str()
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
                    terminal.Print("%s", terminal.AnsiDeltaString(cur_style, style_indentation_guide).data());
                    // Print the required number of guides.
                    for (std::size_t repeat = 0; repeat < stack.size(); repeat++)
                        terminal.Print("%s", chars_indentation.c_str());
                }

                std::size_t gap_to_separator = max_test_name_width - (stack.size() * indentation_width + prefix_width + (is_last_segment ? 0 : 1/*for the slash*/) + segment.size());

                // Print the test name.
                auto st_name = terminal.AnsiDeltaString(cur_style, is_last_segment ? style_summary_failed_name : style_summary_failed_group_name);
                auto st_separator = terminal.AnsiDeltaString(cur_style, style_summary_path_separator);
                terminal.Print("%s%s%.*s%s%*s%s%s",
                    st_name.data(),
                    chars_test_prefix.c_str(),
                    int(segment.size()), segment.data(),
                    is_last_segment ? "" : "/",
                    int(gap_to_separator), "",
                    st_separator.data(),
                    chars_summary_path_separator.c_str()
                );

                // Print the file path for the last segment.
                if (is_last_segment)
                {
                    terminal.Print("%s%s",
                        terminal.AnsiDeltaString(cur_style, style_summary_path).data(),
                        common_data.LocationToString(test->Location()).data()
                    );
                }

                terminal.Print("\n");
            });
        }

        terminal.Print("%s", terminal.AnsiResetString().data());
    }

    state = {};
}

void ta_test::modules::ProgressPrinter::OnPreRunSingleTest(const RunSingleTestInfo &data)
{
    { // Print the message when first starting tests, or when resuming from a failure.
        if (state.test_counter == 0 || !state.failed_test_stack.empty())
        {
            terminal.Print("%s\n%s%s%s\n",
                terminal.AnsiResetString().data(),
                terminal.AnsiDeltaString({}, style_starting_or_continuing_tests).data(),
                (state.test_counter == 0 ? chars_starting_tests : chars_continuing_tests).c_str(),
                terminal.AnsiResetString().data()
            );
        }
    }

    // How much characters in the total test count.
    const int num_tests_width = std::snprintf(nullptr, 0, "%zu", data.all_tests->num_tests);

    ProduceTree(state.stack, data.test->Name(), [&](std::size_t segment_index, std::string_view segment, bool is_last_segment)
    {
        TextStyle cur_style;

        // Test index (if this is the last segment).
        if (is_last_segment)
        {
            auto st_index = terminal.AnsiDeltaString(cur_style, style_index);
            auto st_total_count = terminal.AnsiDeltaString(cur_style, style_total_count);

            terminal.Print("%s%*zu%s/%zu",
                st_index.data(),
                num_tests_width, state.test_counter + 1,
                st_total_count.data(),
                data.all_tests->num_tests
            );

            // Failed test count.
            if (data.all_tests->failed_tests.size() > 0)
            {
                auto st_deco_l = terminal.AnsiDeltaString(cur_style, style_failed_count_decorations);
                auto st_num_failed = terminal.AnsiDeltaString(cur_style, style_failed_count);
                auto st_deco_r = terminal.AnsiDeltaString(cur_style, style_failed_count_decorations);

                terminal.Print(" %s[%s%zu%s]",
                    st_deco_l.data(),
                    st_num_failed.data(),
                    data.all_tests->failed_tests.size(),
                    st_deco_r.data()
                );
            }
        }
        else
        {
            // No test index, just a gap.

            int gap_width = num_tests_width * 2 + 1;

            if (data.all_tests->failed_tests.size() > 0)
                gap_width += std::snprintf(nullptr, 0, "%zu", data.all_tests->failed_tests.size()) + 3;

            terminal.Print("%*s", gap_width, "");
        }

        // The gutter border.
        terminal.Print("%s%s",
            terminal.AnsiDeltaString(cur_style, style_gutter_border).data(),
            chars_test_counter_separator.c_str()
        );

        // The indentation.
        if (!state.stack.empty())
        {
            // Switch to the indentation guide color.
            terminal.Print("%s", terminal.AnsiDeltaString(cur_style, style_indentation_guide).data());
            // Print the required number of guides.
            for (std::size_t repeat = 0; repeat < state.stack.size(); repeat++)
                terminal.Print("%s", chars_indentation.c_str());
        }

        // Whether we're reentering this group after a failed test.
        bool is_continued = segment_index < state.failed_test_stack.size() && state.failed_test_stack[segment_index] == segment;

        // Print the test name.
        terminal.Print("%s%s%.*s%s%s\n",
            terminal.AnsiDeltaString(cur_style, is_continued ? style_continuing_group : is_last_segment ? style_name : style_group_name).data(),
            is_continued ? chars_test_prefix_continuing_group.c_str() : chars_test_prefix.c_str(),
            int(segment.size()), segment.data(),
            is_last_segment ? "" : "/",
            terminal.AnsiResetString().data()
        );
    });
}

void ta_test::modules::ProgressPrinter::OnPostRunSingleTest(const RunSingleTestResults &data)
{
    // Print the ending separator.
    if (data.failed)
    {
        std::size_t separator_segment_width = text::uni::CountFirstBytes(chars_test_failed_ending_separator);
        std::string separator;
        for (std::size_t i = 0; i + separator_segment_width - 1 < separator_line_width; i += separator_segment_width)
            separator += chars_test_failed_ending_separator;

        // The test failure message, and a separator after that.
        terminal.Print("%s%s%s%s\n",
            terminal.AnsiResetString().data(),
            terminal.AnsiDeltaString({}, style_test_failed_ending_separator).data(),
            separator.c_str(),
            terminal.AnsiResetString().data()
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

    state.test_counter++;
}

void ta_test::modules::ProgressPrinter::OnPreFailTest(const RunSingleTestResults &data)
{
    TextStyle cur_style;

    auto st_path = terminal.AnsiDeltaString(cur_style, common_data.style_path);
    auto st_message = terminal.AnsiDeltaString(cur_style, common_data.style_error);
    auto st_group = terminal.AnsiDeltaString(cur_style, style_failed_group_name);
    auto st_name = terminal.AnsiDeltaString(cur_style, style_failed_name);
    auto st_separator = terminal.AnsiDeltaString(cur_style, style_test_failed_separator);

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

    // The test failure message, and a separator after that.
    terminal.Print("\n%s%s%s:\n%s%s%s%.*s%s%.*s %s%s%s\n\n",
        terminal.AnsiResetString().data(),
        st_path.data(),
        common_data.LocationToString(data.test->Location()).c_str(),
        st_message.data(),
        chars_test_failed.c_str(),
        st_group.data(),
        int(group.size()), group.data(),
        st_name.data(),
        int(name.size()), name.data(),
        st_separator.data(),
        separator.c_str(),
        terminal.AnsiResetString().data()
    );
}

// --- modules::ResultsPrinter ---

void ta_test::modules::ResultsPrinter::OnPostRunTests(const RunTestsResults &data)
{
    // Reset color.
    terminal.Print("%s\n", terminal.AnsiResetString().data());

    std::size_t num_passed = data.num_tests - data.failed_tests.size();
    std::size_t num_skipped = data.num_tests_with_skipped - data.num_tests;

    // The number of skipped tests.
    if (num_skipped > 0 && data.num_tests > 0)
    {
        terminal.Print("%s%zu test%s skipped%s\n",
            terminal.AnsiDeltaString({}, style_num_skipped).data(),
            num_skipped,
            num_skipped != 1 ? "s" : "",
            terminal.AnsiResetString().data()
        );
    }

    if (data.num_tests == 0)
    {
        // No tests to run.
        terminal.Print("%sNO TESTS TO RUN%s\n",
            terminal.AnsiDeltaString({}, style_no_tests).data(),
            terminal.AnsiResetString().data()
        );
    }
    else if (data.failed_tests.size() == 0)
    {
        // All passed.

        terminal.Print("%s%s%zu TEST%s PASSED%s\n",
            terminal.AnsiDeltaString({}, style_all_passed).data(),
            num_passed > 1 ? "ALL " : "",
            num_passed,
            num_passed != 1 ? "S" : "",
            terminal.AnsiResetString().data()
        );
    }
    else
    {
        // Some or all failed.

        // Passed tests, if any.
        if (num_passed > 0)
        {
            terminal.Print("%s%zu test%s passed%s\n",
                terminal.AnsiDeltaString({}, style_num_passed).data(),
                num_passed,
                num_passed != 1 ? "s" : "",
                terminal.AnsiResetString().data()
            );
        }

        // Failed tests.
        terminal.Print("%s%s%zu TEST%s FAILED%s\n",
            terminal.AnsiDeltaString({}, style_num_failed).data(),
            num_passed == 0 && data.failed_tests.size() > 1 ? "ALL " : "",
            data.failed_tests.size(),
            data.failed_tests.size() == 1 ? "" : "S",
            terminal.AnsiResetString().data()
        );
    }
}

// --- modules::AssertionPrinter ---

void ta_test::modules::AssertionPrinter::OnAssertionFailed(const BasicAssertionInfo &data, std::optional<std::string_view> message)
{
    PrintAssertionFrameLow(data, message, true);
    text::PrintContext(&data);
}

bool ta_test::modules::AssertionPrinter::PrintContextFrame(const context::BasicFrame &frame)
{
    if (auto assertion_frame = dynamic_cast<const BasicAssertionInfo *>(&frame))
    {
        PrintAssertionFrameLow(*assertion_frame, {}, false);
        return true;
    }

    return false;
}

void ta_test::modules::AssertionPrinter::PrintAssertionFrameLow(const BasicAssertionInfo &data, std::optional<std::string_view> message, bool is_most_nested) const
{
    text::TextCanvas canvas(&common_data);
    std::size_t line_counter = 0;

    // The file path.
    canvas.DrawString(line_counter++, 0, common_data.LocationToString(data.SourceLocation()) + ":", {.style = common_data.style_path, .important = true});

    { // The main error message.
        std::size_t column = 0;
        if (is_most_nested)
            column = canvas.DrawString(line_counter, 0, chars_assertion_failed, {.style = common_data.style_error, .important = true});
        else
            column = canvas.DrawString(line_counter, 0, chars_in_assertion, {.style = common_data.style_stack_frame, .important = true});

        if (message)
            canvas.DrawString(line_counter, column + 1, *message, {.style = style_user_message, .important = true});

        line_counter++;
    }

    line_counter++;

    // This is set later if we actually have an expression to print.
    const BasicModule::BasicAssertionExpr *expr = nullptr;
    std::size_t expr_line = line_counter;
    std::size_t expr_column = 0; // This is also set later.

    { // The assertion call.
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
    canvas.Print(terminal);
}

// --- modules::ExceptionPrinter ---

void ta_test::modules::ExceptionPrinter::OnUncaughtException(const RunSingleTestInfo &test, const std::exception_ptr &e)
{
    (void)test;

    terminal.Print("%s%s%s%s\n",
        terminal.AnsiResetString().data(),
        terminal.AnsiDeltaString({}, common_data.style_error).data(),
        chars_error.c_str(),
        terminal.AnsiResetString().data()
    );

    PrintException(terminal, e);
    terminal.Print("\n");

    text::PrintContext();
}

// --- modules::MustThrowPrinter ---

void ta_test::modules::MustThrowPrinter::OnMissingException(const MustThrowInfo &data)
{
    PrintFrame(data, nullptr, true);
    text::PrintContext(&data);
}

bool ta_test::modules::MustThrowPrinter::PrintContextFrame(const context::BasicFrame &frame)
{
    if (auto ptr = dynamic_cast<const BasicModule::MustThrowInfo *>(&frame))
    {
        PrintFrame(*ptr, nullptr, false);
        return true;
    }
    if (auto ptr = dynamic_cast<const BasicModule::CaughtExceptionInfo *>(&frame))
    {
        PrintFrame(*ptr->must_throw_call, ptr, false);
        return true;
    }

    return false;
}

void ta_test::modules::MustThrowPrinter::PrintFrame(const BasicModule::MustThrowInfo &data, const BasicModule::CaughtExceptionInfo *caught, bool is_most_nested)
{
    const std::string *message = nullptr;
    if (caught)
    {
        message = &chars_throw_location;
        terminal.Print("%s%s%s%s\n",
            terminal.AnsiResetString().data(),
            terminal.AnsiDeltaString({}, common_data.style_stack_frame).data(),
            chars_exception_contents.c_str(),
            terminal.AnsiResetString().data()
        );
        PrintException(terminal, caught->elems.empty() ? nullptr : caught->elems.front().exception);
        terminal.Print("\n");
    }
    else if (is_most_nested)
    {
        message = &chars_expected_exception;
    }
    else
    {
        message = &chars_while_expecting_exception;
    }

    TextStyle cur_style;

    auto st_path = terminal.AnsiDeltaString(cur_style, common_data.style_path);
    auto st_message = terminal.AnsiDeltaString(cur_style, is_most_nested && !caught ? common_data.style_error : common_data.style_stack_frame);

    terminal.Print("%s%s%s:\n%s%s%s\n\n",
        terminal.AnsiResetString().data(),
        st_path.data(),
        common_data.LocationToString(data.loc).c_str(),
        st_message.data(),
        message->c_str(),
        terminal.AnsiResetString().data()
    );

    text::TextCanvas canvas(&common_data);
    std::size_t column = common_data.code_indentation;
    column += canvas.DrawString(0, column, data.macro_name, {.style = common_data.style_failed_macro, .important = true});
    column += canvas.DrawString(0, column, "(", {.style = common_data.style_failed_macro, .important = true});
    column += common_data.spaces_in_macro_call_parentheses;
    column += text::expr::DrawToCanvas(canvas, 0, column, data.expr);
    column += common_data.spaces_in_macro_call_parentheses;
    column += canvas.DrawString(0, column, ")", {.style = common_data.style_failed_macro, .important = true});
    canvas.InsertLineBefore(canvas.NumLines());
    canvas.Print(terminal);
}

// --- modules::TracePrinter ---

bool ta_test::modules::TracePrinter::PrintContextFrame(const context::BasicFrame &frame)
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
        canvas.Print(terminal);

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

void ta_test::modules::DebuggerDetector::OnAssertionFailed(const BasicAssertionInfo &data, std::optional<std::string_view> message)
{
    (void)message;

    if (break_on_failure ? *break_on_failure : IsDebuggerAttached())
        data.should_break = true;
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
        if (detector.break_on_failure && *detector.break_on_failure)
            PrintNote("Will break on failure.");
        else if (!detector.break_on_failure && detector.IsDebuggerAttached())
            PrintNote("Will break on failure (because a debugger is attached, `--catch` to override).");

        if (detector.catch_exceptions && !*detector.catch_exceptions)
            PrintNote("Will not catch exceptions.");
        else if (!detector.catch_exceptions && detector.IsDebuggerAttached())
            PrintNote("Will not catch exceptions (because a debugger is attached, `--no-break` to override).");
        return true;
    });
}
