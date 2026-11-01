#pragma once
#include <cstring>
#include <string>
#include <vector>
namespace ExcRankTooltip
{
struct Line
{
    std::string text;
    int color;
    bool bold;
    Line(std::string t, int c, bool b = false) : text(t), color(c), bold(b)
    {
    }
};
// Remove only spacer rows when necessary; native information and order are preserved.
template <std::size_t Rows, std::size_t Columns, class Color, class Bold>
bool Append(char (&text)[Rows][Columns], Color (&colors)[Rows], Bold (&bold)[Rows], int &count,
            const std::vector<Line> &lines)
{
    if (count < 0 || static_cast<std::size_t>(count) > Rows || lines.size() > Rows)
        return false;
    for (const auto &line : lines)
        if (line.text.size() >= Columns)
            return false;
    unsigned blanks = 0;
    for (int n = 0; n < count; ++n)
        if (!text[n][0] || text[n][0] == '\n' || (text[n][0] == ' ' && !text[n][1]))
            ++blanks;
    if (static_cast<std::size_t>(count) + lines.size() > Rows + blanks)
        return false;
    for (int n = count - 1; n >= 0 && static_cast<std::size_t>(count) + lines.size() > Rows; --n)
        if (!text[n][0] || text[n][0] == '\n' || (text[n][0] == ' ' && !text[n][1]))
        {
            std::memmove(text + n, text + n + 1, (count - n - 1) * sizeof(text[0]));
            std::memmove(colors + n, colors + n + 1, (count - n - 1) * sizeof(colors[0]));
            std::memmove(bold + n, bold + n + 1, (count - n - 1) * sizeof(bold[0]));
            --count;
        }
    for (const auto &line : lines)
    {
        std::memcpy(text[count], line.text.c_str(), line.text.size() + 1);
        colors[count] = static_cast<Color>(line.color);
        bold[count] = line.bold;
        ++count;
    }
    return true;
}
} // namespace ExcRankTooltip
