#ifndef PARAMETERS_HPP
#define PARAMETERS_HPP


#include "expression_visitor.hpp"
#include "expression.hpp"

template <typename T>
class ReferenceStack;

template <typename T>
class ParametersCall;

template <typename T>
class ParametersDefinition
{
public:
    ParametersDefinition() : a_(0), b_(0) {}

    ParametersDefinition(PExpression<T> params, PExpression<T> subexpr) {
        ParametersVisitor<T> params_visitor;
        SubVisitor<T> subexpr_visitor;

        if(params) {
            try {
                params->accept(params_visitor);
            }
            catch(const std::exception&) {
                return;
            }
        }

        if(subexpr) {
            try {
                subexpr->accept(subexpr_visitor);
            }
            catch(const std::exception& ) {
                return;
            }
        }
        parameters_names_ = params_visitor.get_parameters_names();
        parameters_dict_ = params_visitor.get_parameters_dict();
        index_name_ = subexpr_visitor.get_index_name();
        a_ = subexpr_visitor.get_a();
        b_ = subexpr_visitor.get_b();
    }
    ~ParametersDefinition() {}

    void SetCallParameters(const ParametersCall<T>& param_call, EvaluationVisitor<T>& evaluator) {
        ReferenceStack<T>& stack_ = evaluator.stack();
        for(auto definition : parameters_dict_) {
            stack_.Set(definition.first, ParametersDefinition<T>(), PExpression<T>(new ValExpression<T>(definition.second->accept(evaluator))));
        }
        auto pname = parameters_names_.begin();
        for(auto expr : param_call.parameters_expression()) {
            if(pname != parameters_names_.end()) {
                stack_.Set(*pname, ParametersDefinition<T>(), PExpression<T>(new ValExpression<T>(expr->accept(evaluator))));
                ++pname;
            }
        }
        for(auto kwarg : param_call.parameters_dict_) {
            stack_.Set(kwarg.first, ParametersDefinition<T>(), PExpression<T>(new ValExpression<T>(kwarg.second->accept(evaluator))));
        }
    }

    int a() const {return a_;}
    int b() const {return b_;}
    const std::string& index_name() const {return index_name_;}
    const std::vector<std::string>& parameters_names() const {return parameters_names_;}
    const ExprDict<T>& parameters_dict() const {return parameters_dict_;}


protected:
    std::vector<std::string> parameters_names_;
    ExprDict<T> parameters_dict_;
    std::string index_name_;
    int a_;
    int b_;
};

template <typename T>
class ParametersCall
{
public:

    friend class ParametersDefinition<T>;
    ParametersCall() : a_(0), b_(0), indexed_(false) {}

    ParametersCall(PExpression<T> params, PExpression<T> subexpr) {
        ParametersVisitor<T> params_visitor;
        SubVisitor<T> subexpr_visitor;

        if(params) {
            try {
                params->accept(params_visitor);

            }
            catch(const std::exception&) {
                return;
            }
        }

        if(subexpr) {
            indexed_ = true;
            try {
                subexpr->accept(subexpr_visitor);
            }
            catch(const std::exception& ) {
                return;
                subexpr_ = subexpr;
            }
        }
        else {
            indexed_ = false;
        }
        parameters_exprs_ = params_visitor.get_parameters_expr();
        parameters_dict_ = params_visitor.get_parameters_dict();
        index_name_ = subexpr_visitor.get_index_name();
        a_ = subexpr_visitor.get_a();
        b_ = subexpr_visitor.get_b();
    }

    ~ParametersCall() {}

    bool TryEvalIndex(ReferenceStack<T>& stack, int& index_evaluation) const {
        EvaluationVisitor<T> evaluator(stack);
        if(indexed_) {
            if(subexpr_) {
                index_evaluation = numeric_interface<T>::toInt(subexpr_->accept(evaluator));
            }
            else if (index_name_ != "") {
                index_evaluation = numeric_interface<T>::toInt(T(a_)*stack.Eval(index_name_, ParametersCall<T>())+T(b_));
            }
            else {
                index_evaluation = b_;
            }
        }
        return indexed_;
    }

    int a() const {return a_;}
    int b() const {return b_;}
    const std::string& index_name() const {return index_name_;}
    PExpression<T> subexpr() const {return subexpr_;}
    const std::vector<PExpression<T>>& parameters_expression() const {return parameters_exprs_;}
    const ExprDict<T>& parameters_dict() const {return parameters_dict_;}


protected:
    std::vector<PExpression<T>> parameters_exprs_;
    ExprDict<T> parameters_dict_;
    std::string index_name_;
    PExpression<T> subexpr_;
    int a_;
    int b_;
    bool indexed_;
};

#endif // PARAMETERS_HPP
