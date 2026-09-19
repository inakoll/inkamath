#ifndef H_TOKEN
#define H_TOKEN

#include <string>
#include <list>
#include <sstream>

enum Type
{
    Add,Mult,Min,Div,Pow,Fact,Equal, Sub,
    Val,Func, Ref,
    LPar,RPar,LBra,RBra,
    Space, Comma, Semico
};

template <typename T>
struct Token
{
    Token(Type t, T val = 0, std::string n = ""): type(t), value(val), name(n) {}
    Type type;
    T value;
    std::string name;

    std::string Print(void)
    {
        std::string s;
        std::ostringstream oss(s);
        switch (type)
        {
        case LPar :
            s = "(";
            break;
        case RPar :
            s = ")";
            break;
        case LBra :
            s = "[";
            break;
        case RBra :
            s = "]";
            break;
        case Comma :
            s = ",";
            break;
        case Semico :
            s = ";";
            break;
        case Add  :
            s = "+";
            break;
        case Mult :
            s = "*";
            break;
        case Min  :
            s = "-";
            break;
        case Equal:
            s = "=";
            break;
        case Div  :
            s = "/";
            break;
        case Pow  :
            s = "^";
            break;
        case Fact :
            s = "!";
            break;
        case Sub  :
            s = "_";
            break;
        case Val  :
            oss << value;
            s = oss.str();
            break;
        case Func :
            oss << name;
            s = oss.str();
            break;
        case Ref  :
            oss << name;
            s = oss.str();
            break;
        default   :
            s = "" ;
            break;
        }
        return s;
    }
};

//template <typename T>
//void PrintTokens(std::string& tokens, std::list<Token<T> > list)
//{
//    std::cout << std::endl;
//    for_each(list.begin(),list.end(),std::mem_fun_ref(&Token<T>::Print));
//    std::cout << std::endl;
//}

#endif
