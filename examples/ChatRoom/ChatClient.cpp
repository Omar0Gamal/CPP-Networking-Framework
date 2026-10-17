#include "ChatProtocol.hpp"
#include <iostream>
#include <string>

class ChatClient : public darkfire::net::TCPClient<ChatMsg> {
public:
    ChatClient() = default;

protected:
    void OnMessage(darkfire::net::Packet& packet) override {
        auto reader = packet.GetReader();

        switch (packet.GetIdAs<ChatMsg>()) {
            case ChatMsg::SystemNotice: {
                auto msg = reader.ReadString();
                if (msg) {
                    std::cout << "[SYSTEM] " << *msg << "\n";
                }
                break;
            }

            case ChatMsg::ChatMessage: {
                auto room = reader.ReadString();
                auto sender = reader.ReadString();
                auto text = reader.ReadString();
                if (room && sender && text) {
                    std::cout << "[" << *room << "] " << *sender << ": " << *text << "\n";
                }
                break;
            }

            default:
                break;
        }
    }
};

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 60020;

    if (argc > 1) host = argv[1];
    if (argc > 2) port = static_cast<uint16_t>(std::stoi(argv[2]));

    ChatClient client;
    if (!client.Connect(host, port)) {
        std::cerr << "Failed to connect to chat server at " << host << ":" << port << "\n";
        return 1;
    }

    std::cout << "Connected to chat server. Enter your handle: ";
    std::string handle;
    std::getline(std::cin, handle);

    darkfire::net::Packet regPkt(ChatMsg::RegisterHandle);
    regPkt.GetWriter().WriteString(handle);
    client.Send(regPkt);

    std::cout << "Enter room name to join (e.g. 'general'): ";
    std::string room;
    std::getline(std::cin, room);

    darkfire::net::Packet joinPkt(ChatMsg::JoinRoom);
    joinPkt.GetWriter().WriteString(room);
    client.Send(joinPkt);

    std::cout << "Type messages and press Enter (or '/quit' to exit):\n";
    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "/quit") break;
        if (input.empty()) continue;

        darkfire::net::Packet chatPkt(ChatMsg::ChatMessage);
        chatPkt.GetWriter().WriteString(room);
        chatPkt.GetWriter().WriteString(input);
        client.Send(chatPkt);
    }

    client.Disconnect();
    return 0;
}
