#ifndef INKAMATH_DERIVATIVE_HPP
#define INKAMATH_DERIVATIVE_HPP

#include "inkamath/reference_stack.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

// 'grad_(x = a) body' (DESIGN.md, differentiation). The body is evaluated with
// its derivatives, forward, node by node, so that each rule is chosen with the
// values in hand: the clause a guard takes, whether floor is at a jump,
// whether a power's base is zero. Every value carries one derivative per set
// of the grads it is under, so a grad inside a grad is a second derivative.
template <typename T>
class Derivative {
public:
    explicit Derivative(ReferenceStack<T>& stack) : stack_(stack), ordinary_(stack) {}

    T At(GradExpression<T>& grad) {
        const Local local(*this);
        return *Grad(grad)[0];
    }

private:
    // Indexed by a set of grads as bits, [0] being the value; empty where the
    // derivative is zero, so that a zero needs no shape.
    using Jet = std::vector<std::optional<T>>;

    // Each grad doubles a value's parts.
    static constexpr int max_order = 8;

    [[nodiscard]] std::size_t Size() const { return std::size_t(1) << order_; }

    Jet Constant(const T& value) const {
        Jet jet(Size());
        jet[0] = value;
        return jet;
    }

    Jet Padded(Jet jet) const {
        jet.resize(Size());
        return jet;
    }

    static T Zero(Extent extent) { return T(extent); }

    static bool IsZero(const T& value) {
        const typename T::value_type zero(0);
        for (std::size_t k = 0; k < value.Size().count(); ++k)
            if (!(value.data()[k] == zero)) return false;
        return true;
    }

    // Whether a part other than the value is there. What the checks for a jump
    // or a moving exponent ask: a part that happens to be zero at the point,
    // as at a tangent, is no evidence that nothing moves (DESIGN.md, C73).
    static bool Moves(const Jet& jet) {
        for (std::size_t s = 1; s < jet.size(); ++s)
            if (jet[s]) return true;
        return false;
    }

    template <typename F>
    static Jet Map(Jet jet, F f) {
        for (auto& part : jet)
            if (part) part = f(*part);
        return jet;
    }

    static void Add(std::optional<T>& total, const T& term) {
        total = total ? *total + term : term;
    }

    static Jet Sum(const Jet& a, const Jet& b) {
        Jet c(a.size());
        for (std::size_t s = 0; s < a.size(); ++s) {
            if (a[s]) Add(c[s], *a[s]);
            if (b[s]) Add(c[s], *b[s]);
        }
        return c;
    }

    // The order of the factors is kept: a matrix product does not commute.
    static Jet Product(const Jet& a, const Jet& b) {
        Jet c(a.size());
        for (std::size_t s = 0; s < a.size(); ++s)
            for (std::size_t part = s;; part = (part - 1) & s) {
                if (a[part] && b[s ^ part]) Add(c[s], *a[part] * *b[s ^ part]);
                if (part == 0) break;
            }
        return c;
    }

    // Cell by cell, as '/' divides.
    static T Cellwise(const T& a, const T& b) {
        if (a.IsTensor() || b.IsTensor()) return T::Sliced(a, b, Cellwise);
        if (a.IsScalar() || b.IsScalar()) return a * b;
        if (a.Size() != b.Size()) throw std::runtime_error("these matrices have different sizes");
        T c(a.Size());
        for (std::size_t k = 0; k < a.Size().count(); ++k) c.data()[k] = a.data()[k] * b.data()[k];
        return c;
    }

    static Jet Quotient(const Jet& a, const Jet& b) {
        Jet c(a.size());
        c[0] = *a[0] / *b[0];
        for (std::size_t s = 1; s < a.size(); ++s) {
            std::optional<T> top = a[s];
            for (std::size_t part = s; part != 0; part = (part - 1) & s)
                if (b[part] && c[s ^ part]) Add(top, -Cellwise(*c[s ^ part], *b[part]));
            if (top) c[s] = *top / *b[0];
        }
        return c;
    }

    // A matrix's inverse: from A*B = 1, each part of B is what cancels the
    // parts below it.
    static Jet Inverse(const Jet& a) {
        Jet b(a.size());
        b[0] = numeric_interface<T>::pow(*a[0], T(-1));
        for (std::size_t s = 1; s < a.size(); ++s) {
            std::optional<T> sum;
            for (std::size_t part = s; part != 0; part = (part - 1) & s)
                if (a[part] && b[s ^ part]) Add(sum, *a[part] * *b[s ^ part]);
            if (sum) b[s] = -(*b[0] * *sum);
        }
        return b;
    }

