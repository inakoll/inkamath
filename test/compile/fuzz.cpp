// Random models, each run by the interpreter and compiled by CompileC, written
// out as one C file that steps every compiled model and holds each term to the
// interpreter's (DESIGN.md, phase 14, step 2). The hand-written models
// test what they were written for; this tests the claim that a compiled model
// answers as the interpreter does.
//
// Constants and inputs are quarters, which a double holds exactly, so that most
// arithmetic is exact on both sides and a difference is rounding, not noise.

#include "inkamath/compile.hpp"
#include "inkamath/interpreter.hpp"
#include "inkamath/number.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

constexpr int models = 300;
constexpr int steps  = 8;

// A deterministic generator, so that a failure reproduces.
struct Random {
    std::uint64_t state = 0x2545f4914f6cdd1du;
    std::uint64_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
    int  below(int n) { return static_cast<int>(next() % static_cast<std::uint64_t>(n)); }
    bool chance(int percent) { return below(100) < percent; }
};

std::string Quarter(Random& random) {
    const int k = random.below(17) - 8;
    return k % 4 == 0 ? "(" + std::to_string(k / 4) + ")" : "(" + std::to_string(k) + "/4)";
}

// What an expression in a general clause may read: the sequences before it at
// any lag, itself and those after it only back in time, so that no two terms
// of one index need each other.
struct Scope {
    std::vector<std::string> before, after;
    std::string              self;
    bool                     based = false;
};

std::string Read(Random& random, const Scope& scope) {
    switch (random.below(6)) {
        case 0:
            if (!scope.before.empty()) {
                const std::string& name =
                    scope.before[random.below(static_cast<int>(scope.before.size()))];
                return random.chance(50) ? name + "_n" : name + "_(n-1)";
            }
            [[fallthrough]];
        case 1:
            if (scope.based) return scope.self + "_(n-1)";
            [[fallthrough]];
        case 2:
            if (!scope.after.empty() && scope.based)
                return scope.after[random.below(static_cast<int>(scope.after.size()))] + "_(n-1)";
            [[fallthrough]];
        case 3:
            return random.chance(50) ? "u_n" : "u_(n-1)";
        case 4:
            return random.chance(50) ? "w_n" : "w_(n-2)";
        default:
            return "n/8";
    }
}

std::string Expression(Random& random, const Scope& scope, int depth) {
    if (depth == 0 || random.chance(25)) {
        switch (random.below(4)) {
            case 0:
                return Quarter(random);
            case 1:
                return std::string(1, "pqr"[random.below(3)]);
            default:
                return Read(random, scope);
        }
    }
    const std::string a = Expression(random, scope, depth - 1);
    const std::string b = Expression(random, scope, depth - 1);
    switch (random.below(10)) {
        case 0:
        case 1:
            return "(" + a + " + " + b + ")";
        case 8:
            // Of a value a double holds exactly: of any other, the floor of
            // an exact integer's rounding would be a unit off, as floor is.
            return "floor(" + Quarter(random) + "*u_n)";
        case 2:
            return "(" + a + " - " + b + ")";
        case 3:
            return "(" + a + "*" + b + ")/2";
        case 4:
            return "(" + a + ")/(1 + (" + b + ")^2)";
        case 5:
            return "-(" + a + ")";
        case 6:
            return "sum_(k=0)^2 " + Quarter(random) + "*u_(n-k)";
        case 7:
            return "~(" + a + ")";
        default:
            return "(" + a + ")*" + Quarter(random);
    }
}

std::string Comparison(Random& random, const Scope& scope) {
    static const char* const ops[] = {" < ", " > ", " <= ", " >= "};
    const std::string        compared =
        Expression(random, scope, 1) + ops[random.below(4)] + Quarter(random);
    if (!random.chance(30)) return compared;
    return compared + (random.chance(50) ? " and " : " or ") + Comparison(random, scope);
}

