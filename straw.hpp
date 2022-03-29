#ifndef STRAW_HPP
#define STRAW_HPP 1

#include <sstream>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <span>
#include <string>
#include <concepts>
#include <cassert>
#include <iostream>
#include <algorithm>

namespace straw
{

struct color {
    std::uint8_t r, g, b;
    constexpr color(std::uint8_t R, std::uint8_t G, std::uint8_t B) : r(R), g(G), b(B) {}
    constexpr color(std::uint8_t A) : r(A), g(A), b(A) {}

    constexpr uint32_t single() { return (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b; }
    friend constexpr bool operator==(const color &lhs, const color &rhs) = default;
};

struct attribs {
    color bg{0, 0, 0}, fg{255, 255, 255};
    bool bold, underline;
    constexpr attribs() = default;
    constexpr attribs(color Fg) : fg(Fg) {}
    constexpr attribs(color Bg, color Fg) : bg(Bg), fg(Fg) {}
    constexpr attribs(color Bg, color Fg, bool B, bool U) : bg(Bg), fg(Fg), bold(B), underline(U) {}

    friend constexpr bool operator==(const attribs &lhs, const attribs &rhs) = default;
};

template<typename T>
concept char_type = std::integral<T>;

template<char_type chartype>
struct cell {
    chartype chr{};
    attribs attr{};

    constexpr cell() = default;
    constexpr cell(chartype Chr) : chr(Chr) {}
    constexpr cell(chartype Chr, color Fg) : chr(Chr), attr(Fg) {}
    constexpr cell(chartype Chr, color Bg, color Fg) : chr(Chr), attr(Bg, Fg) {}
    constexpr cell(chartype Chr, color Bg, color Fg, bool B, bool U) : chr(Chr), attr(Bg, Fg, B, U) {}
    constexpr cell(chartype Chr, attribs Attr) : chr(Chr), attr(Attr) {}

    friend constexpr bool operator==(const cell<chartype> &lhs, const cell<chartype> &rhs) = default;
};

const std::string ANSI_ESCAPE = "\033[";
static void ANSI_MOVE(unsigned x, unsigned y) { std::stringstream ss; ss << ANSI_ESCAPE << y+1 << ';' << x+1 << 'H'; std::cout << ss.str(); }
static void ANSI_COLOR_FG(color c) { std::stringstream ss; ss << ANSI_ESCAPE << "38;2;" << c.r << ';' << c.g << ';' << c.b << 'm'; std::cout << ss.str(); }
static void ANSI_COLOR_BG(color c) { std::stringstream ss; ss << ANSI_ESCAPE << "38;2;" << c.r << ';' << c.g << ';' << c.b << 'm'; std::cout << ss.str(); }

class screen_command_base {};

template<char_type chartype>
class screen {
public:
    explicit screen(unsigned X, unsigned Y, unsigned W, unsigned H, chartype C, color B, color F) :
        m_x(X), m_y(Y), m_width(W), m_height(H), m_front(W * H, cell{C, B, F}), m_back(m_front),
        m_cursorAttribs(B,F), m_fillChar(C)
        {
            redraw();
        }

    explicit screen(unsigned X, unsigned Y, unsigned W, unsigned H, chartype C) :
        screen(X, Y, W, H, C, color{0, 0, 0}, color{255, 255, 255}) {}
    explicit screen(unsigned X, unsigned Y, unsigned W, unsigned H) : 
        screen(X, Y, W, H, ' ') {}

    constexpr void setcursorxy(unsigned x, unsigned y) { m_cursorX = x; m_cursorY = y; }
    constexpr void setcursorfg(uint8_t r, uint8_t g, uint8_t b) { m_cursorAttribs.fg = color{r, g, b}; }
    constexpr void setcursorbg(uint8_t r, uint8_t g, uint8_t b) { m_cursorAttribs.bg = color{r, g, b}; }
    constexpr void setcursorbold(bool bold) { m_cursorAttribs.bold = bold; }
    constexpr void setcursorunderline(bool underline) { m_cursorAttribs.underline = underline; }

    constexpr unsigned getcursorx() { return m_cursorY; }
    constexpr unsigned getcursory() { return m_cursorY; }
    constexpr unsigned getx() { return m_x; }
    constexpr unsigned gety() { return m_y; }
    constexpr unsigned getwidth() { return m_width; }
    constexpr unsigned getheight() { return m_height; }

    void clear(const chartype c) { std::fill(m_front.begin(), m_front.end(), cell{c, m_cursorAttribs}); }

    void scroll() {
        m_cursorY = m_height - 1;
        m_front = std::vector<cell<chartype>>(m_front.begin() + m_width, m_front.end());
        std::fill(m_front.end() - m_width, m_front.end(), cell{m_fillChar, m_cursorAttribs});
    }

    void setc(unsigned x, unsigned y, chartype c) {
        assert(x < m_width);
        assert(y < m_height);
        m_front[x + (y * m_width)] = cell{c, m_cursorAttribs};
    }

    void putc(chartype c){
        if(m_cursorY == m_height) {
            scroll();
        }
        switch(c) {
            case '\n':
                m_cursorX = 0; m_cursorY++;
                break;
            default:
                (*this)[m_cursorY][m_cursorX++] = 
                    cell{c, m_cursorAttribs};
                break;
        }
        if(m_cursorX == m_width) {
            m_cursorY++;
            m_cursorX = 0;
        }
    }

