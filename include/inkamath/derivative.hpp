#ifndef INKAMATH_DERIVATIVE_HPP
#define INKAMATH_DERIVATIVE_HPP

#include "inkamath/reference_stack.hpp"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
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

    // What grad refuses by the names its body reads, before any value: the
    // compiler asks it too (DESIGN.md, grad compiled).
    void Names(const GradExpression<T>& grad) {
        Unreached(grad.Body(), grad.Variable());
        if (!Mentions(*grad.Body(), grad.Variable()))
            throw std::runtime_error("grad's expression does not read " + grad.Variable());
    }

private:
    struct Tape;
    // What reverse mode cannot do as forward mode does: forward mode does it.
    struct Forward {};

    // A part: a value, or under reverse mode a node of the tape, whose value
    // is NaN so that one read as a value shows.
    struct Part : T {
        Part(T value) : T(std::move(value)) {}
        Part(Tape* on, std::size_t index, Extent size)
            : T(typename T::value_type(std::nan(""))), tape(on), node(index), extent(size) {}
        Tape*       tape = nullptr;
        std::size_t node = 0;
        Extent      extent;

        [[nodiscard]] Extent   Size() const { return node ? extent : T::Size(); }
        [[nodiscard]] const T& Value() const { return *this; }

        friend Part operator+(const Part& a, const Part& b) {
            if (!a.node && !b.node) return Part(a.Value() + b.Value());
            return (a.node ? a.tape : b.tape)
                ->Add(Tape::Kind::Sum, Tape::Wider(a.Size(), b.Size()), a.node, {}, b.node);
        }
        friend Part operator*(const Part& a, const Part& b) {
            if (!a.node && !b.node) return Part(a.Value() * b.Value());
            const Extent extent = Tape::Times(a.Size(), b.Size());
            if (a.node) return a.tape->Add(Tape::Kind::Right, extent, a.node, b.Value());
            return b.tape->Add(Tape::Kind::Left, extent, b.node, a.Value());
        }
        friend Part operator/(const Part& a, const Part& b) {
            if (!a.node && !b.node) return Part(a.Value() / b.Value());
            return a.tape->Add(Tape::Kind::Quotient, Tape::Wider(a.Size(), b.Size()), a.node,
                               b.Value());
        }
        Part operator-() const {
            return node ? tape->Add(Tape::Kind::Negative, extent, node) : Part(-Value());
        }
        friend Part operator+(const T& a, const Part& b) { return Part(a) + b; }
        friend Part operator+(const Part& a, const T& b) { return a + Part(b); }
        friend Part operator*(const T& a, const Part& b) { return Part(a) * b; }
        friend Part operator*(const Part& a, const T& b) { return a * Part(b); }
        friend Part operator/(const Part& a, const T& b) { return a / Part(b); }
    };

    // Reverse mode (DESIGN.md): one walk's parts as nodes, each linear in its
    // operands, and the gradient their adjoints swept back.
    struct Tape {
        enum class Kind { Seed, Sum, Negative, Left, Right, Quotient, Cells, Mark, Gather, Place };
        using Cell = std::pair<std::size_t, std::size_t>;  // a node's, from 0; node 0 for none
        struct Node {
            Kind              kind;
            Extent            extent;
            std::size_t       a;      // the operand's node, 0 for none
            std::optional<T>  value;  // what it is multiplied or divided by
            std::size_t       b;      // the other operand's node, or the cell placed's
            std::size_t       at;     // the cell marked or placed, from 1; 0 for all
            bool              approximated;
            std::vector<Cell> from;  // each cell's, gathered
        };
        std::vector<Node> nodes;
        // Each node by a hash of what it is, so that the same operation on the
        // same nodes is the same node, and a call made again on it is
        // remembered as forward mode remembers one by its parts' values (C323).
        std::unordered_multimap<std::size_t, std::size_t> made;

        static bool Same(const Node& x, const Node& y) {
            return x.kind == y.kind && x.extent == y.extent && x.a == y.a && x.b == y.b &&
                   x.at == y.at && x.approximated == y.approximated && x.from == y.from &&
                   x.value.has_value() == y.value.has_value() &&
                   (!x.value || numeric_interface<T>::same(*x.value, *y.value));
        }

        Part Add(Kind kind, Extent extent, std::size_t a, std::optional<T> value = std::nullopt,
                 std::size_t b = 0, std::size_t at = 0, bool approximated = false,
                 std::vector<Cell> from = {}) {
            // A complex value would need conjugates (DESIGN.md).
            using Scalar = numeric_interface<typename T::value_type>;
            for (std::size_t k = 0; value && k < value->Size().count(); ++k)
                if (!(Scalar::imaginary(value->data()[k]) == 0)) throw Forward();
            Node node{kind, extent, a, std::move(value), b, at, approximated, std::move(from)};
            std::size_t hash = node.value ? numeric_interface<T>::hash(*node.value) : 0;
            for (const std::size_t field : {static_cast<std::size_t>(kind), a, b, at,
                                            static_cast<std::size_t>(approximated), extent.count()})
                hash = hash * 31 + field;
            for (const auto& [operand, cell] : node.from) hash = (hash * 31 + operand) * 31 + cell;
            for (auto [same, end] = made.equal_range(hash); same != end; ++same)
                if (Same(nodes[same->second - 1], node)) return Part(this, same->second, extent);
            nodes.push_back(std::move(node));
            made.emplace(hash, nodes.size());
            return Part(this, nodes.size(), extent);
        }
        Part Mark(const Part& a, bool approximated, std::size_t at = 0) {
            return Add(Kind::Mark, a.Size(), a.node, std::nullopt, 0, at, approximated);
        }
        // Cells numbered from first + 1, so that where a map that only moves
        // cells puts each says which it was.
        static T Numbered(Extent extent, std::size_t first) {
            T t(extent);
            for (std::size_t k = 0; k < extent.count(); ++k)
                t.data()[k] = typename T::value_type(static_cast<long long>(first + k + 1));
            return t;
        }
        // What landed, each cell's number the cell it was.
        template <typename F>
        Part Gather(const T& landed, F was) {
            std::vector<Cell> from(landed.Size().count());
            for (std::size_t k = 0; k < from.size(); ++k)
                if (const auto number = numeric_interface<T>::toInt(T(landed.data()[k]));
                    number > 0)
                    from[k] = was(static_cast<std::size_t>(number - 1));
            return Add(Kind::Gather, landed.Size(), 0, std::nullopt, 0, 0, false, std::move(from));
        }
        template <typename F>
        Part Moved(const Part& a, F f) {
            return Gather(f(Numbered(a.Size(), 0)),
                          [&a](std::size_t k) { return Cell(a.node, k); });
        }

        static bool   One(Extent e) { return e.count() == 1 && !e.slices; }
        static Extent Wider(Extent a, Extent b) {
            if (a.slices != b.slices) return a.slices ? a : b;
            return One(a) ? b : a;
        }
        static Extent Times(Extent a, Extent b) {
            if (One(a)) return b;
            if (One(b)) return a;
            return {a.rows, b.cols, std::max(a.slices, b.slices)};
        }

        static T Total(const T& g) {
            T sum(g.data()[0]);
            for (std::size_t k = 1; k < g.Size().count(); ++k) sum = sum + T(g.data()[k]);
            return sum;
        }
        // An adjoint summed down to the shape of what was widened (C81).
        static T Reduce(T g, Extent to) {
            if (g.Size() == to) return g;
            if (One(to)) return Total(g);
            if (!g.IsTensor() || to.slices)
                throw std::logic_error("an adjoint of " + g.Size().toString() + " for " +
                                       to.toString());
            T sum = g.Slice(1);
            for (std::size_t k = 2; k <= g.Size().slices; ++k) sum = sum + g.Slice(k);
            return sum;
        }
        // An exact 0 of an adjoint is a cell no path reaches, which stays an
        // exact 0 whatever it meets.
        static bool Nothing(const typename T::value_type& c) {
            return numeric_interface<typename T::value_type>::exact(c) &&
                   c == typename T::value_type(0);
        }
        static T Zeroed(T product, const T& g) {
            if (product.Size() != g.Size()) return product;
            for (std::size_t k = 0; k < g.Size().count(); ++k)
                if (Nothing(g.data()[k])) product.data()[k] = g.data()[k];
            return product;
        }
        // a*b, the terms whose factor of the adjoint is an exact 0 left out.
        static T Sparse(const T& a, const T& b, bool adjoint_left) {
            const Extent ea = a.Size(), eb = b.Size();
            if (ea.cols != eb.rows) return a * b;
            T c(Extent{ea.rows, eb.cols});
            for (std::size_t i = 1; i <= ea.rows; ++i)
                for (std::size_t j = 1; j <= eb.cols; ++j) {
                    std::optional<typename T::value_type> sum;
                    for (std::size_t k = 1; k <= ea.cols; ++k) {
                        const auto &x = a(i, k), &y = b(k, j);
                        if (Nothing(adjoint_left ? x : y)) continue;
                        sum = sum ? *sum + x * y : x * y;
                    }
                    if (sum) c(i, j) = *sum;
                }
            return c;
        }
        // The adjoint of v*a, and of a*v, for a.
        static T Left(const T& v, const T& g, Extent a) {
            if (v.IsScalar()) return Zeroed(g * v, g);
            if (One(a)) return Total(Zeroed(Derivative::Cellwise(v, g), g));
            const auto f = [](const T& x, const T& y) {
                return Sparse(numeric_interface<T>::transpose(x), y, false);
            };
            return v.IsTensor() || g.IsTensor() ? T::Sliced(v, g, f) : f(v, g);
        }
        static T Right(const T& v, const T& g, Extent a) {
            if (v.IsScalar()) return Zeroed(g * v, g);
            if (One(a)) return Total(Zeroed(Derivative::Cellwise(g, v), g));
            const auto f = [](const T& y, const T& x) {
                return Sparse(y, numeric_interface<T>::transpose(x), true);
            };
            return v.IsTensor() || g.IsTensor() ? T::Sliced(g, v, f) : f(g, v);
        }

        // The seed's adjoint, the body's being 1.
        std::optional<T> Back(std::size_t body) {
            std::vector<std::optional<T>> adjoint(nodes.size() + 1);
            adjoint[body]   = T(typename T::value_type(1));
            const auto into = [&](std::size_t n, T g) {
                if (!n) return;
                g          = Reduce(std::move(g), nodes[n - 1].extent);
                adjoint[n] = adjoint[n] ? *adjoint[n] + g : std::move(g);
            };
            for (std::size_t n = nodes.size(); n > 1; --n) {
                if (!adjoint[n]) continue;
                T           g = std::move(*adjoint[n]);
                const Node& x = nodes[n - 1];
                const T*    v = x.value ? &*x.value : nullptr;
                switch (x.kind) {
                    case Kind::Seed:
                        break;
                    case Kind::Sum:
                        into(x.b, g);
                        into(x.a, std::move(g));
                        break;
                    case Kind::Negative:
                        into(x.a, -g);
                        break;
                    case Kind::Left:
                        into(x.a, Left(*v, g, nodes[x.a - 1].extent));
                        break;
                    case Kind::Right:
                        into(x.a, Right(*v, g, nodes[x.a - 1].extent));
                        break;
                    case Kind::Quotient:
                        into(x.a, Zeroed(g / *v, g));
                        break;
                    case Kind::Cells:
                        into(x.a, Zeroed(Derivative::Cellwise(*v, g), g));
                        break;
                    case Kind::Mark:
                        if (!x.at)
                            g = Zeroed(Derivative::Marked(x.approximated, g), g);
                        else if (auto& cell = g.data()[x.at - 1]; !Nothing(cell))
                            cell = Derivative::Marked(x.approximated, T(cell)).data()[0];
                        into(x.a, std::move(g));
                        break;
                    case Kind::Place:
                        into(x.b, T(g.data()[x.at - 1]));
                        g.data()[x.at - 1] = typename T::value_type(0);
                        into(x.a, std::move(g));
                        break;
                    case Kind::Gather:
                        for (std::size_t k = 0; k < x.from.size(); ++k) {
                            const auto [from, cell] = x.from[k];
                            if (!from) continue;
                            if (!adjoint[from]) adjoint[from] = T(nodes[from - 1].extent);
                            auto& sum = adjoint[from]->data()[cell];
                            sum       = sum + g.data()[k];
                        }
                        break;
                }
            }
            return adjoint[1];
        }
    };

    // Indexed by a set of grads as bits, [0] being the value; empty where the
    // derivative is zero, so that a zero needs no shape.
    using Jet = std::vector<std::optional<Part>>;

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

    // A map that only moves cells, as a transpose or a cell does, moves a
    // node's.
    template <typename F>
    static Jet Map(Jet jet, F f) {
        for (auto& part : jet)
            if (part) part = part->node ? part->tape->Moved(*part, f) : Part(f(part->Value()));
        return jet;
    }

    static Jet Mark(Jet jet, bool approximated) {
        for (auto& part : jet)
            if (part)
                part = part->node ? part->tape->Mark(*part, approximated)
                                  : Part(Marked(approximated, part->Value()));
        return jet;
    }

    // Approximated past the bound, or only inexact (C241).
    template <typename V>
    static V Marked(bool approximated, const V& v) {
        return approximated ? numeric_interface<V>::marked(v) : numeric_interface<V>::inexact(v);
    }

    static void Add(std::optional<Part>& total, const Part& term) {
        total = total ? *total + term : term;
    }

    static Jet Sum(const Jet& a, const Jet& b) {
        Jet c(a.size());
        for (std::size_t s = 0; s < a.size(); ++s) {
            if (a[s]) Add(c[s], *a[s]);
            if (b[s]) Add(c[s], *b[s]);
        }
        // A part keeps its value's shape, which adding a single value or a
        // matrix to more cells widens (DESIGN.md, C81).
        if (a[0]->Size() == b[0]->Size()) return c;
        for (std::size_t s = 1; s < c.size(); ++s)
            if (c[s] && c[s]->Size() != c[0]->Size()) c[s] = *c[s] + Zero(c[0]->Size());
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

    static Part Cellwise(const Part& a, const Part& b) {
        if (!a.node && !b.node) return Part(Cellwise(a.Value(), b.Value()));
        const Part& node = a.node ? a : b;
        return node.tape->Add(Tape::Kind::Cells, Tape::Wider(a.Size(), b.Size()), node.node,
                              (a.node ? b : a).Value());
    }

    // Cell by cell, as '/' divides.
    static T Cellwise(const T& a, const T& b) {
        if (a.IsTensor() || b.IsTensor())
            return T::Sliced(a, b, [](const T& x, const T& y) { return Cellwise(x, y); });
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
            std::optional<Part> top = a[s];
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
            std::optional<Part> sum;
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
            const auto each = [&](int blocks, const Part& product) {
                auto& slot = known[static_cast<std::size_t>(blocks)];
                if (!slot) slot = derivative(blocks);
                if (*slot) Add(out[s], **slot * product);
            };
            Blocks(u, s, 0, std::nullopt, each);
        }
        return out;
    }

    template <typename F>
    static void Blocks(const Jet& u, std::size_t set, int blocks,
                       const std::optional<Part>& product, F& each) {
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

    // The innermost grad u moves with whose point is complex, empty if none.
    [[nodiscard]] std::string Complex(const Jet& u) const {
        std::size_t with = 0;
        for (std::size_t s = 1; s < u.size(); ++s)
            if (u[s]) with |= s;
        if (!(with & complex_)) return "";
        const auto& [name, point] = grads_[std::bit_width(with & complex_) - 1];
        return name + " = " + point;
    }

    Jet Eval(const PExpression<T>& e) { return Eval(*e); }

    // A dynamic_cast without its search of the bases, which is most of what
    // the dispatch below cost: a node class is final, so a node has no type
    // but its own to match.
    template <typename Node>
    static Node* Exactly(Expression<T>& e) {
        static_assert(std::is_final_v<Node>);
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
        if (auto* x = Exactly<NegExpression<T>>(e)) {
            Jet u = Eval(x->m_e());
            for (auto& part : u)
                if (part) part = -*part;
            return u;
        }
        if (auto* x = Exactly<MultExpression<T>>(e)) {
            const Jet a = Eval(x->m_e1());
            return Product(a, Eval(x->m_e2()));
        }
        if (auto* x = Exactly<DivExpression<T>>(e)) {
            const Jet a = Eval(x->m_e1());
            return Quotient(a, Eval(x->m_e2()));
        }
        if (auto* x = Exactly<PowExpression<T>>(e)) return Power(*x);
        if (auto* x = Exactly<InexactExpression<T>>(e)) return Mark(Eval(x->m_e()), false);
        if (auto* x = Exactly<TransposeExpression<T>>(e))
            return Map(Eval(x->m_e()),
                       [](const T& v) { return numeric_interface<T>::transpose(v); });
        if (auto* x = Exactly<BuiltinExpression<T>>(e)) return Builtin(*x);
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
            if (!Reads(*x)) return Constant(x->accept(ordinary_));
            // A file holds definitions, as the session does; an instance runs a model.
            auto* const call   = dynamic_cast<FuncExpression<T>*>(x->Member().get());
            auto* const object = Exactly<RefExpression<T>>(*x->Object());
            const auto  file   = object ? stack_.Global(object->Name()) : nullptr;
            if (!call || !file || !file->file)
                throw std::runtime_error("grad cannot differentiate through an instance yet");
            const auto found = file->file->names.find(call->Name());
            if (found == file->file->names.end()) return Constant(x->accept(ordinary_));
            return Call(*call, found->second, *x, Index(call->Call()));
        }
        if (auto* x = Exactly<EqualExpression<T>>(e)) {
            EvaluationVisitor<T>::Unparametrized(*x);
            stack_.Definable(x->Name());
            if (x->m_e1()->Children().empty()) return Bind(*x);
        }
        throw std::runtime_error("grad cannot differentiate a local definition yet");
    }

    // A local: its value bound in the evaluator's frame as the evaluator
    // binds it, for what grad reads as a value, and with its parts in grad's.
    Jet Bind(EqualExpression<T>& local) {
        const Jet jet = Eval(local.m_e2());
        stack_.Set(local.Name(), {}, std::make_shared<ValExpression<T>>(*jet[0]));
        std::erase_if(frames_.back(), [&](const auto& b) { return b.first == local.Name(); });
        frames_.back().emplace_back(local.Name(), jet);
        return jet;
    }

    // The names a grad, a call or a local binds, which nothing else reads.
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

    Jet Name(RefExpression<T>& ref) {
        if (const Jet* bound = Lookup(ref.Name())) return Padded(*bound);
        return Constant(ref.accept(ordinary_));
    }

    typedef std::vector<std::pair<std::string, Jet>> Arguments;

    Jet Call(FuncExpression<T>& call) {
        if (Lookup(call.Name()) || !Reads(call)) return Constant(call.accept(ordinary_));
        const std::optional<int> index = Index(call.Call());
        if (stack_.Binds(call.Name())) return Constant(call.accept(ordinary_));
        return Call(call, stack_.Global(call.Name()), call, index);
    }

    // 'written' is the call as the expression has it, 'sq.f(t)' for f in a file.
    Jet Call(FuncExpression<T>& call, const typename ReferenceStack<T>::definition_type& definition,
             Expression<T>& written, std::optional<int> index) {
        if (!definition || !definition->Value() || definition->Clauses().empty())
            return Constant(written.accept(ordinary_));
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
        for (const auto& [given, jet] : arguments) definition->Divides(given, *jet[0], stack_);
        // In its own words, not its guard's im's. max and min are defined on
        // the real line alone, so not where a real argument's parts leave it,
        // as |z| is (C205).
        using Scalar       = numeric_interface<typename T::value_type>;
        const auto  cell   = [](auto f) { return [f](const T& v) { return T::Cells(v, f); }; };
        const auto& names  = stack_.Builtins().names;
        const bool  ranked = definition == names.at("max") || definition == names.at("min");
        if (complex_ && (definition == names.at("abs") || ranked))
            for (const auto& [given, jet] : arguments)
                if (const std::string at = Complex(jet); !at.empty())
                    throw std::runtime_error(definition->Name() + " has no complex derivative at " +
                                             at);
        for (const auto& [given, jet] : arguments) {
            // A node's parts are real, a complex value having given way, and
            // its value is NaN, which IsZero would take for an imaginary part.
            const Jet   im   = ranked && !tape_ ? Map(jet, cell(Scalar::imaginary)) : Jet();
            std::size_t with = 0;
            for (std::size_t s = 1; s < im.size(); ++s)
                if (im[s] && !IsZero(*im[s])) with |= s;
            if (!with || !IsZero(*im[0])) continue;
            const auto& [grad, point] = grads_[std::bit_width(with) - 1];
            throw std::runtime_error(
                definition->Name() +
                " has no derivative where its argument leaves the real line at " + grad + " = " +
                point);
        }
        for (const auto& [given, jet] : arguments)
            if (Moves(jet) && stack_.staircases.contains(definition.get()))
                throw std::runtime_error("grad cannot differentiate " + definition->Name() +
                                         " yet");
        int read = 0;
        if (stack_.checked && stack_.staircases.contains(definition.get()))
            for (auto& [given, jet] : arguments)
                read = std::max(read, stack_.checked(*definition, *jet[0]));
        if (p.limit()) return Limit(*definition, p, arguments);
        Jet out = Term(*definition, p, index.has_value(), index.value_or(0), arguments);
        if (read) out = Mark(std::move(out), read > 1);
        // |z| squared is re^2 + im^2, whose parts past the first the real
        // clauses lack where z is real and its parts are not (C203).
        if (out.size() > 2 && arguments.size() == 1 && !IsZero(*out[0]) &&
            definition == stack_.Builtins().names.at("abs")) {
            const Jet re     = Map(arguments[0].second, cell(Scalar::real));
            const Jet im     = Map(arguments[0].second, cell(Scalar::imaginary));
            const Jet square = Sum(Product(re, re), Product(im, im));
            bool      leaves = false;
            for (std::size_t s = 1; s < im.size(); ++s)
                leaves = leaves || (im[s] && !IsZero(*im[s]));
            for (std::size_t s = 1; leaves && IsZero(*im[0]) && s < out.size(); ++s) {
                if (std::popcount(s) < 2) continue;
                out[s] = square[s];
                for (std::size_t part = (s - 1) & s; part != 0; part = (part - 1) & s)
                    if (out[part] && out[s ^ part]) Add(out[s], -(*out[part] * *out[s ^ part]));
                if (out[s]) out[s] = *out[s] / (T(2) * *out[0]);
            }
        }
        return out;
    }

    std::string Key(const Reference<T>& definition, bool indexed, int index,
                    const Arguments& arguments) const {
        std::string key = std::to_string(reinterpret_cast<std::uintptr_t>(&definition)) + '#' +
                          std::to_string(order_) + (indexed ? "_" + std::to_string(index) : "");
        for (const auto& [name, jet] : arguments) {
            key += '\0' + name + '=';
            for (const auto& part : jet) {
                key += part ? '\2' : '\1';
                if (part && part->node)
                    key += '\3' + std::to_string(part->node);
                else if (part)
                    numeric_interface<T>::key(*part, key);
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
        if (const auto found = memo_.find(key); found != memo_.end()) {
            stepped_ |= found->second.second;
            return found->second.first;
        }
        if (indexed && !filling_ && depth_ >= ReferenceStack<T>::max_depth / 2)
            Fill(definition, call, index, arguments);
        stack_.Step();
        // Refused in the name of the call, as Reference::Eval refuses it.
        std::optional<NotSingle> refused;
        try {
            const Deeper                       deeper(*this);
            if (std::optional<Jet> fast = Compiled(definition, indexed, arguments)) {
                memo_.emplace(key, std::pair(*fast, std::size_t(0)));
                return *fast;
            }
            const std::size_t                  outer = std::exchange(stepped_, 0);
            typename ReferenceStack<T>::Within within(stack_, definition.home);
            typename ReferenceStack<T>::Frame  frame(stack_);
            ParametersDefinition<T>::Bind(definition.captured, stack_);
            for (const auto& [name, jet] : arguments) stack_.BindValue(name, *jet[0]);
            definition.BindDefaults(call, ordinary_);
            const Local local(*this);
            for (const auto& argument : arguments) frames_.back().push_back(argument);
            Hidden(definition, arguments);
            const Setting<bool> unguarded(guard_, false);
            Jet              result = Dispatch(definition, indexed, index);
            // abs, max and min are continuous where their guards step.
            const auto& names = stack_.Builtins().names;
            if (&definition == names.at("abs").get() || &definition == names.at("max").get() ||
                &definition == names.at("min").get())
                stepped_ = 0;
            stepped_ &= Size() - 1;
            memo_.emplace(key, std::pair(result, stepped_));
            stepped_ |= outer;
            return result;
        } catch (const NotSingle& error) {
            refused = error;
        }
        definition.Refuse(*refused, arguments, [](const Jet& jet) { return *jet[0]; }, stack_);
    }

    // The prelude's own function of a double, from its header: the value, and
    // the part where it is the walk's double (DESIGN.md).
    std::optional<Jet> Compiled(const Reference<T>& f, bool indexed,
                                const Arguments& arguments) {
        if (indexed || arguments.size() != 1 || !stack_.compiled) return {};
        const Jet&             x     = arguments[0].second;
        const std::optional<T> value = stack_.compiled(f, *x[0]);
        if (!value) return {};
        if (!Moves(x)) return Constant(*value);
        // The walk's guards read the double that moves (C271).
        rounded_ = true;
        if (&f == stack_.stepwise) return Constant(*value);
        if (x.size() == 2 && x[1]->node) {
            // Its slope, as the part a part of ~1 has.
            const std::optional<T> slope = stack_.differentiated(
                f, *x[0], numeric_interface<T>::inexact(T(typename T::value_type(1))));
            if (!slope) return {};
            return Jet{Part(*value), *slope * *x[1]};
        }
        const std::optional<T> part =
            x.size() == 2 ? stack_.differentiated(f, *x[0], *x[1]) : std::nullopt;
        if (!part) return {};
        return Jet{Part(*value), Part(*part)};
    }

    // A clause's own names hide a parameter evaluated, but the walk binds them
    // where Lookup does not see them, so the parameter would be read (C80).
    static void Hidden(const Reference<T>& definition, const Arguments& arguments) {
        for (const Clause<T>& clause : definition.Clauses()) {
            const ParametersDefinition<T>& p = clause.parameters;
            for (const auto& [kind, name] :
                 {std::pair<const char*, const std::string*>{"index", &p.index_name()},
                  {"slice", &p.slice_name()},
                  {"row", &p.row_name()},
                  {"column", &p.col_name()}})
                for (const auto& argument : arguments)
                    if (!name->empty() && argument.first == *name)
                        throw std::runtime_error("grad cannot differentiate " + definition.Name() +
                                                 " yet: its " + kind + " " + *name +
                                                 " hides its parameter " + *name);
        }
    }

    // A term far from its base, filled from the base up so that each finds
    // the one before it remembered, as Reference::Filled does. What the fill
    // cannot reach, the term says itself.
    void Fill(const Reference<T>& definition, const ParametersCall<T>& call, int index,
              const Arguments& arguments) {
        const Clause<T>* lowest = definition.EndBase(true);
        if (!lowest) return;
        const Setting<bool> filling(filling_, true);
        try {
            for (int k = lowest->parameters.index() + 1; k < index; ++k)
                (void)Term(definition, call, true, k, arguments);
        } catch (const std::runtime_error&) {
        }
    }

    // As Reference::EvalImp chooses: by cells as it walks them, else guarded
    // clauses and base clauses in the order written, then the general clause
    // or the plain one.
    Jet Dispatch(const Reference<T>& definition, bool indexed, int index) {
        Parts parts{*this, definition};
        if (std::optional<Jet> cells = definition.ByCells(indexed, index, parts))
            return std::move(*cells);
        const std::string& name    = definition.Name();
        const Clause<T>*   plain   = nullptr;
        const Clause<T>*   general = nullptr;
        bool               guarded = false;
        std::optional<int> lowest;
        for (const Clause<T>& clause : definition.Clauses()) {
            const ParametersDefinition<T>& p = clause.parameters;
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
            if (definition.Selects(clause, indexed, index, parts))
                return parts.Eval(clause.expression);
        }
        if (indexed) {
            if (plain) throw std::runtime_error(name + " is not a sequence");
            if (general && (!lowest || index >= *lowest)) {
                stack_.BindValue(general->parameters.index_name(), T(index));
                return parts.Eval(general->expression);
            }
            if (guarded) throw std::runtime_error(definition.Unapplied(parts.past));
            throw std::runtime_error(name + " has no clause for index " + std::to_string(index));
        }
        if (plain) return parts.Eval(plain->expression);
        if (guarded) throw std::runtime_error(definition.Unapplied(parts.past));
        throw std::runtime_error(name + " is a sequence; index it (" + name + "_" +
                                 std::to_string(lowest.value_or(0)) + ")");
    }

    // What a guard chooses a clause for, a definition or one of its cells,
    // named only when it is refused, as --check names it: g_2[1,1], T[1,2,1].
    struct Guarded {
        const std::string&             name;
        const ParametersDefinition<T>* cell  = nullptr;
        int                            index = 0, slice = 0, row = 0, col = 0;

        std::string Named() const {
            if (!cell) return name;
            return name + (cell->indexed() ? "_" + std::to_string(index) : "") + "[" +
                   (cell->tensor() ? std::to_string(slice) + "," : "") + std::to_string(row) + "," +
                   std::to_string(col) + "]";
        }
    };

    // A guard is asked for its value; a comparison in it that holds at the
    // point only, an equality where its sides move apart, takes a clause
    // whose slope is not the function's.
    T Holds(const Guarded& what, const PExpression<T>& guard) {
        const Setting<bool>           guarding(guard_, true);
        const Setting<const Guarded*> naming(guarded_, &what);
        return *Eval(guard)[0];
    }

    Jet Compare(CompareExpression<T>& compare) {
        const Jet a     = Eval(compare.m_e1());
        const Jet b     = Eval(compare.m_e2());
        const T   value = numeric_interface<T>::compare(*a[0], *b[0], compare.Op());
        Stepped(a);
        Stepped(b);
        // A jump an approximated number rules out marks the answer (C167), and
        // an inexact one makes it inexact.
        chosen_ = chosen_ || ((Moves(a) || Moves(b)) && numeric_interface<T>::approximated(value));
        rounded_ = rounded_ || ((Moves(a) || Moves(b)) && !numeric_interface<T>::exact(value));
        if (*a[0] == *b[0] && (Moves(a) || Moves(b))) {
            if (!guard_) throw std::runtime_error("a comparison jumps at " + Where());
            if (compare.Op() == Comparison::Equal || compare.Op() == Comparison::NotEqual)
                throw std::runtime_error(guarded_->Named() + " takes a clause at " + Where() +
                                         " that holds only there");
        }
        return Constant(value);
    }

    Jet Logic(LogicExpression<T>& logic) {
        const auto truth = [&](const Jet& jet) {
            return numeric_interface<T>::truth(*jet[0], logic.Word());
        };
        const Jet  first   = Eval(logic.m_e1());
        const bool left    = truth(first);
        const bool decided = left != logic.Conjunction();
        const Jet  second  = decided ? first : Eval(logic.m_e2());
        return Constant(T::Held(decided ? left : truth(second), *first[0], *second[0]));
    }

    // A part of a value moving with a real variable moves as the value's
    // part does; with a complex one it has no derivative.
    Jet Builtin(BuiltinExpression<T>& node) {
        const Jet  u    = Eval(node.m_e());
        const auto cell = [&node](const T& v) { return T::Cells(v, node.function); };
        if (const std::string at = Complex(u); node.name != "floor" && !at.empty())
            throw std::runtime_error(node.name + " has no complex derivative at " + at);
        if (node.name != "floor") return Map(u, cell);
        const T value = cell(*u[0]);
        Stepped(u);
        chosen_       = chosen_ || (Moves(u) && numeric_interface<T>::approximated(*u[0]));
        rounded_      = rounded_ || (Moves(u) && !numeric_interface<T>::exact(*u[0]));
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
            std::vector<typename Tape::Cell> numbered;  // a node's cells, to gather
            for (const Jet& cell : cells) {
                any    = any || cell[s];
                T part = cell[s] ? cell[s]->Value() : Zero(cell[0]->Size());
                if (cell[s] && cell[s]->node) {
                    part = Tape::Numbered(cell[s]->Size(), numbered.size());
                    for (std::size_t k = 0; k < part.Size().count(); ++k)
                        numbered.emplace_back(cell[s]->node, k);
                }
                parts.push_back(std::make_shared<ValExpression<T>>(part));
            }
            if (!any) continue;
            const T built = make(std::move(parts))->accept(ordinary_);
            out[s]        = numbered.empty()
                                ? Part(built)
                                : tape_->Gather(built, [&](std::size_t k) { return numbered[k]; });
        }
        return out;
    }

    // An index is a whole number, so one that moves is at a jump, as floor
    // is at a whole number (C78).
    // Once, as the evaluator does, since it may bind a local (C304).
    int Index(const PExpression<T>& e, const std::string& what = "an index") {
        if (!Reads(*e)) return AsIndex<T>(e->accept(ordinary_));
        const Jet index = Eval(e);
        if (Moves(index)) throw std::runtime_error(what + " jumps at " + Where());
        return AsIndex<T>(*index[0]);
    }

    std::optional<int> Index(const ParametersCall<T>& call) {
        if (!call.subexpr()) return std::nullopt;
        return Index(call.subexpr());
    }

    Jet Cell(CellExpression<T>& cell) {
        const T*   kept   = Lookup(cell.Matrix()->Name()) ? nullptr : stack_.Kept(*cell.Matrix());
        const Jet  matrix = kept ? Jet() : Eval(cell.Matrix());
        const auto read   = [&](auto f) { return kept ? Constant(f(*kept)) : Map(matrix, f); };
        if (cell.Slice()) {
            const int slice = Index(cell.Slice());
            const int row   = Index(cell.Row());
            const int col   = Index(cell.Col());
            return read([slice, row, col](const T& v) {
                return numeric_interface<T>::cell(v, slice, row, col);
            });
        }
        const int row = Index(cell.Row());
        if (!cell.Col())
            return read([row](const T& v) { return numeric_interface<T>::row(v, row); });
        const int col = Index(cell.Col());
        return read([row, col](const T& v) { return numeric_interface<T>::cell(v, row, col); });
    }

    Jet Power(PowExpression<T>& power) {
        const Jet u = Eval(power.m_e1());
        // Asked as the base is read, before the exponent may bind a local e.
        const bool euler = Euler(*power.m_e1());
        const Jet  w     = Eval(power.m_e2());
        if (Moves(w)) {
            if (!euler)
                throw std::runtime_error(
                    "grad cannot differentiate a power whose exponent changes with " +
                    grads_.back().first + ", unless its base is e");
            const T value = numeric_interface<T>::pow(*u[0], *w[0]);
            return Composed(w, [&value](int) { return std::optional<T>(value); }, value);
        }
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

    // A comparison or floor that moves is flat but at its jumps, so a limit
    // of terms that read one, as a bisection's, is a staircase whose terms'
    // derivatives say nothing of its own (C215): the grads one moved with.
    void Stepped(const Jet& u) {
        for (std::size_t s = 1; s < u.size(); ++s)
            if (u[s]) stepped_ |= s;
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
                if (jet[s] && jet[s]->node) throw Forward();
                settled[s] = parts[s].Next(jet[s] ? jet[s]->Value() : Zero(jet[0]->Size()));
                all        = all && settled[s];
            }
            return all;
        }
        // Each part moved, in a run of --check, by what its own walk left.
        Jet Limit(Jet jet) const {
            for (std::size_t s = 0; s < jet.size(); ++s)
                if (jet[s]) jet[s] = parts[s].Reached(*jet[s]);
            return jet;
        }
        // Why it did not converge: the value, or the first derivative that did not.
        [[noreturn]] void Fail(const std::string& what, const std::string& last,
                               const Jet& jet) const {
            std::size_t s = 0;
            while (s < jet.size() && settled[s]) ++s;
            if (s == jet.size()) s = 0;
            const T value = jet[s] ? jet[s]->Value() : Zero(jet[0]->Size());
            throw std::runtime_error((s == 0 ? what : "the derivative of " + what) +
                                     " did not converge within " +
                                     std::to_string(Convergence<T>::max_terms) + " terms (" + last +
                                     " " + Convergence<T>::Last(value) + ")");
        }
        std::vector<Convergence<T>> parts;
        std::vector<bool>           settled;
    };

    // As Reference::Converge walks a limit, and further while its derivatives
    // have not converged.
    Jet Limit(const Reference<T>& definition, const ParametersCall<T>& call,
              const Arguments& arguments) {
        const std::string& name    = definition.Name();
        bool               general = false;
        for (const Clause<T>& clause : definition.Clauses())
            general = general || clause.parameters.general();
        if (!general) throw std::runtime_error(name + " has no general clause, so it has no limit");
        Walk                       walk(name, Size());
        const Setting<std::size_t> stepped(stepped_, 0);
        const auto                 at = [&](long long k) {
            Jet term = Term(definition, call, true, static_cast<int>(k), arguments);
            if (const std::size_t with = stepped_ & (Size() - 1))
                throw std::runtime_error(
                    "grad cannot differentiate lim " + name + ", whose terms step with " +
                    grads_[std::bit_width(with) - 1].first + " at a comparison or a floor");
            return term;
        };
        const Clause<T>* const highest = definition.EndBase(false);
        long long              index   = highest ? highest->parameters.index() : 0;
        if (highest) (void)walk.Next(at(index));
        Jet term;
        for (std::size_t n = 0; n < Convergence<T>::max_terms; ++n) {
            term = at(++index);
            if (walk.Next(term)) return walk.Limit(term);
        }
        walk.Fail(name, "last term", term);
    }

    Jet Series(SeriesExpression<T>& series) {
        const std::string bound    = series.Product() ? "the product's bound" : "the sum's bound";
        const int         first    = Index(series.Lower(), bound);
        const bool        infinite = !series.Upper();
        const int         last     = infinite ? first : Index(series.Upper(), bound);
        std::optional<typename ReferenceStack<T>::Frame> frame;
        if (!stack_.Framed()) frame.emplace(stack_);
        typename ReferenceStack<T>::Trial index(stack_, series.Index());
        const Shadow                      hidden(*this, series.Index(), std::nullopt);
        const auto                        term = [&](int k) {
            stack_.Step();
            stack_.BindValue(series.Index(), T(k), series.Role());
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
            if (walk.Next(total)) return walk.Limit(total);
        }
        walk.Fail("the " + what, "last partial " + what, total);
    }

    Jet Grad(GradExpression<T>& grad) {
        if (tape_) throw Forward();
        const Jet          point = Eval(grad.Point());
        const T&           at    = *point[0];
        const std::string& name  = grad.Variable();
        Names(grad);
        if (order_ == max_order)
            throw std::runtime_error("grad nests more than " + std::to_string(max_order) + " deep");
        const std::size_t bit = Size();
        std::vector<Jet>  parts;
        bool              reversed = false;
        {
            const Order order(*this, name, numeric_interface<T>::toString(at));
            const bool  real =
                IsZero(T::Cells(at, numeric_interface<typename T::value_type>::imaginary));
            const Setting<std::size_t> complex(complex_, complex_ | (real ? 0 : bit));
            std::optional<typename ReferenceStack<T>::Frame> frame;
            if (!stack_.Framed()) frame.emplace(stack_);
            typename ReferenceStack<T>::Trial bound(stack_, name);
            stack_.BindValue(name, at, "grad's variable");
            // A single value's gradient with respect to a matrix, one cell at a time.
            const Extent      extent = at.Size();
            const std::size_t cells  = at.IsScalar() ? 1 : extent.count();
            // One pass's locals are not the next's, in either frame.
            const bool binds = ParametersDefinition<T>::Binds(*grad.Body());
            // A single value's gradient with respect to a real matrix, in one walk.
            reversed = bit == 1 && real && cells > 1 && Reverse(grad, point, binds, parts);
            for (std::size_t k = reversed ? cells : 0; k < cells; ++k) {
                // Each pass has steps of its own, as a term has (C318).
                const typename ReferenceStack<T>::OwnSteps own_steps(stack_);
                const bool                            undo = binds && k + 1 < cells;
                typename ReferenceStack<T>::Tentative locals(stack_, undo);
                const Arguments                       own = undo ? frames_.back() : Arguments();
                Jet                                   x   = Padded(point);
                T   seed(typename T::value_type(1));
                if (!at.IsScalar()) {
                    seed           = Zero(extent);
                    seed.data()[k] = typename T::value_type(1);
                }
                x[bit] = seed;
                const Shadow variable(*this, name, x);
                Jet          body = Eval(grad.Body());
                Single(at, *body[0]);
                parts.push_back(std::move(body));
                if (undo) frames_.back() = own;
            }
        }
        // A local the body bound outlives the grad, its parts by the grad's
        // name do not: the next grad at this depth takes the same bit (C322).
        for (auto& binding : frames_.back()) binding.second.resize(bit);
        Jet out(bit);
        for (std::size_t s = 0; s < bit; ++s) {
            if (at.IsScalar() || reversed) {
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
        // A derivative none of the body has is 0, approximated with its value;
        // and a part a clause lacked is 0 approximated if a guard that read an
        // approximated number chose it, which marks whatever it might reach.
        const bool none = !out[0];
        if (none) out[0] = Zero(parts[0][0]->Size());
        if (chosen_ || (none && numeric_interface<T>::approximated(*parts[0][0])))
            out = Mark(std::move(out), true);
        else if (rounded_)
            out = Mark(std::move(out), false);
        return out;
    }

    // A gradient is of a single value, not a Jacobian.
    static void Single(const T& at, const T& body) {
        if (at.IsScalar() || body.IsScalar()) return;
        const auto kind = [](const T& v) { return v.IsTensor() ? "tensor" : "matrix"; };
        throw std::runtime_error(std::string("grad of a ") + kind(body) + " with respect to a " +
                                 kind(at) + " is a Jacobian, which it does not give");
    }

    // One walk recording its parts on a tape, and one sweep back (DESIGN.md);
    // false where it gives way, which keeps nothing of it.
    bool Reverse(GradExpression<T>& grad, const Jet& point, bool binds, std::vector<Jet>& parts) {
        const Setting<decltype(memo_)>        remembered(memo_, {});
        typename ReferenceStack<T>::Tentative locals(stack_, binds);
        const Arguments                       own    = binds ? frames_.back() : Arguments();
        const bool                            chosen = chosen_, rounded = rounded_;
        const std::size_t                     stepped = stepped_;
        try {
            const typename ReferenceStack<T>::OwnSteps own_steps(stack_);
            const Extent                               extent = point[0]->Size();
            Tape                                       tape;
            const Setting<Tape*>                       taping(tape_, &tape);
            Jet                                        x = Padded(point);
            x[1]                                         = tape.Add(Tape::Kind::Seed, extent, 0);
            const Shadow variable(*this, grad.Variable(), x);
            Jet          body = Eval(grad.Body());
            Single(*point[0], *body[0]);
            body[1] = body[1] ? tape.Back(body[1]->node).value_or(Zero(extent)) : Zero(extent);
            parts.push_back(std::move(body));
            locals.keep();
            return true;
        } catch (const Forward&) {
            if (binds) frames_.back() = own;
            chosen_  = chosen;
            rounded_ = rounded;
            stepped_ = stepped;
            return false;
        }
    }

    // A definition reads the globals where it was written, which a grad's
    // name does not reach: a derivative through one that reads the global of
    // that name would be zero for no reason the reader can see.
    void Unreached(const PExpression<T>& body, const std::string& name) {
        std::set<const Reference<T>*> seen;
        std::set<std::string>         bound{name};
        Scan(body, name, bound, nullptr, seen);
    }

    // In the order of evaluation, a local's name being bound from its
    // binding on, and an index or grad's name in its body alone.
    void Scan(const PExpression<T>& e, const std::string& name, std::set<std::string>& bound,
              const Reference<T>* within, std::set<const Reference<T>*>& seen) {
        if (!e || dynamic_cast<const MemberExpression<T>*>(e.get())) return;
        if (const auto* series = dynamic_cast<const SeriesExpression<T>*>(e.get())) {
            Scan(series->Lower(), name, bound, within, seen);
            Scan(series->Upper(), name, bound, within, seen);
            const bool had = !bound.insert(series->Index()).second;
            Scan(series->Body(), name, bound, within, seen);
            if (!had) bound.erase(series->Index());
            return;
        }
        if (const auto* grad = dynamic_cast<const GradExpression<T>*>(e.get())) {
            Scan(grad->Point(), name, bound, within, seen);
            const bool had = !bound.insert(grad->Variable()).second;
            Scan(grad->Body(), name, bound, within, seen);
            if (!had) bound.erase(grad->Variable());
            return;
        }
        if (const auto* local = dynamic_cast<const EqualExpression<T>*>(e.get())) {
            Scan(local->m_e2(), name, bound, within, seen);
            bound.insert(local->Name());
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
            for (const std::string* other :
                 {&p.index_name(), &p.slice_name(), &p.row_name(), &p.col_name()})
                if (!other->empty()) bound.insert(*other);
            // A signature's sizes are its own, as its names are (C212).
            for (const auto& size : definition.Sizes())
                for (const auto& [given, digits] : size.bounds) bound.insert(given);
            for (const PExpression<T>& read :
                 {p.slices(), p.rows(), p.cols(), p.guard(), clause.expression})
                Scan(read, name, bound, &definition, seen);
        }
    }

    // The walk over a definition's cells for the parts: each cell's clause
    // differentiated where Reference chooses it, its parts stored where its
    // value is, and 0 where a cell has none.
    struct Parts {
        using Result = Jet;
        Derivative&         d;
        const Reference<T>& definition;

        // As Reference's Values has it; a part the clause lacks stays so, as
        // nothing moves it, and grad's answer is marked for it, or inexact.
        bool past = false, rounded = false;

        // Whether the clause just chosen is approximated, if it is marked.
        std::optional<bool> Chosen() {
            const bool marked = std::exchange(past, false), inexact = std::exchange(rounded, false);
            if (!marked && !inexact) return std::nullopt;
            (marked ? d.chosen_ : d.rounded_) = true;
            return marked;
        }
        Jet Eval(const PExpression<T>& e) {
            Jet        jet    = d.Eval(e);
            const auto marked = Chosen();
            return marked ? Mark(std::move(jet), *marked) : jet;
        }
        // A size is a whole number, so one that moves is at a jump.
        T Bound(const PExpression<T>& e) {
            const Jet size = d.Eval(e);
            if (Moves(size))
                throw std::runtime_error("the size of " + definition.Name() + " jumps at " +
                                         d.Where());
            return *size[0];
        }
        bool Holds(const Reference<T>&, const Clause<T>& clause, int index, int slice, int row,
                   int col) {
            typename ReferenceStack<T>::Tentative locals(d.stack_, clause.parameters.binds());
            const Arguments own = clause.parameters.binds() ? d.frames_.back() : Arguments();

            const ParametersDefinition<T>& p = clause.parameters;
            const T                        guard = d.Holds(
                {definition.Name(), row != 0 ? &p : nullptr, index, slice, row, col}, p.guard());
            past    = past || numeric_interface<T>::approximated(guard);
            rounded = rounded || !numeric_interface<T>::exact(guard);
            const bool held = numeric_interface<T>::truth(guard);
            if (held) locals.keep();
            if (!held && clause.parameters.binds()) d.frames_.back() = own;
            return held;
        }
        // A cell's place, from 1, for the tape.
        static std::size_t At(Jet& into, int slice, int row, int col) {
            return static_cast<std::size_t>(&Value(into)(slice, row, col) - Value(into).data()) + 1;
        }
        void Settle(Jet& into, int slice, int row, int col) {
            const auto marked = Chosen();
            for (auto& part : into) {
                if (!marked || !part) continue;
                if (part->node)
                    part = part->tape->Mark(*part, *marked, At(into, slice, row, col));
                else
                    (*part)(slice, row, col) = Marked(*marked, (*part)(slice, row, col));
            }
        }
        static T&   Value(Jet& jet) { return *jet[0]; }
        Jet         Blank(Extent extent) const { return d.Constant(T(extent)); }
        static void Store(Jet& into, int slice, int row, int col, typename T::value_type single,
                          const Jet& cell) {
            (*into[0])(slice, row, col) = single;
            for (std::size_t s = 1; s < into.size(); ++s) {
                if (!cell[s] && !into[s]) continue;
                const std::optional<Part>& node = cell[s] && cell[s]->node ? cell[s] : into[s];
                if (node && node->node) {
                    into[s] = node->tape->Add(
                        Tape::Kind::Place, Value(into).Size(), into[s] ? into[s]->node : 0,
                        std::nullopt, cell[s] ? cell[s]->node : 0, At(into, slice, row, col));
                    continue;
                }
                if (!into[s]) into[s] = Zero(into[0]->Size());
                (*into[s])(slice, row, col) =
                    cell[s] ? (*cell[s])(1, 1) : typename T::value_type(0);
            }
        }
        ReferenceStack<T>& stack() { return d.stack_; }
    };

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

    ReferenceStack<T>&                                    stack_;
    EvaluationVisitor<T>                                  ordinary_;
    int                                                   order_ = 0;
    std::vector<std::pair<std::string, std::string>>      grads_;  // name and point, innermost last
    std::vector<std::vector<std::pair<std::string, Jet>>> frames_;
    std::map<std::string, std::pair<Jet, std::size_t>>    memo_;  // and the grads it stepped with
    std::size_t                                           depth_   = 0;
    bool                                                  guard_   = false;
    bool                                                  filling_ = false;
    bool                                                  chosen_  = false;
    bool                                                  rounded_ = false;
    std::size_t                                           stepped_ = 0;
    std::size_t                                           complex_ = 0;  // grads at a complex point
    const Guarded*                                        guarded_ = nullptr;
    Tape*                                                 tape_    = nullptr;
};

#endif  // INKAMATH_DERIVATIVE_HPP
