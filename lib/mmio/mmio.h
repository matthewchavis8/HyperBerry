// @file mmio.h
// @brief Typed accessors for memory-mapped device registers.
// @ingroup lib
//
// Every driver was open coding the same
// `reinterpret_cast<volatile T*>(base + offset)`. One home for it means one
// place to reason about width and ordering.
//
// @note These carry no barriers. Device-nGnRnE accesses are already ordered
//       against each other, so plain volatile matches what the drivers relied
//       on before. Ordering a device write against normal memory still needs an
//       explicit dsb at the call site.
// @warning Not for walking blobs that happen to live in normal memory, such as
//          a device tree. Those are reads of RAM, not of a device.

#ifndef __MMIO_H__
#define __MMIO_H__

#include <cstdint>

namespace mmio {

namespace detail {

    template <typename T>
    struct nonDeduced {
        using type = T;
    };

    template <typename T>
    inline constexpr bool kIsAccessWidth { __is_integral(T) &&
        (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8) };

    // A register offset is either a plain integer or an enum of one, which is
    // how every driver in the tree spells its register table.
    template <typename T>
    inline constexpr bool kIsOffset { __is_enum(T) || __is_integral(T) };

} // namespace detail

// @brief Read a device register.
// @tparam T Access width: an integral type of 1, 2, 4 or 8 bytes.
// @param addr Absolute address of the register.
// @return The value read, at width @p T.
template <typename T>
inline T Read(uintptr_t addr) {
    static_assert(detail::kIsAccessWidth<T>, "Op must be uint8_t, uint16_t, uint32_t, or uint64_t");

    return *reinterpret_cast<volatile T*>(addr);
}

// @brief Write a device register.
// @tparam T Access width: an integral type of 1, 2, 4 or 8 bytes. Never
//         deduced, so the width is always stated at the call site.
// @param addr Absolute address of the register.
// @param value Value to store.
// @return Nothing.
template <typename T>
inline void Write(uintptr_t addr, typename detail::nonDeduced<T>::type value) {
    static_assert(detail::kIsAccessWidth<T>, "Op must be uint8_t, uint16_t, uint32_t, or uint64_t");

    *reinterpret_cast<volatile T*>(addr) = value;
}

// @brief Read a device register at @p offset from a frame base.
// @tparam T Access width: an integral type of 1, 2, 4 or 8 bytes.
// @tparam Off Register offset type: an integer or an enum of one.
// @param base Base address of the register frame.
// @param offset Offset of the register within the frame.
// @return The value read, at width @p T.
template <typename T, typename Off>
inline T Read(uintptr_t base, Off offset) {
    static_assert(detail::kIsOffset<Off>, "MMIO offset must be an integer or an enum of one");

    return Read<T>(base + static_cast<uintptr_t>(offset));
}

// @brief Write a device register at @p offset from a frame base.
// @tparam T Access width: an integral type of 1, 2, 4 or 8 bytes. Never
//         deduced, so the width is always stated at the call site.
// @tparam Off Register offset type: an integer or an enum of one.
// @param base Base address of the register frame.
// @param offset Offset of the register within the frame.
// @param value Value to store.
// @return Nothing.
template <typename T, typename Off>
inline void Write(uintptr_t base, Off offset, typename detail::nonDeduced<T>::type value) {
    static_assert(detail::kIsOffset<Off>, "MMIO offset must be an integer or an enum of one");

    Write<T>(base + static_cast<uintptr_t>(offset), value);
}

} // namespace mmio

#endif // !__MMIO_H__
