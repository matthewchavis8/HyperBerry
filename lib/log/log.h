// @file log.h
// @brief Console logging and brace-style formatting.
// @ingroup lib
//
// Owns the format engine and the console API. The UART driver underneath it
// only knows how to push a character; everything about how a line is built
// lives here.

#ifndef __LOG_H__
#define __LOG_H__

#include <stddef.h>
#include <stdint.h>

namespace log::detail {

// @brief Uppercase hexadecimal digit lookup table.
// @ingroup lib
inline constexpr char hex[] = "0123456789ABCDEF";


template <typename T>
struct AlwaysFalse {
    static constexpr bool kValue = false;
};

template <typename T>
inline constexpr bool kIsPointer = false;
template <typename T>
inline constexpr bool kIsPointer<T*> = true;

template <typename T>
inline constexpr bool kIsSigned = static_cast<T>(-1) < static_cast<T>(0);

template <typename Writer>
inline void writeCString(Writer&& writer, const char* str) {
    if (str == nullptr) {
        str = "(null)";
    }

    while (*str != '\0') {
        writer(*str++);
    }
}

template <typename Writer>
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

template <typename Writer>
inline void writeSignedDecimal(Writer&& writer, int64_t value) {
    if (value < 0) {
        writer('-');
        uint64_t magnitude = static_cast<uint64_t>(-(value + 1)) + 1U;
        writeUnsignedDecimal(writer, magnitude);
        return;
    }

    writeUnsignedDecimal(writer, static_cast<uint64_t>(value));
}

template <typename Writer>
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

template <typename Writer, typename T>
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
    } else
        static_assert(AlwaysFalse<T>::kValue, "Unsupported log format type");
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

template <typename Writer>
inline FormatResult writeUntilPlaceholder(Writer&& writer, const char*& fmt) {
    if (fmt == nullptr) {
        writeCString(writer, "(null)");
        return { FormatStep::End, FormatSpec::Default };
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
                return { FormatStep::Placeholder, FormatSpec::Default };
            }

            if (fmt[1] == ':' && (fmt[2] == 'x' || fmt[2] == 'X') && fmt[3] == '}') {
                fmt += 4;
                return { FormatStep::Placeholder, FormatSpec::Hex };
            }

            writeCString(writer, "[invalid format]");
            return { FormatStep::Invalid, FormatSpec::Default };
        }

        if (*fmt == '}') {
            if (fmt[1] == '}') {
                writer('}');
                fmt += 2;
                continue;
            }

            writeCString(writer, "[invalid format]");
            return { FormatStep::Invalid, FormatSpec::Default };
        }

        writer(*fmt++);
    }

    return { FormatStep::End, FormatSpec::Default };
}

template <typename Writer, typename T>
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
        static_assert(AlwaysFalse<T>::kValue, "Unsupported log hex format type");
    }
}

template <typename Writer, typename T>
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

template <typename Writer>
inline void formatToSink(Writer&& writer, const char* fmt) {
    if (writeUntilPlaceholder(writer, fmt).step == FormatStep::Placeholder) {
        writeCString(writer, "[missing arg]");
    }
}

template <typename Writer, typename T, typename... Rest>
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

} // namespace log::detail

// @class Log
// @ingroup lib
// @brief Console log sink. Formats values and hands characters to the UART.
//
// Two tiers, split at compile time rather than by severity:
//
//   println/print  the debug console. Compiled to nothing when NDEBUG is set.
//                  Inline on purpose, so the call and its format string both
//                  vanish from a release image instead of becoming a call to
//                  an empty function that still pins the literal in .rodata.
//
//   write/writeLine/writeCh/writeHex
//                  always emitted. The panic path and the integration
//                  harness ride these, and a release build that panics
//                  silently is a release build you cannot debug.
class Log {
private:
    // @brief Character sink the formatter writes through.
    //
    // The single place this class touches the driver.
    // @param ch Character to hand to the UART driver.
    // @return Nothing.
    static void sink(const char ch);

public:
    // @brief Emit a null-terminated string followed by CRLF. Always emitted.
    // @param str Pointer to the null-terminated string to send.
    // @return Nothing.
    static void writeLine(const char* str);

    template <typename... Args>
    static void writeLine(const char* fmt, Args... args) {
        write(fmt, args...);
        writeCh('\r');
        writeCh('\n');
    }

    // @brief Emit a null-terminated string. Always emitted.
    // @param str Pointer to the null-terminated string to send.
    // @return Nothing.
    static void write(const char* str);

    template <typename... Args>
    static void write(const char* fmt, Args... args) {
        log::detail::formatToSink([](char ch) { Log::sink(ch); }, fmt, args...);
    }

    // @brief Emit a single character. Always emitted.
    // @param ch Character to send.
    // @return Nothing.
    static void writeCh(const char ch);

    // @brief Write a 64-bit value as a 16-digit hexadecimal string.
    // @param val The value to print.
    // @note Always emits exactly 16 hex digits (zero-padded). Does not print a
    //       "0x" prefix -- callers must add it themselves.
    // @return Nothing.
    static void writeHex(uint64_t val);

    // @brief Debug console line. Compiled out when NDEBUG is set.
    // @param str Pointer to the null-terminated string to send.
    // @return Nothing.
    static void println([[maybe_unused]] const char* str) {
#ifndef NDEBUG
        writeLine(str);
#endif
    }

    template <typename... Args>
    static void println([[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) {
#ifndef NDEBUG
        writeLine(fmt, args...);
#endif
    }

    // @brief Debug console text, no line ending. Compiled out when NDEBUG is set.
    // @param str Pointer to the null-terminated string to send.
    // @return Nothing.
    static void print([[maybe_unused]] const char* str) {
#ifndef NDEBUG
        write(str);
#endif
    }

    template <typename... Args>
    static void print([[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) {
#ifndef NDEBUG
        write(fmt, args...);
#endif
    }
};

#endif // !__LOG_H__
