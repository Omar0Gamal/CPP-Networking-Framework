# CPP-Networking-Framework

This is a C++ networking framework that uses TCP. The low-level handling is managed by ASIO. This framework provides a simple and object-oriented interface for managing sockets, packets, and connections.

## How to Use

1. Include "df_net.h" in your project.
2. Create an enum class that has the message types like FireBullet or PlayerMove.

## Client Side

1. Inherit from "client_interface" and create a class.
2. Use the `Send` method to send a message to the server. A Message is an object which has a type and a body. You can put most kinds of structured data into it with the `<<` operator and take data out with the `>>` operator. It works like a stack, so it's first in, last out.
3. Connect to a server with the `Connect` method. It takes a hostname and a port.
4. Every client has a thread-safe queue which has all the incoming messages from the server.

You can take a look at [`SimpleClient.cpp`](https://github.com/Omar0Gamal/CPP-Networking-Framework/blob/master/NetClient/SimpleClient.cpp) for a full implementation.

## Server Side

1. Inherit from `server_interface`.
2. Override the following methods as needed: `OnClientConnect`, `OnClientDisconnect`, `OnMessage`, `OnClientValidated`. Note that `OnClientValidated` runs when the client is confirmed to be a client.
3. The server has a method called `update` which takes two arguments: the amount of messages to process at a time (it can be -1 for the max number) and a bool to tell it to sleep or not when there are no messages in the queue.

You can take a look at [`SimpleClient.cpp`](https://github.com/Omar0Gamal/CPP-Networking-Framework/blob/master/NetServer/SimpleServer.cpp) for a full implementation.

## Dependencies

This project depends on the ASIO library. Please make sure you have it installed and configured in your development environment.

## License

This project is licensed under the MIT License. See the [LICENSE](https://github.com/Omar0Gamal/CPP-Networking-Framework/blob/master/LICENSE) file for details.
