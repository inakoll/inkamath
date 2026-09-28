#ifndef INKAMATH_COMPILE_HPP
#define INKAMATH_COMPILE_HPP

#include "inkamath/matrix.hpp"
#include "inkamath/number.hpp"
#include "inkamath/reference_stack.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// What the compiler will not compile, and in which definition.
struct Refusal : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// MODERNIZATION.md, phase 14, step 2: the sequences a session defines, as a C
// header over doubles. Each sequence keeps a window of its latest terms, and a
// step computes the next index's terms in the order they need each other. A
// value of any shape is its cells, each one C expression.
class CompileC : public TransformationVisitor<Matrix<Number>> {
public:
    using Value = Matrix<Number>;
    using TransformationVisitor<Value>::visit;

    static std::string Header(const ReferenceStack<Value>& definitions, const std::string& module,
                              const std::string& source) {
        CompileC compiler(definitions);
        for (const auto& [name, definition] : Sorted(definitions.Globals()))
            compiler.Define(name, *definition);
        for (const auto& [name, sequence] : Sorted(compiler.sequences_)) compiler.Compile(name);
        return compiler.Print(module, source);
    }

private:
    // Why a definition cannot be compiled; Compile names the definition.
    struct Reason : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // A primary, a unary, a product, a sum: an operand is parenthesised only
    // where C would otherwise read it differently.
    enum Level { sum = 1, product, unary, primary };

    struct Cell {
        Cell() = default;
        Cell(std::string written, int precedence) : text(std::move(written)), level(precedence) {}

        std::string text;
        int         level = primary;
        std::string magnitude;  // what it is the negation of, if it is one
        int         magnitude_level = primary;
    };

    struct Code {
        std::size_t          rows = 1, cols = 1;
        std::vector<Cell>    cells;     // row by row
        std::optional<Value> constant;  // its exact value, where it reads no name

        bool Scalar() const { return cells.size() == 1; }
        // A single value stretches to any shape, as it does in arithmetic.
        const Cell& At(std::size_t i, std::size_t j) const {
            return Scalar() ? cells[0] : cells[i * cols + j];
        }
    };

    struct Sequence {
        const Reference<Value>*                 definition = nullptr;  // none for an input
        std::map<int, std::vector<std::string>> bases;                 // cells, by index
        std::vector<std::string>                general;
        std::size_t                             rows = 0, cols = 0;  // 0 until known
        bool                                    based = false, compiling = false, compiled = false;
        std::map<std::string, std::set<int>>    reads;  // lags, by the sequence read
        int                                     depth = 1;
        int                                     start = 0;
    };

    struct Parameter {
        std::size_t         rows, cols;
        std::vector<double> initial;
    };

    explicit CompileC(const ReferenceStack<Value>& definitions) : definitions_(definitions) {}

    template <typename Map>
    static std::map<std::string, typename Map::mapped_type> Sorted(const Map& map) {
        return {map.begin(), map.end()};
    }

    const Reference<Value>* Global(const std::string& name) const {
        const auto found = definitions_.Globals().find(name);
        return found == definitions_.Globals().end() ? nullptr : found->second.get();
    }

    static bool IsSequence(const Reference<Value>& definition) {
        return definition.Clauses().front().parameters.indexed();
    }

    // Every definition is looked at, so that one the target cannot express is
    // refused rather than left out; a plain one is compiled where it is read.
    void Define(const std::string& name, const Reference<Value>& definition) {
        for (const Clause<Value>& clause : definition.Clauses()) {
            if (!clause.parameters.parameters_names().empty())
                throw Refusal("cannot compile " + name + ": a function");
            if (clause.parameters.guarded())
                throw Refusal("cannot compile " + name + ": a guarded clause");
        }
        if (IsSequence(definition)) sequences_[name].definition = &definition;
    }

    // A clause is compiled in a scope of its own, and a refusal inside it
    // names its sequence.
    template <typename Body>
    void Within(const std::string& name, Sequence* reading, const std::string& index, Body body) {
        Sequence* const   outer_reading = std::exchange(reading_, reading);
        const std::string outer_index   = std::exchange(index_, index);
        try {
            body();
        } catch (const Reason& reason) {
            throw Refusal("cannot compile " + name + ": " + reason.what());
        }
        reading_ = outer_reading;
        index_   = outer_index;
    }

