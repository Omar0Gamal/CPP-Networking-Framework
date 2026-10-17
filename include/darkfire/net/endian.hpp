#pragma once

#include <bit>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace darkfire::net {

    // Compile-time swap functions for fundamental types
    template <typename T>
    [[nodiscard]] constexpr T EndianSwap(T value) noexcept {
        if constexpr (sizeof(T) == 1) {
            return value;
        } else if constexpr (sizeof(T) == 2) {
            uint16_t u = std::bit_cast<uint16_t>(value);
            u = static_cast<uint16_t>((u >> 8) | (u << 8));
            return std::bit_cast<T>(u);
        } else if constexpr (sizeof(T) == 4) {
            uint32_t u = std::bit_cast<uint32_t>(value);
            u = ((u & 0x000000FFu) << 24) |
                ((u & 0x0000FF00u) << 8)  |
                ((u & 0x00FF0000u) >> 8)  |
                ((u & 0xFF000000u) >> 24);
            return std::bit_cast<T>(u);
        } else if constexpr (sizeof(T) == 8) {
            uint64_t u = std::bit_cast<uint64_t>(value);
            u = ((u & 0x00000000000000FFull) << 56) |
                ((u & 0x000000000000FF00ull) << 40) |
                ((u & 0x0000000000FF0000ull) << 24) |
                ((u & 0x00000000FF000000ull) << 8)  |
                ((u & 0x000000FF00000000ull) >> 8)  |
                ((u & 0x0000FF0000000000ull) >> 24) |
                ((u & 0x00FF000000000000ull) >> 40) |
                ((u & 0xFF00000000000000ull) >> 56);
            return std::bit_cast<T>(u);
        } else {
            static_assert(sizeof(T) <= 8, "EndianSwap only supports types up to 8 bytes");
            return value;
        }
    }

    // Convert host to network byte order (Big-Endian)
    template <typename T>
    [[nodiscard]] constexpr T HostToNetwork(T value) noexcept {
        if constexpr (std::endian::native == std::endian::big) {
            return value;
        } else {
            return EndianSwap(value);
        }
    }

    // Convert network byte order (Big-Endian) to host
    template <typename T>
    [[nodiscard]] constexpr T NetworkToHost(T value) noexcept {
        if constexpr (std::endian::native == std::endian::big) {
            return value;
        } else {
            return EndianSwap(value);
        }
    }

    // Concept for fundamental scalar types
    template <typename T>
    concept SerializableScalar = std::is_arithmetic_v<T>;

    // Concept for enums
    template <typename T>
    concept SerializableEnum = std::is_enum_v<T>;

    // Opt-in trait for trivially copyable structs
    template <typename T>
    struct is_trivially_serializable : std::false_type {};

    template <typename T>
    inline constexpr bool is_trivially_serializable_v = is_trivially_serializable<T>::value;

    template <typename T>
    concept TriviallySerializable = std::is_trivially_copyable_v<T> && is_trivially_serializable_v<T>;

} // namespace darkfire::net

#define DARKFIRE_DECLARE_TRIVIALLY_SERIALIZABLE(Type) \
    template <> struct darkfire::net::is_trivially_serializable<Type> : std::true_type {};
