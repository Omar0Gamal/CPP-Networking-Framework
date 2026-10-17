#pragma once

#include "tcp_connection.hpp"
#include "lockfree_queue.hpp"
#include "log.hpp"

#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <mutex>

namespace darkfire::net {

    template <typename MessageType = uint16_t>
    class TCPServer {
    public:
        explicit TCPServer(uint16_t port, std::string authKey = "DarkFireSecretAuthKey")
            : m_port(port),
              m_acceptor(m_ioContext),
              m_authKey(std::move(authKey)) {}

        virtual ~TCPServer() {
            Stop();
        }

        bool Start() {
            try {
                asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), m_port);
                m_acceptor.open(endpoint.protocol());
                m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
                m_acceptor.bind(endpoint);
                m_acceptor.listen();

                m_running = true;

                // Spawn coroutine acceptor
                asio::co_spawn(m_ioContext, 
                    [this]() -> asio::awaitable<void> {
                        co_await AcceptorWorker();
                    },
                    asio::detached
                );

                m_workerThread = std::thread([this]() {
                    auto work = asio::make_work_guard(m_ioContext);
                    m_ioContext.run();
                });

                LogInfo("Server started on port ", m_port);
                return true;
            } catch (const std::exception& e) {
                LogError("Server failed to start on port ", m_port, ": ", e.what());
                return false;
            }
        }

        void Stop() {
            if (!m_running) return;
            m_running = false;

            std::error_code ec;
            m_acceptor.close(ec);

            // Disconnect all clients
            {
                std::lock_guard<std::mutex> lock(m_connMutex);
                for (auto& [id, conn] : m_connections) {
                    if (conn) {
                        conn->Disconnect();
                    }
                }
                m_connections.clear();
            }

            m_ioContext.stop();

            if (m_workerThread.joinable()) {
                m_workerThread.join();
            }

            m_ioContext.restart();
            LogInfo("Server stopped");
        }

        void Send(std::shared_ptr<TCPConnection> client, const Packet& packet) {
            if (client && client->IsActive()) {
                client->Send(packet);
            }
        }

        void Send(uint32_t clientId, const Packet& packet) {
            std::shared_ptr<TCPConnection> conn;
            {
                std::lock_guard<std::mutex> lock(m_connMutex);
                auto it = m_connections.find(clientId);
                if (it != m_connections.end()) {
                    conn = it->second;
                }
            }
            if (conn) {
                Send(conn, packet);
            }
        }

        void Broadcast(const Packet& packet, std::shared_ptr<TCPConnection> ignoreClient = nullptr) {
            std::vector<std::shared_ptr<TCPConnection>> targets;
            {
                std::lock_guard<std::mutex> lock(m_connMutex);
                targets.reserve(m_connections.size());
                for (const auto& [id, conn] : m_connections) {
                    if (conn && conn != ignoreClient && conn->IsActive()) {
                        targets.push_back(conn);
                    }
                }
            }

            for (auto& client : targets) {
                client->Send(packet);
            }
        }

        void Update(size_t maxMessages = SIZE_MAX, bool wait = false) {
            if (wait) {
                m_qIncoming.Wait();
            }

            size_t count = 0;
            OwnedPacket item;
            while (count < maxMessages && m_qIncoming.Pop(item)) {
                if (item.connection && item.connection->IsActive()) {
                    OnMessage(item.connection, item.packet);
                }
                count++;
            }
        }

        void SetAsyncDispatch(bool enable = true) noexcept {
            m_asyncDispatch = enable;
        }

        [[nodiscard]] size_t ClientCount() const {
            std::lock_guard<std::mutex> lock(m_connMutex);
            return m_connections.size();
        }

    protected:
        virtual bool OnClientConnect(std::shared_ptr<TCPConnection>) { return true; }
        virtual void OnClientValidated(std::shared_ptr<TCPConnection>) {}
        virtual void OnClientDisconnect(std::shared_ptr<TCPConnection>) {}
        virtual void OnMessage(std::shared_ptr<TCPConnection>, Packet&) {}

    private:
        asio::awaitable<void> AcceptorWorker() {
            while (m_running) {
                auto [ec, socket] = co_await m_acceptor.async_accept(asio::as_tuple(asio::use_awaitable));
                if (ec) {
                    if (ec != asio::error::operation_aborted) {
                        LogError("Acceptor error: ", ec.message());
                    }
                    break;
                }

                uint32_t clientId = m_idCounter++;
                auto conn = std::make_shared<TCPConnection>(
                    TCPConnection::Owner::Server,
                    m_ioContext,
                    std::move(socket),
                    clientId,
                    m_authKey
                );

                if (!OnClientConnect(conn)) {
                    LogWarn("Client connection denied by server veto");
                    conn->Disconnect();
                    continue;
                }

                {
                    std::lock_guard<std::mutex> lock(m_connMutex);
                    m_connections[clientId] = conn;
                }

                conn->SetMessageCallback([this](std::shared_ptr<TCPConnection> c, Packet& pkt) {
                    if (m_asyncDispatch.load()) {
                        OnMessage(c, pkt);
                    } else {
                        m_qIncoming.Push({ c, pkt });
                    }
                });

                conn->SetValidatedCallback([this](std::shared_ptr<TCPConnection> c) {
                    OnClientValidated(c);
                });

                conn->SetDisconnectCallback([this](std::shared_ptr<TCPConnection> c) {
                    {
                        std::lock_guard<std::mutex> lock(m_connMutex);
                        m_connections.erase(c->GetId());
                    }
                    OnClientDisconnect(c);
                });

                conn->StartServerSession();
            }
        }

    protected:
        uint16_t m_port;
        asio::io_context m_ioContext;
        asio::ip::tcp::acceptor m_acceptor;
        std::thread m_workerThread;
        std::atomic<bool> m_running{ false };
        std::atomic<bool> m_asyncDispatch{ false };
        std::atomic<uint32_t> m_idCounter{ 10000 };
        std::string m_authKey;

        mutable std::mutex m_connMutex;
        std::unordered_map<uint32_t, std::shared_ptr<TCPConnection>> m_connections;
        ThreadSafeQueue<OwnedPacket> m_qIncoming;
    };

} // namespace darkfire::net
