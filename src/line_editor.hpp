#ifndef INKAMATH_LINE_EDITOR_HPP
#define INKAMATH_LINE_EDITOR_HPP

#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

// The keys a terminal sends, made into a line. Apart from the terminal so that
// it can be tested by feeding it bytes.
class LineEditor {
public:
    enum class Outcome { editing, done, end_of_input, cancelled };

    explicit LineEditor(const std::vector<std::string>& history)
        : history_(history), recalled_(history.size()) {}

    [[nodiscard]] Outcome Feed(char c) {
        if (!escape_.empty()) return Escape(c);
        switch (c) {
            case '\r':
            case '\n':
                return Outcome::done;
            case '\x1b':
                escape_ = c;
                return Outcome::editing;
            case '\x03':  // Ctrl-C
                return Outcome::cancelled;
            case '\x04':  // Ctrl-D
                if (line_.empty()) return Outcome::end_of_input;
                if (cursor_ < line_.size()) line_.erase(cursor_, 1);
                return Outcome::editing;
            case '\x01':  // Ctrl-A
                cursor_ = 0;
                return Outcome::editing;
            case '\x05':  // Ctrl-E
                cursor_ = line_.size();
                return Outcome::editing;
            case '\x15':  // Ctrl-U
                line_.erase(0, cursor_);
                cursor_ = 0;
                return Outcome::editing;
            case '\x7f':
            case '\x08':
                if (cursor_ > 0) line_.erase(--cursor_, 1);
                return Outcome::editing;
        }
        // A tab is a space, or a pasted '[1<tab>2]' would read as '[12]'.
        if (c == '\t') c = ' ';
        // The language is ASCII, so nothing else can be part of a line, and a
        // byte of UTF-8 would put the cursor out of step with the screen. Left
        // out, it is not on the screen either: what is shown is what is read.
        if (c >= ' ' && c <= '~') line_.insert(cursor_++, 1, c);
        return Outcome::editing;
    }

    [[nodiscard]] const std::string& Line() const { return line_; }
    [[nodiscard]] size_t             Cursor() const { return cursor_; }

private:
    // ESC, '[' or 'O', parameters, and a final byte from '@' to '~'. A key
    // this does not know is swallowed whole: an arrow typed at the old prompt
    // arrived as "\x1b[D", and its '[' opened a continuation (C60).
    Outcome Escape(char c) {
        escape_ += c;
        if (escape_.size() == 2) {
            if (c != '[' && c != 'O') escape_.clear();
            return Outcome::editing;
        }
        if (c < '@' || c > '~') return Outcome::editing;
        const std::string key = escape_.substr(1);
        escape_.clear();
        if (key == "[D" || key == "OD") {
            if (cursor_ > 0) --cursor_;
        } else if (key == "[C" || key == "OC") {
            if (cursor_ < line_.size()) ++cursor_;
        } else if (key == "[H" || key == "OH" || key == "[1~") {
            cursor_ = 0;
        } else if (key == "[F" || key == "OF" || key == "[4~") {
            cursor_ = line_.size();
        } else if (key == "[3~") {
            if (cursor_ < line_.size()) line_.erase(cursor_, 1);
        } else if (key == "[A" || key == "OA") {
            Recall(-1);
        } else if (key == "[B" || key == "OB") {
            Recall(+1);
        }
        return Outcome::editing;
    }

    // Leaving the line being written for history keeps it, as a shell does.
    void Recall(int step) {
        if (step < 0 && recalled_ == 0) return;
        if (step > 0 && recalled_ == history_.size()) return;
        if (recalled_ == history_.size()) draft_ = line_;
        recalled_ += step;
        line_   = recalled_ == history_.size() ? draft_ : history_[recalled_];
        cursor_ = line_.size();
    }

    const std::vector<std::string>& history_;
    size_t                          recalled_;
    std::string                     draft_;
    std::string                     line_;
    size_t                          cursor_ = 0;
    std::string                     escape_;
};

