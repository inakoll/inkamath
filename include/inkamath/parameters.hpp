#ifndef PARAMETERS_HPP
#define PARAMETERS_HPP


#include "inkamath/expression_visitor.hpp"
#include "inkamath/expression.hpp"

#include <algorithm>

template <typename T>
class ReferenceStack;

template <typename T>
class ParametersCall;

// README.md section 4: an index is a whole number. Truncating would make
// 'f_(0.5)' quietly mean 'f_0', and 'f_(2+i)' mean 'f_2'.
template <typename T>
int AsIndex(const T& value) {
    const int index = numeric_interface<T>::toInt(value);
    // Compared exactly: a difference below a double's smallest is not zero.
    if (!(value == T(index))) {
        // Two different complaints: a value an index cannot hold, and a value
        // that is not whole. Saying the second about the first sent the user
        // looking for a fraction that was not there (C56).
        if(numeric_interface<T>::abs(value) > 2147483647.0) {
            throw std::runtime_error("an index must be between -2147483648 and 2147483647, not "
                                     + numeric_interface<T>::toString(value));
        }
        throw std::runtime_error("an index must be a whole number, not "
                                 + numeric_interface<T>::toString(value));
    }
    // Taken as a whole number, an approximated index would lose its '~'.
    if (!numeric_interface<T>::exact(value)) {
        throw std::runtime_error("an index must be exact, and " +
                                 numeric_interface<T>::toString(value) + " was approximated");
    }
    return index;
}

template <typename T>
size_t AsSize(const T& value) {
    if (numeric_interface<T>::abs(value) > 2147483647.0)
        throw std::runtime_error("a size must be between 1 and 2147483647, not " +
                                 numeric_interface<T>::toString(value));
    const int size = AsIndex<T>(value);
    if (size < 1)
        throw std::runtime_error("a size must be at least 1, not " + std::to_string(size));
    return static_cast<size_t>(size);
}

// A parameter as a function's or a model's signature writes it, its places a
// cell's, 'v[j<=n]', or with a default the left side's, 'v[j<=n] = [1; 2]'.
template <typename T>
struct WrittenParameter {
    explicit WrittenParameter(const PExpression<T>& written) : left(written) {
        if (const auto* equal = dynamic_cast<const EqualExpression<T>*>(written.get()))
            left = equal->m_e1(), fallback = equal->m_e2();
        std::vector<PExpression<T>> places;
        if (const auto* cell = dynamic_cast<const CellExpression<T>*>(left.get()))
            places = {cell->Slice(), cell->Row(), cell->Col()}, left = cell->Matrix();
        else if (const auto* sized = dynamic_cast<const FuncExpression<T>*>(left.get()))
            places = {sized->Children()[5], sized->Children()[3], sized->Children()[4]};
        term = dynamic_cast<const FuncExpression<T>*>(left.get());
        for (const PExpression<T>& place : places) {
            const auto* compare = dynamic_cast<const CompareExpression<T>*>(place.get());
            if (compare && compare->Op() == Comparison::LessEqual &&
                dynamic_cast<const RefExpression<T>*>(compare->m_e1().get()))
                bounds.push_back(compare);
            else
                bounded = bounded && !place;
        }
    }

    PExpression<T>                           left, fallback;
    const FuncExpression<T>*                 term = nullptr;
    std::vector<const CompareExpression<T>*> bounds;          // 'j<=2', slices first
    bool                                     bounded = true;  // each place one
};

// The left-hand side of a definition: 'f(x, y)_n' or 'f_0'.
//
// README.md section 4: an index written as an identifier names the
// variable of the general clause; anything else must be a constant integer
// expression and names one base clause. Nothing in between is a definition.
template <typename T>
class ParametersDefinition
{
public:
    // A parameter's size, as 'M[j<=n, k<=n]' states it: its bounds, slices
    // first, each a name and 0, or a whole number and its digits.
    struct Size {
        std::vector<std::pair<std::string, size_t>> bounds;
        std::string                                 written;
    };

