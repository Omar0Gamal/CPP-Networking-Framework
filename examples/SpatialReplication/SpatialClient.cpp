#include "SpatialProtocol.hpp"
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <iomanip>

class SpatialClient : public darkfire::net::DualClient<SpatialMsg> {
public:
    SpatialClient() = default;

    void UpdateReplication() {
        this->UpdateDual();
    }

    std::optional<EntityState> GetInterpolatedEntity(uint32_t entityId, uint64_t renderTime) {
        auto it = m_entityHistories.find(entityId);
        if (it == m_entityHistories.end()) {
            return std::nullopt;
        }

        return it->second.Sample(renderTime, [](const EntityState& a, const EntityState& b, double alpha) {
            return EntityState::Interpolate(a, b, alpha);
        });
    }

protected:
    void OnUDPMessage(darkfire::net::Packet& packet) override {
        if (packet.GetIdAs<SpatialMsg>() == SpatialMsg::WorldSnapshot) {
            auto reader = packet.GetReader();
            uint64_t timestamp = 0;
            reader >> timestamp;

            auto vecRes = reader.ReadVector<EntityState>();
            if (vecRes) {
                for (const auto& ent : *vecRes) {
                    m_entityHistories[ent.entityId].AddSnapshot(timestamp, ent);
                }
            }
        }
    }

private:
    std::unordered_map<uint32_t, darkfire::net::game::SnapshotHistory<EntityState, 64>> m_entityHistories;
};

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t tcpPort = 60030;
    uint16_t udpPort = 60031;

    std::cout << "========================================================\n";
    std::cout << "  SpatialReplication Client (Snapshot Interpolator Demo)\n";
    std::cout << "========================================================\n";

    SpatialClient client;
    if (!client.ConnectDual(host, tcpPort, udpPort)) {
        std::cerr << "Failed to connect to SpatialServer at " << host << "\n";
        return 1;
    }

    std::cout << "Connected! Receiving snapshots and running client-side interpolation...\n";

    // 100ms interpolation buffer delay (standard for smooth remote entity replication)
    constexpr uint64_t InterpDelayMicros = 100000;

    int frames = 0;
    bool benchMode = (argc > 1 && std::string(argv[1]) == "--bench");

    while (frames < (benchMode ? 100 : 300)) {
        // Send player input via fast UDP
        darkfire::net::Packet inputPkt(SpatialMsg::ClientInput);
        uint32_t myEntId = 1;
        float moveX = 1.0f;
        float moveY = 0.5f;
        inputPkt << myEntId << moveX << moveY;
        client.SendUDP(inputPkt);

        client.UpdateReplication();

        uint64_t now = darkfire::net::game::RTTTracker::GetCurrentTimeMicros();
        uint64_t renderTime = (now > InterpDelayMicros) ? (now - InterpDelayMicros) : 0;

        auto ent1 = client.GetInterpolatedEntity(1, renderTime);
        auto ent2 = client.GetInterpolatedEntity(2, renderTime);

        if (ent1.has_value() && ent2.has_value()) {
            if (frames % 10 == 0) {
                std::cout << "[Client Interp] Frame #" << std::setw(3) << frames
                          << " | Ent#1: (" << std::fixed << std::setprecision(2) << ent1->posX << ", " << ent1->posY << ")"
                          << " | Ent#2: (" << std::fixed << std::setprecision(2) << ent2->posX << ", " << ent2->posY << ")\n";
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // 60 FPS render loop
        frames++;
    }

    client.DisconnectDual();
    return 0;
}
