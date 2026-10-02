#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "inkamath/check.hpp"
#include "inkamath/compile.hpp"
#include "inkamath/interpreter.hpp"
#include "inkamath/number.hpp"
#include "inkamath/numeric_interface.hpp"

#include "line_editor.hpp"

// Usage examples live in test/data/*.ink -- they are literal sessions.

using namespace std;

// An unclosed bracket means the line is not finished, which is what makes a
// printed matrix typeable back: it prints over several lines and reads as one.
// Nothing here can hide a bracket -- there are no strings, and '#' comments to
// the end of the line.
static bool unclosed(const string& text) {
    return Unclosed(text) > 0;
}

// A line continued inside a model's braces keeps its break, since the body
// is one definition a line; inside a bracket, a break is a space.
static const char* joint(const string& text) {
    return Unclosed(text, "{", "}") > 0 ? "\n" : " ";
}

using Interp  = Interpreter<Number>;
using Outcome = LineEditor::Outcome;

static const char help[] = R"(Usage: inkamath [options] [file...]
       inkamath --compile file [model] [-o header.h]
       inkamath --check file instance -o check.c

Runs the files in order and exits; with no file, reads standard input.
At a terminal the prompt edits the line and keeps its history.

  -i          read standard input after the files
  --echo      print each input before its answer, as a transcript
  --version   print the version and exit
  --compile   write the sequences the file defines as a C header over
              doubles, named after the header: a struct, an init and a step;
              given a model the file defines, those of the model instead;
              without -o, list what would not compile, and write nothing
  --check     write a C program that steps an instance the file defines,
              compiled, on the inputs the interpreter gives it, holds each
              term to the interpreter's exact one and says where one drifts
  --help      print this and exit

A file whose first line that is not blank or a comment starts with '>>'
is a transcript, and only its '>>' lines are read.
)";

static string rstrip(string text) {
    text.erase(text.find_last_not_of(" \t\r\n") + 1);
    return text;
}

// Only spacing, or only a comment: not an input, in a file or from a pipe.
static bool blank(const string& line) {
    const size_t first = line.find_first_not_of(" \t\r");
    return first == string::npos || line[first] == '#';
}

// A line of a file, whether it is the whole of an input, and where the file
// is, for 'use' to look beside it.
struct Queued {
    string           line;
    bool             whole;
    filesystem::path directory;
};

// A file's inputs. In a transcript they are its '>>' lines, with the '..' lines
// that continue them, each the whole of an input as the recorder evaluated it,
// and the answers it records are left for 'diff' to compare.
static vector<Queued> inputs(istream& in, const filesystem::path& directory) {
    vector<string> lines;
    for (string line; getline(in, line);) lines.push_back(line);
    vector<Queued> queued;
    const auto     first = find_if_not(lines.begin(), lines.end(), blank);
    if (first == lines.end() || first->rfind(">>", 0) != 0) {
        for (const string& line : lines) queued.push_back({line, false, directory});
        return queued;
    }
    const auto spoken = [](const string& line) {
        const string entry = line.substr(2);
        return rstrip(entry.rfind(' ', 0) == 0 ? entry.substr(1) : entry);
    };
    bool entry = false;
    for (const string& line : lines) {
        if (line.rfind(">>", 0) == 0) {
            queued.push_back({spoken(line), true, directory});
            entry = true;
        } else if (entry && line.rfind("..", 0) == 0) {
            queued.back().line += '\n' + spoken(line);
        } else {
            entry = false;
        }
    }
    return queued;
}

static string render(const Interp& interpreter, const Interp::Result& result) {
    ostringstream out;
    if (const Diagnostic* error = get_if<Diagnostic>(&result)) {
        out << "error: " << error->message;
    } else if (const Echo* echo = get_if<Echo>(&result)) {
        out << echo->text;
    } else {
        out << interpreter.Answer(get<Interp::matrix_type>(result));
    }
    return rstrip(out.str());
}

// A header or a program is named after its file, which must be a C name.
static bool c_name(const string& module, const string& what) {
    if (!module.empty() && isalpha(static_cast<unsigned char>(module[0])) &&
        all_of(module.begin(), module.end(),
               [](char c) { return isalnum(static_cast<unsigned char>(c)) || c == '_'; }))
        return true;
    cerr << "inkamath: '" << module << "' is not a C name, and the " << what
         << " is named after it\n";
    return false;
}

