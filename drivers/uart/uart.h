/**
 * @file uart.h
 * @brief PL011 UART driver for serial I/O.
 *
 * Provides transmit and receive functionality over the PL011 UART peripheral
 * for both QEMU virt and Raspberry Pi 5 hardware targets. The MMIO base
 * address is selected from the active BSP definition.
 */

#ifndef __UART_H__
#define __UART_H__

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Uppercase hexadecimal digit lookup table used by writeHex().
 * @ingroup drivers_uart
 */
inline constexpr char hex[] = "0123456789ABCDEF";

namespace uart::detail {

template<typename T>
struct AlwaysFalse {
    static constexpr bool kValue = false;
};

template<typename T>
inline constexpr bool kIsPointer = false;
template<typename T>
inline constexpr bool kIsPointer<T*> = true;

template<typename T>
inline constexpr bool kIsSigned = static_cast<T>(-1) < static_cast<T>(0);

template<typename Writer>
inline void writeCString(Writer&& writer, const char* str) {
    if (str == nullptr) {
        str = "(null)";
    }

    while (*str != '\0') {
        writer(*str++);
    }
}

template<typename Writer>
inline void writeUnsignedDecimal(Writer&& writer, uint64_t value) {
    if (value == 0U) {
        writer('0');
        return;
    }

    char buffer[20];
    size_t len = 0;

    while (value != 0U) {
        buffer[len++] = static_cast<char>('0' + (value % 10U));
        value /= 10U;
    }

    while (len > 0U) {
        writer(buffer[--len]);
    }
}

template<typename Writer>
inline void writeSignedDecimal(Writer&& writer, int64_t value) {
    if (value < 0) {
        writer('-');
        uint64_t magnitude = static_cast<uint64_t>(-(value + 1)) + 1U;
        writeUnsignedDecimal(writer, magnitude);
        return;
    }

    writeUnsignedDecimal(writer, static_cast<uint64_t>(value));
}

template<typename Writer>
inline void writeUnsignedHex(Writer&& writer, uint64_t value) {
    writer('0');
    writer('x');

    if (value == 0U) {
        writer('0');
        return;
    }

    char buffer[16];
    size_t len = 0;

    while (value != 0U) {
        buffer[len++] = hex[value & 0xFU];
        value >>= 4U;
    }

    while (len > 0U) {
        writer(buffer[--len]);
    }
}

template<typename Writer, typename T>
inline void writeValue(Writer&& writer, T value) {
    if constexpr (__is_same(T, decltype(nullptr)))
      writeUnsignedHex(writer, 0U);
    else if constexpr (__is_same(T, const char*) || __is_same(T, char*))
      writeCString(writer, value);
    else if constexpr (kIsPointer<T>)
      writeUnsignedHex(writer, reinterpret_cast<uint64_t>(value));
    else if constexpr (__is_same(T, bool))
      writeCString(writer, value ? "true" : "false");
    else if constexpr (__is_same(T, char))
        writer(value);
    else if constexpr (__is_integral(T)) {
        if constexpr (kIsSigned<T>)
          writeSignedDecimal(writer, static_cast<int64_t>(value));
        else
          writeUnsignedDecimal(writer, static_cast<uint64_t>(value));
    }
    else
      static_assert(AlwaysFalse<T>::kValue, "Unsupported UART format type");
}

enum class FormatStep : uint8_t {
    End,
    Placeholder,
    Invalid,
};

enum class FormatSpec : uint8_t {
    Default,
    Hex,
};

struct FormatResult {
    FormatStep step;
    FormatSpec spec;
};

template<typename Writer>
inline FormatResult writeUntilPlaceholder(Writer&& writer, const char*& fmt) {
    if (fmt == nullptr) {
        writeCString(writer, "(null)");
        return {FormatStep::End, FormatSpec::Default};
    }

    while (*fmt != '\0') {
        if (*fmt == '{') {
            if (fmt[1] == '{') {
                writer('{');
                fmt += 2;
                continue;
            }

            if (fmt[1] == '}') {
                fmt += 2;
                return {FormatStep::Placeholder, FormatSpec::Default};
            }

            if (fmt[1] == ':' && (fmt[2] == 'x' || fmt[2] == 'X') && fmt[3] == '}') {
                fmt += 4;
                return {FormatStep::Placeholder, FormatSpec::Hex};
            }

            writeCString(writer, "[invalid format]");
            return {FormatStep::Invalid, FormatSpec::Default};
        }

        if (*fmt == '}') {
            if (fmt[1] == '}') {
                writer('}');
                fmt += 2;
                continue;
            }

            writeCString(writer, "[invalid format]");
            return {FormatStep::Invalid, FormatSpec::Default};
        }

        writer(*fmt++);
    }

    return {FormatStep::End, FormatSpec::Default};
}

template<typename Writer, typename T>
inline void writeHexValue(Writer&& writer, T value) {
    if constexpr (__is_same(T, decltype(nullptr))) {
        writeUnsignedHex(writer, 0U);
    } else if constexpr (kIsPointer<T>) {
        writeUnsignedHex(writer, reinterpret_cast<uint64_t>(value));
    } else if constexpr (__is_same(T, bool)) {
        writeUnsignedHex(writer, value ? 1U : 0U);
    } else if constexpr (__is_same(T, char)) {
        writeUnsignedHex(writer, static_cast<uint64_t>(static_cast<unsigned char>(value)));
    } else if constexpr (__is_integral(T)) {
        writeUnsignedHex(writer, static_cast<uint64_t>(value));
    } else {
        static_assert(AlwaysFalse<T>::kValue, "Unsupported UART hex format type");
    }
}

template<typename Writer, typename T>
inline void writeFormattedValue(Writer&& writer, FormatSpec spec, T value) {
    if (spec == FormatSpec::Hex) {
        if constexpr (__is_enum(T)) {
            writeHexValue(writer, static_cast<__underlying_type(T)>(value));
        } else {
            writeHexValue(writer, value);
        }
        return;
    }

    if constexpr (__is_enum(T)) {
        writeSignedDecimal(writer, static_cast<int64_t>(value));
    } else {
        writeValue(writer, value);
    }
}

template<typename Writer>
inline void formatToSink(Writer&& writer, const char* fmt) {
    if (writeUntilPlaceholder(writer, fmt).step == FormatStep::Placeholder) {
        writeCString(writer, "[missing arg]");
    }
}

template<typename Writer, typename T, typename... Rest>
inline void formatToSink(Writer&& writer, const char* fmt, T value, Rest... rest) {
    const FormatResult result = writeUntilPlaceholder(writer, fmt);

    if (result.step == FormatStep::Placeholder) {
        writeFormattedValue(writer, result.spec, value);
        formatToSink(writer, fmt, rest...);
        return;
    }

    if (result.step == FormatStep::End) {
        writeCString(writer, "[extra arg]");
    }
}

} // namespace uart::detail

