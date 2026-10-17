#pragma once

#include "../packet.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>

namespace darkfire::net::game {

    class RTTTracker {
    public:
        explicit RTTTracker(double rttAlpha = 0.125, double jitterBeta = 0.25)
            : m_alpha(rttAlpha), m_beta(jitterBeta) {}

        Packet CreatePing(uint32_t pingSeq = 0) {
            Packet pkt(0, PacketFlags::HeartbeatPing);
            uint64_t t0 = GetCurrentTimeMicros();
            pkt << pingSeq << t0;
            pkt.FinalizeUDP();
            return pkt;
        }

        static Packet CreatePong(Packet& pingPacket) {
            uint64_t t1 = GetCurrentTimeMicros();
            auto reader = pingPacket.GetReader();
            uint32_t pingSeq = 0;
            uint64_t t0 = 0;
            reader >> pingSeq >> t0;

            uint64_t t2 = GetCurrentTimeMicros();

            Packet pong(0, PacketFlags::HeartbeatPong);
            pong << pingSeq << t0 << t1 << t2;
            pong.FinalizeUDP();
            return pong;
        }

        void ProcessPong(Packet& pongPacket) {
            uint64_t t3 = GetCurrentTimeMicros();

            auto reader = pongPacket.GetReader();
            uint32_t pingSeq = 0;
            uint64_t t0 = 0;
            uint64_t t1 = 0;
            uint64_t t2 = 0;

            reader >> pingSeq >> t0 >> t1 >> t2;

            if (reader.HasError() || t3 < t0) {
                return;
            }

            double serverProcessing = (t2 >= t1) ? static_cast<double>(t2 - t1) : 0.0;
            double totalRoundTrip = static_cast<double>(t3 - t0);
            double sampleRttMicros = std::max(0.0, totalRoundTrip - serverProcessing);
            double sampleRttMs = sampleRttMicros / 1000.0;

            // Clock offset theta = ((t1 - t0) + (t2 - t3)) / 2
            double offsetMicros = (static_cast<double>(t1 - t0) + static_cast<double>(t2 - t3)) / 2.0;
            m_clockOffsetMs = offsetMicros / 1000.0;

            if (!m_initialized) {
                m_rttMs = sampleRttMs;
                m_jitterMs = sampleRttMs / 2.0;
                m_initialized = true;
            } else {
                double diff = std::abs(sampleRttMs - m_rttMs);
                m_jitterMs = m_beta * diff + (1.0 - m_beta) * m_jitterMs;
                m_rttMs = m_alpha * sampleRttMs + (1.0 - m_alpha) * m_rttMs;
            }
        }

        [[nodiscard]] double GetRTT() const noexcept { return m_rttMs; }
        [[nodiscard]] double GetJitter() const noexcept { return m_jitterMs; }
        [[nodiscard]] double GetClockOffset() const noexcept { return m_clockOffsetMs; }
        [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

        [[nodiscard]] uint64_t GetSyncedServerTimeMicros() const noexcept {
            int64_t local = static_cast<int64_t>(GetCurrentTimeMicros());
            int64_t offset = static_cast<int64_t>(m_clockOffsetMs * 1000.0);
            return static_cast<uint64_t>(local + offset);
        }

        static uint64_t GetCurrentTimeMicros() noexcept {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()
                ).count()
            );
        }

    private:
        double m_alpha;
        double m_beta;
        double m_rttMs = 0.0;
        double m_jitterMs = 0.0;
        double m_clockOffsetMs = 0.0;
        bool m_initialized = false;
    };

} // namespace darkfire::net::game
