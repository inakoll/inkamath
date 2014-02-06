#ifndef HPP_MAPSTACK
#define HPP_MAPSTACK

#include <list>
#include <unordered_map>
#include <vector>
#include <stack>
#include <tuple>
#include <string>
#include <algorithm>
#include <type_traits>
#include <cassert>

#ifdef INKAMATH_USING_BOOST
#include <boost/variant.hpp>
#endif

template <typename KeyType, typename ValueType, typename InternalIterator>
struct mapstack_iterator_base {
	typedef mapstack_iterator_base self_type;
	typedef InternalIterator internal_iterator;
	
	typedef typename InternalIterator::value_type::first_type key_type;
	typedef typename InternalIterator::value_type::second_type stack_type;
	typedef typename stack_type::value_type stack_value_type;
	
    typedef std::tuple<key_type, stack_value_type&> pair_type;

	typedef ValueType  value_type;
	typedef ValueType& reference;
	typedef ValueType* pointer;

    mapstack_iterator_base(const internal_iterator& ai_it) :
        m_it(ai_it) {}

    pair_type operator*() const {
        return pair_type(m_it->first, m_it->second.back());
    }
	
	// no operator->() since operator* return a temporary (moved) object

    mapstack_iterator_base& operator++() {
        ++m_it;
        return *this;
    }

    mapstack_iterator_base operator++(int) {
        return mapstack_iterator_base(m_it++);
    }

	friend
    bool operator==(const self_type& x, const self_type& y) {
        return x.m_it == y.m_it;
    }
	
	friend
    bool operator!=(const self_type& x, const self_type& y) {
        return !(x == y);
    }
	
	friend
    bool operator<(const self_type& x, const self_type& y) {
        return x.m_it< y.m_it;
    }
	
	friend
    bool operator>(const self_type& x, const self_type& y) {
        return y < x;
    }
	
	friend
    bool operator<=(const self_type& x, const self_type& y) {
        return !(y < x);
    }
	
	friend
    bool operator>=(const self_type& x, const self_type& y) {
        return !(x < y);
    }
private:
	internal_iterator m_it;
};

template <typename T1, typename T2, 
		template <class, class, class...> class MapType = std::unordered_map
		>
class Mapstack {
public:
    typedef T1 											key_type;
    typedef T2 											value_type;
    typedef MapType<key_type, std::vector<value_type>>	map_type;
    typedef std::stack<std::vector<key_type>> 			current_stack_type;

    Mapstack() {
        m_stack.push(std::vector<key_type>());
    }
	
	typedef typename map_type::iterator	internal_iterator;
	typedef typename map_type::const_iterator internal_const_iterator;
    typedef mapstack_iterator_base<key_type, value_type, internal_iterator>
		iterator;
		
    typedef mapstack_iterator_base<key_type, const value_type, internal_const_iterator>
		const_iterator;
		
    typedef typename 
        std::vector<key_type>::const_iterator
		current_const_iterator;

    const_iterator begin() const {
        return m_map.begin();
    }

    const_iterator end() const {
        return m_map.end();
    }
    
    iterator begin()
    {
    	return m_map.begin();
    }

    iterator end()
    {
    	return m_map.end();
    }
	
    current_const_iterator CurrentBegin() const {
        return m_stack.top().begin();
    }

    current_const_iterator CurrentEnd() const {
        return m_stack.top().end();
    }

    void Push();
    void Pop();
    void Set(const key_type& ai_key, const value_type&  ai_value);

    template <typename T>
    bool Get(const key_type& ai_key, T& ao_value) const;

    bool Get(const key_type& ai_key, value_type& ao_value) const;


    void Clear();

    struct Context {
        Context(Mapstack& parent) : parent_(parent) {
            parent_.Push();
        }
        ~Context() {
            parent_.Pop();
        };

    private:
        Mapstack& parent_;
    };


private:
    current_stack_type  m_stack;
    map_type 			m_map;
};

template <typename T1, typename T2, template <class, class, class...> class MapType>
void Mapstack<T1, T2, MapType>::Push()
{
    for(const auto& key : m_stack.top()) {
        m_map[key].push_back(m_map[key].back());
    }

    m_stack.push(std::vector<key_type>());
}

template <typename T1, typename T2, template <class, class, class...> class MapType>
void Mapstack<T1, T2, MapType>::Pop()
{
    for(const auto& key: m_stack.top()) {
        assert(!m_map.at(key).empty());
        m_map.at(key).pop_back();

        if(m_map.at(key).empty()) {
            m_map.erase(key);
        }
    }

    m_stack.pop();
    if(m_stack.empty()) {
        m_stack.push(std::vector<key_type>());
    }
}

template <typename T1, typename T2, template <class, class, class...> class MapType>
void Mapstack<T1, T2, MapType>::Set(const key_type& ai_key, const value_type&  ai_value)
{
    std::vector<value_type>& w_valueStack = m_map[ai_key];

    if(w_valueStack.empty()) {
        m_stack.top().push_back(ai_key);
        w_valueStack.push_back(ai_value);

    } else {
        if(std::find(m_stack.top().begin(), m_stack.top().end(), ai_key) == m_stack.top().end()) {
            m_stack.top().push_back(ai_key);
        }

        w_valueStack.back() = ai_value;
    }

    return;
}

template <typename T1, typename T2, template <class, class, class...> class MapType>
bool Mapstack<T1, T2, MapType>::Get(const key_type& ai_key, value_type& ao_value) const
{
    auto it = m_map.find(ai_key);
    bool w_bRet = it != m_map.end();

    if(w_bRet) {
        assert(!it->second.empty());
        ao_value = it->second.back();
    }

    return w_bRet;
}

// Boost::variant aware Get function overload, suppose that Mapstack::value_type is a boost variant 
#ifdef INKAMATH_USING_BOOST
template <typename T1, typename T2, template <class, class, class...> class MapType>
template <typename T>
bool Mapstack<T1, T2, MapType>::Get(const key_type& ai_key, T& ao_value) const
{
    value_type w_variant;
    bool w_bRet = Get(ai_key, w_variant) && boost::get<T>(&w_variant);

    if(w_bRet) {
        ao_value = boost::get<T>(w_variant);
    }

    return w_bRet;
}
#endif

template <typename T1, typename T2, template <class, class, class...> class MapType>
void Mapstack<T1, T2, MapType>::Clear()
{
    while(!m_stack.empty())
        m_stack.pop();

    m_map.clear();
    m_stack.push(std::vector<key_type>());
    return;
}

#endif // MAPSTACK