std::vector<std::string> RandomModel(Random& random) {
    std::vector<std::string>       lines = {"p = " + Quarter(random), "q = " + Quarter(random),
                                            "r = p*q + " + Quarter(random)};
    const std::vector<std::string> names = {"a", "b", "c", "d"};
    const int                      count = 1 + random.below(4);
    for (int s = 0; s < count; ++s) {
        Scope scope;
        scope.self  = names[s];
        scope.based = random.chance(80);
        scope.before.assign(names.begin(), names.begin() + s);
        scope.after.assign(names.begin() + s + 1, names.begin() + count);
        // Two base clauses, so that the general one starts where a read two
        // steps back, as far as any reaches, exists.
        if (scope.based) {
            lines.push_back(scope.self + "_0 = " + Quarter(random));
            lines.push_back(scope.self + "_1 = " + Quarter(random));
        }
        if (random.chance(30))
            lines.push_back(scope.self + "_n | " + Comparison(random, scope) + " = " +
                            Expression(random, scope, 2));
        lines.push_back(scope.self + "_n = " + Expression(random, scope, 3));
    }
    // A 2x2 block: a product, a transpose, an inverse and cells read out.
    if (random.chance(50)) {
        const auto q = [&] { return Quarter(random); };
        lines.push_back("F = [" + q() + " " + q() + "; " + q() + " " + q() + "]");
        lines.push_back("G[j<=2, k<=2] = (j + k)/4");
        lines.push_back("v_0 = [" + q() + "; " + q() + "]");
        lines.push_back("v_n = F*v_(n-1)/2 + [u_n; a_(n-1)]");
        lines.push_back("M_n = (F'*F + G*(1 + a_n^2))^-1");
        lines.push_back("m_n = (M_n*v_n)[1,1] + v_n[2,1]*" + q());
    }
    // A term by its cells: a base with one cell of its own, a cell of every
    // term, the term before read transposed, and a guard on the place and a
    // value together.
    if (random.chance(50)) {
        const auto q = [&] { return Quarter(random); };
        lines.push_back("t_0[j<=2, k<=2] = (j - k)/4");
        lines.push_back("t_0[2,1] = " + q());
        lines.push_back("t_n[j<=2, k<=2] = t_(n-1)[k, j]/2 + " + q() + "*u_n");
        lines.push_back("t_n[j<=2, k<=2] | j == k and a_n > " + q() + " = a_n");
        lines.push_back("t_n[2,1] = " + q() + " + w_n");
        lines.push_back("o_n = t_n[1,2] + t_n[2,2]");
    }
    return lines;
}

// The interpreter's answer as doubles, or nothing where it reports an error.
std::optional<std::vector<double>> Answer(Interpreter<Number>& session, const std::string& line) {
    const auto  result = session.Eval(line);
    const auto* value  = std::get_if<Interpreter<Number>::matrix_type>(&result);
    if (!value) return std::nullopt;
    std::vector<double> cells;
    for (std::size_t i = 1; i <= value->Size().rows; ++i) {
        for (std::size_t j = 1; j <= value->Size().cols; ++j) {
            const auto z = (*value)(i, j).Inexact();
            if (z.imag() != 0 || !std::isfinite(z.real())) return std::nullopt;
            cells.push_back(z.real());
        }
    }
    return cells;
}

