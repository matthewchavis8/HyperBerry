/**
 * @file shared_ptr.h
 * @brief Freestanding minimal shared-ownership smart pointer.
 * @ingroup lib
 *
 * Provides @c hv::shared_ptr, a deliberately reduced shared owner that can
 * only be created through @c hv::make_shared. The control block and the
 * managed object are co-allocated to avoid a second allocation and to keep
 * the lifetime of the control block trivially tied to the object.
 *
 * @note The reference count is non-atomic. HyperBerry currently runs on a
 *       single core and the heap is not interrupt-safe. When SMP support
 *       lands, the refcount must be made SMP-safe.
 */

// TODO(SMP): make refcount SMP-safe once HyperBerry enables multi-core.

#ifndef __SHARED_PTR_H__
#define __SHARED_PTR_H__

#include <stddef.h>

#include "lib/utility/utility.h"

namespace hv {

template <typename T> class shared_ptr;
template <typename T, typename... Args> shared_ptr<T> make_shared(Args&&... args);

namespace detail {

/**
 * @brief Combined control block and storage produced by @ref make_shared.
 * @ingroup lib
 */
template <typename T>
struct sharedBlock {
  size_t m_strongCount;
  T      m_object;

  template <typename... Args>
  explicit sharedBlock(Args&&... args)
      : m_strongCount(1), m_object(hv::forward<Args>(args)...) {}
};

}

/**
 * @brief Shared ownership smart pointer with a non-atomic refcount.
 * @ingroup lib
 *
 * Construction from a raw pointer, custom deleters, weak references,
 * aliasing, and cross-type conversions are intentionally omitted in this
 * first version. See the kernel-heap-and-smart-pointers PRD.
 */
template <typename T>
class shared_ptr {
 private:
  using Block = detail::sharedBlock<T>;

  Block* m_block {nullptr};

  explicit shared_ptr(Block* b) noexcept : m_block(b) {}

  void retain() noexcept {
    if (m_block != nullptr)
      ++m_block->m_strongCount;
  }

  void release() noexcept {
    if (m_block == nullptr)
      return;
    if (--m_block->m_strongCount == 0)
      delete m_block;
    m_block = nullptr;
  }

  template <typename U, typename... Args>
  friend shared_ptr<U> make_shared(Args&&... args);

 public:
  using element_type = T;

  /** @brief Construct an empty shared owner. */
  constexpr shared_ptr() noexcept = default;
  /** @brief Construct an empty shared owner from @c nullptr. */
  constexpr shared_ptr(decltype(nullptr)) noexcept {}

  /** @brief Share ownership with @p other. */
  shared_ptr(const shared_ptr& other) noexcept : m_block(other.m_block) { retain(); }

  /** @brief Take ownership from @p other, leaving it empty. */
  shared_ptr(shared_ptr&& other) noexcept : m_block(other.m_block) {
    other.m_block = nullptr;
  }

  /** @brief Share ownership with @p other, releasing the current reference. */
  shared_ptr& operator=(const shared_ptr& other) noexcept {
    if (this != &other) {
      release();
      m_block = other.m_block;
      retain();
    }
    return *this;
  }

  /** @brief Take ownership from @p other, releasing the current reference. */
  shared_ptr& operator=(shared_ptr&& other) noexcept {
    if (this != &other) {
      release();
      m_block       = other.m_block;
      other.m_block = nullptr;
    }
    return *this;
  }

  /** @brief Drop the strong reference; deletes the object if last. */
  ~shared_ptr() { release(); }

  /** @brief Drop the strong reference and become empty. */
  void reset() noexcept { release(); }

  /** @brief Number of @c shared_ptr instances currently sharing ownership. */
  size_t use_count() const noexcept {
    return m_block ? m_block->m_strongCount : 0;
  }

  /** @brief Raw pointer to the managed object, or @c nullptr if empty. */
  T* get() const noexcept {
    return m_block ? &m_block->m_object : nullptr;
  }

  /** @brief @c true when the pointer currently shares ownership of an object. */
  explicit operator bool() const noexcept { return m_block != nullptr; }

  T& operator*()  const noexcept { return m_block->m_object;  }
  T* operator->() const noexcept { return &m_block->m_object; }
};

template <typename T>
inline bool operator==(const shared_ptr<T>& a, const shared_ptr<T>& b) noexcept {
  return a.get() == b.get();
}
template <typename T>
inline bool operator!=(const shared_ptr<T>& a, const shared_ptr<T>& b) noexcept {
  return a.get() != b.get();
}
template <typename T>
inline bool operator==(const shared_ptr<T>& a, decltype(nullptr)) noexcept {
  return a.get() == nullptr;
}
template <typename T>
inline bool operator==(decltype(nullptr), const shared_ptr<T>& a) noexcept {
  return a.get() == nullptr;
}
template <typename T>
inline bool operator!=(const shared_ptr<T>& a, decltype(nullptr)) noexcept {
  return a.get() != nullptr;
}
template <typename T>
inline bool operator!=(decltype(nullptr), const shared_ptr<T>& a) noexcept {
  return a.get() != nullptr;
}

/**
 * @brief Construct a @c T in-place inside a shared control block.
 * @ingroup lib
 *
 * The control block and the object are allocated together, so the entire
 * managed lifetime requires a single allocation.
 */
template <typename T, typename... Args>
inline shared_ptr<T> make_shared(Args&&... args) {
  using Block = detail::sharedBlock<T>;
  return shared_ptr<T>(new Block(hv::forward<Args>(args)...));
}

}

#endif // __SHARED_PTR_H__