// A terminal read a key at a time, for as long as one line is being read and
// not while it is evaluated: Ctrl-C during a long evaluation still stops the
// process, and leaves the terminal as it found it.
#ifdef _WIN32
// A console sends the same sequences as a Unix terminal once asked to, which
// is Windows 10 onwards; an older one is read a line at a time. Checked by CI
// for building only -- no runner has a console to press an arrow key in.
class RawTerminal {
public:
    static bool Interactive() {
        const HANDLE in = GetStdHandle(STD_INPUT_HANDLE), out = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD        in_mode = 0, out_mode = 0;
        if (!GetConsoleMode(in, &in_mode) || !GetConsoleMode(out, &out_mode)) return false;
        const bool sequences = SetConsoleMode(in, in_mode | ENABLE_VIRTUAL_TERMINAL_INPUT) &&
                               SetConsoleMode(out, out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        SetConsoleMode(in, in_mode);
        SetConsoleMode(out, out_mode);
        return sequences;
    }

    RawTerminal() : in_(GetStdHandle(STD_INPUT_HANDLE)), out_(GetStdHandle(STD_OUTPUT_HANDLE)) {
        GetConsoleMode(in_, &saved_in_);
        GetConsoleMode(out_, &saved_out_);
        SetConsoleMode(in_, (saved_in_ & ~static_cast<DWORD>(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT |
                                                             ENABLE_PROCESSED_INPUT)) |
                                ENABLE_VIRTUAL_TERMINAL_INPUT);
        SetConsoleMode(out_, saved_out_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    ~RawTerminal() {
        SetConsoleMode(in_, saved_in_);
        SetConsoleMode(out_, saved_out_);
    }
    RawTerminal(const RawTerminal&)            = delete;
    RawTerminal& operator=(const RawTerminal&) = delete;

    bool Read(char& c) {
        DWORD read = 0;
        return ReadFile(in_, &c, 1, &read, nullptr) && read == 1;
    }

    size_t Width() const {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetConsoleScreenBufferInfo(out_, &info)) return 80;
        return static_cast<size_t>(info.srWindow.Right - info.srWindow.Left + 1);
    }

private:
    HANDLE in_, out_;
    DWORD  saved_in_ = 0, saved_out_ = 0;
};
#else
class RawTerminal {
public:
    // A pipe, a file and the transcripts are read a line at a time, as before.
    static bool Interactive() { return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO); }

    // TCSANOW and not TCSAFLUSH: flushing drops what is still to be read, and
    // a pasted matrix is several lines that arrive at once.
    RawTerminal() {
        tcgetattr(STDIN_FILENO, &saved_);
        termios raw = saved_;
        raw.c_iflag &= ~static_cast<tcflag_t>(ICRNL | IXON);
        raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | ISIG | IEXTEN);
        raw.c_cc[VMIN]  = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    ~RawTerminal() { tcsetattr(STDIN_FILENO, TCSANOW, &saved_); }
    RawTerminal(const RawTerminal&)            = delete;
    RawTerminal& operator=(const RawTerminal&) = delete;

    bool Read(char& c) { return read(STDIN_FILENO, &c, 1) == 1; }

    // Asked on every key, so that a window resized between two keys is drawn
    // at its new width.
    size_t Width() const {
        winsize size{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) return size.ws_col;
        return 80;
    }

private:
    termios saved_;
};
#endif

// What to write for the terminal to show 'prompt' and 'line' with the cursor
// before 'cursor'. A line wider than the terminal wraps onto the rows below it,
// and '\r' goes back only to the start of the row the cursor is on, so every
// render first climbs 'row' rows to its first one, and leaves in 'row' the row
// it put the cursor on.
inline std::string Render(const std::string& prompt, const std::string& line, size_t cursor,
                          size_t width, size_t& row) {
    std::string screen;
    if (row > 0) screen += "\x1b[" + std::to_string(row) + "A";
    screen += "\r\x1b[J" + prompt + line;
    // A row written to its last column leaves the cursor on that column until
    // the next character, and terminals disagree about where it goes then.
    // Moving to the next row explicitly takes the question away.
    const size_t end = prompt.size() + line.size();
    if (end > 0 && end % width == 0) screen += "\r\n";
    const size_t at = prompt.size() + cursor;
    row             = at / width;
    if (at == end) return screen;
    if (const size_t up = end / width - row; up > 0) screen += "\x1b[" + std::to_string(up) + "A";
    screen += "\r";
    if (at % width > 0) screen += "\x1b[" + std::to_string(at % width) + "C";
    return screen;
}

// One line from a terminal, edited in place: done with the line in 'line', or
// end_of_input, or cancelled.
inline LineEditor::Outcome ReadLine(const std::string&              prompt,
                                    const std::vector<std::string>& history, std::string& line) {
    LineEditor  editor(history);
    RawTerminal terminal;
    size_t      row = 0;
    for (;;) {
        // Redrawn whole on every key: a line is short, and it is the simplest
        // thing that is always right.
        const size_t width  = terminal.Width();
        std::string  screen = Render(prompt, editor.Line(), editor.Cursor(), width, row);
        std::fwrite(screen.data(), 1, screen.size(), stdout);
        std::fflush(stdout);

        char c;
        if (!terminal.Read(c)) return LineEditor::Outcome::end_of_input;
        const LineEditor::Outcome outcome = editor.Feed(c);
        if (outcome == LineEditor::Outcome::editing) continue;
        // Past the end of the line before anything else is written, or what
        // comes next lands on the rows the line wrapped onto.
        screen = Render(prompt, editor.Line(), editor.Line().size(), width, row);
        if (outcome == LineEditor::Outcome::cancelled)
            screen += "^C\r\n";
        else if ((prompt.size() + editor.Line().size()) % width != 0)
            screen += "\r\n";
        std::fwrite(screen.data(), 1, screen.size(), stdout);
        line = editor.Line();
        return outcome;
    }
}

#endif  // INKAMATH_LINE_EDITOR_HPP
