#pragma once

#include "tcp_server.hpp"
#include "udp_channel.hpp"

#include <unordered_map>
#include <mutex>
#include <memory>

namespace darkfire::net {

    template <typename MessageType = uint16_t>
    class DualServer : public TCPServer<MessageType> {
    public:
        DualServer(uint16_t tcpPort, uint16_t udpPort, std::string authKey = "DarkFireSecretAuthKey")
            : TCPServer<MessageType>(tcpPort, std::move(authKey)),
              m_udpPort(udpPort) {}

        virtual ~DualServer() {
            StopDual();
        }

        bool StartDual() {
            // 1. Start TCP Server
            if (!this->Start()) {
                return false;
            }

            // 2. Start UDP Server
            try {
                m_udpChannel = std::make_shared<UDPChannel>(this->m_ioContext, m_udpPort);
                m_udpChannel->SetMessageCallback([this](const asio::ip::udp::endpoint& sender, Packet& pkt) {
                    HandleUDPIncoming(sender, pkt);
                });
                m_udpChannel->Start();

                LogInfo("DualServer running: TCP port ", this->m_port, ", UDP port ", m_udpPort);
                return true;
            } catch (const std::exception& e) {
                LogError("Failed to bind UDP server on port ", m_udpPort, ": ", e.what());
                this->Stop();
                return false;
            }
        }

        void StopDual() {
            if (m_udpChannel) {
                m_udpChannel->Close();
                m_udpChannel.reset();
            }
            this->Stop();
        }

        void SendUDP(uint32_t clientId, const Packet& packet) {
            asio::ip::udp::endpoint target;
            {
                std::lock_guard<std::mutex> lock(m_endpointMutex);
                auto it = m_clientUdpEndpoints.find(clientId);
                if (it != m_clientUdpEndpoints.end()) {
                    target = it->second;
                }
            }
            if (target.port() != 0 && m_udpChannel) {
                m_udpChannel->SendTo(target, packet);
            }
        }

        void SendUDPSequenced(uint32_t clientId, const Packet& packet) {
            asio::ip::udp::endpoint target;
            {
                std::lock_guard<std::mutex> lock(m_endpointMutex);
                auto it = m_clientUdpEndpoints.find(clientId);
                if (it != m_clientUdpEndpoints.end()) {
                    target = it->second;
                }
            }
            if (target.port() != 0 && m_udpChannel) {
                m_udpChannel->SendSequenced(target, packet);
            }
        }

        void SendUDPReliable(uint32_t clientId, const Packet& packet) {
            asio::ip::udp::endpoint target;
            {
                std::lock_guard<std::mutex> lock(m_endpointMutex);
                auto it = m_clientUdpEndpoints.find(clientId);
                if (it != m_clientUdpEndpoints.end()) {
                    target = it->second;
                }
            }
            if (target.port() != 0 && m_udpChannel) {
                m_udpChannel->SendReliable(target, packet);
            }
        }

        void BroadcastUDP(const Packet& packet, uint32_t ignoreClientId = 0xFFFFFFFF) {
            std::vector<asio::ip::udp::endpoint> targets;
            {
                std::lock_guard<std::mutex> lock(m_endpointMutex);
                targets.reserve(m_clientUdpEndpoints.size());
                for (const auto& [id, ep] : m_clientUdpEndpoints) {
                    if (id != ignoreClientId && ep.port() != 0) {
                        targets.push_back(ep);
                    }
                }
            }

            if (m_udpChannel) {
                for (const auto& ep : targets) {
                    m_udpChannel->SendTo(ep, packet);
                }
            }
        }

        void UpdateDual(size_t maxMessages = SIZE_MAX, bool wait = false) {
            this->Update(maxMessages, wait);

            size_t count = 0;
            UDPOwnedPacket item;
            while (count < maxMessages && m_qUdpIncoming.Pop(item)) {
                OnUDPMessage(item.endpoint, item.packet);
                count++;
            }
        }

    protected:
        virtual void OnUDPMessage(const asio::ip::udp::endpoint&, Packet&) {}

        void OnClientDisconnect(std::shared_ptr<TCPConnection> client) override {
            if (client) {
                std::lock_guard<std::mutex> lock(m_endpointMutex);
                m_clientUdpEndpoints.erase(client->GetId());
            }
            TCPServer<MessageType>::OnClientDisconnect(client);
        }

    private:
        void HandleUDPIncoming(const asio::ip::udp::endpoint& sender, Packet& pkt) {
            // Check if this is a system registration datagram
            if (pkt.HasFlag(PacketFlags::SystemMessage)) {
                auto reader = pkt.GetReader();
                auto resId = reader.ReadUInt32();
                if (resId) {
                    uint32_t clientId = *resId;
                    std::lock_guard<std::mutex> lock(m_endpointMutex);
                    m_clientUdpEndpoints[clientId] = sender;
                    LogInfo("Registered UDP endpoint for client [", clientId, "]: ", sender.address().to_string(), ":", sender.port());
                }
                return;
            }

            {
                std::lock_guard<std::mutex> lock(m_endpointMutex);
                bool found = false;
                for (const auto& [id, ep] : m_clientUdpEndpoints) {
                    if (ep == sender) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    uint32_t autoId = static_cast<uint32_t>(m_clientUdpEndpoints.size() + 20000);
                    m_clientUdpEndpoints[autoId] = sender;
                }
            }

            m_qUdpIncoming.Push({ sender, pkt });
        }

    protected:
        uint16_t m_udpPort;
        std::shared_ptr<UDPChannel> m_udpChannel;
        mutable std::mutex m_endpointMutex;
        std::unordered_map<uint32_t, asio::ip::udp::endpoint> m_clientUdpEndpoints;
        ThreadSafeQueue<UDPOwnedPacket> m_qUdpIncoming;
    };

} // namespace darkfire::net
