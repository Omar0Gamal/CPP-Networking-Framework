#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace darkfire::net {

    // Non-owning zero-copy view of a contiguous byte buffer
    class BufferView {
    public:
        constexpr BufferView() noexcept : m_span() {}
        constexpr BufferView(const uint8_t* data, size_t size) noexcept : m_span(data, size) {}
        constexpr BufferView(std::span<const uint8_t> span) noexcept : m_span(span) {}
        BufferView(const std::vector<uint8_t>& vec) noexcept : m_span(vec.data(), vec.size()) {}
        BufferView(std::string_view sv) noexcept 
            : m_span(reinterpret_cast<const uint8_t*>(sv.data()), sv.size()) {}

        [[nodiscard]] constexpr const uint8_t* Data() const noexcept { return m_span.data(); }
        [[nodiscard]] constexpr size_t Size() const noexcept { return m_span.size(); }
        [[nodiscard]] constexpr bool Empty() const noexcept { return m_span.empty(); }

        [[nodiscard]] constexpr const uint8_t& operator[](size_t idx) const noexcept { return m_span[idx]; }
        [[nodiscard]] constexpr std::span<const uint8_t> AsSpan() const noexcept { return m_span; }
        
        [[nodiscard]] std::string_view AsStringView() const noexcept {
            return { reinterpret_cast<const char*>(m_span.data()), m_span.size() };
        }

        [[nodiscard]] constexpr BufferView SubView(size_t offset, size_t count = std::dynamic_extent) const {
            if (offset >= m_span.size()) {
                return BufferView{};
            }
            return BufferView(m_span.subspan(offset, count));
        }

        constexpr auto begin() const noexcept { return m_span.begin(); }
        constexpr auto end() const noexcept { return m_span.end(); }
        constexpr auto cbegin() const noexcept { return m_span.begin(); }
        constexpr auto cend() const noexcept { return m_span.end(); }

    private:
        std::span<const uint8_t> m_span;
    };

} // namespace darkfire::net