    void Shape(Sequence& sequence, const Code& code) {
        if (sequence.rows && (code.rows != sequence.rows || code.cols != sequence.cols))
            throw Reason("its clauses have different shapes");
        sequence.rows = code.rows;
        sequence.cols = code.cols;
    }

    static std::vector<std::string> Texts(const Code& code) {
        std::vector<std::string> texts;
        for (const Cell& cell : code.cells) texts.push_back(cell.text);
        return texts;
    }

    // The base clauses give a sequence its shape without its general clause,
    // which may read the very sequences that need the shape.
    void Bases(const std::string& name) {
        Sequence& sequence = sequences_.at(name);
        if (sequence.based || !sequence.definition) return;
        sequence.based = true;
        Within(name, nullptr, std::string(), [&] {
            Unreserved(name);
            for (const Clause<Value>& clause : sequence.definition->Clauses()) {
                if (clause.parameters.general()) continue;
                const Code code = Emit(clause.expression);
                Shape(sequence, code);
                sequence.bases[clause.parameters.index()] = Texts(code);
            }
        });
    }

    // In turn, or earlier where a reader needs a shape no base clause gives.
    void Compile(const std::string& name) {
        Sequence& sequence = sequences_.at(name);
        if (sequence.compiled || !sequence.definition) return;
        if (sequence.compiling)
            throw Refusal("cannot compile " + name + ": its shape depends on itself");
        sequence.compiling = true;
        Bases(name);
        for (const Clause<Value>& clause : sequence.definition->Clauses()) {
            if (!clause.parameters.general()) continue;
            Within(name, &sequence, clause.parameters.index_name(), [&] {
                const Code code = Emit(clause.expression);
                Shape(sequence, code);
                sequence.general = Texts(code);
            });
        }
        if (sequence.general.empty())
            throw Refusal("cannot compile " + name + ": a sequence with no general clause");
        sequence.compiled = true;
    }

    Code Emit(const PExpression<Value>& expression) {
        expression->accept(*this);
        return code_;
    }

    static std::string Wrap(const std::string& text, int level, int needed) {
        return level < needed ? "(" + text + ")" : text;
    }
    static std::string Wrap(const Cell& cell, int needed) {
        return Wrap(cell.text, cell.level, needed);
    }

    PExpression<Value> Answer(Code code) {
        code_ = std::move(code);
        return PExpression<Value>();
    }
    PExpression<Value> Answer(Cell cell) {
        Code code;
        code.cells = {std::move(cell)};
        return Answer(std::move(code));
    }

    // Exactly, as the interpreter would, and rounded once: 0.1 + 0.2 is 3/10
    // here, which a C compiler folding the doubles would not find.
    PExpression<Value> Fold(Expression<Value>* expression) {
        ReferenceStack<Value>    scratch;
        EvaluationVisitor<Value> evaluator(scratch);
        try {
            return Answer(Literal(expression->accept(evaluator)));
        } catch (const Reason&) {
            throw;
        } catch (const std::runtime_error& error) {
            throw Reason(error.what());
        }
    }

    static std::vector<double> Doubles(const Value& value) {
        std::vector<double> doubles;
        for (std::size_t i = 1; i <= value.Size().rows; ++i) {
            for (std::size_t j = 1; j <= value.Size().cols; ++j) {
                const auto number = value(i, j).Inexact();
                if (number.imag() != 0) throw Reason("a complex number");
                doubles.push_back(number.real());
            }
        }
        return doubles;
    }

    static Code Literal(const Value& value) {
        Code code;
        code.rows     = value.Size().rows;
        code.cols     = value.Size().cols;
        code.constant = value;
        for (const double x : Doubles(value)) {
            Cell cell(Double(std::abs(x)), primary);
            if (std::signbit(x)) {
                cell.magnitude = cell.text;
                cell.text      = "-" + cell.text;
                cell.level     = unary;
            }
            code.cells.push_back(cell);
        }
        return code;
    }

    static std::string Double(double x) {
        if (std::isnan(x)) return "NAN";
        if (std::isinf(x)) return "INFINITY";
        char        text[32];
        const auto  end = std::to_chars(text, text + sizeof text, x).ptr;
        std::string written(text, end);
        if (written.find_first_of(".e") == std::string::npos) written += ".0";
        return written;
    }

