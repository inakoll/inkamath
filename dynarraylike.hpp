#ifndef DYNARRAYLIKE_HPP
#define DYNARRAYLIKE_HPP

#include <iostream>
#include <memory>
#include <initializer_list>
#include <algorithm>

// waiting for C++14 or C++1y dynarrays
// no magic here, just a fix runtime sized array on the heap

template <typename T>
class dynarray {
private:
  size_t size_;
  std::unique_ptr<T[]> p;

public:
  using iterator = T*;
  using const_iterator = const T*;

  dynarray() = default;

  dynarray(size_t size) : size_(size), p(new T[size]) {}

  dynarray(const std::initializer_list<T>& init)
      : size_(init.size()), p(new T[init.size()])
  {
      std::copy(init.begin(), init.end(), this->begin());
  }

  dynarray(dynarray&& other)
      : size_(other.size), p(std::move(other.p))
  {}

  iterator begin() {return this->p.get();}
  iterator end() {return this->p.get()+this->size_;}

  const_iterator begin() const {return this->p.get();}
  const_iterator end() const {return this->p.get()+this->size_;}

  T& operator[](size_t i) {return *(this->p.get()+i);}
  const T& operator[](size_t i) const {return *(this->p.get()+i);}

  size_t size() {return size_;}
};

template <typename T>
dynarray<T> make_dynarray(const std::initializer_list<T>& init)
{
    return dynarray<T>(init);
}

template <typename T>
dynarray<T> make_dynarray(size_t size)
{
    return dynarray<T>(size);
}

#endif // DYNARRAYLIKE_HPP
