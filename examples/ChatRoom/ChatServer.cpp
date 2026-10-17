#include "ChatProtocol.hpp"
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <string>

class ChatServer : public darkfire::net::TCPServer<ChatMsg> {
public:
    explicit ChatServer(uint16_t port) : darkfire::net::TCPServer<ChatMsg>(port) {}

protected:
    void OnClientValidated(std::shared_ptr<darkfire::net::TCPConnection> client) override {
        darkfire::net::Packet welcome(ChatMsg::SystemNotice);
        welcome.GetWriter().WriteString("Welcome to DarkFire Chat Server! Please register your handle.");
        Send(client, welcome);
    }

    void OnClientDisconnect(std::shared_ptr<darkfire::net::TCPConnection> client) override {
        uint32_t id = client->GetId();
        std::string handle;
        std::string room;

        {
            std::lock_guard<std::mutex> lock(m_chatMutex);
            auto hIt = m_handles.find(id);
            if (hIt != m_handles.end()) {
                handle = hIt->second;
                m_handles.erase(hIt);
            }

            auto rIt = m_clientRooms.find(id);
            if (rIt != m_clientRooms.end()) {
                room = rIt->second;
                m_rooms[room].erase(id);
                m_clientRooms.erase(rIt);
            }
        }

        if (!handle.empty() && !room.empty()) {
            BroadcastToRoom(room, handle + " has left the chat.", 0);
        }
    }

    void OnMessage(std::shared_ptr<darkfire::net::TCPConnection> client, darkfire::net::Packet& packet) override {
        auto reader = packet.GetReader();
        uint32_t clientId = client->GetId();

        switch (packet.GetIdAs<ChatMsg>()) {
            case ChatMsg::RegisterHandle: {
                auto res = reader.ReadString();
                if (res) {
                    std::lock_guard<std::mutex> lock(m_chatMutex);
                    m_handles[clientId] = *res;
                    std::cout << "[Server] Client [" << clientId << "] registered as '" << *res << "'\n";

                    darkfire::net::Packet ack(ChatMsg::SystemNotice);
                    ack.GetWriter().WriteString("Handle registered as '" + *res + "'. You may now join a room.");
                    Send(client, ack);
                }
                break;
            }

            case ChatMsg::JoinRoom: {
                auto res = reader.ReadString();
                if (res) {
                    std::string newRoom = *res;
                    std::string handle = "Anonymous";

                    {
                        std::lock_guard<std::mutex> lock(m_chatMutex);
                        auto hIt = m_handles.find(clientId);
                        if (hIt != m_handles.end()) {
                            handle = hIt->second;
                        }

                        // Remove from old room if in one
                        auto oldRoomIt = m_clientRooms.find(clientId);
                        if (oldRoomIt != m_clientRooms.end()) {
                            m_rooms[oldRoomIt->second].erase(clientId);
                        }

                        m_clientRooms[clientId] = newRoom;
                        m_rooms[newRoom].insert(clientId);
                    }

                    std::cout << "[Server] " << handle << " joined room '" << newRoom << "'\n";
                    BroadcastToRoom(newRoom, handle + " entered room #" + newRoom, 0);
                }
                break;
            }

            case ChatMsg::ChatMessage: {
                auto roomRes = reader.ReadString();
                auto msgRes = reader.ReadString();
                if (roomRes && msgRes) {
                    std::string handle = "Anonymous";
                    {
                        std::lock_guard<std::mutex> lock(m_chatMutex);
                        auto hIt = m_handles.find(clientId);
                        if (hIt != m_handles.end()) {
                            handle = hIt->second;
                        }
                    }

                    darkfire::net::Packet broadcast(ChatMsg::ChatMessage);
                    broadcast.GetWriter().WriteString(*roomRes);
                    broadcast.GetWriter().WriteString(handle);
                    broadcast.GetWriter().WriteString(*msgRes);

                    BroadcastPacketToRoom(*roomRes, broadcast);
                }
                break;
            }

            default:
                break;
        }
    }

private:
    void BroadcastToRoom(const std::string& room, const std::string& text, uint32_t ignoreId) {
        darkfire::net::Packet notice(ChatMsg::SystemNotice);
        notice.GetWriter().WriteString(text);
        BroadcastPacketToRoom(room, notice, ignoreId);
    }

    void BroadcastPacketToRoom(const std::string& room, const darkfire::net::Packet& packet, uint32_t ignoreId = 0) {
        std::vector<uint32_t> targets;
        {
            std::lock_guard<std::mutex> lock(m_chatMutex);
            auto it = m_rooms.find(room);
            if (it != m_rooms.end()) {
                for (uint32_t id : it->second) {
                    if (id != ignoreId) {
                        targets.push_back(id);
                    }
                }
            }
        }

        for (uint32_t id : targets) {
            Send(id, packet);
        }
    }

private:
    std::mutex m_chatMutex;
    std::unordered_map<uint32_t, std::string> m_handles;
    std::unordered_map<uint32_t, std::string> m_clientRooms;
    std::unordered_map<std::string, std::unordered_set<uint32_t>> m_rooms;
};

int main(int argc, char* argv[]) {
    uint16_t port = 60020;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::stoi(argv[1]));
    }

    ChatServer server(port);
    if (!server.Start()) {
        return 1;
    }

    std::cout << "========================================================\n";
    std::cout << "  DarkFire ChatServer running on port " << port << "\n";
    std::cout << "  Press Enter to stop the server.\n";
    std::cout << "========================================================\n";

    std::string line;
    std::getline(std::cin, line);

    server.Stop();
    return 0;
}