    // Cell by cell, a single value stretching to the other's shape, as the
    // interpreter's arithmetic does.
    template <typename Combine>
    static Code Cellwise(const Code& left, const Code& right, Combine combine) {
        if (!left.Scalar() && !right.Scalar() &&
            (left.rows != right.rows || left.cols != right.cols))
            throw Reason("these matrices have different sizes");
        Code code;
        code.rows = left.Scalar() ? right.rows : left.rows;
        code.cols = left.Scalar() ? right.cols : left.cols;
        for (std::size_t i = 0; i < code.rows; ++i)
            for (std::size_t j = 0; j < code.cols; ++j)
                code.cells.push_back(combine(left.At(i, j), right.At(i, j)));
        return code;
    }

    // a + (-b) is a - b in IEEE arithmetic, and reads as what was written.
    static Cell Added(const Cell& left, const Cell& right) {
        if (!right.magnitude.empty())
            return {Wrap(left, sum) + " - " + Wrap(right.magnitude, right.magnitude_level, product),
                    sum};
        return {Wrap(left, sum) + " + " + Wrap(right, product), sum};
    }
    static Cell Multiplied(const Cell& left, const Cell& right) {
        return {Wrap(left, product) + " * " + Wrap(right, unary), product};
    }
    static Cell Divided(const Cell& left, const Cell& right) {
        return {Wrap(left, product) + " / " + Wrap(right, unary), product};
    }

    PExpression<Value> visit(ValExpression<Value>* expression) override {
        return Answer(Literal(expression->value));
    }

    PExpression<Value> visit(AddExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        return Answer(Cellwise(left, right, Added));
    }

    PExpression<Value> visit(DivExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        return Answer(Cellwise(left, right, Divided));
    }

    // A matrix product is a sum over the inner dimension, in the interpreter's
    // order.
    PExpression<Value> visit(MultExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        if (left.Scalar() || right.Scalar()) return Answer(Cellwise(left, right, Multiplied));
        if (left.cols != right.rows)
            throw Reason("a matrix product needs as many columns on the left as rows on the right");
        Code code;
        code.rows = left.rows;
        code.cols = right.cols;
        for (std::size_t i = 0; i < code.rows; ++i) {
            for (std::size_t j = 0; j < code.cols; ++j) {
                Cell cell = Multiplied(left.At(i, 0), right.At(0, j));
                for (std::size_t k = 1; k < left.cols; ++k)
                    cell = Added(cell, Multiplied(left.At(i, k), right.At(k, j)));
                code.cells.push_back(cell);
            }
        }
        return Answer(code);
    }

    PExpression<Value> visit(NegExpression<Value>* expression) override {
        Code code = Emit(expression->m_e());
        if (code.constant) return Fold(expression);
        for (Cell& cell : code.cells) {
            Cell negation(
                "-" + (cell.magnitude.empty() ? Wrap(cell, unary) : "(" + cell.text + ")"), unary);
            negation.magnitude       = cell.text;
            negation.magnitude_level = cell.level;
            cell                     = negation;
        }
        return Answer(code);
    }

    PExpression<Value> visit(InexactExpression<Value>* expression) override {
        const Code operand = Emit(expression->m_e());
        return operand.constant ? Fold(expression) : Answer(operand);
    }

    PExpression<Value> visit(PowExpression<Value>* expression) override {
        const Code base = Emit(expression->m_e1()), exponent = Emit(expression->m_e2());
        if (base.constant && exponent.constant) return Fold(expression);
        if (!exponent.Scalar()) throw Reason("a matrix cannot be an exponent");
        if (!base.Scalar()) throw Reason("a matrix power");
        return Answer(
            Cell("pow(" + base.cells[0].text + ", " + exponent.cells[0].text + ")", primary));
    }

    PExpression<Value> visit(CompareExpression<Value>* expression) override {
        static const char* const ops[] = {" < ", " > ", " <= ", " >= ", " == ", " != "};
        const Code               left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        if (!left.Scalar() || !right.Scalar()) throw Reason("a comparison of matrices");
        return Answer(Cell("(" + Wrap(left.cells[0], sum) +
                               ops[static_cast<int>(expression->Op())] + Wrap(right.cells[0], sum) +
                               " ? 1.0 : 0.0)",
                           primary));
    }

    PExpression<Value> visit(MatExpression<Value>* expression) override {
        Code code;
        code.rows     = expression->Size().rows;
        code.cols     = expression->Size().cols;
        bool constant = true;
        for (const PExpression<Value>& child : expression->Children()) {
            const Code cell = Emit(child);
            if (!cell.Scalar()) throw Reason("a matrix built from matrices");
            constant = constant && cell.constant;
            code.cells.push_back(cell.cells[0]);
        }
        return constant ? Fold(expression) : Answer(code);
    }

