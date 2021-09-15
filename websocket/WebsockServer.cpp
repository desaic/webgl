#include "WebsockServer.h"

#include <algorithm>
#include <functional>
#include <iostream>

//The name of the special JSON field that holds the message type for messages
#define MESSAGE_FIELD "__MESSAGE__"

nlohmann::json WebsockServer::parseJson(const std::string& jsonStr)
{
	nlohmann::json j;
	try {
		j = nlohmann::json::parse(jsonStr);
	}
	catch (nlohmann::json::parse_error& ex) {
		std::cout << "parse error at byte " << ex.what() << std::endl;
	}
	return j;
}

std::string WebsockServer::stringifyJson(const nlohmann::json&j)
{
	return j.dump();
}

WebsockServer::WebsockServer()
{
	//Wire up our event handlers
	this->endpoint.set_open_handler(std::bind(&WebsockServer::onOpen, this, std::placeholders::_1));
	this->endpoint.set_close_handler(std::bind(&WebsockServer::onClose, this, std::placeholders::_1));
	this->endpoint.set_message_handler(std::bind(&WebsockServer::onMessage, this, std::placeholders::_1, std::placeholders::_2));

	//Initialise the Asio library, using our own event loop object
	this->endpoint.init_asio(&(this->eventLoop));
}

void WebsockServer::run(int port)
{
	//Listen on the specified port number and start accepting connections
	this->endpoint.listen(port);
	this->endpoint.start_accept();

	//Start the Asio event loop
	this->endpoint.run();
}

size_t WebsockServer::numConnections()
{
	//Prevent concurrent access to the list of open connections from multiple threads
	std::lock_guard<std::mutex> lock(this->connectionListMutex);

	return this->openConnections.size();
}

void WebsockServer::sendMessage(ClientConnection conn, const std::string& messageType, const nlohmann::json& arguments)
{
	//Copy the argument values, and bundle the message type into the object
	nlohmann::json messageData = arguments;
	messageData[MESSAGE_FIELD] = messageType;

	//Send the JSON data to the client (will happen on the networking thread's event loop)
	this->endpoint.send(conn, WebsockServer::stringifyJson(messageData), websocketpp::frame::opcode::text);
}

void WebsockServer::broadcastMessage(const std::string& messageType, const nlohmann::json& arguments)
{
	//Prevent concurrent access to the list of open connections from multiple threads
	std::lock_guard<std::mutex> lock(this->connectionListMutex);

	for (auto conn : this->openConnections) {
		this->sendMessage(conn, messageType, arguments);
	}
}

void WebsockServer::onOpen(ClientConnection conn)
{
	{
		//Prevent concurrent access to the list of open connections from multiple threads
		std::lock_guard<std::mutex> lock(this->connectionListMutex);

		//Add the connection handle to our list of open connections
		this->openConnections.push_back(conn);
	}

	//Invoke any registered handlers
	for (auto handler : this->connectHandlers) {
		handler(conn);
	}
}

void WebsockServer::onClose(ClientConnection conn)
{
	{
		//Prevent concurrent access to the list of open connections from multiple threads
		std::lock_guard<std::mutex> lock(this->connectionListMutex);

		//Remove the connection handle from our list of open connections
		auto connVal = conn.lock();
		auto newEnd = std::remove_if(this->openConnections.begin(), this->openConnections.end(), [&connVal](ClientConnection elem)
		{
			//If the pointer has expired, remove it from the vector
			if (elem.expired() == true) {
				return true;
			}

			//If the pointer is still valid, compare it to the handle for the closed connection
			auto elemVal = elem.lock();
			if (elemVal.get() == connVal.get()) {
				return true;
			}

			return false;
		});

		//Truncate the connections vector to erase the removed elements
		this->openConnections.resize(std::distance(openConnections.begin(), newEnd));
	}

	//Invoke any registered handlers
	for (auto handler : this->disconnectHandlers) {
		handler(conn);
	}
}

void WebsockServer::onMessage(ClientConnection conn, WebsocketEndpoint::message_ptr msg)
{
	//Validate that the incoming message contains valid JSON
	nlohmann::json messageObject = WebsockServer::parseJson(msg->get_payload());
	if (messageObject.empty() == false)
	{
		//Validate that the JSON object contains the message type field
		if (messageObject.contains(MESSAGE_FIELD))
		{
			//Extract the message type and remove it from the payload
			std::string messageType = messageObject[MESSAGE_FIELD].dump();
			messageObject.erase(MESSAGE_FIELD);

			//If any handlers are registered for the message type, invoke them
			auto& handlers = this->messageHandlers[messageType];
			for (auto handler : handlers) {
				handler(conn, messageObject);
			}
		}
	}
}