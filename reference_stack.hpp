#ifndef EXPRESSION_STACK_HPP
#define EXPRESSION_STACK_HPP

#include <string>
#include "mapstack.hpp"

template <typename T>
class Expression;

template <typename T>
using PExpression = std::shared_ptr<Expression<T>>;

template <typename T>
class ParametersDefinition;

template <typename T>
class ParametersCall;

#include "reference.hpp"

template <typename T>
class ReferenceStack {
public:

    typedef Mapstack<std::string, Reference<T>> stack_type;

    ReferenceStack() {
        this->Set("pi", ParametersDefinition<T>(), PExpression<T>( new ValExpression<T>(T(3.1415926535898))));
        this->Set("e",  ParametersDefinition<T>(), PExpression<T>( new ValExpression<T>(T(2.7182818284590))));
        stack_.Push();
    }

    void Set(const std::string& ai_reference_name, const ParametersDefinition<T>& ai_parameters, PExpression<T>  ai_expression) {
        // Try to get a copy of the actual reference
        Reference<T> reference;
        stack_.Get(ai_reference_name, reference);

        auto expr = WrapRecursiveExpression(ai_reference_name, ai_parameters, ai_expression);

        // The fact that the reference was in the stack or not doesn't matter
        // Updating or initializing is the same operation
        reference.add_expression(ai_reference_name, ai_parameters, expr);
        stack_.Set(ai_reference_name, reference);
    }


    T Eval(const std::string& ai_reference_name, const ParametersCall<T>& ai_parameters)  {
        // Evaluation of an expression might mutate the internal stack_ object
        // The following line ensure that the stack will be restored at the end of the function
        // or in case of an exception thanks to RAII.
        // The context guard might have a huge impact on performance.
        // Consider to move it closer to the reference parameters assignation as a future optimisation.
        typename stack_type::Context guard(stack_);

        // Just evaluate the reference with the parameters if it's in the stack
        Reference<T> reference;
        if(stack_.Get(ai_reference_name, reference)) {
            return reference.Eval(ai_parameters,*this);
        }
        else {
            return {};
        }
    }

    T SafeRecursiveEval(const std::string& ai_reference_name, const ParametersCall<T>& ai_parameters)  {
        // Just evaluate the reference with the parameters if it's in the stack
        Reference<T> reference;
        if(stack_.Get(ai_reference_name, reference)) {
            return reference.SafeRecursiveEval(ai_parameters,*this);
        }
        else {
            return {};
        }
    }

    PExpression<T> WrapRecursiveExpression(const std::string& ai_reference_name, const ParametersDefinition<T>& ai_parameters, PExpression<T>  ai_expression) {
         auto wrapped_recursive_expr = ai_expression;
         auto root_expr = ai_expression->Clone();
         auto wrapped_exprs = RecursiveExprVisitor<T>(ai_reference_name, ai_parameters, root_expr).wrapped();
         using map_type = decltype(wrapped_exprs);
         using value_type = typename map_type::value_type;
         if(wrapped_exprs.size() != 0) {
             dynarray<PExpression<T>> recursive_placeholders(wrapped_exprs.size());
             auto it = std::adjacent_find(wrapped_exprs.begin(), wrapped_exprs.end(),
                                [](const value_type& a,
                                const value_type& b) {
                                    return (a.first+1)!=(b.first);
                                });
             if(it == wrapped_exprs.end()) {
                 size_t i = 0;
                 for(auto expr_tuple : wrapped_exprs) {
                     *std::get<0>(expr_tuple.second) = std::get<1>(expr_tuple.second);
                     recursive_placeholders[i] = *std::get<0>(expr_tuple.second);
                 }
             }
             wrapped_recursive_expr = std::make_shared<RecursiveExpression<T>>(root_expr, std::move(recursive_placeholders));
         }
         return wrapped_recursive_expr;
    }
    friend struct Guard;
    struct Guard {
    public:
        Guard(ReferenceStack<T>& stack) : guard(stack.stack_) {}
    private:
        typename stack_type::Context guard;
    };

    void Pop() {
        stack_.Pop();
    }

    void Clear() {
        stack_.Clear();
    }

private:
    mutable stack_type stack_;
};

#endif // EXPRESSION_STACK_HPP
