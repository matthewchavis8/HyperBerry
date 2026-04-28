/**
 * @file unique_ptr.h
 * @brief Freestanding object-owning smart pointer with unique ownership.
 * @ingroup lib
 *
 * Provides @c hv::unique_ptr, a minimal RAII wrapper around a raw pointer
 * that runs the configured deleter exactly once when the wrapper is
 * destroyed. Behaves like @c std::unique_ptr for single objects with a
 * deliberately reduced surface: no array specialization, no cross-type
 * conversions, and no nothrow allocation paths.
 */

#ifndef __UNIQUE_PTR_H__
#define __UNIQUE_PTR_H__

#include <stddef.h>

#include "lib/utility/utility.h"

namespace hv {

/**
 * @brief Default deleter invoking the global @c delete expression.
 * @ingroup lib
 */
template <typename T>
struct defaultDelete {
  constexpr defaultDelete() noexcept = default;
  void operator()(T* p) const noexcept { delete p; }
};

/**
 * @brief RAII owner of a single heap-allocated object.
 * @ingroup lib
 *
 * @tparam T Pointee type. Array specialization is intentionally omitted.
 * @tparam Deleter Stateless or stateful deleter invoked on the owned pointer.
 */
template <typename T, typename Deleter = defaultDelete<T>>
class unique_ptr {
 private:
  T*      m_ptr     {nullptr};
  Deleter m_deleter {};

 public:
  using pointer       = T*;
  using element_type  = T;
  using deleter_type  = Deleter;

  /** @brief Construct an empty owner. */
  constexpr unique_ptr() noexcept = default;

  /** @brief Construct an empty owner from @c nullptr. */
  constexpr unique_ptr(decltype(nullptr)) noexcept {}

  /** @brief Adopt ownership of @p p. */
  explicit unique_ptr(pointer p) noexcept : m_ptr(p) {}

  /** @brief Adopt ownership of @p p with a custom deleter. */
  unique_ptr(pointer p, Deleter d) noexcept : m_ptr(p), m_deleter(hv::move(d)) {}

  unique_ptr(const unique_ptr&)            = delete;
  unique_ptr& operator=(const unique_ptr&) = delete;

  /** @brief Transfer ownership from @p other. */
  unique_ptr(unique_ptr&& other) noexcept
      : m_ptr(other.m_ptr), m_deleter(hv::move(other.m_deleter)) {
    other.m_ptr = nullptr;
  }

  /** @brief Transfer ownership from @p other, releasing the current object. */
  unique_ptr& operator=(unique_ptr&& other) noexcept {
    if (this != &other) {
      reset(other.m_ptr);
      other.m_ptr = nullptr;
      m_deleter   = hv::move(other.m_deleter);
    }
    return *this;
  }

  /** @brief Assigning @c nullptr releases the owned object. */
  unique_ptr& operator=(decltype(nullptr)) noexcept {
    reset();
    return *this;
  }

  ~unique_ptr() { reset(); }

  /** @brief Return the owned raw pointer without releasing ownership. */
  pointer get() const noexcept { return m_ptr; }

  /** @brief Access the deleter object. */
  Deleter&       get_deleter() noexcept       { return m_deleter; }
  const Deleter& get_deleter() const noexcept { return m_deleter; }

  /** @brief @c true when the wrapper currently owns an object. */
  explicit operator bool() const noexcept { return m_ptr != nullptr; }

  /** @brief Dereference the owned object. */
  T& operator*()  const noexcept { return *m_ptr; }
  /** @brief Member access on the owned object. */
  T* operator->() const noexcept { return  m_ptr; }

  /**
   * @brief Relinquish ownership and return the raw pointer.
   *
   * The deleter is *not* invoked; the caller assumes responsibility.
   */
  pointer release() noexcept {
    pointer p = m_ptr;
    m_ptr     = nullptr;
    return p;
  }

  /**
   * @brief Replace the owned pointer, deleting any previous object.
   * @param p New raw pointer to own (defaults to @c nullptr).
   */
  void reset(pointer p = nullptr) noexcept {
    pointer old = m_ptr;
    m_ptr       = p;
    if (old)
      m_deleter(old);
  }

  /** @brief Swap pointers and deleters with @p other. */
  void swap(unique_ptr& other) noexcept {
    hv::swap(m_ptr,     other.m_ptr);
    hv::swap(m_deleter, other.m_deleter);
  }
};

template <typename T, typename D>
inline bool operator==(const unique_ptr<T, D>& a, const unique_ptr<T, D>& b) noexcept {
  return a.get() == b.get();
}
template <typename T, typename D>
inline bool operator!=(const unique_ptr<T, D>& a, const unique_ptr<T, D>& b) noexcept {
  return a.get() != b.get();
}
template <typename T, typename D>
inline bool operator==(const unique_ptr<T, D>& a, decltype(nullptr)) noexcept {
  return a.get() == nullptr;
}
template <typename T, typename D>
inline bool operator==(decltype(nullptr), const unique_ptr<T, D>& a) noexcept {
  return a.get() == nullptr;
}
template <typename T, typename D>
inline bool operator!=(const unique_ptr<T, D>& a, decltype(nullptr)) noexcept {
  return a.get() != nullptr;
}
template <typename T, typename D>
inline bool operator!=(decltype(nullptr), const unique_ptr<T, D>& a) noexcept {
  return a.get() != nullptr;
}

/**
 * @brief Construct a @c T in-place and return a @c unique_ptr that owns it.
 * @ingroup lib
 *
 * Array specializations are intentionally not provided.
 */
template <typename T, typename... Args>
inline unique_ptr<T> make_unique(Args&&... args) {
  return unique_ptr<T>(new T(hv::forward<Args>(args)...));
}

}

#endif // __UNIQUE_PTR_H__