    PExpression<Value> visit(RefExpression<Value>* expression) override {
        const std::string& name = expression->Name();
        if (!index_.empty() && name == index_) return Answer(Cell("(double)m_->index_", unary));
        const Reference<Value>* definition = Global(name);
        if (!definition) return Answer(Field(name, Value(Number(NAN))));
        if (IsSequence(*definition)) throw Reason(name + " is a sequence; index it");
        if (!reading_plain_.insert(name).second) throw Reason(name + " is defined by itself");
        // A global is evaluated in a scope of its own, where no index is seen.
        Sequence* const   reading = std::exchange(reading_, nullptr);
        const std::string index   = std::exchange(index_, std::string());
        Code              code    = Emit(definition->Clauses().front().expression);
        reading_                  = reading;
        index_                    = index;
        reading_plain_.erase(name);
        // A value that reads no other is a parameter the host may change; one
        // that does is recomputed where it is read, so that it follows them.
        return Answer(code.constant ? Field(name, *code.constant) : code);
    }

    PExpression<Value> visit(FuncExpression<Value>* expression) override {
        const std::string&           name = expression->Name();
        const ParametersCall<Value>& call = expression->Call();
        if (call.limit()) throw Reason("a limit");
        if (!call.parameters_expression().empty() || !call.parameters_dict().empty())
            throw Reason("a function");
        if (!reading_) throw Reason(name + "_...: a term read outside a general clause");
        const Reference<Value>* definition = Global(name);
        if (definition && !IsSequence(*definition)) throw Reason(name + " is not a sequence");
        Sequence& read = sequences_[name];
        if (!definition) {
            Unreserved(name);
            read.rows = read.cols = 1;
        }
        const int lag = Lag(call.subexpr(), name);
        Bases(name);
        if (!read.rows) Compile(name);
        reading_->reads[name].insert(lag);
        const std::string at     = "m_->" + name + "[" + std::to_string(lag) + "]";
        const bool        scalar = read.rows * read.cols == 1;
        Code              code;
        code.rows = read.rows;
        code.cols = read.cols;
        for (std::size_t i = 0; i < code.rows; ++i)
            for (std::size_t j = 0; j < code.cols; ++j)
                code.cells.emplace_back(scalar ? at : at + Subscript(i, j), primary);
        return Answer(code);
    }

    static std::string Subscript(std::size_t i, std::size_t j) {
        return "[" + std::to_string(i) + "][" + std::to_string(j) + "]";
    }

    // How far back a term reads: its own index, or that less a constant.
    int Lag(const PExpression<Value>& index, const std::string& name) {
        const std::string written = name + "_(...)";
        if (const auto* ref = dynamic_cast<RefExpression<Value>*>(index.get());
            ref && ref->Name() == index_)
            return 0;
        const auto* sum = dynamic_cast<AddExpression<Value>*>(index.get());
        const auto* ref = sum ? dynamic_cast<RefExpression<Value>*>(sum->m_e1().get()) : nullptr;
        const std::string only = written + ": an index other than " + index_ + " less a constant";
        if (!ref || ref->Name() != index_) throw Reason(only);
        const Code offset = Emit(sum->m_e2());
        if (!offset.constant) throw Reason(only);
        int step = 0;
        try {
            step = AsIndex<Value>(*offset.constant);
        } catch (const std::runtime_error& error) {
            throw Reason(error.what());
        }
        if (step > 0) throw Reason(written + ": a term after the one being computed");
        return -step;
    }

    PExpression<Value> visit(EqualExpression<Value>*) override {
        throw Reason("a local definition");
    }
    PExpression<Value> visit(CellExpression<Value>*) override {
        throw Reason("a cell of a matrix");
    }
    PExpression<Value> visit(FactExpression<Value>*) override { throw Reason("a factorial"); }
    PExpression<Value> visit(SeriesExpression<Value>*) override {
        throw Reason("a sum or a product");
    }
    PExpression<Value> visit_other(Expression<Value>*) override { throw Reason("this expression"); }

