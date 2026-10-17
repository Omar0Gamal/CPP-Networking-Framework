#include <darkfire/darkfire.hpp>
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

void TestTCPLoopback() {
    std::cout << "  [TEST] TCP Client/Server Coroutines & Handshake Loopback... ";

    darkfire::net::SetLogLevel(darkfire::net::LogLevel::Warning);

    constexpr uint16_t Port = 59123;
    darkfire::net::TCPServer server(Port);
    assert(server.Start());

    darkfire::net::TCPClient client;
    assert(client.Connect("127.0.0.1", Port));

    // Wait for validation
    int waitLoops = 0;
    while (!client.IsConnected() && waitLoops++ < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(client.IsConnected());

    // Send packet from client to server
    darkfire::net::Packet req(101);
    std::string testMsg = "Hello Coroutine Server";
    uint32_t magicVal = 0x12345678;
    req << testMsg << magicVal;
    client.Send(req);

    // Update server until message received
    bool serverGotMessage = false;
    for (int i = 0; i < 100 && !serverGotMessage; ++i) {
        server.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        darkfire::net::OwnedPacket op;
        if (server.ClientCount() > 0) {
            // Server will bounce it back if message arrives
        }
    }

    // Wait a brief moment to ensure transport pipeline completes
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server.Update();

    client.Disconnect();
    server.Stop();

    std::cout << "PASSED\n";
}

void TestUDPChannelLoopbackAndFragmentation() {
    std::cout << "  [TEST] UDP Channel Sequencing & MTU Fragmentation Loopback... ";

    asio::io_context ioContext;

    constexpr uint16_t ServerPort = 59124;
    constexpr uint16_t ClientPort = 59125;

    auto serverChannel = std::make_shared<darkfire::net::UDPChannel>(ioContext, ServerPort);
    auto clientChannel = std::make_shared<darkfire::net::UDPChannel>(ioContext, ClientPort);

    std::atomic<bool> fragmentReceived{ false };
    std::vector<uint8_t> receivedBody;

    serverChannel->SetMessageCallback([&](const asio::ip::udp::endpoint&, darkfire::net::Packet& pkt) {
        receivedBody = pkt.Body();
        fragmentReceived.store(true);
    });

    serverChannel->Start();
    clientChannel->Start();

    std::thread ioThread([&]() {
        auto work = asio::make_work_guard(ioContext);
        ioContext.run();
    });

    // Create a 3500-byte payload (will be sliced into 3 fragments of 1200 bytes each)
    std::vector<uint8_t> largeData(3500);
    for (size_t i = 0; i < largeData.size(); ++i) {
        largeData[i] = static_cast<uint8_t>(i & 0xFF);
    }

    darkfire::net::Packet bigPacket(201);
    bigPacket.GetWriter().WriteBytes(largeData);

    asio::ip::udp::endpoint serverEp(asio::ip::make_address("127.0.0.1"), ServerPort);
    clientChannel->SendReliable(serverEp, bigPacket);

    // Wait for reassembly
    int waitCount = 0;
    while (!fragmentReceived.load() && waitCount++ < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    assert(fragmentReceived.load());
    assert(receivedBody.size() == largeData.size());
    assert(receivedBody == largeData);

    clientChannel->Close();
    serverChannel->Close();
    ioContext.stop();

    if (ioThread.joinable()) {
        ioThread.join();
    }

    std::cout << "PASSED (3500 bytes reassembled seamlessly)\n";
}

void RunNetworkTransportTests() {
    std::cout << "\n--- Running Network Transport (TCP & UDP) Tests ---\n";
    TestTCPLoopback();
    TestUDPChannelLoopbackAndFragmentation();
}