    // f(u) for a single value u, from f's derivatives at its value: each part
    // is a sum over the ways of splitting its set into blocks (Faa di Bruno).
    template <typename F>
    Jet Composed(const Jet& u, F derivative, const T& value) const {
        Jet                                          out(u.size());
        std::vector<std::optional<std::optional<T>>> known(static_cast<std::size_t>(order_) + 1);
        out[0] = value;
        for (std::size_t s = 1; s < u.size(); ++s) {
            const auto each = [&](int blocks, const T& product) {
                auto& slot = known[static_cast<std::size_t>(blocks)];
                if (!slot) slot = derivative(blocks);
                if (*slot) Add(out[s], **slot * product);
            };
            Blocks(u, s, 0, std::nullopt, each);
        }
        return out;
    }

    template <typename F>
    static void Blocks(const Jet& u, std::size_t set, int blocks, const std::optional<T>& product,
                       F& each) {
        if (set == 0) {
            each(blocks, *product);
            return;
        }
        const std::size_t low = set & (~set + 1), rest = set ^ low;
        for (std::size_t sub = rest;; sub = (sub - 1) & rest) {
            const std::size_t block = sub | low;
            if (u[block])
                Blocks(u, set ^ block, blocks + 1, product ? *product * *u[block] : *u[block],
                       each);
            if (sub == 0) break;
        }
    }

    // The innermost grad's name at its point, for a message.
    [[nodiscard]] std::string Where() const {
        return grads_.back().first + " = " + grads_.back().second;
    }

    Jet Eval(const PExpression<T>& e) { return Eval(*e); }

    // A dynamic_cast without its search of the bases, which is most of what
    // the dispatch below cost: no node class derives from another, so a node
    // has no type but its own to match.
    template <typename Node>
    static Node* Exactly(Expression<T>& e) {
        return typeid(e) == typeid(Node) ? static_cast<Node*>(&e) : nullptr;
    }

    Jet Eval(Expression<T>& e) {
        if (auto* x = Exactly<ValExpression<T>>(e)) return Constant(x->value);
        if (auto* x = Exactly<RefExpression<T>>(e)) return Name(*x);
        if (auto* x = Exactly<FuncExpression<T>>(e)) return Call(*x);
        if (auto* x = Exactly<AddExpression<T>>(e)) {
            const Jet a = Eval(x->m_e1());
            return Sum(a, Eval(x->m_e2()));
        }
        if (auto* x = Exactly<NegExpression<T>>(e))
            return Map(Eval(x->m_e()), [](const T& v) { return -v; });
        if (auto* x = Exactly<MultExpression<T>>(e)) {
            const Jet a = Eval(x->m_e1());
            return Product(a, Eval(x->m_e2()));
        }
        if (auto* x = Exactly<DivExpression<T>>(e)) {
            const Jet a = Eval(x->m_e1());
            return Quotient(a, Eval(x->m_e2()));
        }
        if (auto* x = Exactly<PowExpression<T>>(e)) return Power(*x);
        if (auto* x = Exactly<InexactExpression<T>>(e))
            return Map(Eval(x->m_e()), [](const T& v) { return numeric_interface<T>::inexact(v); });
        if (auto* x = Exactly<TransposeExpression<T>>(e))
            return Map(Eval(x->m_e()),
                       [](const T& v) { return numeric_interface<T>::transpose(v); });
        if (auto* x = Exactly<FloorExpression<T>>(e)) return Floor(*x);
        if (auto* x = Exactly<FactExpression<T>>(e)) {
            const Jet u = Eval(x->m_e());
            if (Moves(u)) throw std::runtime_error("grad cannot differentiate a factorial");
            return Constant(T(numeric_interface<T>::fact(*u[0])));
        }
        if (auto* x = Exactly<MatExpression<T>>(e)) {
            if (x->numbers) return Constant(x->accept(ordinary_));
            return Literal(*x, [x](std::vector<PExpression<T>> parts) {
                return std::make_shared<MatExpression<T>>(x->Size().rows, x->Size().cols,
                                                          std::move(parts));
            });
        }
        if (auto* x = Exactly<TensorExpression<T>>(e))
            return Literal(*x, [](std::vector<PExpression<T>> parts) {
                return std::make_shared<TensorExpression<T>>(std::move(parts));
            });
        if (auto* x = Exactly<CellExpression<T>>(e)) return Cell(*x);
        if (auto* x = Exactly<CompareExpression<T>>(e)) return Compare(*x);
        if (auto* x = Exactly<LogicExpression<T>>(e)) return Logic(*x);
        if (auto* x = Exactly<SeriesExpression<T>>(e)) return Series(*x);
        if (auto* x = Exactly<GradExpression<T>>(e)) return Grad(*x);
        if (auto* x = Exactly<MemberExpression<T>>(e)) {
            if (Reads(*x))
                throw std::runtime_error("grad cannot differentiate through an instance yet");
            return Constant(x->accept(ordinary_));
        }
        throw std::runtime_error("grad cannot differentiate a local definition yet");
    }

