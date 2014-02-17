#ifndef DYNARRAYLIKE_HPP
#define DYNARRAYLIKE_HPP

#include <iostream>
#include <memory>
#include <initializer_list>
#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>

// waiting for C++14 or C++1y dynarrays
// no magic here, just a fix runtime sized array on the heap
// missing allocator support

template <typename T>
class dynarray {
public:
  using value_type = T;
  using reference = T&;
  using const_reference = const T&;

  using iterator = T*;
  using const_iterator = const T*;

  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  using difference_type =  typename std::iterator_traits<iterator>::difference_type;
  using size_type = size_t;

  dynarray() : size_(0), p() {}

  explicit dynarray(size_type size) : size_(size), p(new value_type[size]) {}

  dynarray(std::initializer_list<T> x)
      : size_(x.size()), p(new value_type[x.size()])
  {
      std::move(x.begin(), x.end(), this->begin());
  }

  dynarray& operator=(std::initializer_list<T> x)
  {
      if(this->size_ != x.size()) {
          this->p.reset(new value_type[x.size()]);
      }
      this->size_ = x.size();
      std::move(x.begin(), x.end(), this->begin());
      return *this;
  }

  dynarray(dynarray&& x)
      : size_(x.size_), p(std::move(x.p))
  {}

  dynarray& operator=(dynarray&& x)
  {
      this->p = std::move(x.p);
      this->size_ = x.size_;
      return *this;
  }

  dynarray(const dynarray& x)
      : size_(x.size_), p(new value_type[x.size_])
  {
      std::copy(x.cbegin(), x.cend(), this->begin());
  }

  dynarray& operator=(const dynarray& x)
  {
      if(this->size_ != x.size_) {
        this->p.reset(new value_type[x.size_]);
      }
      this->size_ = x.size_;
      std::move(x.cbegin(), x.cend(), this->begin());
      return *this;
  }

  inline iterator begin() {return this->p.get();}
  inline iterator end() {return this->p.get()+this->size_;}

  inline const_iterator begin() const {return this->p.get();}
  inline const_iterator end() const {return this->p.get()+this->size_;}

  inline const_iterator cbegin() const {return this->p.get();}
  inline const_iterator cend() const {return this->p.get()+this->size_;}

  inline reverse_iterator rbegin() {return reverse_iterator(end());}
  inline reverse_iterator rend() {return reverse_iterator(begin());}

  inline const_reverse_iterator rbegin() const {return const_reverse_iterator(end());}
  inline const_reverse_iterator rend() const {return const_reverse_iterator(begin());}

  inline const_reverse_iterator crbegin() const {return const_reverse_iterator(cend());}
  inline const_reverse_iterator crend() const {return const_reverse_iterator(cbegin());}

  inline T& operator[](size_type pos) {return *(this->p.get()+pos);}
  inline const T& operator[](size_type pos) const {return *(this->p.get()+pos);}

  T& at(size_type pos)
  {
       if (!(pos < size()))
           throw std::out_of_range("dynarray out of range.");
       else
           return *(this->p.get()+pos);
  }

  const T& at(size_type pos) const
  {
       if (!(pos < size()))
           throw std::out_of_range("dynarray out of range.");
       else
          return *(this->p.get()+pos);
  }

  T& front() {return *(this->p.get());}
  const T& front() const {return *(this->p.get());}

  T& back() {return *(this->p.get()+this->size_-1);}
  const T& back() const {return *(this->p.get()+this->size_-1);}

  T* data() {return this->p.get();}
  const T* data() const {return this->p.get();}

  void swap(dynarray& x) {
      this->p.swap(x.p);
      std::swap(this->size_, x.size_);
  }

  void fill( const T& value )
  {
      std::fill_n(this->begin(), this->size_, value);
  }

  size_type size() const {return size_;}
  bool empty() const {return size_ == 0;}

  constexpr static size_type max_size() {return std::numeric_limits<size_type>::max();}

private:
  size_type size_;
  std::unique_ptr<value_type[]> p;
};

template <typename T>
bool operator==(const dynarray<T>& a, const dynarray<T>& b) {
    return (a.size() == b.size()) && std::equal(a.cbegin(), a.cend(), b.cbegin());
}

template <typename T>
bool operator!=(const dynarray<T>& a, const dynarray<T>& b) {
    return !(a == b);
}

template <typename T>
bool operator<(const dynarray<T>& a, const dynarray<T>& b) {
    return std::lexicographical_compare(a.cbegin(), a.cend(), b.cbegin(), b.cend());
}

template <typename T>
bool operator>(const dynarray<T>& a, const dynarray<T>& b) {
    return b < a;
}

template <typename T>
bool operator<=(const dynarray<T>& a, const dynarray<T>& b) {
    return !(a > b);
}

template <typename T>
bool operator>=(const dynarray<T>& a, const dynarray<T>& b) {
    return !(a < b);
}

template <typename T>
void swap(dynarray<T>& a, dynarray<T>& b) {
    a.swap(b);
}

template <typename T>
dynarray<T> make_dynarray(std::initializer_list<T> init)
{
    return dynarray<T>(std::move(init));
}

template <typename T>
dynarray<T> make_dynarray(size_t size)
{
    return dynarray<T>(size);
}

#endif // DYNARRAYLIKE_HPP