    void puts(const std::basic_string<chartype> &s){
        for(const chartype &c : s) this->putc(c);
    }
    
    void redraw() {
        attribs cattr = (*this)[0][0].attr;
        for(unsigned y = 0; y < m_height; y++) {
            ANSI_MOVE(0, y);
            for(cell c : (*this)[y]) {
                if(c.attr != cattr) {
                    ANSI_COLOR_FG(c.attr.fg);
                    ANSI_COLOR_BG(c.attr.bg);
                    cattr = c.attr;
                }
                std::cout << c.chr;
            }
            std::cout << std::flush;
        }
        m_back = m_front;
    }

    void flush() {
        for(unsigned y = 0; y < m_height; y++) {
            auto backspan = std::span<cell<chartype>>(
                    m_back.begin() + (y * m_width),
                    m_back.begin() + (y * m_width) + m_width);
            auto frontspan = (*this)[y];
            if(std::equal(backspan.begin(), backspan.end(), frontspan.begin(), frontspan.end())) continue;
            for(unsigned x = 0; x < m_width; x++) {
                if(frontspan[x] == backspan[x]) continue;
                ANSI_MOVE(x, y);
                if(frontspan[x].attr != backspan[x].attr) {
                    ANSI_COLOR_FG(frontspan[x].attr.fg);
                    ANSI_COLOR_BG(frontspan[x].attr.bg);
                }
                std::cout << frontspan[x].chr;
            }
            std::cout << std::flush;
        }
    }

    std::span<cell<chartype>> operator[](std::size_t i) { 
        assert(i < m_width * m_height);
        return std::span<cell<chartype>>(
                m_front.begin() + (i * m_width),
                m_front.begin() + (i * m_width) + m_width);
    }

    template<typename T>
        requires(!std::is_base_of<screen_command_base, T>::value)
    friend screen &operator<<(screen &o, const T &rhs) {
        std::basic_stringstream<chartype> ss;
        ss << rhs;
        o.puts(ss.str());
        return o;
    }

private:
    unsigned m_x{}, m_y{};
    unsigned m_width{}, m_height{};

    std::vector<cell<chartype>> m_front;
    std::vector<cell<chartype>> m_back;
    
    unsigned m_cursorX{}, m_cursorY{};
    attribs m_cursorAttribs;
    chartype m_fillChar;
};

struct screen_command_flush : public screen_command_base {
    explicit screen_command_flush() = default;
    template<char_type T>
    friend screen<T> &operator<<(screen<T> &o, const screen_command_flush &cmd) {
        (void)cmd;
        o.flush();
        return o;
    }
};
[[nodiscard]] static screen_command_flush flush() { return screen_command_flush{}; }

template<char_type chartype>
struct screen_command_clear : public screen_command_base {
    chartype c;
    explicit screen_command_clear(chartype C) : c(C) {}
    friend screen<chartype> &operator<<(screen<chartype> &o, const screen_command_clear &cmd) {
        o.clear(cmd.c);
        return o;
    }
};
template<char_type chartype>
[[nodiscard]] static screen_command_clear<chartype> clear() { return screen_command_clear{chartype{}}; }
template<char_type chartype>
[[nodiscard]] static screen_command_clear<chartype> clear(chartype c) { return screen_command_clear{c}; }

struct screen_command_move : public screen_command_base {
    unsigned x, y;
    explicit screen_command_move(unsigned X, unsigned Y) : x(X), y(Y) {}
    template<char_type T>
    friend screen<T> &operator<<(screen<T> &o, const screen_command_move &cmd) {
        o.setcursorxy(cmd.x, cmd.y);
        return o;
    }
};
[[nodiscard]] static screen_command_move move(unsigned x, unsigned y) { return screen_command_move{x, y}; }

template<char_type chartype>
struct screen_command_plot : public screen_command_base {
    chartype c;
    unsigned x, y;
    explicit screen_command_plot(unsigned X, unsigned Y, chartype C) : c(C), x(X), y(Y) {}
    friend screen<chartype> &operator<<(screen<chartype> &o, const screen_command_plot &cmd) {
        o.setc(cmd.x, cmd.y, cmd.c);
        return o;
    }
};
template<char_type T>
[[nodiscard]] static screen_command_plot<T> plot(unsigned x, unsigned y, T c) { return screen_command_plot{x, y, c}; }

struct screen_command_recolor : public screen_command_base {
    color fg, bg;
    bool rfg, rbg;
    explicit screen_command_recolor(color Fg, bool side) : fg(Fg), bg(Fg), rfg(side ? true : false), rbg(side ? false : true) {}
    explicit screen_command_recolor(color Fg, color Bg) : fg(Fg), bg(Bg), rfg(true), rbg(true) {}

    template<char_type T>
    friend screen<T> &operator<<(screen<T> &o, const screen_command_recolor &cmd) {
        if(cmd.rbg) o.setcursorbg(cmd.bg);
        if(cmd.rfg) o.setcursorfg(cmd.fg);
        return o;
    }
};
[[nodiscard]] static screen_command_recolor setfg(uint8_t r, uint8_t g, uint8_t b) { return screen_command_recolor{color{r, g, b}, true}; }
[[nodiscard]] static screen_command_recolor setbg(uint8_t r, uint8_t g, uint8_t b) { return screen_command_recolor{color{r, g, b}, false}; }
[[nodiscard]] static screen_command_recolor setcolor(color fg, color bg) { return screen_command_recolor{fg, bg}; }


};

#endif