    Code Field(const std::string& name, const Value& value) {
        Unreserved(name);
        const Parameter parameter{value.Size().rows, value.Size().cols, Doubles(value)};
        const bool      scalar = parameter.rows * parameter.cols == 1;
        parameters_.emplace(name, parameter);
        Code code;
        code.rows = parameter.rows;
        code.cols = parameter.cols;
        for (std::size_t i = 0; i < code.rows; ++i)
            for (std::size_t j = 0; j < code.cols; ++j)
                code.cells.emplace_back("m_->" + name + (scalar ? "" : Subscript(i, j)), primary);
        return code;
    }

    // A name here is letters and digits, so it can only collide with C's own.
    static void Unreserved(const std::string& name) {
        static const std::set<std::string> keywords = {
            "auto",    "break",  "case",     "char",   "const",    "continue", "default",
            "do",      "double", "else",     "enum",   "extern",   "float",    "for",
            "goto",    "if",     "inline",   "int",    "long",     "register", "restrict",
            "return",  "short",  "signed",   "sizeof", "static",   "struct",   "switch",
            "typedef", "union",  "unsigned", "void",   "volatile", "while"};
        if (keywords.count(name)) throw Reason(name + " is a word C keeps for itself");
    }

    // Where each sequence starts: at its lowest base clause, or, without one,
    // at the first index where every term it reads exists -- which is where
    // the interpreter, asked for the term, would first answer. An input starts
    // with the earliest.
    int Starts() {
        std::optional<int> earliest;
        for (auto& [name, sequence] : sequences_) {
            if (sequence.bases.empty()) continue;
            sequence.start = sequence.bases.begin()->first;
            earliest       = std::min(earliest.value_or(sequence.start), sequence.start);
        }
        for (auto& [name, sequence] : sequences_)
            if (sequence.bases.empty()) sequence.start = earliest.value_or(0);
        for (std::size_t round = 0;; ++round) {
            bool moved = false;
            for (auto& [name, sequence] : sequences_) {
                if (!sequence.bases.empty() || !sequence.definition) continue;
                for (const auto& [read, lags] : sequence.reads) {
                    const int needed = sequences_.at(read).start + *lags.rbegin();
                    if (needed <= sequence.start) continue;
                    sequence.start = needed;
                    moved          = true;
                }
            }
            if (!moved) break;
            if (round > sequences_.size())
                throw Refusal(
                    "cannot compile: a sequence with no base clause reads back into "
                    "itself, so it never starts");
        }
        // One with base clauses answers from them until what it reads exists.
        for (const auto& [name, sequence] : sequences_) {
            for (const auto& [read, lags] : sequence.reads) {
                const Sequence& other = sequences_.at(read);
                for (const int lag : lags) {
                    for (int n = sequence.start; n < other.start + lag; ++n) {
                        if (sequence.bases.count(n)) continue;
                        throw Refusal("cannot compile " + name + ": " + name + "_" +
                                      std::to_string(n) + " reads " + read + "_" +
                                      std::to_string(n - lag) + ", before it starts at " +
                                      std::to_string(other.start));
                    }
                }
            }
        }
        return earliest.value_or(0);
    }

    std::vector<std::string> Order() const {
        std::map<std::string, std::set<std::string>> waiting;
        for (const auto& [name, sequence] : sequences_) {
            if (!sequence.definition) continue;
            waiting[name];
            for (const auto& [read, lags] : sequence.reads) {
                if (!lags.count(0) || !sequences_.at(read).definition) continue;
                if (read == name) throw Refusal("cannot compile " + name + ": a term reads itself");
                waiting[name].insert(read);
            }
        }
        std::vector<std::string> order;
        while (!waiting.empty()) {
            const auto ready = std::find_if(waiting.begin(), waiting.end(),
                                            [](const auto& entry) { return entry.second.empty(); });
            if (ready == waiting.end())
                throw Refusal("cannot compile " + waiting.begin()->first +
                              ": it and the terms it reads need each other");
            const std::string name = ready->first;
            order.push_back(name);
            waiting.erase(ready);
            for (auto& entry : waiting) entry.second.erase(name);
        }
        return order;
    }

    static std::string Dimensions(std::size_t rows, std::size_t cols) {
        return rows * cols == 1 ? "" : Subscript(rows, cols);
    }