    ParametersDefinition() = default;

    ParametersDefinition(PExpression<T> params, PExpression<T> subexpr,
                         EvaluationVisitor<T>& evaluator, PExpression<T> guard = PExpression<T>(),
                         std::string    signature = std::string(),
                         PExpression<T> row       = PExpression<T>(),
                         PExpression<T> col       = PExpression<T>(),
                         PExpression<T> slice     = PExpression<T>())
        : guard_(guard), signature_(std::move(signature)) {
        if (params)
            for (const PExpression<T>& written : params->Children()) Parameter(written, evaluator);
        if(subexpr) {
            indexed_ = true;
            if(RefExpression<T>* variable = dynamic_cast<RefExpression<T>*>(subexpr.get())) {
                index_name_ = variable->Name();
            }
            else {
                index_ = AsIndex<T>(subexpr->accept(evaluator));
            }
        }
        if (row) {
            cells_ = true;
            if (slice) {
                rank_ = 3;
                Place(slice, slice_name_, slices_, slice_, evaluator);
            }
            Place(row, row_name_, rows_, row_, evaluator);
            if (col) {
                Place(col, col_name_, cols_, col_, evaluator);
            } else {
                // One index is a row, and a vector a column: its one column
                // under a name nothing can be written to read.
                rank_ = 1;
                col_  = 1;
                if (!row_name_.empty()) {
                    col_name_ = " column";
                    cols_     = std::make_shared<ValExpression<T>>(T(1));
                }
            }
            if (row_name_.empty() != col_name_.empty() ||
                (tensor() && slice_name_.empty() != row_name_.empty())) {
                throw std::runtime_error(
                    tensor() ? "a clause for cells names its slice, row and column, as"
                               " 'T[b<=2, j<=2, k<=2]', or none, as 'T[1,1,2]'"
                             : "a clause for cells names both its row and its column, as"
                               " 'M[j<=2, k<=2]', or neither, as 'M[1,2]'");
            }
            if (!row_name_.empty() &&
                (row_name_ == col_name_ || slice_name_ == row_name_ || slice_name_ == col_name_)) {
                throw std::runtime_error(tensor()
                                             ? "a cell's slice, row and column need three names"
                                             : "a cell's row and column need two names");
            }
        }
    }

    // A parameter is a name, 'x' or 'x = 1', or a name and its size,
    // 'v[j<=n]': anything else was dropped in silence (C150).
    void Parameter(const PExpression<T>& written, EvaluationVisitor<T>& evaluator) {
        const WrittenParameter<T> w(written);
        if (!w.fallback && !parameters_dict_.empty())
            throw std::runtime_error("a positional argument cannot follow a keyword argument");
        if (!w.bounded || (w.term ? w.term->m_e1() || w.term->m_e2() || w.term->Children()[2]
                                  : !dynamic_cast<const RefExpression<T>*>(w.left.get())))
            throw std::runtime_error(
                "a parameter is a name, as 'x' or 'x = 1', or a name and its "
                "size, as 'v[j<=n]'");
        Size size;
        for (const CompareExpression<T>* compare : w.bounds) {
            const PExpression<T>& bound = compare->m_e2();
            const bool            named = dynamic_cast<const RefExpression<T>*>(bound.get());
            if (!named && !dynamic_cast<const ValExpression<T>*>(bound.get()))
                throw std::runtime_error(
                    "a size is a whole number or a name, as 'v[j<=3]' or 'v[j<=n]'");
            const size_t number = named ? 0 : AsSize<T>(bound->accept(evaluator));
            size.bounds.emplace_back(named ? bound->Name() : std::to_string(number), number);
            size.written += (size.written.empty() ? "" : ", ") + compare->m_e1()->Name() +
                            "<=" + size.bounds.back().first;
        }
        if (!size.bounds.empty()) size.written = w.left->Name() + "[" + size.written + "]";
        parameters_names_.push_back(w.left->Name());
        if (w.fallback) parameters_dict_[w.left->Name()] = w.fallback;
        sizes_.push_back(std::move(size));
    }

