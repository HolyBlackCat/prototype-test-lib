#if CFG_TA_SHARED
#define CFG_TA_API __declspec(dllexport)
#endif

#include "testlib.h"

#if CFG_TA_CXXABI_DEMANGLE
#include <cxxabi.h>
#endif

#if defined(_WIN32)
#include <windows.h> // IsDebuggerPresent
#elif defined(__linux__)
#include <fstream> // To read `/proc/self/status` to detect the debugger.
#endif

bool ta_test::GlobalConfig::DefaultIsDebuggerAttached()
{
    #if defined(_WIN32)
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
            if (detail::IsDigit(line[i]) && line[i] != '0')
                return true;
        }
    }
    return false;
    #else
    return false;
    #endif
}

std::optional<std::string> ta_test::GlobalConfig::DefaultExceptionToMessage(const std::exception_ptr &e)
{
    try
    {
        std::rethrow_exception(e);
    }
    catch (const std::exception &e)
    {
        return CFG_TA_FORMAT("{}:  {}", detail::Demangler{}(typeid(e).name()), e.what());
    }
}

ta_test::GlobalConfig &ta_test::Config()
{
    static GlobalConfig ret;
    return ret;
}

ta_test::detail::Demangler::Demangler() {}

ta_test::detail::Demangler::~Demangler()
{
    #if CFG_TA_CXXABI_DEMANGLE
    // Freeing a nullptr is a no-op.
    std::free(buf_ptr);
    #endif
}

const char *ta_test::detail::Demangler::operator()(const char *name)
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

void ta_test::detail::HardError(std::string_view message, HardErrorKind kind)
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

    FILE *stream = Config().output_stream;
    if (!stream)
        stream = stdout;
    if (!Config().text_color)
        Config().text_color = false;

    std::fprintf(stream, "%s%s %s: %.*s %s\n",
        AnsiResetString().data(),
        AnsiDeltaString({}, Config().style.internal_error).buf,
        kind == HardErrorKind::internal ? "Internal error" : "Error",
        int(message.size()), message.data(),
        AnsiResetString().data()
    );

    // Stop.
    // Don't need to check whether the debugger is attached, a crash is fine.
    CFG_TA_BREAKPOINT();
    std::terminate();
}

void ta_test::detail::PrintNote(std::string_view message)
{
    std::fprintf(Config().output_stream, "%s%sNOTE: %.*s%s\n",
        AnsiResetString().data(),
        AnsiDeltaString({}, Config().style.note).buf,
        int(message.size()), message.data(),
        AnsiResetString().data()
    );
}

std::string ta_test::detail::ExceptionToMessage(const std::exception_ptr &e)
{
    std::optional<std::string> opt;
    for (const auto &func : Config().exception_to_message)
    {
        try
        {
            opt = func(e);
        }
        catch (...)
        {
            // This means the user doesn't have to write `catch (...)` in every handler.
            // They'd likely forget that.
            continue;
        }

        if (opt)
            return *opt;
    }

    return "??";
}

void ta_test::detail::TextCanvas::Print() const
{
    FILE *stream = Config().output_stream;
    PrintToCallback([&](std::string_view string){fwrite(string.data(), string.size(), 1, stream);});
}

std::size_t ta_test::detail::TextCanvas::NumLines() const
{
    return lines.size();
}

void ta_test::detail::TextCanvas::EnsureNumLines(std::size_t size)
{
    if (lines.size() < size)
        lines.resize(size);
}

void ta_test::detail::TextCanvas::EnsureLineSize(std::size_t line_number, std::size_t size)
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

void ta_test::detail::TextCanvas::InsertLineBefore(std::size_t line_number)
{
    if (line_number >/*sic*/ lines.size())
        HardError("Line number is out of range.");

    lines.insert(lines.begin() + std::ptrdiff_t(line_number), Line{});
}

bool ta_test::detail::TextCanvas::IsCellFree(std::size_t line, std::size_t column) const
{
    if (line >= lines.size())
        return true;
    const Line &this_line = lines[line];
    if (column >= this_line.info.size())
        return true;
    return !this_line.info[column].important;
}

bool ta_test::detail::TextCanvas::IsLineFree(std::size_t line, std::size_t column, std::size_t width, std::size_t gap) const
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

