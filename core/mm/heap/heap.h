/**
 * @file heap.h
 * @brief PMM-backed kernel heap lifecycle.
 * @ingroup mm
 *
 * The kernel heap services dynamic C++ object allocation through the
 * standard global @c new and @c delete operators. There is intentionally
 * no public @c hmalloc/@c hfree surface: dynamic memory always flows
 * through C++ object lifetime semantics or the smart pointer factories.
 *
 * The heap must be initialised after @ref pmm and before the host
 * stage-1 MMU is enabled. Allocation before initialisation panics.
 *
 * @note The heap is not interrupt-safe in this initial implementation.
 */
// TODO(IRQ): make heap allocation interrupt-safe once IRQ handling and
// locking policy mature.
// TODO(SMP): make slab/large allocator paths SMP-safe once HyperBerry
// enables multi-core.

#ifndef __HEAP_H__
#define __HEAP_H__

namespace hv {
namespace heap {

/**
 * @brief Initialise the kernel heap.
 * @ingroup mm
 *
 * Must be called once during boot, after @c pmm::init() and before any
 * code path that allocates dynamic objects. Subsequent calls re-arm the
 * heap; the existing slab/large bookkeeping is reset.
 */
void init();

}
}

#endif // __HEAP_H__