/**
 * @brief PL011 register offsets from @ref b::UART_BASE.
 * @ingroup drivers_uart
 */
enum class UART_REG : uint8_t {
    DR   = 0x00,
    FR   = 0x18,
    IBRD = 0x24,
    FBRD = 0x28,
    LCRH = 0x2C,
    CR   = 0x30,
    ICR  = 0x44,
};

/**
 * @class Uart
 * @ingroup drivers_uart
 * @brief Static PL011 UART driver for early serial I/O.
 *
 */
class Uart {
  private:
    /**
     * @brief returns the register address for UART PL011
     *
     * @param reg PL011 register offset.
     * @return Pointer to the memory-mapped PL011 register.
     *
     * */
    static volatile uint32_t* reg(UART_REG reg);

  public:
    /**
     * @brief Initialize the PL011 UART.
     *
     * @note Must be called before print().
     */
    static void init();

    /**
     * @brief Transmit a null-terminated string.
     * @param str Pointer to the null-terminated string to send.
     * @note Each character is sent via putc(), which spins on the
     *       TX FIFO. For large strings this blocks proportionally.
     */
    static void println(const char* str);

    template<typename... Args>
    static void println(const char* fmt, Args... args) {
        print(fmt, args...);
        putc('\r');
        putc('\n');
    }
    
    /**
     * @brief Transmit a null-terminated string.
     * @param str Pointer to the null-terminated string to send.
     * @note Each character is sent via putc(), which spins on the
     *       TX FIFO. For large strings this blocks proportionally.
     */
    static void print(const char* str);

    template<typename... Args>
    static void print(const char* fmt, Args... args) {
        uart::detail::formatToSink([](char ch) { Uart::putc(ch); }, fmt, args...);
    }

    /**
     * @brief Transmit a single character over the UART.
     * @param ch Character to send.
     * @warning Busy-waits until the TX FIFO has space. Do not call
     *          from an interrupt context or time-critical path.
     */
    static void putc(const char ch);

    /**
     * @brief Write a 64-bit value as a 16-digit hexadecimal string.
     * @param val  The value to print.
     * @note Always emits exactly 16 hex digits (zero-padded).
     *       Does not print a "0x" prefix -- callers must add it
     *       themselves.
     */
    static void writeHex(uint64_t val);
};

#endif // !__UART_H__