    // The names a grad or a call binds, which nothing else reads.
    const Jet* Lookup(const std::string& name) const {
        if (frames_.empty()) return nullptr;
        for (const auto& [bound, jet] : frames_.back())
            if (bound == name) return &jet;
        return nullptr;
    }

    // Whether an expression reads a name the innermost frame binds; what does
    // not is a constant, whatever it calls, since a call sees only its own.
    bool Reads(const Expression<T>& e) const {
        if ((dynamic_cast<const RefExpression<T>*>(&e) ||
             dynamic_cast<const FuncExpression<T>*>(&e)) &&
            Lookup(e.Name()))
            return true;
        for (const PExpression<T>& child : e.Children())
            if (child && Reads(*child)) return true;
        return false;
    }

    static bool Mentions(const Expression<T>& e, const std::string& name) {
        if ((dynamic_cast<const RefExpression<T>*>(&e) ||
             dynamic_cast<const FuncExpression<T>*>(&e)) &&
            e.Name() == name)
            return true;
        for (const PExpression<T>& child : e.Children())
            if (child && Mentions(*child, name)) return true;
        return false;
    }

    Jet Name(RefExpression<T>& ref) {
        if (const Jet* bound = Lookup(ref.Name())) return Padded(*bound);
        return Constant(ref.accept(ordinary_));
    }

    typedef std::vector<std::pair<std::string, Jet>> Arguments;

    Jet Call(FuncExpression<T>& call) {
        if (Lookup(call.Name()) || !Reads(call)) return Constant(call.accept(ordinary_));
        const auto definition = stack_.Global(call.Name());
        if (stack_.Binds(call.Name()))
            throw std::runtime_error("grad cannot differentiate a local definition yet");
        if (!definition || !definition->Value()) return Constant(call.accept(ordinary_));
        const ParametersCall<T>&       p          = call.Call();
        const ParametersDefinition<T>& parameters = definition->Clauses().front().parameters;
        parameters.CheckArity(definition->Name(), p);
        Arguments arguments;
        auto      name = parameters.parameters_names().begin();
        for (const auto& argument : p.parameters_expression())
            if (name != parameters.parameters_names().end())
                arguments.emplace_back(*name++, Eval(argument));
        for (const auto& [keyword, argument] : p.parameters_dict())
            arguments.emplace_back(keyword, Eval(argument));
        if (p.limit()) return Limit(*definition, p, arguments);
        int        index   = 0;
        const bool indexed = p.TryEvalIndex(stack_, index);
        return Term(*definition, p, indexed, index, arguments);
    }

    std::string Key(const Reference<T>& definition, bool indexed, int index,
                    const Arguments& arguments) const {
        std::string key = std::to_string(reinterpret_cast<std::uintptr_t>(&definition)) + '#' +
                          std::to_string(order_) + (indexed ? "_" + std::to_string(index) : "");
        for (const auto& [name, jet] : arguments) {
            key += '\0' + name + '=';
            for (const auto& part : jet) {
                key += part ? '\2' : '\1';
                if (part) numeric_interface<T>::key(*part, key);
            }
        }
        return key;
    }

    // A call, in a frame of its own as its evaluation would be, its arguments
    // bound as values for what reads them as values and with their derivatives
    // for what differentiates. Remembered by its arguments' every part, so a
    // term remembered without its derivative never answers for one.
    Jet Term(const Reference<T>& definition, const ParametersCall<T>& call, bool indexed, int index,
             const Arguments& arguments) {
        const std::string key = Key(definition, indexed, index, arguments);
        if (const auto found = memo_.find(key); found != memo_.end()) return found->second;
        if (indexed && !filling_ && depth_ >= ReferenceStack<T>::max_depth / 2)
            Fill(definition, call, index, arguments);
        stack_.Step();
        const Deeper                       deeper(*this);
        typename ReferenceStack<T>::Within within(stack_, definition.home);
        typename ReferenceStack<T>::Frame  frame(stack_);
        ParametersDefinition<T>::Bind(definition.captured, stack_);
        for (const auto& [name, jet] : arguments) stack_.BindValue(name, *jet[0]);
        definition.Clauses().front().parameters.BindDefaults(call, ordinary_);
        const Local local(*this);
        for (const auto& argument : arguments) frames_.back().push_back(argument);
        const Flag unguarded(guard_, false);
        Jet        result = Dispatch(definition, indexed, index);
        memo_.emplace(key, result);
        return result;
    }

