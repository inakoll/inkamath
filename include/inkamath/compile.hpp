#ifndef INKAMATH_COMPILE_HPP
#define INKAMATH_COMPILE_HPP

#include "inkamath/matrix.hpp"
#include "inkamath/number.hpp"
#include "inkamath/reference_stack.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// What the compiler will not compile, said in the words of what was written.
struct Refusal : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// MODERNIZATION.md, phase 14, step 2: the sequences a session defines, as a C
// header over doubles. Each sequence keeps a window of its latest terms, and a
// step computes the next index's terms in the order they need each other.
class CompileC : public TransformationVisitor<Matrix<Number>> {
public:
    using Value = Matrix<Number>;
    using TransformationVisitor<Value>::visit;

    static std::string Header(const ReferenceStack<Value>& definitions, const std::string& module,
                              const std::string& source) {
        CompileC compiler(definitions);
        for (const auto& [name, definition] : Sorted(definitions.Globals())) {
            try {
                compiler.Define(name, *definition);
            } catch (const Refusal& refusal) {
                throw Refusal("cannot compile " + name + ": " + refusal.what());
            }
        }
        return compiler.Print(module, source);
    }

private:
    // A primary, a unary, a product, a sum: an operand is parenthesised only
    // where C would otherwise read it differently.
    enum Level { sum = 1, product, unary, primary };

    struct Code {
        Code() = default;
        Code(std::string written, int precedence) : text(std::move(written)), level(precedence) {}

        std::string          text;
        int                  level = primary;
        std::optional<Value> constant;   // its exact value, where it has no name in it
        std::string          magnitude;  // what it is the negation of, if it is one
        int                  magnitude_level = primary;
    };

