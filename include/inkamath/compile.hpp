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
#include <limits>
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
        compiler.module_ = module;
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
        bool        atom            = false;  // a name or a number, as cheap to repeat as to store
    };

    static Cell Atom(std::string text) {
        Cell cell(std::move(text), primary);
        cell.atom = true;
        return cell;
    }

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

    // A value a step computes once, before the sequence that reads it: its
    // name, what it computes, and the lines that declare it.
    struct Temporary {
        std::string              name, value;
        std::vector<std::string> lines;
    };

    using Reads = std::map<std::string, std::set<int>>;  // lags, by the sequence read

    // A guarded clause, and what its guard and its value each read: its guard
    // is evaluated wherever the chain reaches it, its value only where it holds,
    // so each has its own index from which it can be.
    struct Guarded {
        std::string              condition;
        std::vector<std::string> cells;
        Reads                    guard, value;
        int                      guard_from = 0, value_from = 0;
    };

    struct Sequence {
        const Reference<Value>*                 definition = nullptr;  // none for an input
        std::map<int, std::vector<std::string>> bases;                 // cells, by index
        std::vector<Guarded>                    guarded;               // in the order tried
        Reads                                   general_reads;
        std::vector<std::string>                general;
        std::size_t                             rows = 0, cols = 0;  // 0 until known
        bool                                    based = false, compiling = false, compiled = false;
        std::map<std::string, std::set<int>>    reads;  // lags, by the sequence read
        int                                     depth = 1;
        int                                     start = 0;
        std::vector<Temporary>                  temporaries;  // in the order declared
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
            const ParametersDefinition<Value>& p = clause.parameters;
            if (p.guarded() && !p.cells() && !p.general())
                throw Refusal("cannot compile " + name + ": a guarded " +
                              (p.indexed() ? "base clause" : "value"));
        }
        // A base clause written after a guarded one is reached only if the guard
        // fails, which the chain a step computes would not say.
        bool guarded = false;
        for (const Clause<Value>& clause : definition.Clauses()) {
            guarded = guarded || (clause.parameters.guarded() && clause.parameters.general());
            if (guarded && clause.parameters.indexed() && !clause.parameters.general())
                throw Refusal("cannot compile " + name + ": a base clause after a guarded one");
        }
        if (IsSequence(definition)) sequences_[name].definition = &definition;
    }

    // A clause is compiled in a scope of its own, and a refusal inside it
    // names its sequence.
    template <typename Body>
    void Within(const std::string& name, Sequence* reading, const std::string& index, Body body) {
        Sequence* const   outer_reading = std::exchange(reading_, reading);
        const std::string outer_index   = std::exchange(index_, index);
        auto* const       outer_temporaries =
            std::exchange(temporaries_, &sequences_.at(name).temporaries);
        try {
            body();
        } catch (const Reason& reason) {
            throw Refusal("cannot compile " + name + ": " + reason.what());
        }
        reading_     = outer_reading;
        index_       = outer_index;
        temporaries_ = outer_temporaries;
    }

    // A cell the compiler would write out more than once -- an operand of a
    // matrix product, a single value stretched over a matrix -- is computed once
    // per step instead, into a temporary: the same value, in fewer lines.
    Cell Shared(const Cell& cell) {
        if (cell.atom || !temporaries_) return cell;
        return Atom(Declare(cell.text, [&](const std::string& name) {
            return std::vector<std::string>{"const double " + name + " = " + cell.text + ";"};
        }));
    }

    // The same value is the same temporary within one sequence's step.
    template <typename Lines>
    std::string Declare(const std::string& value, Lines lines) {
        if (!temporaries_) throw Reason("a matrix inverse outside a sequence");
        for (const Temporary& temporary : *temporaries_)
            if (temporary.value == value) return temporary.name;
        const std::string name = "t" + std::to_string(temporary_count_++) + "_";
        temporaries_->push_back({name, value, lines(name)});
        return name;
    }
    Code Shared(Code code) {
        for (Cell& cell : code.cells) cell = Shared(cell);
        return code;
    }

    template <typename Combine>
    Code Broadcast(Code left, Code right, Combine combine) {
        if (left.Scalar() && !right.Scalar()) left = Shared(left);
        if (right.Scalar() && !left.Scalar()) right = Shared(right);
        return Cellwise(left, right, combine);
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
        Reads* const outer_reads = clause_reads_;
        // The guarded clauses in the order written, then the unguarded one, as
        // the interpreter tries them; a guard that always holds ends the chain.
        bool settled = false;
        for (const bool guarded : {true, false}) {
            for (const Clause<Value>& clause : sequence.definition->Clauses()) {
                const ParametersDefinition<Value>& p = clause.parameters;
                if (settled || !p.general() || p.guarded() != guarded) continue;
                Within(name, &sequence, p.index_name(), [&] {
                    Reads guard_reads, value_reads;
                    clause_reads_ = &guard_reads;
                    const std::optional<std::string> condition =
                        guarded ? Condition(p.guard()) : std::optional<std::string>("");
                    if (!condition) return;
                    clause_reads_   = &value_reads;
                    const Code code = Emit(clause.expression);
                    clause_reads_   = nullptr;
                    Shape(sequence, code);
                    if (condition->empty()) {
                        sequence.general       = Texts(code);
                        sequence.general_reads = value_reads;
                        settled                = true;
                    } else {
                        sequence.guarded.push_back(
                            {*condition, Texts(code), guard_reads, value_reads, 0, 0});
                    }
                });
            }
        }
        if (!settled && sequence.guarded.empty())
            throw Refusal("cannot compile " + name + ": a sequence with no general clause");
        // Where no guard holds the interpreter says so; a step can only say NaN.
        if (!settled) sequence.general.assign(sequence.rows * sequence.cols, "NAN");
        clause_reads_     = outer_reads;
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
        ReferenceStack<Value> scratch;
        for (const auto& [name, value] : places_)
            scratch.Set(name, ParametersDefinition<Value>(),
                        std::make_shared<ValExpression<Value>>(value));
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
            Cell cell = Atom(Double(std::abs(x)));
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
        return Answer(Broadcast(left, right, Added));
    }

    PExpression<Value> visit(DivExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        return Answer(Broadcast(left, right, Divided));
    }

    // A matrix product is a sum over the inner dimension, in the interpreter's
    // order.
    PExpression<Value> visit(MultExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        return Answer(Product(left, right));
    }

    // Each left cell is read once for each right column, each right cell once
    // for each left row.
    Code Product(Code left, Code right) {
        if (left.Scalar() || right.Scalar()) return Broadcast(left, right, Multiplied);
        if (right.cols > 1) left = Shared(left);
        if (left.rows > 1) right = Shared(right);
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
        return code;
    }

    PExpression<Value> visit(NegExpression<Value>* expression) override {
        Code code = Emit(expression->m_e());
        if (code.constant) return Fold(expression);
        for (Cell& cell : code.cells) {
            Cell negation(
                "-" + (cell.magnitude.empty() ? Wrap(cell, unary) : "(" + cell.text + ")"), unary);
            negation.magnitude       = cell.text;
            negation.magnitude_level = cell.level;
            negation.atom            = cell.atom;
            cell                     = negation;
        }
        return Answer(code);
    }

    PExpression<Value> visit(InexactExpression<Value>* expression) override {
        const Code operand = Emit(expression->m_e());
        return operand.constant ? Fold(expression) : Answer(operand);
    }

    PExpression<Value> visit(TransposeExpression<Value>* expression) override {
        const Code operand = Emit(expression->m_e());
        if (operand.constant) return Fold(expression);
        Code code;
        code.rows = operand.cols;
        code.cols = operand.rows;
        for (std::size_t i = 0; i < code.rows; ++i)
            for (std::size_t j = 0; j < code.cols; ++j) code.cells.push_back(operand.At(j, i));
        return Answer(code);
    }

    PExpression<Value> visit(PowExpression<Value>* expression) override {
        const Code base = Emit(expression->m_e1()), exponent = Emit(expression->m_e2());
        if (base.constant && exponent.constant) return Fold(expression);
        if (!exponent.Scalar()) throw Reason("a matrix cannot be an exponent");
        if (!base.Scalar()) return Answer(Power(base, exponent));
        return Answer(
            Cell("pow(" + base.cells[0].text + ", " + exponent.cells[0].text + ")", primary));
    }

    // By squaring, as the interpreter does it, of the inverse for a negative
    // exponent. The interpreter starts from the identity and multiplies it in;
    // starting from the first factor instead changes only a cell that is
    // infinite, where the identity's zeros would make it NaN.
    Code Power(Code base, const Code& exponent) {
        if (!exponent.constant) throw Reason("a matrix power whose exponent is not a constant");
        const auto power = (*exponent.constant)(1, 1).Inexact();
        if (power.imag() != 0 || power.real() != std::floor(power.real()) ||
            std::abs(power.real()) > 2147483647.0)
            throw Reason("a matrix power must be a whole number");
        if (base.rows != base.cols) throw Reason("only a square matrix has a power");
        const long long whole = static_cast<long long>(power.real());
        if (whole == 0) {
            // The identity whatever the base, but not a constant: a constant is
            // what reads no name, and an enclosing fold would read this one's.
            Code identity = Literal(Value::Identity(Extent{base.rows, base.cols}));
            identity.constant.reset();
            return identity;
        }
        if (whole < 0) base = Inverse(base);
        std::optional<Code> result;
        for (unsigned long long n = static_cast<unsigned long long>(whole < 0 ? -whole : whole);
             n != 0; n >>= 1) {
            if (n & 1) result = result ? Product(*result, base) : base;
            if (n > 1) base = Product(base, base);
        }
        return *result;
    }

    // In place, by the helper the header defines for its size, which pivots as
    // the interpreter does: that depends on the values, so it is done as the
    // step runs.
    Code Inverse(const Code& matrix) {
        const std::size_t n     = matrix.rows;
        std::string       value = "inverse";
        std::string       rows;
        for (std::size_t i = 0; i < n; ++i) {
            std::string row;
            for (std::size_t j = 0; j < n; ++j) row += (j ? ", " : "") + matrix.At(i, j).text;
            rows += (i ? ", {" : "{") + row + "}";
            value += "\x1f" + row;
        }
        inverses_.insert(n);
        const std::string name = Declare(value, [&](const std::string& t) {
            return std::vector<std::string>{
                "double " + t + Subscript(n, n) + " = {" + rows + "};",
                module_ + "_inverse" + std::to_string(n) + "_(" + t + ");"};
        });
        Code              code;
        code.rows = code.cols = n;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j) code.cells.push_back(Atom(name + Subscript(i, j)));
        return code;
    }

    static const char* Operator(Comparison op) {
        static const char* const ops[] = {" < ", " > ", " <= ", " >= ", " == ", " != "};
        return ops[static_cast<int>(op)];
    }

    PExpression<Value> visit(CompareExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        if (!left.Scalar() || !right.Scalar()) throw Reason("a comparison of matrices");
        return Answer(Cell("(" + Wrap(left.cells[0], sum) + Operator(expression->Op()) +
                               Wrap(right.cells[0], sum) + " ? 1.0 : 0.0)",
                           primary));
    }

    // A guard as C tests it: a comparison as itself, anything else against
    // zero, which is the interpreter's truth, NaN holding. Empty where it
    // always holds, and nothing where it never does.
    std::optional<std::string> Condition(const PExpression<Value>& guard) {
        const Code code = Emit(guard);
        if (!code.Scalar()) throw Reason("a guard that is a matrix");
        if (code.constant) {
            if (!Value::truth(*code.constant)) return std::nullopt;
            return std::string();
        }
        if (const auto* compare = dynamic_cast<CompareExpression<Value>*>(guard.get())) {
            const Code left = Emit(compare->m_e1()), right = Emit(compare->m_e2());
            return Wrap(left.cells[0], sum) + Operator(compare->Op()) + Wrap(right.cells[0], sum);
        }
        return Wrap(code.cells[0], sum) + " != 0.0";
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
        if (const auto place = places_.find(name); place != places_.end())
            return Answer(Literal(place->second));
        if (!index_.empty() && name == index_) return Answer(Atom(index_text_));
        const Reference<Value>* definition = Global(name);
        if (!definition) return Answer(Field(name, Value(Number(NAN))));
        if (IsSequence(*definition)) throw Reason(name + " is a sequence; index it");
        if (!reading_plain_.insert(name).second) throw Reason(name + " is defined by itself");
        // A global is evaluated in a scope of its own, where no index or place
        // is seen.
        Sequence* const   reading = std::exchange(reading_, nullptr);
        const std::string index   = std::exchange(index_, std::string());
        const auto        places  = std::exchange(places_, {});
        const auto        cells   = [](const Clause<Value>& c) { return c.parameters.cells(); };
        Code code = std::any_of(definition->Clauses().begin(), definition->Clauses().end(), cells)
                        ? Cells(name, *definition)
                        : Emit(definition->Clauses().front().expression);
        reading_                  = reading;
        index_                    = index;
        places_                   = places;
        reading_plain_.erase(name);
        // A value that reads no other is a parameter the host may change; one
        // that does is recomputed where it is read, so that it follows them.
        return Answer(code.constant ? Field(name, *code.constant) : code);
    }

    // A matrix defined by its cells, one cell at a time with its names bound to
    // the cell's place. They are constants, so the size, every guard and which
    // clause gives the cell are decided here, as the interpreter would decide
    // them; one that cannot be is refused.
    Code Cells(const std::string& name, const Reference<Value>& definition) {
        std::optional<Extent> extent;
        std::optional<Code>   whole;  // the matrix written whole, if it is
        for (const Clause<Value>& clause : definition.Clauses()) {
            if (clause.parameters.cells()) continue;
            whole  = Emit(clause.expression);
            extent = Extent{whole->rows, whole->cols};
        }
        for (const Clause<Value>& clause : definition.Clauses()) {
            const ParametersDefinition<Value>& p = clause.parameters;
            if (p.row_name().empty()) continue;
            const Extent size{Size(Emit(p.rows())), Size(Emit(p.cols()))};
            if (extent && *extent != size)
                throw Reason("the clauses of " + name + " give it different sizes");
            extent = size;
        }
        if (!extent) throw Reason(name + " has no size");
        for (const Clause<Value>& clause : definition.Clauses()) {
            const ParametersDefinition<Value>& p = clause.parameters;
            if (!p.cells() || !p.row_name().empty()) continue;
            if (p.row() < 1 || static_cast<std::size_t>(p.row()) > extent->rows || p.col() < 1 ||
                static_cast<std::size_t>(p.col()) > extent->cols)
                throw Reason("row " + std::to_string(p.row()) + ", column " +
                             std::to_string(p.col()) + " is outside a " +
                             std::to_string(extent->rows) + "x" + std::to_string(extent->cols) +
                             " matrix");
        }
        Code  code;
        Value exact(*extent);
        bool  constant = true;
        code.rows      = extent->rows;
        code.cols      = extent->cols;
        for (std::size_t row = 1; row <= code.rows; ++row) {
            for (std::size_t col = 1; col <= code.cols; ++col) {
                const std::optional<Code> cell =
                    CellOf(definition, static_cast<int>(row), static_cast<int>(col));
                Code given = cell ? *cell : Literal(Value(Number(0)));
                if (!cell && whole) {
                    given       = Code();
                    given.cells = {whole->At(row - 1, col - 1)};
                    if (whole->constant) given.constant = Value((*whole->constant)(row, col));
                }
                if (!given.Scalar())
                    throw Reason("a cell of " + name + " must be a single value, not a " +
                                 std::to_string(given.rows) + "x" + std::to_string(given.cols) +
                                 " matrix");
                code.cells.push_back(given.cells[0]);
                if (given.constant) exact(row, col) = (*given.constant)(1, 1);
                constant = constant && given.constant;
            }
        }
        if (constant) code.constant = exact;
        return code;
    }

    std::optional<Code> CellOf(const Reference<Value>& definition, int row, int col) {
        for (const Clause<Value>& clause : definition.Clauses()) {
            const ParametersDefinition<Value>& p = clause.parameters;
            if (!p.cells() || !p.row_name().empty() || p.row() != row || p.col() != col) continue;
            if (p.guarded() && !Holds(p.guard())) continue;
            return Emit(clause.expression);
        }
        for (const bool guarded : {true, false}) {
            for (const Clause<Value>& clause : definition.Clauses()) {
                const ParametersDefinition<Value>& p = clause.parameters;
                if (p.row_name().empty() || p.guarded() != guarded) continue;
                places_[p.row_name()]           = Value(Number(row));
                places_[p.col_name()]           = Value(Number(col));
                const bool                holds = !p.guarded() || Holds(p.guard());
                const std::optional<Code> cell =
                    holds ? std::optional<Code>(Emit(clause.expression)) : std::nullopt;
                places_.erase(p.row_name());
                places_.erase(p.col_name());
                if (cell) return cell;
            }
        }
        return std::nullopt;
    }

    bool Holds(const PExpression<Value>& guard) {
        const Code code = Emit(guard);
        if (!code.constant) throw Reason("a guard on cells that is not a constant");
        return Value::truth(*code.constant);
    }

    static std::size_t Size(const Code& code) {
        if (!code.constant) throw Reason("a matrix whose size is not a constant");
        int size = 0;
        try {
            size = AsIndex<Value>(*code.constant);
        } catch (const std::runtime_error& error) {
            throw Reason(error.what());
        }
        if (size < 1) throw Reason("a size must be at least 1, not " + std::to_string(size));
        return static_cast<std::size_t>(size);
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
        if (lag > 0 && ClosedForm(read)) return Answer(At(read, lag));
        reading_->reads[name].insert(lag);
        if (clause_reads_) (*clause_reads_)[name].insert(lag);
        const std::string at     = "m_->" + name + "[" + std::to_string(lag) + "]";
        const bool        scalar = read.rows * read.cols == 1;
        Code              code;
        code.rows = read.rows;
        code.cols = read.cols;
        for (std::size_t i = 0; i < code.rows; ++i)
            for (std::size_t j = 0; j < code.cols; ++j)
                code.cells.push_back(Atom(scalar ? at : at + Subscript(i, j)));
        return Answer(code);
    }

    // A sequence with no base clause that reads no term is a closed form, and
    // the interpreter answers it at every index, before the model starts too:
    // 'c_n = a_(n-1)' with 'a_n = n/8' reads a_(-1) at 0. So a closed form read
    // back in time is compiled again at that index, rather than read from a
    // window that holds nothing from before the start.
    bool ClosedForm(const Sequence& sequence) const {
        return sequence.definition && sequence.compiled && sequence.bases.empty() &&
               sequence.reads.empty();
    }

    Code At(const Sequence& sequence, int lag) {
        Code                                      chain;
        std::optional<Code>                       otherwise;
        std::vector<std::pair<std::string, Code>> guarded;
        Sequence* const                           reading = std::exchange(reading_, nullptr);
        const std::string                         index   = std::exchange(index_, std::string());
        const std::string                         text =
            std::exchange(index_text_, "(double)(m_->index_ - " + std::to_string(lag) + ")");
        const auto   places = std::exchange(places_, {});
        Reads* const reads  = std::exchange(clause_reads_, nullptr);
        for (const bool guard : {true, false}) {
            for (const Clause<Value>& clause : sequence.definition->Clauses()) {
                const ParametersDefinition<Value>& p = clause.parameters;
                if (otherwise || !p.general() || p.guarded() != guard) continue;
                index_ = p.index_name();
                const std::optional<std::string> condition =
                    guard ? Condition(p.guard()) : std::optional<std::string>("");
                if (!condition) continue;
                if (condition->empty())
                    otherwise = Emit(clause.expression);
                else
                    guarded.emplace_back(*condition, Emit(clause.expression));
            }
        }
        reading_      = reading;
        index_        = index;
        index_text_   = text;
        places_       = places;
        clause_reads_ = reads;
        chain.rows    = sequence.rows;
        chain.cols    = sequence.cols;
        for (std::size_t c = 0; c < sequence.rows * sequence.cols; ++c) {
            std::string cell;
            for (const auto& [condition, value] : guarded)
                cell +=
                    condition + " ? " + value.At(c / sequence.cols, c % sequence.cols).text + " : ";
            if (guarded.empty() && otherwise) {
                chain.cells.push_back(otherwise->At(c / sequence.cols, c % sequence.cols));
                continue;
            }
            cell += otherwise ? otherwise->At(c / sequence.cols, c % sequence.cols).text : "NAN";
            chain.cells.emplace_back(cell, 0);  // a conditional, below every operator
        }
        return chain;
    }

    static std::string Subscript(std::size_t i, std::size_t j) {
        return "[" + std::to_string(i) + "][" + std::to_string(j) + "]";
    }

    // How far back a term reads: its own index, or that less a constant.
    int Lag(const PExpression<Value>& index, const std::string& name) {
        const std::string written = name + "_(...)";
        const int         offset  = Offset(index, written);
        if (offset > 0) throw Reason(written + ": a term after the one being computed");
        return -offset;
    }

    // How far an index is from the clause's own: 'n', or that plus constants
    // however they are spelled -- 'n-1', 'n-k-1' in a sum over k.
    int Offset(const PExpression<Value>& index, const std::string& written) {
        const std::string only = written + ": an index other than " + index_ + " less a constant";
        if (const auto* ref = dynamic_cast<RefExpression<Value>*>(index.get());
            ref && ref->Name() == index_ && !places_.count(index_))
            return 0;
        const auto* sum = dynamic_cast<AddExpression<Value>*>(index.get());
        if (!sum) throw Reason(only);
        const Code left = Emit(sum->m_e1()), right = Emit(sum->m_e2());
        if (right.constant && !left.constant) return Offset(sum->m_e1(), written) + Whole(right);
        if (left.constant && !right.constant) return Whole(left) + Offset(sum->m_e2(), written);
        throw Reason(only);
    }

    static int Whole(const Code& code) {
        try {
            return AsIndex<Value>(*code.constant);
        } catch (const std::runtime_error& error) {
            throw Reason(error.what());
        }
    }

    PExpression<Value> visit(EqualExpression<Value>*) override {
        throw Reason("a local definition");
    }
    PExpression<Value> visit(CellExpression<Value>* expression) override {
        const Code matrix = Emit(expression->Matrix());
        const Code row = Emit(expression->Row()), col = Emit(expression->Col());
        if (matrix.constant && row.constant && col.constant) return Fold(expression);
        if (!row.constant || !col.constant) throw Reason("a cell whose place is not a constant");
        int i = 0, j = 0;
        try {
            i = AsIndex<Value>(*row.constant);
            j = AsIndex<Value>(*col.constant);
        } catch (const std::runtime_error& error) {
            throw Reason(error.what());
        }
        if (i < 1 || static_cast<std::size_t>(i) > matrix.rows || j < 1 ||
            static_cast<std::size_t>(j) > matrix.cols)
            throw Reason("row " + std::to_string(i) + ", column " + std::to_string(j) +
                         " is outside a " + std::to_string(matrix.rows) + "x" +
                         std::to_string(matrix.cols) + " matrix");
        return Answer(matrix.At(static_cast<std::size_t>(i - 1), static_cast<std::size_t>(j - 1)));
    }
    PExpression<Value> visit(FactExpression<Value>*) override { throw Reason("a factorial"); }
    // Unrolled, since each term is a line of C: a thousand is a filter no one
    // would write out as one.
    static constexpr int max_terms = 1000;

    // A sum or a product with constant bounds, unrolled with its index bound
    // as a constant, as a cell's names are: the terms that read it fold, and
    // the lags it gives are constants.
    PExpression<Value> visit(SeriesExpression<Value>* expression) override {
        if (!expression->Upper()) throw Reason("a sum or a product with no upper bound");
        const Code lower = Emit(expression->Lower()), upper = Emit(expression->Upper());
        if (!lower.constant || !upper.constant)
            throw Reason("a sum or a product whose bounds are not constants");
        const int first = Whole(lower), last = Whole(upper);
        if (last < first) return Answer(Literal(Value(Number(expression->Product() ? 1 : 0))));
        if (last - first >= max_terms)
            throw Reason("a sum or a product of more than " + std::to_string(max_terms) + " terms");
        const std::string&         name  = expression->Index();
        const auto                 found = places_.find(name);
        const std::optional<Value> outer =
            found == places_.end() ? std::nullopt : std::optional<Value>(found->second);
        Code total;
        bool constant = true;
        for (int k = first; k <= last; ++k) {
            places_[name]   = Value(Number(k));
            const Code term = Emit(expression->Body());
            constant        = constant && term.constant;
            total           = k == first              ? term
                              : expression->Product() ? Product(total, term)
                                                      : Broadcast(total, term, Added);
        }
        if (outer)
            places_[name] = *outer;
        else
            places_.erase(name);
        return constant ? Fold(expression) : Answer(total);
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
                code.cells.push_back(Atom("m_->" + name + (scalar ? "" : Subscript(i, j))));
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
        // The interpreter's guarded clauses answer below the lowest base
        // clause too, where its unguarded one does not: '_(-1)' of a sequence
        // based at 0 is a term wherever a guard holds. A step has no such
        // terms, so a read that could reach one is refused.
        for (const auto& [name, sequence] : sequences_) {
            int first = sequence.start;
            while (sequence.bases.count(first)) ++first;  // where the general clauses begin
            for (const auto& [read, lags] : sequence.reads) {
                const Sequence& other = sequences_.at(read);
                if (other.bases.empty() || other.guarded.empty()) continue;
                const int reached = first - *lags.rbegin();
                if (reached < other.start)
                    throw Refusal("cannot compile " + name + ": " + name + "_" +
                                  std::to_string(first) + " reads " + read + "_" +
                                  std::to_string(reached) + ", below " + read +
                                  "'s base clauses, where only its guards could give a term");
            }
        }
        for (std::size_t round = 0;; ++round) {
            bool moved = false;
            for (auto& [name, sequence] : sequences_) {
                if (!sequence.bases.empty() || !sequence.definition) continue;
                const int needed = std::max(sequence.start, Answers(sequence));
                if (needed <= sequence.start) continue;
                sequence.start = needed;
                moved          = true;
            }
            if (!moved) break;
            if (round > sequences_.size())
                throw Refusal(
                    "cannot compile: a sequence with no base clause reads back into "
                    "itself, so it never starts");
        }
        // Where a clause's reads begin is where it can be evaluated: before it,
        // the interpreter reports the term it could not read, and a step says
        // NaN. Only a sequence with guards has more than one path to choose.
        for (auto& [name, sequence] : sequences_) {
            for (Guarded& guarded : sequence.guarded) {
                guarded.guard_from = From(guarded.guard);
                guarded.value_from = From(guarded.value);
            }
        }
        // Without guards, the one clause is always evaluated, so a term it
        // reads before that term exists is an error in the model, said now.
        for (const auto& [name, sequence] : sequences_) {
            if (!sequence.guarded.empty()) continue;
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

    // The first index where every term these reads name exists.
    int From(const Reads& reads) const {
        int from = std::numeric_limits<int>::min();
        for (const auto& [read, lags] : reads)
            from = std::max(from, sequences_.at(read).start + *lags.rbegin());
        return from;
    }

    // The first index where some path through a sequence's clauses answers:
    // the guards tried before a clause must be evaluable, and the clause.
    int Answers(const Sequence& sequence) const {
        if (sequence.guarded.empty()) return From(sequence.reads);
        int guards = std::numeric_limits<int>::min();
        int first  = std::numeric_limits<int>::max();
        for (const Guarded& guarded : sequence.guarded) {
            guards = std::max(guards, From(guarded.guard));
            first  = std::min(first, std::max(guards, From(guarded.value)));
        }
        if (sequence.general.front() != "NAN")
            first = std::min(first, std::max(guards, From(sequence.general_reads)));
        return first;
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

    // Those something reads: a cell read out of a product, or a clause that was
    // compiled only to be refused, leaves some that nothing does, and C would
    // say so.
    static std::string Temporaries(const std::vector<Temporary>& declared,
                                   const std::string& assignments, const std::string& indent) {
        std::string read = assignments, kept;
        for (auto temporary = declared.rbegin(); temporary != declared.rend(); ++temporary) {
            if (read.find(temporary->name) == std::string::npos) continue;
            std::string lines;
            for (const std::string& line : temporary->lines) lines += indent + line + "\n";
            read += lines;
            kept = lines + kept;
        }
        return kept;
    }

    // The interpreter's Gauss-Jordan (Matrix::Inverse), step for step: the
    // largest pivot, any rather than an exact zero, and NaN for every cell
    // where the matrix is singular, which the interpreter reports.
    std::string InverseHelper(std::size_t n) const {
        const std::string size = std::to_string(n);
        return "/* The inverse of a " + size + "x" + size +
               " matrix, in place, as the interpreter takes it: Gauss-Jordan\n"
               " * on the largest pivot, and NaN in every cell where there is none. */\n"
               "static inline void " +
               module_ + "_inverse" + size + "_(double a[" + size + "][" + size +
               "]) {\n"
               "    double r[" +
               size + "][" + size +
               "];\n"
               "    for (int i = 0; i < " +
               size + "; ++i)\n        for (int j = 0; j < " + size +
               "; ++j) r[i][j] = i == j;\n"
               "    for (int col = 0; col < " +
               size +
               "; ++col) {\n"
               "        int pivot = col;\n"
               "        for (int row = col + 1; row < " +
               size +
               "; ++row)\n"
               "            if (fabs(a[row][col]) > fabs(a[pivot][col]) ||\n"
               "                (a[pivot][col] == 0.0 && a[row][col] != 0.0))\n"
               "                pivot = row;\n"
               "        if (a[pivot][col] == 0.0) {\n"
               "            for (int i = 0; i < " +
               size + "; ++i)\n                for (int j = 0; j < " + size +
               "; ++j) a[i][j] = NAN;\n"
               "            return;\n"
               "        }\n"
               "        for (int j = 0; j < " +
               size +
               "; ++j) {\n"
               "            const double s = a[pivot][j], t = r[pivot][j];\n"
               "            a[pivot][j] = a[col][j];\n"
               "            r[pivot][j] = r[col][j];\n"
               "            a[col][j]   = s;\n"
               "            r[col][j]   = t;\n"
               "        }\n"
               "        const double scale = a[col][col];\n"
               "        for (int j = 0; j < " +
               size +
               "; ++j) {\n"
               "            a[col][j] = a[col][j] / scale;\n"
               "            r[col][j] = r[col][j] / scale;\n"
               "        }\n"
               "        for (int row = 0; row < " +
               size +
               "; ++row) {\n"
               "            const double factor = a[row][col];\n"
               "            if (row == col || factor == 0.0) continue;\n"
               "            for (int j = 0; j < " +
               size +
               "; ++j) {\n"
               "                a[row][j] = a[row][j] - factor * a[col][j];\n"
               "                r[row][j] = r[row][j] - factor * r[col][j];\n"
               "            }\n"
               "        }\n"
               "    }\n"
               "    memcpy(a, r, sizeof r);\n"
               "}\n\n";
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

        for (const std::size_t n : inverses_) out += InverseHelper(n);

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
            const bool        late     = sequence.start > earliest;
            const std::string indent   = late ? "        " : "    ";
            if (late) out += "    if (m_->index_ >= " + std::to_string(sequence.start) + ") {\n";
            std::string assignments;
            for (std::size_t c = 0; c < sequence.general.size(); ++c) {
                assignments += indent + "m_->" + name + "[0]" +
                               (scalar ? "" : Subscript(c / sequence.cols, c % sequence.cols)) +
                               " = ";
                for (const auto& [index, base] : sequence.bases)
                    assignments +=
                        "m_->index_ == " + std::to_string(index) + " ? " + base[c] + " : ";
                // Checked only where an index no base clause gives needs it.
                const auto before = [&](int from) {
                    for (int n = sequence.start; n < from; ++n)
                        if (!sequence.bases.count(n))
                            return "m_->index_ < " + std::to_string(from) + " ? NAN : ";
                    return std::string();
                };
                for (const Guarded& guarded : sequence.guarded) {
                    const std::string value = before(guarded.value_from);
                    assignments +=
                        before(guarded.guard_from) + guarded.condition + " ? " +
                        (value.empty() ? guarded.cells[c] : "(" + value + guarded.cells[c] + ")") +
                        " : ";
                }
                if (!sequence.guarded.empty()) assignments += before(From(sequence.general_reads));
                assignments += sequence.general[c] + ";\n";
            }
            out += Temporaries(sequence.temporaries, assignments, indent) + assignments;
            if (late) out += "    }\n";
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
    std::vector<Temporary>*          temporaries_ = nullptr;  // where this sequence's are declared
    Reads*      clause_reads_ = nullptr;               // what the clause being compiled reads
    std::string index_text_   = "(double)m_->index_";  // the index, as the step has it
    std::string                      module_;
    std::set<std::size_t>            inverses_;  // the sizes a helper is needed for
    int                              temporary_count_ = 0;
    std::map<std::string, Value>     places_;             // a cell's row and column, by their names
    Code        code_;
};

#endif  // INKAMATH_COMPILE_HPP