    // Arity is checked here rather than at the call site because this is the
    // only place that knows both the definition's parameters and the call's.
    void CheckArity(const std::string& reference_name, const ParametersCall<T>& param_call) const {
        const size_t provided = param_call.parameters_expression().size()
                              + param_call.parameters_dict().size();
        if(parameters_names_.empty()) {
            if(provided != 0) {
                throw std::runtime_error(reference_name + " takes no arguments");
            }
            return;
        }
        const size_t required = parameters_names_.size() - parameters_dict_.size();
        if(provided < required || provided > parameters_names_.size()) {
            const size_t expected = parameters_names_.size();
            throw std::runtime_error(reference_name + " expects " + std::to_string(expected)
                                     + (expected == 1 ? " argument, got " : " arguments, got ")
                                     + std::to_string(provided));
        }

        // Counting the arguments is not enough: an unknown keyword satisfies
        // the count, leaves a real parameter unbound and lets it fall through
        // to a global.
        const size_t positional = param_call.parameters_expression().size();
        for(const auto& kwarg : param_call.parameters_dict()) {
            auto named = std::find(parameters_names_.begin(), parameters_names_.end(), kwarg.first);
            if(named == parameters_names_.end()) {
                throw std::runtime_error(reference_name + " has no parameter " + kwarg.first);
            }
            if(static_cast<size_t>(named - parameters_names_.begin()) < positional) {
                throw std::runtime_error(reference_name + " got two values for " + kwarg.first);
            }
        }

        // Nor is the count enough the other way: a keyword can fill an
        // optional parameter and leave a required one with nothing.
        for(size_t i = positional; i < parameters_names_.size(); ++i) {
            const std::string& name = parameters_names_[i];
            if(param_call.parameters_dict().count(name) == 0
               && parameters_dict_.count(name) == 0) {
                throw std::runtime_error(reference_name + " has no value for " + name);
            }
        }
    }

    typedef std::vector<std::pair<std::string, T>> Arguments;

    // Evaluated in the caller's scope and bound in the callee's, which is why
    // the two halves are separate: inside the callee, 'h(i*x)' would resolve
    // x against the parameter this very call is about to bind.
    Arguments EvaluateArguments(const ParametersCall<T>& param_call, EvaluationVisitor<T>& evaluator) const {
        Arguments arguments;
        auto pname = parameters_names_.begin();
        for(const auto& expr : param_call.parameters_expression()) {
            if(pname != parameters_names_.end()) {
                arguments.emplace_back(*pname, expr->accept(evaluator));
                ++pname;
            }
        }
        for(const auto& kwarg : param_call.parameters_dict_) {
            arguments.emplace_back(kwarg.first, kwarg.second->accept(evaluator));
        }
        return arguments;
    }

    static void Bind(const Arguments& arguments, ReferenceStack<T>& stack) {
        for(const auto& argument : arguments) {
            stack.BindValue(argument.first, argument.second);
        }
    }

    // 'j<=2' names a row and bounds it; a name alone is left unbounded, for the
    // definition to say it has no size; anything else is one row.
    static void Place(const PExpression<T>& place, std::string& name, PExpression<T>& bound,
                      int& index, EvaluationVisitor<T>& evaluator) {
        if (auto* compare = dynamic_cast<CompareExpression<T>*>(place.get());
            compare && compare->Op() == Comparison::LessEqual) {
            if (auto* ref = dynamic_cast<RefExpression<T>*>(compare->m_e1().get())) {
                name  = ref->Name();
                bound = compare->m_e2();
                return;
            }
        }
        if (auto* ref = dynamic_cast<RefExpression<T>*>(place.get())) {
            name = ref->Name();
            return;
        }
        index = AsIndex<T>(place->accept(evaluator));
    }

