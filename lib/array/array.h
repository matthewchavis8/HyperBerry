/**
 * @file array.h
 * @brief Freestanding fixed-size array container.
 * @ingroup lib
 *
 * Provides @c hv::array<T,N>, a minimal replacement for @c std::array
 * that compiles without a hosted C++ standard library.
 */

#ifndef __ARRAY_H__
#define __ARRAY_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus

namespace hv {

/**
 * @brief Fixed-size aggregate array with STL-compatible interface.
 * @ingroup lib
 *
 * @tparam T  Element type.
 * @tparam N  Number of elements (compile-time constant).
 *
 * @note This is an aggregate -- it has no user-declared constructors,
 *       so brace initialization and zero-initialization work as expected.
 */
template <typename T, size_t N>
struct array {
  T m_data[N];  /**< Raw storage for @p N elements of type @p T. */

  using value_type      = T;
  using size_type       = size_t;
  using difference_type = ptrdiff_t;
  using reference       = T&;
  using const_reference = const T&;
  using pointer         = T*;
  using const_pointer   = const T*;
  using iterator        = T*;
  using const_iterator  = const T*;

  /** @brief Access element at index @p i (no bounds checking). */
  constexpr reference operator[](size_type i) { return m_data[i]; }
  constexpr const_reference operator[](size_type i) const { return m_data[i]; }

  /** @brief Return reference to the first element. */
  constexpr reference front() { return m_data[0]; }
  constexpr const_reference front() const { return m_data[0]; }

  /** @brief Return reference to the last element. */
  constexpr reference back() { return m_data[N - 1]; }
  constexpr const_reference back() const { return m_data[N - 1]; }

  /** @brief Return pointer to contiguous underlying storage. */
  constexpr pointer data() { return m_data; }
  constexpr const_pointer data() const { return m_data; }

  /** @brief Return the number of elements. */
  constexpr size_type size() const { return N; }
  /** @brief Return the maximum number of elements (same as @ref size). */
  constexpr size_type max_size() const { return N; }
  /** @brief Return true when @p N is zero. */
  constexpr bool empty() const { return N == 0; }

  /** @brief Return iterator to the first element. */
  constexpr iterator begin() { return m_data; }
  constexpr const_iterator begin() const { return m_data; }
  /** @brief Return const iterator to the first element. */
  constexpr const_iterator cbegin() const { return m_data; }

  /** @brief Return iterator one past the last element. */
  constexpr iterator end() { return m_data + N; }
  constexpr const_iterator end() const { return m_data + N; }
  /** @brief Return const iterator one past the last element. */
  constexpr const_iterator cend() const { return m_data + N; }

  /** @brief Assign @p value to every element in the array. */
  constexpr void fill(const T& value) {
    for (size_type i = 0; i < N; ++i) {
      m_data[i] = value;
    }
  }
};

}

#endif // __cplusplus
#endif // __ARRAY_H__
