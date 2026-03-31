#ifndef __ARRAY_H__
#define __ARRAY_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus

namespace hv {

template <typename T, size_t N>
struct array {
  T m_data[N];

  using value_type      = T;
  using size_type       = size_t;
  using difference_type = ptrdiff_t;
  using reference       = T&;
  using const_reference = const T&;
  using pointer         = T*;
  using const_pointer   = const T*;
  using iterator        = T*;
  using const_iterator  = const T*;

  constexpr reference operator[](size_type i) { return m_data[i]; }
  constexpr const_reference operator[](size_type i) const { return m_data[i]; }

  constexpr reference front() { return m_data[0]; }
  constexpr const_reference front() const { return m_data[0]; }

  constexpr reference back() { return m_data[N - 1]; }
  constexpr const_reference back() const { return m_data[N - 1]; }

  constexpr pointer data() { return m_data; }
  constexpr const_pointer data() const { return m_data; }

  constexpr size_type size() const { return N; }
  constexpr size_type max_size() const { return N; }
  constexpr bool empty() const { return N == 0; }

  constexpr iterator begin() { return m_data; }
  constexpr const_iterator begin() const { return m_data; }
  constexpr const_iterator cbegin() const { return m_data; }

  constexpr iterator end() { return m_data + N; }
  constexpr const_iterator end() const { return m_data + N; }
  constexpr const_iterator cend() const { return m_data + N; }

  constexpr void fill(const T& value) {
    for (size_type i = 0; i < N; ++i) {
      m_data[i] = value;
    }
  }
};

}

#endif // __cplusplus
#endif // __ARRAY_H__