    std::string Print(const std::string& module, const std::string& source) {
        for (const auto& [name, sequence] : sequences_)
            if (!sequence.definition && parameters_.count(name))
                throw Refusal("cannot compile: " + name +
                              " is read both as a value and as a sequence");
        const int                      earliest = Starts();
        const std::vector<std::string> order    = Order();
        std::vector<std::string>       fields;
        for (auto& [name, sequence] : sequences_) {
            if (!sequence.definition) fields.push_back(name);
            for (const auto& [read, lags] : sequence.reads)
                sequences_.at(read).depth = std::max(sequences_.at(read).depth, *lags.rbegin() + 1);
        }
        const std::vector<std::string> inputs = fields;
        fields.insert(fields.end(), order.begin(), order.end());

        std::string guard;
        for (const char c : module)
            guard += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        guard += "_H";

        std::string out;
        out += "/* Generated by 'inkamath --compile' from " + source + ": edit that, not this.\n";
        out += " * The arithmetic is in the order the definitions give it, so do not build it\n";
        out += " * with -ffast-math, which reorders. */\n";
        out += "#ifndef " + guard + "\n#define " + guard + "\n\n";
        out += "#include <math.h>\n#include <string.h>\n\n";
        out += "/* The parameters, which the host may assign, then the index of the latest\n";
        out += " * step and each sequence's terms from that index back. */\n";
        out += "typedef struct " + module + " {\n";
        for (const auto& [name, parameter] : parameters_)
            out += "    double " + name + Dimensions(parameter.rows, parameter.cols) + ";\n";
        out += "    long long index_;\n";
        for (const std::string& name : fields) {
            const Sequence& sequence = sequences_.at(name);
            out += "    double " + name + "[" + std::to_string(sequence.depth) + "]" +
                   Dimensions(sequence.rows, sequence.cols) + ";\n";
        }
        out += "} " + module + ";\n\n";

        out += "static inline void " + module + "_init(" + module + "* m_) {\n";
        out += "    memset(m_, 0, sizeof *m_);\n";
        for (const auto& [name, parameter] : parameters_) {
            const bool scalar = parameter.rows * parameter.cols == 1;
            for (std::size_t c = 0; c < parameter.initial.size(); ++c)
                out += "    m_->" + name +
                       (scalar ? "" : Subscript(c / parameter.cols, c % parameter.cols)) + " = " +
                       Double(parameter.initial[c]) + ";\n";
        }
        out += "    m_->index_ = " + std::to_string(earliest - 1) + ";\n}\n\n";

        out += "/* Advances to the next index, the first at " + std::to_string(earliest) +
               ", and computes its terms. */\n";
        out += "static inline void " + module + "_step(" + module + "* m_";
        for (const std::string& name : inputs) out += ", double " + name;
        out += ") {\n    ++m_->index_;\n";
        for (const std::string& name : fields) {
            const Sequence& sequence = sequences_.at(name);
            for (int k = sequence.depth - 1; k > 0; --k) {
                const std::string to   = "m_->" + name + "[" + std::to_string(k) + "]";
                const std::string from = "m_->" + name + "[" + std::to_string(k - 1) + "]";
                out += sequence.rows * sequence.cols == 1
                           ? "    " + to + " = " + from + ";\n"
                           : "    memcpy(" + to + ", " + from + ", sizeof " + to + ");\n";
            }
        }
        for (const std::string& name : inputs) out += "    m_->" + name + "[0] = " + name + ";\n";
        for (const std::string& name : order) {
            const Sequence&   sequence = sequences_.at(name);
            const bool        scalar   = sequence.general.size() == 1;
            const bool        guarded  = sequence.start > earliest;
            const std::string indent   = guarded ? "        " : "    ";
            if (guarded) out += "    if (m_->index_ >= " + std::to_string(sequence.start) + ") {\n";
            for (std::size_t c = 0; c < sequence.general.size(); ++c) {
                out += indent + "m_->" + name + "[0]" +
                       (scalar ? "" : Subscript(c / sequence.cols, c % sequence.cols)) + " = ";
                for (const auto& [index, base] : sequence.bases)
                    out += "m_->index_ == " + std::to_string(index) + " ? " + base[c] + " : ";
                out += sequence.general[c] + ";\n";
            }
            if (guarded) out += "    }\n";
        }
        out += "}\n\n#endif\n";
        return out;
    }

    const ReferenceStack<Value>&     definitions_;
    std::map<std::string, Sequence>  sequences_;
    std::map<std::string, Parameter> parameters_;
    std::set<std::string>            reading_plain_;
    Sequence*   reading_ = nullptr;  // the sequence whose general clause this is
    std::string index_;              // and the name of its index
    Code        code_;
};

#endif  // INKAMATH_COMPILE_HPP
