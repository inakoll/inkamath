#ifndef HPP_INKREFERENCE
#define HPP_INKREFERENCE

#include "inkamath/expression.hpp"
#include "inkamath/pexpression.hpp"
#include "inkamath/parameters.hpp"
#include "inkamath/mapstack.hpp"
#include "inkamath/expression_visitor.hpp"

#include <map>
#include <tuple>
#include <stdexcept>

template <typename T>
using ExpressionDefinition =
        std::tuple<
            ParametersDefinition<T>,
            PExpression<T>
        >;

template <typename T>
class ReferenceStack;

template <typename T>
class EvaluationVisitor;

template <typename T>
class Reference {
public:

    void add_expression(const std::string& ai_reference_name, const ParametersDefinition<T>& ai_parameters, PExpression<T>  ai_expression) {
        if(reference_name_.empty()) {
            reference_name_ = ai_reference_name;
        }
        else if(reference_name_ != ai_reference_name) {
            throw std::runtime_error(std::string("Interpreter internal error : invalid reference names ") + ai_reference_name + " and " + reference_name_);
        }

        // One definition per name. An indexed clause extends a sequence,
        // creating one if the name held a plain definition; a plain
        // definition replaces whatever was there.
        if(ai_parameters.a() != 0) {
            plain_ = ExpressionDefinition<T>();
            general_ = ExpressionDefinition<T>(ai_parameters, ai_expression);
        }
        else if(ai_parameters.indexed()) {
            plain_ = ExpressionDefinition<T>();
            base_[ai_parameters.b()] = ExpressionDefinition<T>(ai_parameters, ai_expression);
        }
        else {
            base_.clear();
            general_ = ExpressionDefinition<T>();
            memo_.clear();
            plain_ = ExpressionDefinition<T>(ai_parameters, ai_expression);
        }

        ParametersDefinition<T> gen_params_def;
        gen_params_def = std::get<0>(general_);
        if( gen_params_def.parameters_dict() != ai_parameters.parameters_dict()
                || gen_params_def.parameters_names() != ai_parameters.parameters_names()) {
            // memoized index is invalidated when adding a new expression
            // with different parameters
            memo_.clear();
        }
    }

    T Eval( const ParametersCall<T>& ai_parameters, ReferenceStack<T>& stack) {
//        if(ai_parameters.parameters_dict().empty()
//          && std::get<0>(this->general_).parameters_names().empty()) {
//            // Evaluation of an expression might mutate the internal stack_ object
//            // The following line ensure that the stack will be restored at the end of the function
//            // or in case of an exception thanks to RAII.
//            // The context guard might have a huge impact on performance.
//            // Consider to move it closer to the reference parameters assignation as a future optimisation.
//            typename ReferenceStack<T>::Guard guard(stack);
//            return this->EvalImp(ai_parameters, stack);
//        }
//        else {
//            return this->EvalImp(ai_parameters, stack);
//        }
        // Only the user's own call is checked. The recursion machinery builds
        // synthetic ParametersCalls that deliberately carry no arguments --
        // they are already bound in the enclosing scope -- and those reach
        // SafeRecursiveEval, not here.
        CallParameters().CheckArity(reference_name_, ai_parameters);
        return this->EvalImp(ai_parameters, stack);

    }
	

    T SafeRecursiveEval( const ParametersCall<T>& ai_parameters, ReferenceStack<T>& stack) {
        // if functionnal parameters are identical to the general expr
        // return the memoized value at the evaluated index of this potentially recursive function
        // or return {}
        T evaluation = {};
        EvaluationVisitor<T> evaluator(stack);

        // Important note: Indexed expression shall not be recursive! ==> stack overflow
        if(!TryEvaluateBaseClause(ai_parameters, evaluator, evaluation)) {
            PExpression<T> gen_expr_def;
            ParametersDefinition<T> gen_params_def;
            std::tie(gen_params_def, gen_expr_def) = general_;
            if(     gen_params_def.a() == ai_parameters.a()
                    &&  gen_params_def.parameters_dict().empty()
                    &&  ai_parameters.parameters_dict().empty()
                    &&  gen_params_def.parameters_names() == ai_parameters.parameters_names()) {
                int index_value;
                if(ai_parameters.TryEvalIndex(stack, index_value)) {
                    auto it = memo_.find(index_value);
                    if(it != memo_.end()) {
                        evaluation = it->second;
                    }
                    else if(index_value < 0) {
                        evaluation = {};
                        memo_[index_value] = evaluation;
                    }
                    else if(!TryEvaluateBaseClause(ai_parameters, evaluator, evaluation)) {
                        ParametersCall<T> fwd_parameter(0,index_value,true);
                        TryEvaluateGeneralClause(fwd_parameter, evaluator, evaluation);
                    }
                }
            }
        }
        return evaluation;
    }
	
private:
    // The clauses of one name share their parameter list; any of them answers
    // for the whole definition.
    const ParametersDefinition<T>& CallParameters() const {
        if(std::get<1>(general_)) return std::get<0>(general_);
        if(!base_.empty()) return std::get<0>(base_.begin()->second);
        return std::get<0>(plain_);
    }

