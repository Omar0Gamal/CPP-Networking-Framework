#include <darkfire/darkfire.hpp>
#include <cassert>
#include <iostream>
#include <thread>

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    static Vector3 Lerp(const Vector3& a, const Vector3& b, double t) {
        float f = static_cast<float>(t);
        return {
            a.x + (b.x - a.x) * f,
            a.y + (b.y - a.y) * f,
            a.z + (b.z - a.z) * f
        };
    }
};

void TestRTTTracker() {
    std::cout << "  [TEST] RTTTracker EWMA Calculation... ";

    darkfire::net::game::RTTTracker clientTracker;
    assert(!clientTracker.IsInitialized());

    // Generate Ping
    auto ping = clientTracker.CreatePing(1);
    assert(ping.HasFlag(darkfire::net::PacketFlags::HeartbeatPing));

    // Simulate minor delay
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Server receives and creates Pong
    auto pong = darkfire::net::game::RTTTracker::CreatePong(ping);
    assert(pong.HasFlag(darkfire::net::PacketFlags::HeartbeatPong));

    // Simulate return trip delay
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Client processes Pong
    clientTracker.ProcessPong(pong);

    assert(clientTracker.IsInitialized());
    double rtt = clientTracker.GetRTT();
    assert(rtt >= 30.0 && rtt <= 100.0); // Should be roughly ~40ms

    std::cout << "PASSED (Estimated RTT: " << rtt << " ms, Jitter: " << clientTracker.GetJitter() << " ms)\n";
}

void TestSnapshotHistoryInterpolation() {
    std::cout << "  [TEST] SnapshotHistory Interpolation & Clamping... ";

    darkfire::net::game::SnapshotHistory<Vector3, 16> history;
    assert(history.Empty());

    // Add snapshots at t=1000, 2000, 3000 ms
    history.AddSnapshot(1000, Vector3{ 0.0f, 0.0f, 0.0f });
    history.AddSnapshot(2000, Vector3{ 10.0f, 20.0f, 0.0f });
    history.AddSnapshot(3000, Vector3{ 30.0f, 40.0f, 0.0f });

    assert(history.Count() == 3);

    auto lerp = [](const Vector3& a, const Vector3& b, double t) {
        return Vector3::Lerp(a, b, t);
    };

    // Sample at t = 1500 (halfway between t=1000 and t=2000)
    auto s1500 = history.Sample(1500, lerp);
    assert(s1500.has_value());
    assert(std::abs(s1500->x - 5.0f) < 0.001f);
    assert(std::abs(s1500->y - 10.0f) < 0.001f);

    // Sample at t = 2500 (halfway between t=2000 and t=3000)
    auto s2500 = history.Sample(2500, lerp);
    assert(s2500.has_value());
    assert(std::abs(s2500->x - 20.0f) < 0.001f);
    assert(std::abs(s2500->y - 30.0f) < 0.001f);

    // Sample beyond newest: t = 4000 -> clamps to t=3000
    auto s4000 = history.Sample(4000, lerp);
    assert(s4000.has_value());
    assert(s4000->x == 30.0f);

    // Sample before oldest: t = 500 -> clamps to t=1000
    auto s500 = history.Sample(500, lerp);
    assert(s500.has_value());
    assert(s500->x == 0.0f);

    std::cout << "PASSED\n";
}

void TestNetworkSimulator() {
    std::cout << "  [TEST] NetworkSimulator Artificial Latency & Packet Loss... ";

    darkfire::net::game::NetworkSimulator sim;
    sim.SetLatency(30, 50); // 30 - 50 ms
    sim.SetPacketLoss(0.20); // 20% loss

    constexpr int Total = 100;
    int enqueued = 0;
    for (int i = 0; i < Total; ++i) {
        darkfire::net::Packet pkt(1);
        pkt << i;
        if (sim.Enqueue(pkt)) {
            enqueued++;
        }
    }

    assert(sim.DroppedCount() > 0); // With 20% loss over 100 packets, drop count > 0
    assert(enqueued == Total - sim.DroppedCount());

    // Initially immediately after enqueue, 0 should be ready (since min latency is 30ms)
    auto readyInitial = sim.PollReady();
    assert(readyInitial.empty());

    // Wait 60ms
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    auto readyLater = sim.PollReady();
    assert(readyLater.size() == static_cast<size_t>(enqueued));

    std::cout << "PASSED (Sent: " << Total << ", Dropped: " << sim.DroppedCount() << ", Delivered: " << readyLater.size() << ")\n";
}

void RunGamePrimitivesTests() {
    std::cout << "\n--- Running Game Networking Primitives Tests ---\n";
    TestRTTTracker();
    TestSnapshotHistoryInterpolation();
    TestNetworkSimulator();
}
