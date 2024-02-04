#pragma once
#include "net_common.h"

namespace df 
{
	namespace net
	{
		// Message Header is sent at the start of all messages.
		// It contains the size of the message and the ID of the message
		// the Template allows us to use "enum class" to ensure that the messages are valid at compile time
		template <typename T>
		struct message_header
		{
			T id{};
			uint32_t size = 0;
		};

		template <typename T>
		struct message
		{
			message_header<T> header{};
			std::vector<uint8_t> body;

			size_t size() const
			{
				return sizeof(message_header<T>) + body.size();
			}

			// Override for std::cout compatibility - produces friendly description of the message
			friend std::ostream& operator << (std::ostream& os, const message<T>& msg)
			{
				os << "ID:" << int(msg.header.id) << " Size:" << msg.header.size;
				return os;
			}	

			// Pushes any POD-like data into the message buffer
			template <typename DataType>
			friend message<T>& operator << (message<T>& msg, const DataType& data)
			{
				// Check that the type of the data being pushed is trivially copyable
				static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed into vector");

				// Cache current size of the body vector
				size_t i = msg.body.size();

				// Resize the body vector by the size of the data being pushed
				msg.body.resize(msg.body.size() + sizeof(DataType));

				// Copy the data into the newly allocated vector space
				std::memcpy(msg.body.data() + i, &data, sizeof(DataType));

				// Recalculate the message size
				msg.header.size = msg.size();

				// Return the target message so that we can chain "<<" messages into the same line
				return msg;
			}

			// Pulls any POD-like data from the message buffer
			template <typename DataType>
			friend message<T>& operator >> (message<T>& msg, DataType& data)
			{
				// Check that the type of the data being pushed is trivially copyable
				static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed into vector");

				// Cache current size of the body vector
				size_t i = msg.body.size() - sizeof(DataType);

				// Copy the data from the body vector into the target data
				std::memcpy(&data, msg.body.data() + i, sizeof(DataType));

				// Shrink the vector to remove read bytes, and reset the size of the message
				msg.body.resize(i);

				// Recalculate the message size
				msg.header.size = msg.size();

				// Return the target message so that we can chain ">>" messages into the same line
				return msg;
			}
		};

		// Forward declare the connection
		template <typename T>
		class connection;

		template <typename T>
		struct owned_message
		{
			std::shared_ptr<connection<T>> remote = nullptr;
			message<T> msg;

			// Again, a friendly string maker
			friend std::ostream& operator << (std::ostream& os, const owned_message<T>& msg)
			{
				os << msg.msg;
				return os;
			}
		};
	}
}