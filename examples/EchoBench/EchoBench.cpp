#include <darkfire/darkfire.hpp>
#include <iostream>
#include <chrono>
#include <atomic>
#include <iomanip>

enum class EchoMsg : uint16_t {
    Ping = 1,
    Pong = 2
};

class BenchServer : public darkfire::net::TCPServer<EchoMsg> {
public:
    explicit BenchServer(uint16_t port) : darkfire::net::TCPServer<EchoMsg>(port) {}

protected:
    void OnMessage(std::shared_ptr<darkfire::net::TCPConnection> client, darkfire::net::Packet& packet) override {
        if (packet.GetId() == static_cast<uint16_t>(EchoMsg::Ping)) {
            packet.SetId(static_cast<uint16_t>(EchoMsg::Pong));
            client->Send(packet);
        }
    }
};

class BenchClient : public darkfire::net::TCPClient<EchoMsg> {
public:
    BenchClient() = default;

    void SetResponseCallback(std::function<void(uint64_t sentTime)> cb) {
        m_onPong = std::move(cb);
    }

protected:
    void OnMessage(darkfire::net::Packet& packet) override {
        if (packet.GetId() == static_cast<uint16_t>(EchoMsg::Pong)) {
            auto reader = packet.GetReader();
            uint64_t t0 = 0;
            reader >> t0;
            if (m_onPong) {
                m_onPong(t0);
            }
        }
    }

private:
    std::function<void(uint64_t)> m_onPong;
};

int main() {
    darkfire::net::SetLogLevel(darkfire::net::LogLevel::Warning);

    std::cout << "========================================================\n";
    std::cout << "  DarkFireNetworking 2.0 - EchoBench Benchmark\n";
    std::cout << "========================================================\n";

    constexpr uint16_t Port = 60010;
    BenchServer server(Port);
    server.SetAsyncDispatch(true);
    if (!server.Start()) {
        std::cerr << "Failed to start bench server\n";
        return 1;
    }

    BenchClient client;
    if (!client.Connect("127.0.0.1", Port)) {
        std::cerr << "Failed to connect bench client\n";
        return 1;
    }

    // Wait for connection validation
    while (!client.IsConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    constexpr int TotalPackets = 10000;
    std::atomic<int> receivedCount{ 0 };
    std::mutex latMutex;
    std::vector<double> latenciesMicros;
    latenciesMicros.reserve(TotalPackets);

    client.SetResponseCallback([&](uint64_t t0) {
        uint64_t t1 = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count()
        );
        {
            std::lock_guard<std::mutex> lock(latMutex);
            latenciesMicros.push_back(static_cast<double>(t1 - t0));
        }
        receivedCount.fetch_add(1, std::memory_order_relaxed);
    });

    std::cout << "Sending " << TotalPackets << " high-frequency ping/pong packets...\n";

    auto benchStart = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < TotalPackets; ++i) {
        darkfire::net::Packet pkt(EchoMsg::Ping);
        uint64_t t0 = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count()
        );
        pkt << t0;
        client.Send(pkt);

        // Throttle slightly if buffer fills up
        if (i % 500 == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    // Wait until all packets have echoed back or timeout
    int waitLoops = 0;
    while (receivedCount.load(std::memory_order_relaxed) < TotalPackets && waitLoops++ < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    auto benchEnd = std::chrono::high_resolution_clock::now();
    double totalSeconds = std::chrono::duration<double>(benchEnd - benchStart).count();

    int actualReceived = receivedCount.load();
    double pps = actualReceived / totalSeconds;

    double totalLatency = 0.0;
    double minLatency = 1e9;
    double maxLatency = 0.0;
    for (double lat : latenciesMicros) {
        totalLatency += lat;
        if (lat < minLatency) minLatency = lat;
        if (lat > maxLatency) maxLatency = lat;
    }
    double avgLatency = latenciesMicros.empty() ? 0.0 : (totalLatency / latenciesMicros.size());

    std::cout << "\n--- Benchmark Results ---\n";
    std::cout << "  Total Packets Sent:      " << TotalPackets << "\n";
    std::cout << "  Total Packets Echoed:    " << actualReceived << "\n";
    std::cout << "  Duration:                " << std::fixed << std::setprecision(3) << totalSeconds << " s\n";
    std::cout << "  Throughput:              " << std::fixed << std::setprecision(0) << pps << " packets/sec\n";
    std::cout << "  Round-Trip Latency (Avg):" << std::fixed << std::setprecision(1) << avgLatency << " us (" << (avgLatency / 1000.0) << " ms)\n";
    std::cout << "  Round-Trip Latency (Min):" << std::fixed << std::setprecision(1) << minLatency << " us\n";
    std::cout << "  Round-Trip Latency (Max):" << std::fixed << std::setprecision(1) << maxLatency << " us\n";
    std::cout << "========================================================\n";

    client.Disconnect();
    server.Stop();
    return 0;
}
