#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <algorithm>

namespace darkfire::net::game {

    template <typename T>
    struct Snapshot {
        uint64_t timestamp = 0; // Microseconds or milliseconds
        T state{};
    };

    template <typename T, size_t BufferSize = 64>
    class SnapshotHistory {
        static_assert(BufferSize >= 2, "BufferSize must be at least 2");

    public:
        SnapshotHistory() = default;

        void AddSnapshot(uint64_t timestamp, const T& state) {
            size_t idx = (m_head + 1) % BufferSize;
            m_buffer[idx] = { timestamp, state };
            m_head = idx;
            if (m_count < BufferSize) {
                m_count++;
            }
        }

        [[nodiscard]] size_t Count() const noexcept {
            return m_count;
        }

        [[nodiscard]] bool Empty() const noexcept {
            return m_count == 0;
        }

        void Clear() noexcept {
            m_count = 0;
            m_head = 0;
        }

        [[nodiscard]] std::optional<Snapshot<T>> GetNewest() const noexcept {
            if (m_count == 0) return std::nullopt;
            return m_buffer[m_head];
        }

        [[nodiscard]] std::optional<Snapshot<T>> GetOldest() const noexcept {
            if (m_count == 0) return std::nullopt;
            size_t oldestIdx = (m_head + BufferSize - m_count + 1) % BufferSize;
            return m_buffer[oldestIdx];
        }

        // Interpolate between two surrounding snapshots at targetTime
        // LerpFunc signature: T(const T& a, const T& b, double alpha)
        template <typename LerpFunc>
        std::optional<T> Sample(uint64_t targetTime, LerpFunc&& lerp) const {
            if (m_count == 0) {
                return std::nullopt;
            }

            if (m_count == 1) {
                return m_buffer[m_head].state;
            }

            auto newest = GetNewest().value();
            auto oldest = GetOldest().value();

            if (targetTime >= newest.timestamp) {
                return newest.state; // Clamped to newest
            }
            if (targetTime <= oldest.timestamp) {
                return oldest.state; // Clamped to oldest
            }

            // Search backward from head to find surrounding interval [t0, t1]
            for (size_t i = 0; i < m_count - 1; ++i) {
                size_t currIdx = (m_head + BufferSize - i) % BufferSize;
                size_t prevIdx = (currIdx + BufferSize - 1) % BufferSize;

                const auto& s1 = m_buffer[currIdx]; // newer
                const auto& s0 = m_buffer[prevIdx]; // older

                if (targetTime >= s0.timestamp && targetTime <= s1.timestamp) {
                    if (s1.timestamp == s0.timestamp) {
                        return s1.state;
                    }
                    double alpha = static_cast<double>(targetTime - s0.timestamp) /
                                   static_cast<double>(s1.timestamp - s0.timestamp);
                    alpha = std::clamp(alpha, 0.0, 1.0);
                    return lerp(s0.state, s1.state, alpha);
                }
            }

            return newest.state;
        }

    private:
        std::array<Snapshot<T>, BufferSize> m_buffer{};
        size_t m_head = 0;
        size_t m_count = 0;
    };

} // namespace darkfire::net::game