    // A term far from its base, filled from the base up so that each finds
    // the one before it remembered, as Reference::Filled does. What the fill
    // cannot reach, the term says itself.
    void Fill(const Reference<T>& definition, const ParametersCall<T>& call, int index,
              const Arguments& arguments) {
        std::optional<int> lowest;
        for (const Clause<T>& clause : definition.Clauses()) {
            const ParametersDefinition<T>& p = clause.parameters;
            if (p.indexed() && !p.general() && !p.guarded())
                lowest = std::min(lowest.value_or(p.index()), p.index());
        }
        if (!lowest) return;
        const Flag filling(filling_, true);
        try {
            for (int k = *lowest + 1; k < index; ++k)
                (void)Term(definition, call, true, k, arguments);
        } catch (const std::runtime_error&) {
        }
    }

    // As Reference::EvalImp chooses: guarded clauses and base clauses in the
    // order written, then the general clause or the plain one.
    Jet Dispatch(const Reference<T>& definition, bool indexed, int index) {
        const std::string& name    = definition.Name();
        const Clause<T>*   plain   = nullptr;
        const Clause<T>*   general = nullptr;
        bool               guarded = false;
        std::optional<int> lowest;
        for (const Clause<T>& clause : definition.Clauses()) {
            const ParametersDefinition<T>& p = clause.parameters;
            if (p.cells())
                throw std::runtime_error("grad cannot differentiate a definition by cells yet");
            if (p.guarded())
                guarded = true;
            else if (!p.indexed())
                plain = plain ? plain : &clause;
            else if (p.general())
                general = general ? general : &clause;
            else
                lowest = std::min(lowest.value_or(p.index()), p.index());
        }
        for (const Clause<T>& clause : definition.Clauses()) {
            const ParametersDefinition<T>& p = clause.parameters;
            if (!p.guarded() && (!p.indexed() || p.general())) continue;
            if (Selects(definition, clause, indexed, index)) return Eval(clause.expression);
        }
        if (indexed) {
            if (plain) throw std::runtime_error(name + " is not a sequence");
            if (general && (!lowest || index >= *lowest)) {
                stack_.BindValue(general->parameters.index_name(), T(index));
                return Eval(general->expression);
            }
            if (guarded) throw std::runtime_error("no clause of " + name + " applies");
            throw std::runtime_error(name + " has no clause for index " + std::to_string(index));
        }
        if (plain) return Eval(plain->expression);
        if (guarded) throw std::runtime_error("no clause of " + name + " applies");
        throw std::runtime_error(name + " is a sequence; index it (" + name + "_" +
                                 std::to_string(lowest.value_or(0)) + ")");
    }

    bool Selects(const Reference<T>& definition, const Clause<T>& clause, bool indexed, int index) {
        const ParametersDefinition<T>& p = clause.parameters;
        if (p.indexed() != indexed) return false;
        if (!p.general() && indexed && p.index() != index) return false;
        if (!p.general()) return !p.guarded() || Holds(definition, p.guard());
        typename ReferenceStack<T>::Trial trial(stack_, p.index_name());
        stack_.BindValue(p.index_name(), T(index));
        if (p.guarded() && !Holds(definition, p.guard())) return false;
        trial.keep();
        return true;
    }

    // A guard is asked for its value; a comparison in it that holds at the
    // point only, an equality where its sides move apart, takes a clause
    // whose slope is not the function's.
    bool Holds(const Reference<T>& definition, const PExpression<T>& guard) {
        const Flag               guarding(guard_, true);
        const std::string* const previous = guarded_;
        guarded_                          = &definition.Name();
        const Jet held                    = Eval(guard);
        guarded_                          = previous;
        return numeric_interface<T>::truth(*held[0]);
    }

    Jet Compare(CompareExpression<T>& compare) {
        const Jet a     = Eval(compare.m_e1());
        const Jet b     = Eval(compare.m_e2());
        const T   value = numeric_interface<T>::compare(*a[0], *b[0], compare.Op());
        if (*a[0] == *b[0] && (Moves(a) || Moves(b))) {
            if (!guard_) throw std::runtime_error("a comparison jumps at " + Where());
            if (compare.Op() == Comparison::Equal || compare.Op() == Comparison::NotEqual)
                throw std::runtime_error(*guarded_ + " takes a clause at " + Where() +
                                         " that holds only there");
        }
        return Constant(value);
    }

