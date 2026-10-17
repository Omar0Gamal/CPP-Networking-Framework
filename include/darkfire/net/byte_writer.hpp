#pragma once

#include "error.hpp"
#include "endian.hpp"
#include "buffer_view.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace darkfire::net {

    class ByteWriter {
    public:
        explicit ByteWriter(size_t initialCapacity = 256) {
            m_buffer.reserve(initialCapacity);
        }

        void Reserve(size_t capacity) {
            m_buffer.reserve(capacity);
        }

        void Clear() noexcept {
            m_buffer.clear();
        }

        [[nodiscard]] const uint8_t* Data() const noexcept {
            return m_buffer.data();
        }

        [[nodiscard]] size_t Size() const noexcept {
            return m_buffer.size();
        }

        [[nodiscard]] bool Empty() const noexcept {
            return m_buffer.empty();
        }

        [[nodiscard]] std::span<const uint8_t> Span() const noexcept {
            return { m_buffer.data(), m_buffer.size() };
        }

        [[nodiscard]] BufferView View() const noexcept {
            return BufferView(Span());
        }

        [[nodiscard]] const std::vector<uint8_t>& Buffer() const noexcept {
            return m_buffer;
        }

        [[nodiscard]] std::vector<uint8_t>& Buffer() noexcept {
            return m_buffer;
        }

        [[nodiscard]] std::vector<uint8_t> Release() noexcept {
            return std::move(m_buffer);
        }

        // Primitives & Scalars
        template <SerializableScalar T>
        ByteWriter& Write(T value) {
            T netVal = HostToNetwork(value);
            const auto* ptr = reinterpret_cast<const uint8_t*>(&netVal);
            m_buffer.insert(m_buffer.end(), ptr, ptr + sizeof(T));
            return *this;
        }

        ByteWriter& WriteUInt8(uint8_t v)   { return Write(v); }
        ByteWriter& WriteUInt16(uint16_t v) { return Write(v); }
        ByteWriter& WriteUInt32(uint32_t v) { return Write(v); }
        ByteWriter& WriteUInt64(uint64_t v) { return Write(v); }
        ByteWriter& WriteInt8(int8_t v)     { return Write(v); }
        ByteWriter& WriteInt16(int16_t v)   { return Write(v); }
        ByteWriter& WriteInt32(int32_t v)   { return Write(v); }
        ByteWriter& WriteInt64(int64_t v)   { return Write(v); }
        ByteWriter& WriteFloat(float v)     { return Write(v); }
        ByteWriter& WriteDouble(double v)   { return Write(v); }
        ByteWriter& WriteBool(bool v)       { return Write(static_cast<uint8_t>(v ? 1 : 0)); }

        // Enums (constrained via concept)
        template <SerializableEnum E>
        ByteWriter& WriteEnum(E value) {
            using Underlying = std::underlying_type_t<E>;
            return Write(static_cast<Underlying>(value));
        }

        // Strings & String Views
        ByteWriter& WriteString(std::string_view str) {
            uint32_t length = static_cast<uint32_t>(str.size());
            WriteUInt32(length);
            if (length > 0) {
                const auto* ptr = reinterpret_cast<const uint8_t*>(str.data());
                m_buffer.insert(m_buffer.end(), ptr, ptr + length);
            }
            return *this;
        }

        ByteWriter& WriteString(const std::string& str) {
            return WriteString(std::string_view(str));
        }

        ByteWriter& WriteString(const char* str) {
            return WriteString(str ? std::string_view(str) : std::string_view(""));
        }

        // Raw bytes
        ByteWriter& WriteBytes(std::span<const uint8_t> bytes) {
            m_buffer.insert(m_buffer.end(), bytes.begin(), bytes.end());
            return *this;
        }

        ByteWriter& WriteBytes(const void* data, size_t size) {
            if (data && size > 0) {
                const auto* ptr = static_cast<const uint8_t*>(data);
                m_buffer.insert(m_buffer.end(), ptr, ptr + size);
            }
            return *this;
        }

        // Vectors
        template <typename T>
        ByteWriter& WriteVector(const std::vector<T>& vec) {
            uint32_t count = static_cast<uint32_t>(vec.size());
            WriteUInt32(count);
            for (const auto& item : vec) {
                *this << item;
            }
            return *this;
        }

        // Trivially copyable structs
        template <TriviallySerializable T>
        ByteWriter& WriteStruct(const T& obj) {
            return WriteBytes(&obj, sizeof(T));
        }

        // Streaming operator<<
        template <SerializableScalar T>
        friend ByteWriter& operator<<(ByteWriter& writer, T value) {
            return writer.Write(value);
        }

        template <SerializableEnum E>
        friend ByteWriter& operator<<(ByteWriter& writer, E value) {
            return writer.WriteEnum(value);
        }

        friend ByteWriter& operator<<(ByteWriter& writer, std::string_view str) {
            return writer.WriteString(str);
        }

        friend ByteWriter& operator<<(ByteWriter& writer, const std::string& str) {
            return writer.WriteString(str);
        }

        friend ByteWriter& operator<<(ByteWriter& writer, const char* str) {
            return writer.WriteString(str);
        }

        template <typename T>
        friend ByteWriter& operator<<(ByteWriter& writer, const std::vector<T>& vec) {
            return writer.WriteVector(vec);
        }

        template <TriviallySerializable T>
        friend ByteWriter& operator<<(ByteWriter& writer, const T& obj) {
            return writer.WriteStruct(obj);
        }

    private:
        std::vector<uint8_t> m_buffer;
    };

} // namespace darkfire::net