std::size_t ta_test::detail::TextCanvas::FindFreeSpace(std::size_t starting_line, std::size_t column, std::size_t height, std::size_t width, std::size_t gap, std::size_t vertical_step) const
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

char32_t &ta_test::detail::TextCanvas::CharAt(std::size_t line, std::size_t pos)
{
    if (line >= lines.size())
        HardError("Line index is out of range.");

    Line &this_line = lines[line];
    if (pos >= this_line.text.size())
        HardError("Character index is out of range.");

    return this_line.text[pos];
}

// Accesses the cell info for the specified cell. The cell must exist.
ta_test::detail::CellInfo &ta_test::detail::TextCanvas::CellInfoAt(std::size_t line, std::size_t pos)
{
    if (line >= lines.size())
        HardError("Line index is out of range.");

    Line &this_line = lines[line];
    if (pos >= this_line.info.size())
        HardError("Character index is out of range.");

    return this_line.info[pos];
}

std::size_t ta_test::detail::TextCanvas::DrawText(std::size_t line, std::size_t start, std::u32string_view text, const CellInfo &info)
{
    EnsureNumLines(line + 1);
    EnsureLineSize(line, start + text.size());
    std::copy(text.begin(), text.end(), lines[line].text.begin() + (std::ptrdiff_t)start);
    for (std::size_t i = start; i < start + text.size(); i++)
        lines[line].info[i] = info;
    return text.size();
}

std::size_t ta_test::detail::TextCanvas::DrawText(std::size_t line, std::size_t start, std::string_view text, const CellInfo &info)
{
    std::u32string decoded_text = uni::Decode(text);
    return DrawText(line, start, decoded_text, info);
}

std::size_t ta_test::detail::TextCanvas::DrawRow(char32_t ch, std::size_t line, std::size_t column, std::size_t width, bool skip_important, const CellInfo &info)
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

void ta_test::detail::TextCanvas::DrawColumn(char32_t ch, std::size_t line_start, std::size_t column, std::size_t height, bool skip_important, const CellInfo &info)
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

void ta_test::detail::TextCanvas::DrawHorBracket(std::size_t line_start, std::size_t column_start, std::size_t height, std::size_t width, const CellInfo &info)
{
    if (width < 2 || height < 1)
        return;

    // Sides.
    if (height > 1)
    {
        DrawColumn(Config().vis.bar, line_start, column_start, height - 1, true, info);
        DrawColumn(Config().vis.bar, line_start, column_start + width - 1, height - 1, true, info);
    }

    // Bottom.
    if (width > 2)
        DrawRow(Config().vis.bracket_bottom, line_start + height - 1, column_start + 1, width - 2, false, info);

    // Corners.
    DrawRow(Config().vis.bracket_corner_bottom_left, line_start + height - 1, column_start, 1, false, info);
    DrawRow(Config().vis.bracket_corner_bottom_right, line_start + height - 1, column_start + width - 1, 1, false, info);
}

void ta_test::detail::TextCanvas::DrawOverline(std::size_t line, std::size_t column_start, std::size_t width, const CellInfo &info)
{
    if (width < 2)
        return;

    // Middle part.
    if (width > 2)
        DrawRow(Config().vis.bracket_top, line, column_start + 1, width - 2, false, info);

    // Corners.
    DrawRow(Config().vis.bracket_corner_top_left, line, column_start, 1, false, info);
    DrawRow(Config().vis.bracket_corner_top_right, line, column_start + width - 1, 1, false, info);
}