// The file run as it would be at a prompt, stopping at the first error.
static bool run(const string& source, ifstream& in, Interp& p) {
    string line;
    for (const Queued& queued : inputs(in, filesystem::path(source).parent_path())) {
        if (line.empty() && blank(queued.line)) continue;
        line += (line.empty() ? "" : joint(line)) + queued.line;
        if (!queued.whole && unclosed(line)) continue;
        const Interp::Result result = p.Eval(line);
        if (const Diagnostic* error = get_if<Diagnostic>(&result)) {
            cerr << "inkamath: " << source << ": " << line << ": " << error->message << '\n';
            return false;
        }
        line.clear();
    }
    return true;
}

static bool write(const string& target, const string& text) {
    // Binary, so that the file reads alike from every platform it is made on.
    ofstream out(target, ios::binary);
    out << text;
    if (!out) cerr << "inkamath: cannot write '" << target << "'\n";
    return static_cast<bool>(out);
}

// DESIGN.md, phase 14, step 2. The file is run as it would be at a
// prompt, so the definitions compiled are the ones it leaves behind. Without a
// target, what would not compile is listed and nothing is written.
static int compile(const string& source, const string& name, const string& target) {
    ifstream in(source);
    if (!in) {
        cerr << "inkamath: cannot open '" << source << "'\n";
        return 2;
    }
    const string module = filesystem::path(target).stem().string();
    if (!target.empty() && !c_name(module, "header")) return 2;
    const string file = filesystem::path(source).filename().string();
    Interp       p;
    p.Directory(filesystem::path(source).parent_path());
    string         header;
    vector<string> refused;
    try {
        if (name.empty()) {
            if (!run(source, in, p)) return 1;
            if (target.empty())
                refused = CompileC::Refusals(p.Definitions(), file);
            else
                header = CompileC::Header(p.Definitions(), module, file);
        } else {
            // Read as 'use' reads it, so that the model sees the file's names.
            const auto used  = p.Read(filesystem::path(source).stem().string());
            const auto found = used->names.find(name);
            if (found == used->names.end() || !found->second->model) {
                cerr << "inkamath: " << file << " defines no model " << name << '\n';
                return 1;
            }
            const Reference<Interp::matrix_type>& model    = *found->second;
            const auto                            defaults = p.Defaults(model);
            if (target.empty())
                refused = CompileC::Refusals(p.Definitions(), name + " in " + file,
                                             model.model.get(), defaults.get());
            else
                header = CompileC::Header(p.Definitions(), module, name + " in " + file,
                                          model.model.get(), defaults.get());
        }
    } catch (const Refusal& refusal) {
        cerr << "inkamath: " << refusal.what() << '\n';
        return 1;
    } catch (const runtime_error& error) {
        cerr << "inkamath: " << error.what() << '\n';
        return 1;
    }
    if (target.empty()) {
        for (const string& refusal : refused) cout << refusal << '\n';
        return refused.empty() ? 0 : 1;
    }
    return write(target, header) ? 0 : 2;
}

// The oracle (DESIGN.md, next in line): an instance the file defines,
// compiled, and the program that holds it to the interpreter.
static int check(const string& source, const string& name, const string& target) {
    ifstream in(source);
    if (!in) {
        cerr << "inkamath: cannot open '" << source << "'\n";
        return 2;
    }
    const string module = filesystem::path(target).stem().string();
    if (!c_name(module, "program")) return 2;
    const string file = filesystem::path(source).filename().string();
    Interp       p;
    p.Directory(filesystem::path(source).parent_path());
    string program;
    try {
        if (!run(source, in, p)) return 1;
        const auto& names = p.Definitions().Session().names;
        const auto  found = names.find(name);
        const auto  unfed = found == names.end() ? nullptr : p.Definitions().Unfed(found->second);
        if (!unfed) {
            cerr << "inkamath: " << file << " defines no instance " << name << '\n';
            return 1;
        }
        program = CheckC::Program(p, name, found->second, module, name + " in " + file, *unfed);
    } catch (const runtime_error& error) {
        cerr << "inkamath: " << error.what() << '\n';
        return 1;
    }
    return write(target, program) ? 0 : 2;
}

