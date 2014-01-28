#ifndef H_EXPR_DICT
#define H_EXPR_DICT

#include <string>


template <typename T>
class Expression;

template <typename T>
using PExpression = std::shared_ptr<Expression<T>>;


template <typename T>
using ExprDict = std::unordered_map<std::string,PExpression<T> >;

#endif // H_EXPR_DICT
