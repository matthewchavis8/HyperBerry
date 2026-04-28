/**
 * @file utility.h
 * @brief Freestanding move/forward primitives and minimal type traits.
 * @ingroup lib
 *
 * Provides the small subset of @c <utility> and @c <type_traits> that the
 * hypervisor's smart pointers and containers need, without relying on a
 * hosted C++ standard library.
 */

#ifndef __UTILITY_H__
#define __UTILITY_H__

#include <stddef.h>

#ifdef __cplusplus

namespace hv {

/** @brief Strip a reference qualifier from @p T. */
template <typename T> struct removeReference        { using type = T; };
template <typename T> struct removeReference<T&>    { using type = T; };
template <typename T> struct removeReference<T&&>   { using type = T; };

template <typename T>
using removeReferenceT = typename removeReference<T>::type;

/** @brief Cast @p t to an rvalue reference, enabling move semantics. */
template <typename T>
constexpr removeReferenceT<T>&& move(T&& t) noexcept {
  return static_cast<removeReferenceT<T>&&>(t);
}

/**
 * @brief Forward an lvalue argument while preserving its value category.
 * @ingroup lib
 */
template <typename T>
constexpr T&& forward(removeReferenceT<T>& t) noexcept {
  return static_cast<T&&>(t);
}

/**
 * @brief Forward an rvalue argument while preserving its value category.
 * @ingroup lib
 */
template <typename T>
constexpr T&& forward(removeReferenceT<T>&& t) noexcept {
  return static_cast<T&&>(t);
}

/** @brief Swap two values using move semantics. */
template <typename T>
constexpr void swap(T& a, T& b) noexcept {
  T tmp = hv::move(a);
  a     = hv::move(b);
  b     = hv::move(tmp);
}

}

#endif // __cplusplus
#endif // __UTILITY_H__