    struct Sequence {
        std::map<int, std::string>           bases;  // the base clauses, by index
        std::string                          general;
        bool                                 input = false;
        std::map<std::string, std::set<int>> reads;  // lags, by the sequence read
        int                                  depth = 1;
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
            if (!clause.parameters.parameters_names().empty()) throw Refusal("a function");
            if (clause.parameters.guarded()) throw Refusal("a guarded clause");
        }
        if (!IsSequence(definition)) return;
        Unreserved(name);
        Sequence& sequence = sequences_[name];
        for (const Clause<Value>& clause : definition.Clauses()) {
            if (clause.parameters.general()) {
                reading_         = &sequence;
                index_           = clause.parameters.index_name();
                sequence.general = Emit(clause.expression).text;
                reading_         = nullptr;
                index_.clear();
            } else {
                sequence.bases[clause.parameters.index()] = Emit(clause.expression).text;
            }
        }
        if (sequence.general.empty()) throw Refusal("a sequence with no general clause");
    }

    Code Emit(const PExpression<Value>& expression) {
        expression->accept(*this);
        return code_;
    }

    static std::string Wrap(const std::string& text, int level, int needed) {
        return level < needed ? "(" + text + ")" : text;
    }
    static std::string Wrap(const Code& code, int needed) {
        return Wrap(code.text, code.level, needed);
    }

    PExpression<Value> Answer(Code code) {
        code_ = std::move(code);
        return PExpression<Value>();
    }

    // Exactly, as the interpreter would, and rounded once: 0.1 + 0.2 is 3/10
    // here, which a C compiler folding the doubles would not find.
    PExpression<Value> Fold(Expression<Value>* expression) {
        ReferenceStack<Value>    scratch;
        EvaluationVisitor<Value> evaluator(scratch);
        try {
            const Value value = expression->accept(evaluator);
            return Answer(Literal(value));
        } catch (const Refusal&) {
            throw;
        } catch (const std::runtime_error& error) {
            throw Refusal(error.what());
        }
    }

    static Code Literal(const Value& value) {
        if (!value.IsScalar()) throw Refusal("a matrix");
        const auto number = value(1, 1).Inexact();
        if (number.imag() != 0) throw Refusal("a complex number");
        Code code(Double(std::abs(number.real())), primary);
        code.constant = value;
        if (std::signbit(number.real())) {
            code.magnitude = code.text;
            code.text      = "-" + code.text;
            code.level     = unary;
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

    PExpression<Value> visit(ValExpression<Value>* expression) override {
        return Answer(Literal(expression->value));
    }

    PExpression<Value> visit(AddExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        // a + (-b) is a - b in IEEE arithmetic, and reads as what was written.
        if (!right.magnitude.empty()) {
            return Answer(
                {Wrap(left, sum) + " - " + Wrap(right.magnitude, right.magnitude_level, product),
                 sum});
        }
        return Answer({Wrap(left, sum) + " + " + Wrap(right, product), sum});
    }

    PExpression<Value> visit(NegExpression<Value>* expression) override {
        const Code operand = Emit(expression->m_e());
        if (operand.constant) return Fold(expression);
        const std::string text =
            operand.magnitude.empty() ? Wrap(operand, unary) : "(" + operand.text + ")";
        Code negation("-" + text, unary);
        negation.magnitude       = operand.text;
        negation.magnitude_level = operand.level;
        return Answer(negation);
    }

    PExpression<Value> visit(InexactExpression<Value>* expression) override {
        const Code operand = Emit(expression->m_e());
        return operand.constant ? Fold(expression) : Answer(operand);
    }

    PExpression<Value> Binary(BinaryExpression<Value>* expression, const char* op, int level) {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        return Answer({Wrap(left, level) + op + Wrap(right, level + 1), level});
    }

    PExpression<Value> visit(MultExpression<Value>* expression) override {
        return Binary(expression, " * ", product);
    }
    PExpression<Value> visit(DivExpression<Value>* expression) override {
        return Binary(expression, " / ", product);
    }

    PExpression<Value> visit(PowExpression<Value>* expression) override {
        const Code base = Emit(expression->m_e1()), exponent = Emit(expression->m_e2());
        if (base.constant && exponent.constant) return Fold(expression);
        return Answer({"pow(" + base.text + ", " + exponent.text + ")", primary});
    }

    PExpression<Value> visit(CompareExpression<Value>* expression) override {
        static const char* const ops[] = {" < ", " > ", " <= ", " >= ", " == ", " != "};
        const Code               left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        return Answer({"(" + Wrap(left, sum) + ops[static_cast<int>(expression->Op())] +
                           Wrap(right, sum) + " ? 1.0 : 0.0)",
                       primary});
    }

    PExpression<Value> visit(RefExpression<Value>* expression) override {
        const std::string& name = expression->Name();
        if (!index_.empty() && name == index_) return Answer({"(double)m_->index_", unary});
        const Reference<Value>* definition = Global(name);
        if (!definition) return Answer({Field(name, NAN), primary});
        if (IsSequence(*definition)) throw Refusal(name + " is a sequence; index it");
        if (!reading_plain_.insert(name).second) throw Refusal(name + " is defined by itself");
        // A global is evaluated in a scope of its own, where no index is seen.
        Sequence* const   reading = std::exchange(reading_, nullptr);
        const std::string index   = std::exchange(index_, std::string());
        Code              code    = Emit(definition->Clauses().front().expression);
        reading_                  = reading;
        index_                    = index;
        reading_plain_.erase(name);
        // A value that reads no other is a parameter the host may change; one
        // that does is recomputed where it is read, so that it follows them.
        if (code.constant) {
            const auto number = (*code.constant)(1, 1).Inexact();
            code              = {Field(name, number.real()), primary};
        }
        return Answer(code);
    }

    PExpression<Value> visit(FuncExpression<Value>* expression) override {
        const std::string&           name = expression->Name();
        const ParametersCall<Value>& call = expression->Call();
        if (call.limit()) throw Refusal("a limit");
        if (!call.parameters_expression().empty() || !call.parameters_dict().empty())
            throw Refusal("a function");
        if (!reading_) throw Refusal(name + "_...: a term read outside a general clause");
        const Reference<Value>* definition = Global(name);
        if (definition && !IsSequence(*definition)) throw Refusal(name + " is not a sequence");
        if (!definition) {
            Unreserved(name);
            sequences_[name].input = true;
        }
        const int lag = Lag(call.subexpr(), name);
        reading_->reads[name].insert(lag);
        return Answer({"m_->" + name + "[" + std::to_string(lag) + "]", primary});
    }

    // How far back a term reads: its own index, or that less a constant.
    int Lag(const PExpression<Value>& index, const std::string& name) {
        const std::string written = name + "_(...)";
        if (const auto* ref = dynamic_cast<RefExpression<Value>*>(index.get());
            ref && ref->Name() == index_)
            return 0;
        const auto* sum = dynamic_cast<AddExpression<Value>*>(index.get());
        const auto* ref = sum ? dynamic_cast<RefExpression<Value>*>(sum->m_e1().get()) : nullptr;
        if (!ref || ref->Name() != index_)
            throw Refusal(written + ": an index other than " + index_ + " less a constant");
        const Code offset = Emit(sum->m_e2());
        if (!offset.constant)
            throw Refusal(written + ": an index other than " + index_ + " less a constant");
        const int step = AsIndex<Value>(*offset.constant);
        if (step > 0) throw Refusal(written + ": a term after the one being computed");
        return -step;
    }

    PExpression<Value> visit(EqualExpression<Value>*) override {
        throw Refusal("a local definition");
    }
    PExpression<Value> visit(MatExpression<Value>*) override { throw Refusal("a matrix"); }
    PExpression<Value> visit(CellExpression<Value>*) override { throw Refusal("a matrix"); }
    PExpression<Value> visit(FactExpression<Value>*) override { throw Refusal("a factorial"); }
    PExpression<Value> visit(SeriesExpression<Value>*) override {
        throw Refusal("a sum or a product");
    }
    PExpression<Value> visit_other(Expression<Value>*) override {
        throw Refusal("this expression");
    }

    std::string Field(const std::string& name, double initial) {
        Unreserved(name);
        parameters_.emplace(name, initial);
        return "m_->" + name;
    }

    // A name here is letters and digits, so it can only collide with C's own.
    static void Unreserved(const std::string& name) {
        static const std::set<std::string> keywords = {
            "auto",    "break",  "case",     "char",   "const",    "continue", "default",
            "do",      "double", "else",     "enum",   "extern",   "float",    "for",
            "goto",    "if",     "inline",   "int",    "long",     "register", "restrict",
            "return",  "short",  "signed",   "sizeof", "static",   "struct",   "switch",
            "typedef", "union",  "unsigned", "void",   "volatile", "while"};
        if (keywords.count(name)) throw Refusal(name + " is a word C keeps for itself");
    }

    // The index every sequence starts at, and the order a step computes them.
    int Start() const {
        std::optional<int> start;
        std::string        first;
        for (const auto& [name, sequence] : sequences_) {
            if (sequence.bases.empty()) continue;
            const int lowest = sequence.bases.begin()->first;
            if (start && lowest != *start)
                throw Refusal("cannot compile: " + first + " starts at " + std::to_string(*start) +
                              " and " + name + " at " + std::to_string(lowest));
            if (!start) start = lowest, first = name;
        }
        return start.value_or(0);
    }

    std::vector<std::string> Order() const {
        std::map<std::string, std::set<std::string>> waiting;
        for (const auto& [name, sequence] : sequences_) {
            if (sequence.input) continue;
            waiting[name];
            for (const auto& [read, lags] : sequence.reads) {
                if (!lags.count(0) || sequences_.at(read).input) continue;
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

    // A term read before its sequence starts would be the interpreter's error
    // at run time; here it is known before anything runs.
    void CheckReach(int start) {
        for (auto& [name, sequence] : sequences_) {
            for (const auto& [read, lags] : sequence.reads) {
                Sequence& other = sequences_.at(read);
                other.depth     = std::max(other.depth, *lags.rbegin() + 1);
                for (int n = start; n < start + *lags.rbegin(); ++n) {
                    if (sequence.bases.count(n)) continue;
                    for (const int lag : lags) {
                        if (n - lag < start)
                            throw Refusal("cannot compile " + name + ": " + name + "_" +
                                          std::to_string(n) + " reads " + read + "_" +
                                          std::to_string(n - lag) + ", before it starts at " +
                                          std::to_string(start));
                    }
                }
            }
        }
    }

    std::string Print(const std::string& module, const std::string& source) {
        for (const auto& [name, sequence] : sequences_)
            if (sequence.input && parameters_.count(name))
                throw Refusal("cannot compile: " + name +
                              " is read both as a value and as a sequence");
        const int                      start = Start();
        const std::vector<std::string> order = Order();
        CheckReach(start);
        std::vector<std::string> inputs;
        for (const auto& [name, sequence] : sequences_)
            if (sequence.input) inputs.push_back(name);
        std::vector<std::string> fields = inputs;
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
        for (const auto& [name, initial] : parameters_) out += "    double " + name + ";\n";
        out += "    long long index_;\n";
        for (const std::string& name : fields)
            out += "    double " + name + "[" + std::to_string(sequences_.at(name).depth) + "];\n";
        out += "} " + module + ";\n\n";

        out += "static inline void " + module + "_init(" + module + "* m_) {\n";
        out += "    memset(m_, 0, sizeof *m_);\n";
        for (const auto& [name, initial] : parameters_)
            out += "    m_->" + name + " = " + Double(initial) + ";\n";
        out += "    m_->index_ = " + std::to_string(start - 1) + ";\n}\n\n";

        out += "/* Advances to the next index, the first at " + std::to_string(start) +
               ", and computes its terms. */\n";
        out += "static inline void " + module + "_step(" + module + "* m_";
        for (const std::string& name : inputs) out += ", double " + name;
        out += ") {\n    ++m_->index_;\n";
        for (const std::string& name : fields) {
            for (int k = sequences_.at(name).depth - 1; k > 0; --k)
                out += "    m_->" + name + "[" + std::to_string(k) + "] = m_->" + name + "[" +
                       std::to_string(k - 1) + "];\n";
        }
        for (const std::string& name : inputs) out += "    m_->" + name + "[0] = " + name + ";\n";
        for (const std::string& name : order) {
            const Sequence& sequence = sequences_.at(name);
            out += "    m_->" + name + "[0] = ";
            for (const auto& [index, base] : sequence.bases)
                out += "m_->index_ == " + std::to_string(index) + " ? " + base + " : ";
            out += sequence.general + ";\n";
        }
        out += "}\n\n#endif\n";
        return out;
    }

    const ReferenceStack<Value>&    definitions_;
    std::map<std::string, Sequence> sequences_;
    std::map<std::string, double>   parameters_;
    std::set<std::string>           reading_plain_;
    Sequence*   reading_ = nullptr;  // the sequence whose general clause this is
    std::string index_;              // and the name of its index
    Code        code_;
};

#endif  // INKAMATH_COMPILE_HPP