std::string Double(double x) {
    char text[40];
    std::snprintf(text, sizeof text, "%.17g", x);
    return text;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: fuzz output.c\n";
        return 2;
    }
    Random      random;
    std::string checks, calls;
    int         compiled = 0;
    for (int id = 0; id < models; ++id) {
        const std::vector<std::string> lines  = RandomModel(random);
        const std::string              module = "f" + std::to_string(id);
        Interpreter<Number>            session;
        bool                           accepted = true;
        for (const std::string& line : lines)
            accepted = accepted && !std::holds_alternative<Diagnostic>(session.Eval(line));
        if (!accepted) continue;
        CompileC::Compiled step;
        try {
            step = CompileC::Build(session.Definitions(), module, "fuzz");
        } catch (const Refusal&) {
            continue;
        }
        ++compiled;
        const std::string&              header = step.header;
        const std::vector<std::string>& inputs = step.inputs;
        // Written for C as doubles: '(7/4)' there is a division of integers.
        std::vector<std::vector<std::string>> samples(inputs.size());
        for (std::size_t k = 0; k < inputs.size(); ++k) {
            for (int n = 0; n < steps; ++n) {
                const int quarters = random.below(17) - 8;
                samples[k].push_back(Double(quarters / 4.0));
                (void)session.Eval(inputs[k] + "_" + std::to_string(n) + " = " +
                                   std::to_string(quarters) + "/4");
            }
        }

        std::string check = "\n/*";
        for (const std::string& line : lines) check += "\n * " + line;
        check += "\n */\n" + header + "\nstatic int check_" + module + "(void) {\n";
        for (std::size_t k = 0; k < inputs.size(); ++k) {
            check +=
                "    static const double in_" + inputs[k] + "[" + std::to_string(steps) + "] = {";
            for (int n = 0; n < steps; ++n) check += (n ? ", " : "") + samples[k][n];
            check += "};\n";
        }
        std::string compare;
        for (const auto& field : step.sequences) {
            const std::size_t cells  = field.rows * field.cols;
            const bool        matrix = cells > 1;
            std::string       want, known;
            for (int n = 0; n < steps; ++n) {
                const auto answer = Answer(session, field.name + "_" + std::to_string(n));
                for (std::size_t c = 0; c < cells; ++c) {
                    want += (n || c ? ", " : "") + (answer ? Double((*answer)[c]) : "0.0");
                    known += (n || c ? ", " : "") + std::string(answer ? "1" : "0");
                }
            }
            const std::string size = std::to_string(steps * cells);
            check +=
                "    static const double want_" + field.name + "[" + size + "] = {" + want + "};\n";
            check += "    static const unsigned char known_" + field.name + "[" + size + "] = {" +
                     known + "};\n";
            for (std::size_t c = 0; c < cells; ++c) {
                const std::string at = "n * " + std::to_string(cells) + " + " + std::to_string(c);
                const std::string got = "m." + field.name + "[0]" +
                                        (matrix ? "[" + std::to_string(c / field.cols) + "][" +
                                                      std::to_string(c % field.cols) + "]"
                                                : "");
                compare += "        ok &= near_(\"" + module + "\", \"" + field.name + "\", n, " +
                           got + ", want_" + field.name + "[" + at + "], known_" + field.name +
                           "[" + at + "]);\n";
            }
        }
        check += "    " + module + " m;\n    int ok = 1;\n    " + module + "_init(&m);\n";
        check += "    for (int n = 0; n < " + std::to_string(steps) + "; ++n) {\n        " +
                 module + "_step(&m";
        for (const std::string& input : inputs) check += ", in_" + input + "[n]";
        check += ");\n" + compare + "    }\n    return ok;\n}\n";
        checks += check;
        calls += "    ok &= check_" + module + "();\n";
    }
    // A generator that drifted into models the compiler refuses would pass by
    // testing nothing.
    if (compiled < models / 2) {
        std::cerr << "fuzz: only " << compiled << " of " << models << " models compiled\n";
        return 1;
    }
    std::ofstream out(argv[1], std::ios::binary);
    out << "/* " << compiled << " of " << models
        << " random models, compiled and held to the interpreter's answers. */\n"
           "#include <math.h>\n#include <stdio.h>\n\n"
           "static int near_(const char* model, const char* what, int n, double got, double want,\n"
           "                 int known) {\n"
           "    if (!known || fabs(got - want) <= 1e-9 * (1.0 + fabs(want))) return 1;\n"
           "    printf(\"%s: %s at %d is %.17g, not %.17g\\n\", model, what, n, got, want);\n"
           "    return 0;\n}\n"
        << checks << "\nint main(void) {\n    int ok = 1;\n"
        << calls << "    return !ok;\n}\n";
    return out ? 0 : 2;
}
