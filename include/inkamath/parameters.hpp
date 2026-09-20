#ifndef PARAMETERS_HPP
#define PARAMETERS_HPP


#include "inkamath/expression_visitor.hpp"
#include "inkamath/expression.hpp"

template <typename T>
class ReferenceStack;

template <typename T>
class ParametersCall;

// README.md section 4.2: an index is a whole number. Truncating would make
// 'f_(0.5)' quietly mean 'f_0', and 'f_(2+i)' mean 'f_2'.
template <typename T>
int AsIndex(const T& value) {
    const int index = numeric_interface<T>::toInt(value);
    if(numeric_interface<T>::abs(value - T(index)) != 0) {
        throw std::runtime_error("an index must be a whole number, not "
                                 + numeric_interface<T>::toString(value));
    }
    return index;
}

// The left-hand side of a definition: 'f(x, y)_n' or 'f_0'.
//
// README.md sections 4.2 and 4.3: an index written as an identifier names the
// variable of the general clause; anything else must be a constant integer
// expression and names one base clause. Nothing in between is a definition.
template <typename T>
class ParametersDefinition
{
public:
    ParametersDefinition() = default;

    ParametersDefinition(PExpression<T> params, PExpression<T> subexpr, EvaluationVisitor<T>& evaluator) {
        if(params) {
            ParametersVisitor<T> params_visitor;
            params->accept(params_visitor);
            parameters_names_ = params_visitor.get_parameters_names();
            parameters_dict_ = params_visitor.get_parameters_dict();
        }
        if(subexpr) {
            indexed_ = true;
            if(RefExpression<T>* variable = dynamic_cast<RefExpression<T>*>(subexpr.get())) {
                index_name_ = variable->Name();
            }
            else {
                index_ = AsIndex<T>(subexpr->accept(evaluator));
            }
        }
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
    }

    typedef std::vector<std::pair<std::string, T>> Arguments;

    // Evaluated in the caller's scope and bound in the callee's, which is why
    // the two halves are separate: inside the callee, 'h(i*x)' would resolve
    // x against the parameter this very call is about to bind.
    Arguments EvaluateArguments(const ParametersCall<T>& param_call, EvaluationVisitor<T>& evaluator) const {
        Arguments arguments;
        for(const auto& definition : parameters_dict_) {
            arguments.emplace_back(definition.first, definition.second->accept(evaluator));
        }
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
            stack.Set(argument.first, ParametersDefinition<T>(),
                      PExpression<T>(new ValExpression<T>(argument.second)));
        }
    }

    int index() const {return index_;}
    const std::string& index_name() const {return index_name_;}
    const std::vector<std::string>& parameters_names() const {return parameters_names_;}
    const ExprDict<T>& parameters_dict() const {return parameters_dict_;}
    bool indexed() const {return indexed_;}
    bool general() const {return indexed_ && !index_name_.empty();}


protected:
    std::vector<std::string> parameters_names_;
    ExprDict<T> parameters_dict_;
    std::string index_name_;
    int index_ = 0;
    bool indexed_ = false;
};

// The right-hand side of a call: 'f(1, 2)_(n-1)'. Unlike a definition's index,
// a call's is an arbitrary expression, evaluated in the calling scope.
template <typename T>
class ParametersCall
{
public:

    friend class ParametersDefinition<T>;
    ParametersCall() = default;

    ParametersCall(PExpression<T> params, PExpression<T> subexpr) {
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
    const std::vector<PExpression<T>>& parameters_expression() const {return parameters_exprs_;}
    const ExprDict<T>& parameters_dict() const {return parameters_dict_;}
    bool indexed() const {return indexed_;}


protected:
    std::vector<std::string> parameters_names_;
    std::vector<PExpression<T>> parameters_exprs_;
    ExprDict<T> parameters_dict_;
    PExpression<T> subexpr_;
    bool indexed_ = false;
};

#endif // PARAMETERS_HPP