    Jet Logic(LogicExpression<T>& logic) {
        const auto truth = [&](const Jet& jet) {
            return numeric_interface<T>::truth(*jet[0],
                                               std::string(logic.Word()) + " needs single values");
        };
        const bool left    = truth(Eval(logic.m_e1()));
        const bool decided = left != logic.Conjunction();
        const bool answer  = decided ? left : truth(Eval(logic.m_e2()));
        return Constant(T(typename T::value_type(answer ? 1 : 0)));
    }

    Jet Floor(FloorExpression<T>& floor) {
        const Jet u     = Eval(floor.m_e());
        const T   value = numeric_interface<T>::floor(*u[0]);
        for (std::size_t k = 0; k < value.Size().count(); ++k) {
            if (!(u[0]->data()[k] == value.data()[k])) continue;
            if (Moves(u)) throw std::runtime_error("floor jumps at " + Where());
        }
        return Constant(value);
    }

    // Each part a literal of its own, of the parts of the cells, or of a
    // tensor's slices: one with none there is a zero of its own size.
    template <typename Make>
    Jet Literal(Expression<T>& literal, Make make) {
        std::vector<Jet> cells;
        for (const PExpression<T>& cell : literal.Children()) cells.push_back(Eval(cell));
        Jet out(Size());
        for (std::size_t s = 0; s < out.size(); ++s) {
            bool                        any = s == 0;
            std::vector<PExpression<T>> parts;
            for (const Jet& cell : cells) {
                any = any || cell[s];
                parts.push_back(
                    std::make_shared<ValExpression<T>>(cell[s] ? *cell[s] : Zero(cell[0]->Size())));
            }
            if (!any) continue;
            out[s] = make(std::move(parts))->accept(ordinary_);
        }
        return out;
    }

    Jet Cell(CellExpression<T>& cell) {
        const T*   kept   = Lookup(cell.Matrix()->Name()) ? nullptr : stack_.Kept(*cell.Matrix());
        const Jet  matrix = kept ? Jet() : Eval(cell.Matrix());
        const auto read   = [&](auto f) { return kept ? Constant(f(*kept)) : Map(matrix, f); };
        if (cell.Slice()) {
            const int slice = AsIndex<T>(cell.Slice()->accept(ordinary_));
            const int row   = AsIndex<T>(cell.Row()->accept(ordinary_));
            const int col   = AsIndex<T>(cell.Col()->accept(ordinary_));
            return read([slice, row, col](const T& v) {
                return numeric_interface<T>::cell(v, slice, row, col);
            });
        }
        const int row    = AsIndex<T>(cell.Row()->accept(ordinary_));
        if (!cell.Col())
            return read([row](const T& v) { return numeric_interface<T>::row(v, row); });
        const int col = AsIndex<T>(cell.Col()->accept(ordinary_));
        return read([row, col](const T& v) { return numeric_interface<T>::cell(v, row, col); });
    }

    Jet Power(PowExpression<T>& power) {
        const Jet w = Eval(power.m_e2());
        if (Moves(w)) {
            if (!Euler(*power.m_e1()))
                throw std::runtime_error(
                    "grad cannot differentiate a power whose exponent changes with " +
                    grads_.back().first + ", unless its base is e");
            const T value = numeric_interface<T>::pow(power.m_e1()->accept(ordinary_), *w[0]);
            return Composed(w, [&value](int) { return std::optional<T>(value); }, value);
        }
        const Jet u        = Eval(power.m_e1());
        const T&  exponent = *w[0];
        const T   value    = numeric_interface<T>::pow(*u[0], exponent);
        if (!Moves(u)) return Constant(value);
        if (!u[0]->IsScalar()) {
            // A product of factors that do not commute, each one's part in
            // its place; of the inverse when the power is negative.
            const int whole = numeric_interface<T>::toInt(exponent);
            Jet       base  = whole < 0 ? Inverse(u) : u;
            unsigned  n =
                whole < 0 ? 0u - static_cast<unsigned>(whole) : static_cast<unsigned>(whole);
            Jet result = Constant(T::Identity(u[0]->Size()));
            for (; n != 0; n >>= 1) {
                if (n & 1) result = Product(result, base);
                if (n > 1) base = Product(base, base);
            }
            result[0] = value;
            return result;
        }
        // u^c, whose j-th derivative is c(c-1)...(c-j+1) u^(c-j): zero once
        // a whole c is passed, infinite where u is zero and c-j negative.
        const auto derivative = [&](int j) -> std::optional<T> {
            T coefficient(typename T::value_type(1));
            for (int k = 0; k < j; ++k) coefficient = coefficient * (exponent - T(k));
            if (IsZero(coefficient)) return std::nullopt;
            const T lower = exponent - T(j);
            if (IsZero(*u[0]) && numeric_interface<T>::truth(
                                     numeric_interface<T>::compare(lower, T(0), Comparison::Less)))
                throw std::runtime_error("a power's derivative is infinite at " + Where());
            return coefficient * numeric_interface<T>::pow(*u[0], lower);
        };
        return Composed(u, derivative, value);
    }

