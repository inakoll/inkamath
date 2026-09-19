#ifndef H_EXPR_DICT
#define H_EXPR_DICT

#include <string>
#include <unordered_map>
#include "pexpression.hpp"

template <typename T>
using ExprDict = std::unordered_map<std::string,PExpression<T> >;

#endif // H_EXPR_DICT