    // A clause for the cells of a matrix: all of them, named and bounded, or
    // one, numbered.
    bool                  cells() const { return cells_; }
    const std::string&    row_name() const { return row_name_; }
    const std::string&    col_name() const { return col_name_; }
    const PExpression<T>& rows() const { return rows_; }
    const PExpression<T>& cols() const { return cols_; }
    int                   row() const { return row_; }
    int                   col() const { return col_; }
    // A tensor's clause names its slice first; a matrix's has one slice.
    const std::string&    slice_name() const { return slice_name_; }
    const PExpression<T>& slices() const { return slices_; }
    int                   slice() const { return slice_; }
    // How many indices name a cell: one for a column, three for a tensor.
    int  rank() const { return rank_; }
    bool column() const { return rank_ == 1; }
    bool tensor() const { return rank_ == 3; }

    PExpression<T> guard() const {return guard_;}
    const std::string& signature() const {return signature_;}
    bool guarded() const {return bool(guard_);}
    int index() const {return index_;}
    const std::string& index_name() const {return index_name_;}
    const std::vector<std::string>& parameters_names() const {return parameters_names_;}
    const ExprDict<T>& parameters_dict() const {return parameters_dict_;}
    const std::vector<Size>&        sizes() const { return sizes_; }
    bool indexed() const {return indexed_;}
    bool general() const {return indexed_ && !index_name_.empty();}


protected:
    std::vector<Size>        sizes_;  // as this clause states them
    PExpression<T> guard_;
    std::string signature_;
    std::vector<std::string> parameters_names_;
    ExprDict<T> parameters_dict_;
    std::string index_name_;
    int index_ = 0;
    bool indexed_ = false;
    bool                     cells_   = false;
    std::string              row_name_, col_name_, slice_name_;
    PExpression<T>           rows_, cols_, slices_;
    int                      row_ = 0, col_ = 0, slice_ = 1;
    int                      rank_ = 2;
};

// The right-hand side of a call: 'f(1, 2)_(n-1)'. Unlike a definition's index,
// a call's is an arbitrary expression, evaluated in the calling scope.
template <typename T>
class ParametersCall
{
public:

    friend class ParametersDefinition<T>;
    ParametersCall() = default;

    ParametersCall(PExpression<T> params, PExpression<T> subexpr, bool limit = false)
        : arguments_(params), limit_(limit) {
        if(params) {
            ParametersVisitor<T> params_visitor;
            params->accept(params_visitor);
            parameters_names_ = params_visitor.get_parameters_names();
            parameters_exprs_ = params_visitor.get_parameters_expr();
            parameters_dict_ = params_visitor.get_parameters_dict();
        }
        subexpr_ = subexpr;
        indexed_ = bool(subexpr);
    }

    bool TryEvalIndex(ReferenceStack<T>& stack, int& index_evaluation) const {
        if(!subexpr_) {
            return false;
        }
        EvaluationVisitor<T> evaluator(stack);
        index_evaluation = AsIndex<T>(subexpr_->accept(evaluator));
        return true;
    }

    const std::vector<std::string>& parameters_names() const {return parameters_names_;}
    PExpression<T> subexpr() const {return subexpr_;}
    // The arguments as written, which a model reads for the index an input's
    // is written with: 'x_n = n'.
    const PExpression<T>&              arguments() const { return arguments_; }
    const std::vector<PExpression<T>>& parameters_expression() const {return parameters_exprs_;}
    const ExprDict<T>& parameters_dict() const {return parameters_dict_;}
    bool indexed() const {return indexed_;}
    bool limit() const {return limit_;}


protected:
    PExpression<T> guard_;
    std::string signature_;
    std::vector<std::string> parameters_names_;
    std::vector<PExpression<T>> parameters_exprs_;
    ExprDict<T> parameters_dict_;
    PExpression<T>              arguments_;
    PExpression<T> subexpr_;
    bool indexed_ = false;
    bool limit_ = false;
};

template <typename T>
const ParametersCall<T>& FuncExpression<T>::Call() const {
    if (!call_) call_ = std::make_unique<const ParametersCall<T>>(m_e1(), m_e2(), limit_);
    return *call_;
}

#endif // PARAMETERS_HPP