    T EvalImp( const ParametersCall<T>& ai_parameters, ReferenceStack<T>& stack) {
        EvaluationVisitor<T> evaluator(stack);
        T result;

        if(std::get<1>(plain_)) {
            TryEvaluatePlainExpression(ai_parameters, evaluator, result);
        }
        else if(!TryEvaluateBaseClause(ai_parameters, evaluator, result)
                && !TryEvaluateGeneralClause(ai_parameters, evaluator, result)) {
            // Nothing sensible to invent: a sequence has no value under its
            // bare name, and no value at an index no clause covers.
            int index = 0;
            if(ai_parameters.TryEvalIndex(stack, index)) {
                throw std::runtime_error(reference_name_ + " has no clause for index "
                                         + std::to_string(index));
            }
            throw std::runtime_error(reference_name_ + " is a sequence; index it, as in "
                                     + reference_name_ + "_"
                                     + std::to_string(base_.begin()->first));
        }

        return result;
    }

    bool TryEvaluateBaseClause(const ParametersCall<T>& ai_parameters, EvaluationVisitor<T>& evaluator, T& evaluation) {
        bool succeed = false;
        ReferenceStack<T>& stack = evaluator.stack();
        int index_value;
        if(ai_parameters.TryEvalIndex(stack, index_value)) {
            // Evaluation to an index is requested
            auto ind_definition = base_.find(index_value);
            if(ind_definition != base_.end()) {
                ParametersDefinition<T> ind_params_def;
                PExpression<T> ind_expr_def;
                std::tie(ind_params_def, ind_expr_def) = ind_definition->second;

                ind_params_def.SetCallParameters(ai_parameters, evaluator);
                if(ind_expr_def) {
                    evaluation = ind_expr_def->accept(evaluator);
                    succeed = true;
                }
            }
       }
       return succeed;
    }

    bool TryEvaluateGeneralClause(const ParametersCall<T>& ai_parameters, EvaluationVisitor<T>& evaluator, T& evaluation) {
        bool succeed = false;
        ReferenceStack<T>& stack = evaluator.stack();
        PExpression<T> gen_expr_def;
        ParametersDefinition<T> gen_params_def;
        std::tie(gen_params_def, gen_expr_def) = general_;
        if(gen_expr_def) {
            if(ai_parameters.indexed()) {
                long long index = ai_parameters.b() - gen_params_def.b();
                if(ai_parameters.a() != 0) {
                    index *= ai_parameters.a();
                }
                if(gen_params_def.a() != 0) {
                    index /= gen_params_def.a();
                }
                gen_params_def.SetCallParameters(ai_parameters, evaluator);
                typename ReferenceStack<T>::Guard guard(stack);
                stack.Set(gen_params_def.index_name(), ParametersDefinition<T>(), PExpression<T>(new ValExpression<T>(T(index))));
                evaluation = gen_expr_def->accept(evaluator);
                memo_[index] = evaluation;
                succeed = true;
            }
            else {
                long long start_index = 0;
                T start_evaluation;
                if(!memo_.empty() || !base_.empty()) {
                    if(!memo_.empty()) {
                        start_index = memo_.rbegin()->first;
                    }
                    if(!base_.empty()) {
                        start_index = std::max(start_index, base_.rbegin()->first);
                    }
                    if(!memo_.empty() && start_index == memo_.rbegin()->first) {
                        start_evaluation = memo_.rbegin()->second;
                    }
                    else {

                        ParametersDefinition<T> ind_params_def;
                        PExpression<T> ind_expr_def;
                        std::tie(ind_params_def, ind_expr_def) = base_.rbegin()->second;

                        ind_params_def.SetCallParameters(ai_parameters, evaluator);
                        start_evaluation = ind_expr_def->accept(evaluator);
                    }
                }
                gen_params_def.SetCallParameters(ai_parameters, evaluator);

                evaluation = start_evaluation;
                using difference_type = decltype(numeric_interface<T>::abs(std::declval<T>()));
                difference_type diff = numeric_interface<difference_type>::one();
                size_t iter_count = 0;
                typename ReferenceStack<T>::Guard guard(stack);
                while(diff > 1E-10 && iter_count < 30) {
                    start_index += gen_params_def.a();
                    stack.Set(gen_params_def.index_name(), ParametersDefinition<T>(), PExpression<T>(new ValExpression<T>(T(start_index))));
                    evaluation = gen_expr_def->accept(evaluator);
                    diff = numeric_interface<T>::abs(evaluation-start_evaluation);
                    start_evaluation = evaluation;
                    ++iter_count;
                }
                memo_[start_index] = evaluation;
                succeed = true;
            }
        }
        return succeed;
    }

    bool TryEvaluatePlainExpression(const ParametersCall<T>& ai_parameters, EvaluationVisitor<T>& evaluator, T& evaluation) {
        bool succeed = false;
        ParametersDefinition<T> single_params_def;
        PExpression<T> plain_def;
        std::tie(single_params_def, plain_def) = plain_;
        if(plain_def) {

            single_params_def.SetCallParameters(ai_parameters, evaluator);
            evaluation = plain_def->accept(evaluator);
            succeed = true;
        }
        return succeed;
    }

    // Signed: an index may be negative, and these maps are read in order
    // (rbegin) to pick the highest known term.
    typedef std::map<long long, ExpressionDefinition<T>> BaseClauses;
    typedef std::map<long long, T> MemoisedTerms;

    std::string reference_name_;
	
    // Exactly one of these is populated: a plain definition, or a sequence
    // made of base clauses and at most one general clause.
    ExpressionDefinition<T>     plain_;

    BaseClauses                 base_;
    ExpressionDefinition<T>     general_;
    MemoisedTerms               memo_;
	
};

#endif // HPP_INKREFERENCE
