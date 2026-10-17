#include "SpatialProtocol.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>

class SpatialServer : public darkfire::net::DualServer<SpatialMsg> {
public:
    SpatialServer(uint16_t tcpPort, uint16_t udpPort)
        : darkfire::net::DualServer<SpatialMsg>(tcpPort, udpPort) {
        // Initialize sample game entities
        m_entities.push_back({ 1, 0.0f, 0.0f, 0.0f, 5.0f, 2.0f, 0.0f });
        m_entities.push_back({ 2, 50.0f, 10.0f, 0.0f, -3.0f, 4.0f, 0.0f });
    }

    void TickSimulation(float dt) {
        m_simTime += dt;
        for (auto& e : m_entities) {
            e.posX += e.velX * dt;
            e.posY += e.velY * dt;

            // Simple bounce within boundaries [-100, 100]
            if (e.posX > 100.0f || e.posX < -100.0f) e.velX = -e.velX;
            if (e.posY > 100.0f || e.posY < -100.0f) e.velY = -e.velY;
        }
    }

    void BroadcastWorldState() {
        darkfire::net::Packet snapshot(SpatialMsg::WorldSnapshot);
        uint64_t timestamp = darkfire::net::game::RTTTracker::GetCurrentTimeMicros();
        snapshot << timestamp;
        snapshot.GetWriter().WriteVector(m_entities);

        // Fast broadcast via UDP
        BroadcastUDP(snapshot);
    }

    const std::vector<EntityState>& GetEntities() const noexcept {
        return m_entities;
    }

protected:
    void OnUDPMessage(const asio::ip::udp::endpoint&, darkfire::net::Packet& packet) override {
        if (packet.GetIdAs<SpatialMsg>() == SpatialMsg::ClientInput) {
            auto reader = packet.GetReader();
            uint32_t entityId = 0;
            float moveX = 0.0f;
            float moveY = 0.0f;
            reader >> entityId >> moveX >> moveY;

            for (auto& e : m_entities) {
                if (e.entityId == entityId) {
                    e.velX = moveX * 10.0f;
                    e.velY = moveY * 10.0f;
                    break;
                }
            }
        }
    }

private:
    float m_simTime = 0.0f;
    std::vector<EntityState> m_entities;
};

int main(int argc, char* argv[]) {
    uint16_t tcpPort = 60030;
    uint16_t udpPort = 60031;

    SpatialServer server(tcpPort, udpPort);
    if (!server.StartDual()) {
        std::cerr << "Failed to start SpatialServer\n";
        return 1;
    }

    std::cout << "========================================================\n";
    std::cout << "  SpatialReplication Server running (TCP: " << tcpPort << ", UDP: " << udpPort << ")\n";
    std::cout << "  Simulating 60 Hz physics, broadcasting 20 Hz snapshots...\n";
    std::cout << "========================================================\n";

    constexpr auto tickDuration = std::chrono::milliseconds(16); // ~60 Hz
    auto lastSnapshotTime = std::chrono::steady_clock::now();

    bool running = true;
    int tickCount = 0;
    while (running) {
        auto start = std::chrono::steady_clock::now();

        server.TickSimulation(0.016f);
        server.UpdateDual();

        // Broadcast snapshot at ~20 Hz (every 50ms)
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSnapshotTime).count() >= 50) {
            server.BroadcastWorldState();
            lastSnapshotTime = now;

            if (++tickCount % 40 == 0) {
                const auto& ents = server.GetEntities();
                std::cout << "[Server] Ent#1: Pos(" << ents[0].posX << ", " << ents[0].posY 
                          << ") | Ent#2: Pos(" << ents[1].posX << ", " << ents[1].posY << ")\n";
            }
        }

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed < tickDuration) {
            std::this_thread::sleep_for(tickDuration - elapsed);
        }

        // Run for 300 ticks (~5 seconds) in automatic demo mode if not interactive
        if (argc > 1 && std::string(argv[1]) == "--bench" && tickCount >= 100) {
            break;
        }
    }

    server.StopDual();
    return 0;
}