std::size_t ta_test::detail::DrawExprToCanvas(TextCanvas &canvas, std::size_t line, std::size_t start, std::string_view expr)
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

    auto lambda = [&](const char &ch, CharKind kind)
    {
        CellInfo &info = canvas.CellInfoAt(line, start + i);
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
                canvas.CellInfoAt(line, start + i - j - 1).style = Config().style.expr_raw_string_delimiters;
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
                        canvas.CellInfoAt(line, start + i - 1).style = Config().style.expr_number;
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
                    info.style = Config().style.expr_string_suffix;
                    break;
                  case CharKind::character:
                    info.style = Config().style.expr_char_suffix;
                    break;
                  case CharKind::raw_string:
                    info.style = Config().style.expr_raw_string_suffix;
                    break;
                  default:
                    HardError("Lexer error during pretty-printing.");
                    break;
                }
            }
            else if (is_number_suffix)
                info.style = Config().style.expr_number_suffix;
            else if (is_number)
                info.style = Config().style.expr_number;
            else if (is_punct)
                info.style = Config().style.expr_punct;
            else
                info.style = Config().style.expr_normal;
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
                        target_style = Config().style.expr_string_prefix;
                        break;
                      case CharKind::character:
                        target_style = Config().style.expr_char_prefix;
                        break;
                      case CharKind::raw_string:
                        target_style = Config().style.expr_raw_string_prefix;
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
                info.style = Config().style.expr_string;
                break;
              case CharKind::character:
                info.style = Config().style.expr_char;
                break;
              case CharKind::raw_string:
              case CharKind::raw_string_initial_sep:
                if (kind == CharKind::raw_string_initial_sep || prev_kind == CharKind::raw_string_initial_sep)
                    info.style = Config().style.expr_raw_string_delimiters;
                else
                    info.style = Config().style.expr_raw_string;
                break;
              default:
                HardError("Lexer error during pretty-printing.");
                break;
            }
            break;
          case CharKind::string_escape_slash:
            info.style = Config().style.expr_string;
            break;
          case CharKind::character_escape_slash:
            info.style = Config().style.expr_char;
            break;
        }

        // Finalize identifiers.
        if (prev_identifier_start && !identifier_start)
        {
            const TextStyle *style = nullptr;

            // Check if this is a keyword.
            std::string_view ident(prev_identifier_start, &ch);
            auto it = Config().highlighted_keywords.find(ident);
            if (it != Config().highlighted_keywords.end())
            {
                switch (it->second)
                {
                  case KeywordKind::generic:
                    style = &Config().style.expr_keyword_generic;
                    break;
                  case KeywordKind::value:
                    style = &Config().style.expr_keyword_value;
                    break;
                  case KeywordKind::op:
                    style = &Config().style.expr_keyword_op;
                    break;
                }
            }
            else if (std::all_of(ident.begin(), ident.end(), [](char ch){return IsIdentifierChar(ch) && !IsAlphaLowercase(ch);}))
            {
                style = &Config().style.expr_all_caps;
            }

            // If this identifier needs a custom style...
            if (style)
            {
                for (std::size_t j = 0; j < ident.size(); j++)
                    canvas.CellInfoAt(line, start + i - j - 1).style = *style;
            }
        }

        prev_kind = kind;

        i++;
    };
    ParseExpr(expr, lambda, nullptr);

    return expr.size();
}

ta_test::detail::GlobalThreadState &ta_test::detail::ThreadState()
{
    thread_local GlobalThreadState ret;
    return ret;
}