    // The built-in e, not one the session or a frame defines.
    bool Euler(const Expression<T>& base) const {
        if (!dynamic_cast<const RefExpression<T>*>(&base) || base.Name() != "e") return false;
        if (Lookup("e") || stack_.Binds("e")) return false;
        const auto builtin = stack_.Builtins().names.find("e");
        return builtin != stack_.Builtins().names.end() && stack_.Global("e") == builtin->second;
    }

    // Each part converging by the rule lim follows, and the limit when all
    // have at the same term.
    struct Walk {
        explicit Walk(const std::string& what, std::size_t size) {
            for (std::size_t s = 0; s < size; ++s)
                parts.emplace_back(s == 0 ? what : "the derivative of " + what);
            settled.assign(size, false);
        }
        bool Next(const Jet& jet) {
            bool all = true;
            for (std::size_t s = 0; s < jet.size(); ++s) {
                settled[s] = parts[s].Next(jet[s] ? *jet[s] : Zero(jet[0]->Size()));
                all        = all && settled[s];
            }
            return all;
        }
        static Jet Limit(Jet jet) {
            return Map(std::move(jet), [](const T& v) { return Convergence<T>::Limit(v); });
        }
        // Why it did not converge: the value, or the first derivative that did not.
        [[noreturn]] void Fail(const std::string& what, const std::string& last,
                               const Jet& jet) const {
            std::size_t s = 0;
            while (s < jet.size() && settled[s]) ++s;
            if (s == jet.size()) s = 0;
            const T value = jet[s] ? *jet[s] : Zero(jet[0]->Size());
            throw std::runtime_error((s == 0 ? what : "the derivative of " + what) +
                                     " did not converge within " +
                                     std::to_string(Convergence<T>::max_terms) + " terms (" + last +
                                     " " + numeric_interface<T>::toString(value) + ")");
        }
        std::vector<Convergence<T>> parts;
        std::vector<bool>           settled;
    };

    // As Reference::Converge walks a limit, and further while its derivatives
    // have not converged.
    Jet Limit(const Reference<T>& definition, const ParametersCall<T>& call,
              const Arguments& arguments) {
        const std::string&       name = definition.Name();
        std::optional<long long> highest;
        bool                     general = false;
        for (const Clause<T>& clause : definition.Clauses()) {
            const ParametersDefinition<T>& p = clause.parameters;
            general                          = general || p.general();
            if (p.indexed() && !p.general() && !p.guarded() && !p.cells())
                highest = std::max(highest.value_or(p.index()), static_cast<long long>(p.index()));
        }
        if (!general) throw std::runtime_error(name + " has no general clause, so it has no limit");
        Walk      walk(name, Size());
        long long index = highest.value_or(0);
        if (highest)
            (void)walk.Next(Term(definition, call, true, static_cast<int>(index), arguments));
        Jet term;
        for (std::size_t n = 0; n < Convergence<T>::max_terms; ++n) {
            term = Term(definition, call, true, static_cast<int>(++index), arguments);
            if (walk.Next(term)) return Walk::Limit(term);
        }
        walk.Fail(name, "last term", term);
    }

    Jet Series(SeriesExpression<T>& series) {
        const int  first    = AsIndex<T>(series.Lower()->accept(ordinary_));
        const bool infinite = !series.Upper();
        const int  last     = infinite ? first : AsIndex<T>(series.Upper()->accept(ordinary_));
        std::optional<typename ReferenceStack<T>::Frame> frame;
        if (!stack_.Framed()) frame.emplace(stack_);
        typename ReferenceStack<T>::Trial index(stack_, series.Index());
        const Shadow                      hidden(*this, series.Index(), std::nullopt);
        const auto                        term = [&](int k) {
            stack_.Step();
            stack_.BindValue(series.Index(), T(k));
            return Eval(series.Body());
        };
        const auto combine = [&](const Jet& total, const Jet& next) {
            return series.Product() ? Product(total, next) : Sum(total, next);
        };
        if (!infinite) {
            if (last < first) return Constant(T(series.Product() ? 1 : 0));
            Jet total = term(first);
            for (int k = first; k != last;) total = combine(total, term(++k));
            return total;
        }
        const std::string what = series.Product() ? "product" : "sum";
        Walk              walk("the " + what, Size());
        Jet               total;
        for (int n = 0; n < static_cast<int>(Convergence<T>::max_terms); ++n) {
            total = n == 0 ? term(first) : combine(total, term(first + n));
            if (walk.Next(total)) return Walk::Limit(total);
        }
        walk.Fail("the " + what, "last partial " + what, total);
    }

