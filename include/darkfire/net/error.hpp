#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace darkfire::net {

    enum class NetErrorCode : int {
        Success = 0,
        BufferUnderflow,
        BufferOverflow,
        InvalidMagicBytes,
        InvalidChecksum,
        PacketTooLarge,
        HandshakeFailed,
        Disconnected,
        ConnectionTimedOut,
        InvalidState,
        SequenceTooOld,
        FragmentMissing,
        SerializationError,
        SocketError
    };

    class NetErrorCategory : public std::error_category {
    public:
        [[nodiscard]] const char* name() const noexcept override {
            return "darkfire.net";
        }

        [[nodiscard]] std::string message(int ev) const override {
            switch (static_cast<NetErrorCode>(ev)) {
                case NetErrorCode::Success: return "Success";
                case NetErrorCode::BufferUnderflow: return "Buffer underflow during deserialization";
                case NetErrorCode::BufferOverflow: return "Buffer overflow during serialization";
                case NetErrorCode::InvalidMagicBytes: return "Packet header has invalid magic bytes";
                case NetErrorCode::InvalidChecksum: return "Packet CRC32 checksum mismatch";
                case NetErrorCode::PacketTooLarge: return "Packet exceeds maximum allowed size";
                case NetErrorCode::HandshakeFailed: return "Authentication / handshake failed";
                case NetErrorCode::Disconnected: return "Connection is disconnected";
                case NetErrorCode::ConnectionTimedOut: return "Connection or request timed out";
                case NetErrorCode::InvalidState: return "Operation invalid in current state";
                case NetErrorCode::SequenceTooOld: return "Packet sequence number is outdated or duplicate";
                case NetErrorCode::FragmentMissing: return "Fragment missing during reassembly";
                case NetErrorCode::SerializationError: return "Serialization type constraint error";
                case NetErrorCode::SocketError: return "Underlying socket error";
                default: return "Unknown network error";
            }
        }
    };

    inline const std::error_category& GetNetErrorCategory() noexcept {
        static const NetErrorCategory category;
        return category;
    }

    inline std::error_code make_error_code(NetErrorCode e) noexcept {
        return { static_cast<int>(e), GetNetErrorCategory() };
    }

    // Lightweight Result type compatible with C++20
    template <typename T>
    class NetResult {
    public:
        NetResult(const T& value) : m_data(value) {}
        NetResult(T&& value) : m_data(std::move(value)) {}
        NetResult(NetErrorCode error) : m_data(error) {}
        NetResult(std::error_code ec) : m_data(static_cast<NetErrorCode>(ec.value())) {}

        [[nodiscard]] bool HasValue() const noexcept {
            return std::holds_alternative<T>(m_data);
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return HasValue();
        }

        [[nodiscard]] const T& Value() const {
            if (!HasValue()) {
                throw std::system_error(make_error_code(std::get<NetErrorCode>(m_data)));
            }
            return std::get<T>(m_data);
        }

        [[nodiscard]] T& Value() {
            if (!HasValue()) {
                throw std::system_error(make_error_code(std::get<NetErrorCode>(m_data)));
            }
            return std::get<T>(m_data);
        }

        [[nodiscard]] const T& operator*() const { return Value(); }
        [[nodiscard]] T& operator*() { return Value(); }
        [[nodiscard]] const T* operator->() const { return &Value(); }
        [[nodiscard]] T* operator->() { return &Value(); }

        [[nodiscard]] NetErrorCode Error() const noexcept {
            if (HasValue()) {
                return NetErrorCode::Success;
            }
            return std::get<NetErrorCode>(m_data);
        }

        [[nodiscard]] T ValueOr(T&& defaultValue) const {
            return HasValue() ? std::get<T>(m_data) : std::forward<T>(defaultValue);
        }

    private:
        std::variant<T, NetErrorCode> m_data;
    };

    // Specialization for void
    template <>
    class NetResult<void> {
    public:
        NetResult() : m_error(NetErrorCode::Success) {}
        NetResult(NetErrorCode error) : m_error(error) {}
        NetResult(std::error_code ec) : m_error(static_cast<NetErrorCode>(ec.value())) {}

        [[nodiscard]] bool HasValue() const noexcept {
            return m_error == NetErrorCode::Success;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return HasValue();
        }

        [[nodiscard]] NetErrorCode Error() const noexcept {
            return m_error;
        }

    private:
        NetErrorCode m_error;
    };

} // namespace darkfire::net

namespace std {
    template <>
    struct is_error_code_enum<darkfire::net::NetErrorCode> : true_type {};
}
