#pragma once

#include "error.hpp"
#include "endian.hpp"
#include "buffer_view.hpp"
#include "byte_writer.hpp"
#include "byte_reader.hpp"

namespace darkfire::net {

    // BinaryStream combining both read and write capabilities in a single interface
    class BinaryStream {
    public:
        BinaryStream() = default;
        explicit BinaryStream(size_t initialCapacity) : m_writer(initialCapacity) {}
        explicit BinaryStream(std::vector<uint8_t> buffer) : m_writer(buffer.size()) {
            m_writer.WriteBytes(buffer);
        }

        ByteWriter& Writer() noexcept { return m_writer; }
        const ByteWriter& Writer() const noexcept { return m_writer; }

        [[nodiscard]] ByteReader Reader() const noexcept {
            return ByteReader(m_writer.Span());
        }

        [[nodiscard]] BufferView View() const noexcept {
            return m_writer.View();
        }

        [[nodiscard]] const uint8_t* Data() const noexcept { return m_writer.Data(); }
        [[nodiscard]] size_t Size() const noexcept { return m_writer.Size(); }
        [[nodiscard]] bool Empty() const noexcept { return m_writer.Empty(); }
        void Clear() noexcept { m_writer.Clear(); }

        template <typename T>
        BinaryStream& operator<<(const T& value) {
            m_writer << value;
            return *this;
        }

    private:
        ByteWriter m_writer;
    };

} // namespace darkfire::net
