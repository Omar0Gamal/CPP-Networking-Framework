#pragma once

#include "net_common.h"

namespace df
{
	namespace net
	{
		template <typename T>
		class client_interface
		{
		public:
			client_interface() : m_socket(m_asioContext)
			{

			}
			virtual ~client_interface()
			{
				Disconnect();
			}
		public:
			bool Connect(const std::string& host, const uint16_t port)
			{
				try
				{
					// Resolve host into IP address
					asio::ip::tcp::resolver resolver(m_asioContext);
					asio::ip::tcp::resolver::results_type m_endpoints = resolver.resolve(host, std::to_string(port));

					// Create connection
					m_connection = std::make_unique<connection<T>>(
						connection<T>::owner::client,
						m_asioContext, asio::ip::tcp::socket(m_asioContext), m_qMessagesIn);

					// Tell the connection object to connect to the server
					m_connection->ConnectToSever(m_endpoints);

					// Start thrContext thread
					thrContext = std::thread([this]() { m_asioContext.run(); });
				}
				catch (std::exception& e)
				{
					std::cerr << "Client Exception: " << e.what() << "\n";
					return false;
				}
				return true;
			}
			void Disconnect()
			{
				// If connection exists, and it's connected then...
				if (IsConnected())
				{
					m_connection->Disconnect();
				}

				// Stop the asio context
				m_asioContext.stop();

				// Tidy up the context thread
				if (thrContext.joinable())
					thrContext.join();

				// Release the connection object
				m_connection.release();
			}
			bool IsConnected()
			{
				if(m_connection)
					return m_connection->IsConnected();
				else
					return false;
			}
			// Send message to server
			void Send(const message<T>& msg)
			{
				if (IsConnected())
					m_connection->Send(msg);
			}
			
			// Retrieve queue of messages from server
			tsqueue<owned_message<T>>& Incoming()
			{
				return m_qMessagesIn;
			}
		protected:
			// asio context handles the data transfer
			asio::io_context m_asioContext;
			std::thread thrContext;

			// Connection to the server
			asio::ip::tcp::socket m_socket;
			std::unique_ptr<connection<T>> m_connection;
		private:
			// this is the thread safe queue of incoming messages from the server
			tsqueue<owned_message<T>> m_qMessagesIn;
		};
	}
}