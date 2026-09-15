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
inline void WriteCString(Writer&& writer, const char* str) {
    if (str == nullptr) {
        str = "(null)";
    }

    while (*str != '\0') {
        writer(*str++);
    }
}

template <typename Writer>
inline void WriteUnsignedDecimal(Writer&& writer, uint64_t value) {
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
inline void WriteSignedDecimal(Writer&& writer, int64_t value) {
    if (value < 0) {
        writer('-');
        uint64_t magnitude = static_cast<uint64_t>(-(value + 1)) + 1U;
        WriteUnsignedDecimal(writer, magnitude);
        return;
    }

    WriteUnsignedDecimal(writer, static_cast<uint64_t>(value));
}

template <typename Writer>
inline void WriteUnsignedHex(Writer&& writer, uint64_t value) {
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
inline void WriteValue(Writer&& writer, T value) {
    if constexpr (__is_same(T, decltype(nullptr)))
        WriteUnsignedHex(writer, 0U);
    else if constexpr (__is_same(T, const char*) || __is_same(T, char*))
        WriteCString(writer, value);
    else if constexpr (kIsPointer<T>)
        WriteUnsignedHex(writer, reinterpret_cast<uint64_t>(value));
    else if constexpr (__is_same(T, bool))
        WriteCString(writer, value ? "true" : "false");
    else if constexpr (__is_same(T, char))
        writer(value);
    else if constexpr (__is_integral(T)) {
        if constexpr (kIsSigned<T>)
            WriteSignedDecimal(writer, static_cast<int64_t>(value));
        else
            WriteUnsignedDecimal(writer, static_cast<uint64_t>(value));
    } else
        static_assert(AlwaysFalse<T>::kValue, "Unsupported log format type");
}

enum class FormatStep : uint8_t {
    END,
    PLACEHOLDER,
    INVALID,
};

enum class FormatSpec : uint8_t {
    DEFAULT,
    HEX,
};

struct FormatResult {
    FormatStep step;
    FormatSpec spec;
};

template <typename Writer>
inline FormatResult WriteUntilPlaceholder(Writer&& writer, const char*& fmt) {
    if (fmt == nullptr) {
        WriteCString(writer, "(null)");
        return { FormatStep::END, FormatSpec::DEFAULT };
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
                return { FormatStep::PLACEHOLDER, FormatSpec::DEFAULT };
            }

            if (fmt[1] == ':' && (fmt[2] == 'x' || fmt[2] == 'X') && fmt[3] == '}') {
                fmt += 4;
                return { FormatStep::PLACEHOLDER, FormatSpec::HEX };
            }

            WriteCString(writer, "[invalid format]");
            return { FormatStep::INVALID, FormatSpec::DEFAULT };
        }

        if (*fmt == '}') {
            if (fmt[1] == '}') {
                writer('}');
                fmt += 2;
                continue;
            }

            WriteCString(writer, "[invalid format]");
            return { FormatStep::INVALID, FormatSpec::DEFAULT };
        }

        writer(*fmt++);
    }

    return { FormatStep::END, FormatSpec::DEFAULT };
}

template <typename Writer, typename T>
inline void WriteHexValue(Writer&& writer, T value) {
    if constexpr (__is_same(T, decltype(nullptr))) {
        WriteUnsignedHex(writer, 0U);
    } else if constexpr (kIsPointer<T>) {
        WriteUnsignedHex(writer, reinterpret_cast<uint64_t>(value));
    } else if constexpr (__is_same(T, bool)) {
        WriteUnsignedHex(writer, value ? 1U : 0U);
    } else if constexpr (__is_same(T, char)) {
        WriteUnsignedHex(writer, static_cast<uint64_t>(static_cast<unsigned char>(value)));
    } else if constexpr (__is_integral(T)) {
        WriteUnsignedHex(writer, static_cast<uint64_t>(value));
    } else {
        static_assert(AlwaysFalse<T>::kValue, "Unsupported log hex format type");
    }
}

template <typename Writer, typename T>
inline void WriteFormattedValue(Writer&& writer, FormatSpec spec, T value) {
    if (spec == FormatSpec::HEX) {
        if constexpr (__is_enum(T)) {
            WriteHexValue(writer, static_cast<__underlying_type(T)>(value));
        } else {
            WriteHexValue(writer, value);
        }
        return;
    }

    if constexpr (__is_enum(T)) {
        WriteSignedDecimal(writer, static_cast<int64_t>(value));
    } else {
        WriteValue(writer, value);
    }
}

template <typename Writer>
inline void FormatToSink(Writer&& writer, const char* fmt) {
    if (WriteUntilPlaceholder(writer, fmt).step == FormatStep::PLACEHOLDER) {
        WriteCString(writer, "[missing arg]");
    }
}

template <typename Writer, typename T, typename... Rest>
inline void FormatToSink(Writer&& writer, const char* fmt, T value, Rest... rest) {
    const FormatResult result = WriteUntilPlaceholder(writer, fmt);

    if (result.step == FormatStep::PLACEHOLDER) {
        WriteFormattedValue(writer, result.spec, value);
        FormatToSink(writer, fmt, rest...);
        return;
    }

    if (result.step == FormatStep::END) {
        WriteCString(writer, "[extra arg]");
    }
}

template <typename Writer, typename... Args>
inline void FormatLineToSink(Writer&& writer, const char* fmt, Args... args) {
    FormatToSink(writer, fmt, args...);
    writer('\r');
    writer('\n');
}

} // namespace log::detail

// @class Log
// @ingroup lib
// @brief The debug console. Formats values and hands characters to the UART.
//
// println and print are the only way to print, and both compile to nothing
// when NDEBUG is set. They are inline on purpose, so the call and its format
// string vanish from a release image instead of becoming a call to an empty
// function that still pins the literal in .rodata.
//
// The panic path does not print through Log. It formats with log::detail and
// writes straight to the UART, so a release panic still reports.
class Log {
private:
    // @brief Character sink the formatter writes through.
    //
    // The single place this class touches the driver.
    // @param ch Character to hand to the UART driver.
    // @return Nothing.
    static void sink(const char ch);

public:
    // @brief Print a string followed by CRLF. Compiled out when NDEBUG is set.
    // @param str Null terminated string, printed as is.
    // @return Nothing.
    static void Println([[maybe_unused]] const char* str) {
#ifndef NDEBUG
        log::detail::FormatLineToSink([](char ch) { Log::sink(ch); }, "{}", str);
#endif
    }

    // @brief Format a line followed by CRLF. Compiled out when NDEBUG is set.
    // @param fmt Format string.
    // @param args Values for its placeholders.
    // @return Nothing.
    template <typename... Args>
    static void Println([[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) {
#ifndef NDEBUG
        log::detail::FormatLineToSink([](char ch) { Log::sink(ch); }, fmt, args...);
#endif
    }

    // @brief Print a string with no line ending. Compiled out when NDEBUG is set.
    // @param str Null terminated string, printed as is.
    // @return Nothing.
    static void Print([[maybe_unused]] const char* str) {
#ifndef NDEBUG
        log::detail::FormatToSink([](char ch) { Log::sink(ch); }, "{}", str);
#endif
    }

    // @brief Format text with no line ending. Compiled out when NDEBUG is set.
    // @param fmt Format string.
    // @param args Values for its placeholders.
    // @return Nothing.
    template <typename... Args>
    static void Print([[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) {
#ifndef NDEBUG
        log::detail::FormatToSink([](char ch) { Log::sink(ch); }, fmt, args...);
#endif
    }
};

#endif // !__LOG_H__
