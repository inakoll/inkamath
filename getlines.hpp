#ifndef GETLINES_HPP
#define GETLINES_HPP

#include <iostream>
#include <string>
#include <iterator>

struct lines_iterator
  : std::iterator<
        std::input_iterator_tag,
        std::string
    >
{
    lines_iterator() : psin_{}, pstr_{}, delim_{} {}
    lines_iterator(std::istream* psin,
                   std::string* pstr,
                   char delim)
        : psin_(psin), pstr_(pstr), delim_(delim)
    {
        increment();
    }

    lines_iterator(const lines_iterator&) = default;


    lines_iterator& operator++() {
        increment();
        return *this;
    }


    lines_iterator operator++(int) {
        lines_iterator tmp(*this);
        increment();
        return tmp;
    }

    std::string& operator*() const
    {
        return *pstr_;
    }


    friend bool operator==(const lines_iterator& a, const lines_iterator & b)
    {
        return a.pstr_ == b.pstr_;
    }

    friend bool operator!=(const lines_iterator& a, const lines_iterator & b)
    {
        return !(a == b);
    }

private:
    void increment()
    {
        if(!std::getline(*psin_, *pstr_, delim_))
            *this = lines_iterator{};
    }

    std::istream *psin_;
    std::string *pstr_;
    char delim_;
};

template <typename Iterator>
class iterator_range {
public:
    using iterator = Iterator;

    iterator_range(iterator first, iterator last) :
        first(first), last(last)
    {}

    iterator  begin() const {return first;}
    iterator  end() const {return last;}

private:
    iterator first;
    iterator last;
};

using lines_range_base =
    iterator_range<lines_iterator>;

struct lines_range_data {std::string str_;};

struct lines_range
    : private lines_range_data, lines_range_base
{
    explicit lines_range(std::istream & sin,
                         char delim = '\n')
        : lines_range_base{
              lines_iterator{&sin, &str_, delim},
              lines_iterator{}}
    {}
};

inline
lines_range getlines(std::istream& sin, char delim = '\n')
{
    return lines_range{sin, delim};
}

#endif // GETLINES_HPP
