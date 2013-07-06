#ifndef SEQUENCE_HPP
#define SEQUENCE_HPP

#include <string>
#include <deque>

template <typename T>
class Sequence
{
public:
    Sequence() {}
    bool Get(size_t i, T& val)
    {
        if(m_values.size() > i)
        {
            val = m_values[i];
            return m_defines[i];
        }
        else
        {
            return false;
        }
    }

    bool Get(T& val)
    {
        if(!m_values.empty() && !(m_values.size() > 1))
        {
            val = m_values.back(); return m_defines.back();
        }
        else
        {
            return false;
        }
    }

    bool Set(size_t i, const T& val)
    {
        bool ret = i>=m_values.size();
        if(ret)
        {
            m_values.resize(i+1,T());
            m_defines.resize(i+1,false);
        }
        m_values[i] = val;
        m_defines[i] = true;

        return ret;
    }

    bool Set(const T& val)
    {
        bool ret = m_values.empty() || m_values.size() > 1;
        m_values.resize(1,val);
        m_defines.resize(1,true);
        return ret;
    }

    void Clear() {m_values.clear(); m_defines.clear();}

    size_t Size() {return m_values.size();}
private:
    std::deque<T> m_values;
    std::deque<bool> m_defines;


};

#endif // SEQUENCE_HPP