    Jet Grad(GradExpression<T>& grad) {
        const Jet          point = Eval(grad.Point());
        const T&           at    = *point[0];
        const std::string& name  = grad.Variable();
        Unreached(grad.Body(), name);
        if (!Mentions(*grad.Body(), name))
            throw std::runtime_error("grad's expression does not read " + name);
        if (order_ == max_order)
            throw std::runtime_error("grad nests more than " + std::to_string(max_order) + " deep");
        const std::size_t bit = Size();
        std::vector<Jet>  parts;
        {
            const Order order(*this, name, numeric_interface<T>::toString(at));
            std::optional<typename ReferenceStack<T>::Frame> frame;
            if (!stack_.Framed()) frame.emplace(stack_);
            typename ReferenceStack<T>::Trial bound(stack_, name);
            stack_.BindValue(name, at);
            // A single value's gradient with respect to a matrix, one cell at a time.
            const Extent      extent = at.Size();
            const std::size_t cells  = at.IsScalar() ? 1 : extent.count();
            for (std::size_t k = 0; k < cells; ++k) {
                Jet x = Padded(point);
                T   seed(typename T::value_type(1));
                if (!at.IsScalar()) {
                    seed           = Zero(extent);
                    seed.data()[k] = typename T::value_type(1);
                }
                x[bit] = seed;
                const Shadow variable(*this, name, x);
                Jet          body = Eval(grad.Body());
                if (!at.IsScalar() && !body[0]->IsScalar()) {
                    const auto kind = [](const T& v) { return v.IsTensor() ? "tensor" : "matrix"; };
                    throw std::runtime_error(std::string("grad of a ") + kind(*body[0]) +
                                             " with respect to a " + kind(at) +
                                             " is a Jacobian, which it does not give");
                }
                parts.push_back(std::move(body));
            }
        }
        Jet out(bit);
        for (std::size_t s = 0; s < bit; ++s) {
            if (at.IsScalar()) {
                out[s] = parts[0][s | bit];
                continue;
            }
            T    gradient = Zero(at.Size());
            bool any      = s == 0;
            for (std::size_t k = 0; k < parts.size(); ++k) {
                if (!parts[k][s | bit]) continue;
                gradient.data()[k] = parts[k][s | bit]->data()[0];
                any                = true;
            }
            if (any) out[s] = gradient;
        }
        if (!out[0]) out[0] = Zero(parts[0][0]->Size());
        return out;
    }

    // A definition reads the globals where it was written, which a grad's
    // name does not reach: a derivative through one that reads the global of
    // that name would be zero for no reason the reader can see.
    void Unreached(const PExpression<T>& body, const std::string& name) {
        std::set<const Reference<T>*> seen;
        Scan(body, name, {name}, nullptr, seen);
    }

    void Scan(const PExpression<T>& e, const std::string& name, std::set<std::string> bound,
              const Reference<T>* within, std::set<const Reference<T>*>& seen) {
        if (!e || dynamic_cast<const MemberExpression<T>*>(e.get())) return;
        if (const auto* series = dynamic_cast<const SeriesExpression<T>*>(e.get())) {
            Scan(series->Lower(), name, bound, within, seen);
            Scan(series->Upper(), name, bound, within, seen);
            bound.insert(series->Index());
            Scan(series->Body(), name, bound, within, seen);
            return;
        }
        if (const auto* grad = dynamic_cast<const GradExpression<T>*>(e.get())) {
            Scan(grad->Point(), name, bound, within, seen);
            bound.insert(grad->Variable());
            Scan(grad->Body(), name, bound, within, seen);
            return;
        }
        if ((dynamic_cast<const RefExpression<T>*>(e.get()) ||
             dynamic_cast<const FuncExpression<T>*>(e.get())) &&
            !bound.count(e->Name()))
            Follow(e->Name(), name, within, seen);
        for (const PExpression<T>& child : e->Children()) Scan(child, name, bound, within, seen);
    }