int main(int argc, char* argv[]) {
    bool           echo = false, then_input = false, compiling = false, checking = false;
    string         target;
    vector<string> files;
    for (int i = 1; i < argc; ++i) {
        const string arg = argv[i];
        if (arg == "--help") {
            cout << help;
            return 0;
        }
        if (arg == "--version") {
            cout << "inkamath " INKAMATH_VERSION "\n";
            return 0;
        }
        if (arg == "--compile") {
            compiling = true;
        } else if (arg == "--check") {
            checking = true;
        } else if (arg == "-o" && i + 1 < argc) {
            target = argv[++i];
        } else if (arg == "--echo") {
            echo = true;
        } else if (arg == "-i") {
            then_input = true;
        } else if (arg.size() > 1 && arg[0] == '-') {
            cerr << "inkamath: unknown option '" << arg << "'\nTry 'inkamath --help'.\n";
            return 2;
        } else {
            files.push_back(arg);
        }
    }

    if (compiling) {
        if (files.empty() || files.size() > 2) {
            cerr << "inkamath: --compile takes a file, optionally a model it defines, and -o "
                    "header.h\nTry 'inkamath --help'.\n";
            return 2;
        }
        return compile(files[0], files.size() == 2 ? files[1] : string(), target);
    }

    if (checking) {
        if (files.size() != 2 || target.empty()) {
            cerr << "inkamath: --check takes a file, an instance it defines, and -o check.c\n"
                    "Try 'inkamath --help'.\n";
            return 2;
        }
        return check(files[0], files[1], target);
    }

    // Every file is read before anything runs, so that a command line which
    // cannot be carried out stops before it has done half of it.
    vector<Queued> queued;
    for (const string& name : files) {
        ifstream in(name);
        if (!in) {
            cerr << "inkamath: cannot open '" << name << "'\n";
            return 2;
        }
        const vector<Queued> lines = inputs(in, filesystem::path(name).parent_path());
        queued.insert(queued.end(), lines.begin(), lines.end());
    }

    const bool     read_input = files.empty() || then_input;
    const bool     terminal   = read_input && RawTerminal::Interactive();
    size_t         next       = 0;
    bool           typed      = false;  // whether the line came from the terminal
    bool           whole      = false;  // whether the line is a transcript's entry
    bool           greeted    = false;
    vector<string> history;
    Interp         p;

    // The files, then standard input: from a person, edited at the prompt, or
    // from a pipe, where spacing and comments are skipped as they are in files.
    const auto read_line = [&](const string& prompt, string& line) {
        for (;;) {
            typed = whole = false;
            p.Directory(next < queued.size() ? queued[next].directory : filesystem::path("."));
            if (next < queued.size()) {
                line  = queued[next].line;
                whole = queued[next++].whole;
            } else if (!read_input) {
                return Outcome::end_of_input;
            } else if (terminal) {
                if (!greeted) cout << "inkamath " INKAMATH_VERSION "\n\n";
                greeted = typed = true;
                return ReadLine(prompt, history, line);
            } else if (!getline(cin, line)) {
                return Outcome::end_of_input;
            }
            if (whole || !blank(line)) return Outcome::done;
        }
    };

    bool   failed = false;
    for (;;) {
        string  s;
        Outcome read = read_line(">> ", s);
        if (read == Outcome::end_of_input) break;
        if (read == Outcome::cancelled) continue;
        if (s == "q") break;  // quit interpreter

        while (!whole && unclosed(s)) {
            string more;
            read = read_line(".. ", more);
            if (read != Outcome::done) break;  // end of input: let it fail as written
            s += joint(s) + more;
        }
        // Ctrl-C at a continuation drops the whole line, as it does at the first.
        if (read == Outcome::cancelled) continue;

        const Interp::Result result = p.Eval(s);
        const string         answer = render(p, result);
        if (typed) {
            // The whole of a line that continued, so that recalling it gives
            // back something that reads.
            // A model's lines are not one to edit.
            if (!s.empty() && s.find('\n') == string::npos &&
                (history.empty() || history.back() != s))
                history.push_back(s);
            cout << answer << "\n\n";
            continue;
        }
        failed = failed || holds_alternative<Diagnostic>(result);
        if (echo) {
            cout << ">> ";
            for (const char c : s) cout << (c == '\n' ? "\n.. " : string(1, c));
            cout << '\n';
        }
        cout << answer << (echo ? "\n\n" : "\n");
    }
    return failed ? 1 : 0;
}