void ta_test::detail::PrintAssertionFailure(
    std::string_view raw_expr,
    std::size_t num_args,
    const ArgInfo *arg_info,               // Array of size `num_args`.
    const std::size_t *args_in_draw_order, // Array of size `num_args`.
    const StoredArg *stored_args,          // Array of size `num_args`.
    std::string_view file_name,
    int line_number,
    std::size_t depth // Starts at 0, increments when we go deeper into the assertion stack.
)
{
    TextCanvas canvas;
    std::size_t line_counter = 0;

    // line_counter++;

    if (depth == 0)
        canvas.DrawText(line_counter++, 0, Config().vis.assertion_failed, {.style = Config().style.assertion_failed, .important = true});
    else
        canvas.DrawText(line_counter++, 0, Config().vis.while_checking_assertion, {.style = Config().style.stack_error, .important = true});

    { // The file path.
        CellInfo cell_info = {.style = Config().style.stack_path, .important = true};
        std::size_t column = 0;
        column += canvas.DrawText(line_counter, column, Config().vis.filename_prefix, {.style = Config().style.stack_path_prefix, .important = true});
        column += canvas.DrawText(line_counter, column, file_name, cell_info);
        column += canvas.DrawText(line_counter, column, Config().vis.filename_linenumber_separator, cell_info);
        column += canvas.DrawText(line_counter, column, std::to_string(line_number), cell_info);
        column += canvas.DrawText(line_counter, column, Config().vis.filename_linenumber_suffix, cell_info);
        line_counter++;
    }

    line_counter++;

    std::size_t expr_line = line_counter;

    { // The assertion call.
        std::size_t column = Config().vis.assertion_macro_indentation;

        const CellInfo assertion_macro_cell_info = {.style = Config().style.expr_assertion_macro, .important = true};
        column += canvas.DrawText(line_counter, column, Config().vis.assertion_macro_prefix, assertion_macro_cell_info);
        column += DrawExprToCanvas(canvas, line_counter, column, raw_expr);
        column += canvas.DrawText(line_counter, column, Config().vis.assertion_macro_suffix, assertion_macro_cell_info);
        line_counter++;
    }

    std::size_t expr_column = Config().vis.assertion_macro_indentation + Config().vis.assertion_macro_prefix.size();

    std::u32string this_value;

    // The bracket above the expression.
    std::size_t overline_start = 0;
    std::size_t overline_end = 0;
    // How many subexpressions want an overline.
    // More than one should be impossible, but if it happens, we just combine them into a single fat one.
    int num_overline_parts = 0;

    // Incremented when we print an argument.
    std::size_t color_index = 0;

    for (std::size_t i = 0; i < num_args; i++)
    {
        const std::size_t arg_index = args_in_draw_order[i];
        const StoredArg &this_arg = stored_args[arg_index];
        const ArgInfo &this_info = arg_info[arg_index];

        bool dim_parentheses = true;

        if (this_arg.state == StoredArg::State::in_progress)
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

        if (this_arg.state == StoredArg::State::done)
        {

            this_value = uni::Decode(this_arg.value);

            std::size_t center_x = expr_column + this_info.expr_offset + (this_info.expr_size + 1) / 2 - 1;
            std::size_t value_x = center_x - (this_value.size() + 1) / 2 + 1;
            // Make sure `value_x` didn't underflow.
            if (value_x > std::size_t(-1) / 2)
                value_x = 0;

            const CellInfo this_cell_info = {.style = Config().style.arguments[color_index++ % Config().style.arguments.size()], .important = true};

            if (!this_info.need_bracket)
            {
                std::size_t value_y = canvas.FindFreeSpace(line_counter, value_x, 2, this_value.size(), 1, 2) + 1;
                canvas.DrawText(value_y, value_x, this_value, this_cell_info);
                canvas.DrawColumn(Config().vis.bar, line_counter, center_x, value_y - line_counter, true, this_cell_info);

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
                    canvas.CharAt(bracket_y, center_x) = Config().vis.bracket_bottom_tail;

                // Draw the column connecting us to the text, if it's not directly below.
                canvas.DrawColumn(Config().vis.bar, bracket_y + 1, center_x, value_y - bracket_y - 1, true, this_cell_info);

                // Color the parentheses with the argument color.
                dim_parentheses = false;
                canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset - 1).style.color = this_cell_info.style.color;
                canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset + this_info.expr_size).style.color = this_cell_info.style.color;
            }
        }

        // Dim the macro name.
        for (std::size_t i = 0; i < this_info.ident_size; i++)
            canvas.CellInfoAt(line_counter - 1, expr_column + this_info.ident_offset + i).style.color = Config().style.color_dim;

        // Dim the parentheses.
        if (dim_parentheses)
        {
            canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset - 1).style.color = Config().style.color_dim;
            canvas.CellInfoAt(line_counter - 1, expr_column + this_info.expr_offset + this_info.expr_size).style.color = Config().style.color_dim;
        }
    }

    // The overline.
    if (num_overline_parts > 0)
    {
        if (overline_start > 0)
            overline_start--;
        overline_end++;

        std::u32string_view this_value = num_overline_parts > 1 ? Config().vis.in_this_subexpr_inexact : Config().vis.in_this_subexpr;

        std::size_t center_x = expr_column + overline_start + (overline_end - overline_start) / 2;
        std::size_t value_x = center_x - this_value.size() / 2;

        canvas.InsertLineBefore(expr_line++);

        canvas.DrawOverline(expr_line - 1, expr_column + overline_start, overline_end - overline_start, {.style = Config().style.overline, .important = true});
        canvas.DrawText(expr_line - 2, value_x, this_value, {.style = Config().style.overline, .important = true});

        // Color the parentheses.
        canvas.CellInfoAt(expr_line, expr_column + overline_start).style.color = Config().style.overline.color;
        canvas.CellInfoAt(expr_line, expr_column + overline_end - 1).style.color = Config().style.overline.color;
    }

    canvas.InsertLineBefore(canvas.NumLines());

    canvas.Print();
}