    void Follow(const std::string& read, const std::string& name, const Reference<T>* within,
                std::set<const Reference<T>*>& seen) {
        if (!within) {
            if (Lookup(read) || stack_.Binds(read)) return;
            if (const auto definition = stack_.Global(read)) Definition(*definition, name, seen);
            return;
        }
        if (read == name) {
            const std::string label =
                within->home ? within->home->Qualified(within->Name()) : within->Name();
            throw std::runtime_error(label + " reads the global " + name + ", which grad's " +
                                     name + " does not reach");
        }
        for (const ::Scope<T>* scope = within->home; scope; scope = scope->parent) {
            const auto found = scope->names.find(read);
            if (found == scope->names.end()) continue;
            Definition(*found->second, name, seen);
            return;
        }
    }

    void Definition(const Reference<T>& definition, const std::string& name,
                    std::set<const Reference<T>*>& seen) {
        if (!seen.insert(&definition).second || !definition.Value() || !definition.home) return;
        for (const Clause<T>& clause : definition.Clauses()) {
            const ParametersDefinition<T>& p = clause.parameters;
            std::set<std::string> bound(p.parameters_names().begin(), p.parameters_names().end());
            for (const std::string* other : {&p.index_name(), &p.row_name(), &p.col_name()})
                if (!other->empty()) bound.insert(*other);
            Scan(clause.expression, name, bound, &definition, seen);
            Scan(p.guard(), name, bound, &definition, seen);
        }
    }

    struct Local {
        explicit Local(Derivative& d) : d_(d) { d_.frames_.emplace_back(); }
        ~Local() { d_.frames_.pop_back(); }
        Local(const Local&)            = delete;
        Local& operator=(const Local&) = delete;

    private:
        Derivative& d_;
    };

    // A name bound in the innermost frame, or hidden there, while this
    // lives; what it shadowed is put back.
    struct Shadow {
        Shadow(Derivative& d, const std::string& name, std::optional<Jet> jet)
            : d_(d), frame_(d.frames_.size() - 1), name_(name) {
            auto& frame = d_.frames_[frame_];
            for (auto at = frame.begin(); at != frame.end(); ++at) {
                if (at->first != name_) continue;
                previous_ = std::move(at->second);
                frame.erase(at);
                break;
            }
            if (jet) frame.emplace_back(name_, std::move(*jet));
        }
        ~Shadow() {
            auto& frame = d_.frames_[frame_];
            std::erase_if(frame, [this](const auto& binding) { return binding.first == name_; });
            if (previous_) frame.emplace_back(name_, std::move(*previous_));
        }
        Shadow(const Shadow&)            = delete;
        Shadow& operator=(const Shadow&) = delete;

    private:
        Derivative&        d_;
        std::size_t        frame_;
        std::string        name_;
        std::optional<Jet> previous_;
    };

    struct Order {
        Order(Derivative& d, const std::string& name, const std::string& point) : d_(d) {
            ++d_.order_;
            d_.grads_.emplace_back(name, point);
        }
        ~Order() {
            --d_.order_;
            d_.grads_.pop_back();
        }
        Order(const Order&)            = delete;
        Order& operator=(const Order&) = delete;

    private:
        Derivative& d_;
    };

    struct Deeper {
        explicit Deeper(Derivative& d) : d_(d) {
            if (d_.depth_ >= ReferenceStack<T>::max_depth)
                throw std::runtime_error("evaluation nests more than " +
                                         std::to_string(ReferenceStack<T>::max_depth) +
                                         " references deep");
            ++d_.depth_;
        }
        ~Deeper() { --d_.depth_; }
        Deeper(const Deeper&)            = delete;
        Deeper& operator=(const Deeper&) = delete;

    private:
        Derivative& d_;
    };

    struct Flag {
        Flag(bool& flag, bool value) : flag_(flag), previous_(flag) { flag_ = value; }
        ~Flag() { flag_ = previous_; }
        Flag(const Flag&)            = delete;
        Flag& operator=(const Flag&) = delete;

    private:
        bool& flag_;
        bool  previous_;
    };

    ReferenceStack<T>&                                    stack_;
    EvaluationVisitor<T>                                  ordinary_;
    int                                                   order_ = 0;
    std::vector<std::pair<std::string, std::string>>      grads_;  // name and point, innermost last
    std::vector<std::vector<std::pair<std::string, Jet>>> frames_;
    std::map<std::string, Jet>                            memo_;
    std::size_t                                           depth_   = 0;
    bool                                                  guard_   = false;
    bool                                                  filling_ = false;
    const std::string*                                    guarded_ = nullptr;
};

#endif  // INKAMATH_DERIVATIVE_HPP
