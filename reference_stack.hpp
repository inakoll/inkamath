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
            return T();
        }
    }

    PExpression<T> WrapRecursiveExpression(const std::string& ai_reference_name, const ParametersDefinition<T>& ai_parameters, PExpression<T>  ai_expression) {
         auto wrapped_exprs = RecursiveExprVisitor<T>(ai_reference_name, ai_expression).wrapped();

         // Compute the relative index difference between the current reference definition
         // and all the encountered recursive references.
         dynarray<long long int> diff(wrapped_exprs.size());
         int i = 0;
         for(auto w : wrapped_exprs) {
             auto wrapped = std::static_pointer_cast<RecursivePlaceholderExpression<T>>(w);
             if(wrapped->params().a() == ai_parameters.a()) {
                 if(wrapped->params().a() != 0) {
                     diff[i] = (wrapped->params().b()-ai_parameters.b())/wrapped->params().a();
                 }
                 else if(wrapped->params().b() == 0 && ai_parameters.b() == 0) {
                     diff[i] = -1;
                 }
                 // what else ?? try to stop before stack overflow : http://stackoverflow.com/questions/1578878/catching-stack-overflow-exceptions-in-recursive-c-functions
             }
             ++i;
         }
         // to be continuated...


         // For now, no recursion handling. Return the expression unchanged.
         return ai_expression;
    }

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