std::vector<std::size_t> ta_test::detail::GlobalState::GetTestListInExecutionOrder() const
{
    std::vector<std::size_t> ret(tests.size());
    std::iota(ret.begin(), ret.end(), std::size_t(0));

    std::sort(ret.begin(), ret.end(), [&](std::size_t a, std::size_t b)
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

    return ret;
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
            TestLocation old_loc = state.tests[it->second]->Location();
            TestLocation new_loc = singleton->Location();
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

void ta_test::RunTests()
{
    const auto &state = detail::State();

    // Print a message if a debugger is attached.
    // Note, it can become attached later, and we'll re-check that later. This is by design.
    if (auto &func = Config().is_debugger_attached)
    {
        if (func())
            detail::PrintNote("Will break on the first problem.");
    }
    else
    {
        if (GlobalConfig::DefaultIsDebuggerAttached())
        {
            detail::PrintNote("Debugger is attached, will break on the first problem.");
            func = []{return true;};
        }
        else
        {
            func = []{return false;};
        }
    }

    auto ordered_tests = state.GetTestListInExecutionOrder();

    std::vector<std::string_view> stack;

    // How much characters in the test counter.
    const int test_counter_width = std::snprintf(nullptr, 0, "%zu", ordered_tests.size());

    std::size_t test_counter = 0;

    for (std::size_t test_index : ordered_tests)
    {
        const detail::BasicTest *test = state.tests[test_index];
        std::string_view test_name = test->Name();

        { // Print the test name, and any prefixes if we're starting a new group.
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
                        detail::AnsiDeltaString style_a(cur_style, Config().style.test_started_index);
                        detail::AnsiDeltaString style_b(cur_style, Config().style.test_started_total_count);

                        std::fprintf(Config().output_stream, "%s%*zu%s/%zu",
                            style_a.buf,
                            test_counter_width, test_counter + 1,
                            style_b.buf,
                            ordered_tests.size()
                        );
                    }
                    else
                    {
                        // No test index, just a gap.
                        std::fprintf(Config().output_stream, "%*s", test_counter_width * 2 + 1, "");
                    }

                    // The gutter border.
                    std::fprintf(Config().output_stream, "%s%.*s",
                        detail::AnsiDeltaString(cur_style, Config().style.test_started_gutter_border).buf,
                        int(Config().vis.starting_test_counter_separator.size()), Config().vis.starting_test_counter_separator.data()
                    );

                    // The indentation.
                    if (!stack.empty())
                    {
                        // Switch to the indentation guide color.
                        std::fprintf(Config().output_stream, "%s", detail::AnsiDeltaString(cur_style, Config().style.test_started_indentation).buf);
                        // Print the required number of guides.
                        for (std::size_t repeat = 0; repeat < stack.size(); repeat++)
                            std::fprintf(Config().output_stream, "%s", Config().vis.starting_test_indent.c_str());
                    }
                    // Print the test name, and reset the color.
                    std::fprintf(Config().output_stream, "%s%s%.*s%s\n",
                        detail::AnsiDeltaString(cur_style, Config().style.test_started).buf,
                        Config().vis.starting_test_prefix.c_str(),
                        int(segment.size()), segment.data(),
                        detail::AnsiResetString().data()
                    );

                    // Push to the stack.
                    stack.push_back(segment);
                }

                if (new_it == test_name.end())
                    break;

                segment_index++;
                it = new_it + 1;
            }
        }

        try
        {
            test->Run();
        }
        catch (InterruptTestException) {}
        catch (...)
        {
            std::string message = detail::ExceptionToMessage(std::current_exception());

            TextStyle cur_style;

            detail::AnsiDeltaString style_a(cur_style, Config().style.test_failed);
            detail::AnsiDeltaString style_b(cur_style, Config().style.test_failed_exception_message);
            fprintf(Config().output_stream, "%s%s%s%s%s\n\n%s",
                detail::AnsiResetString().data(),
                style_a.buf,
                Config().vis.test_failed_exception.c_str(),
                style_b.buf,
                message.c_str(),
                detail::AnsiResetString().data()
            );
        }

        test_counter++;
    }
}
