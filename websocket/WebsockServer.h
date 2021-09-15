#ifndef _WEBSOCK_SERVER
#define _WEBSOCK_SERVER

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_STL_

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.h>

#include <functional>
#include <string>
#include <vector>
#include <mutex>
#include <map>

typedef websocketpp::server<websocketpp::config::asio> WebsocketEndpoint;
typedef websocketpp::connection_hdl ClientConnection;

class WebsockServer
{
public:
	WebsockServer();
	void run(int port);

	//Returns the number of currently connected clients
	size_t numConnections();

	//Registers a callback for when a client connects
	template <typename CallbackTy>
	void connect(CallbackTy handler)
	{
		//Make sure we only access the handlers list from the networking thread
		this->eventLoop.post([this, handler]() {
			this->connectHandlers.push_back(handler);
		});
	}

	//Registers a callback for when a client disconnects
	template <typename CallbackTy>
	void disconnect(CallbackTy handler)
	{
		//Make sure we only access the handlers list from the networking thread
		this->eventLoop.post([this, handler]() {
			this->disconnectHandlers.push_back(handler);
		});
	}

	//Registers a callback for when a particular type of message is received
	template <typename CallbackTy>
	void message(const std::string& messageType, CallbackTy handler)
	{
		//Make sure we only access the handlers list from the networking thread
		this->eventLoop.post([this, messageType, handler]() {
			this->messageHandlers[messageType].push_back(handler);
		});
	}

	//Sends a message to an individual client
	//(Note: the data transmission will take place on the thread that called WebsocketServer::run())
	void sendMessage(ClientConnection conn, const std::string& messageType, const nlohmann::json & arguments);

	//Sends a message to all connected clients
	//(Note: the data transmission will take place on the thread that called WebsocketServer::run())
	void broadcastMessage(const std::string& messageType, const nlohmann::json& arguments);

protected:
	static nlohmann::json parseJson(const std::string& jsonStr);
	static std::string stringifyJson(const nlohmann::json& val);

	void onOpen(ClientConnection conn);
	void onClose(ClientConnection conn);
	void onMessage(ClientConnection conn, WebsocketEndpoint::message_ptr msg);

	asio::io_service eventLoop;
	WebsocketEndpoint endpoint;
	std::vector<ClientConnection> openConnections;
	std::mutex connectionListMutex;

	std::vector<std::function<void(ClientConnection)>> connectHandlers;
	std::vector<std::function<void(ClientConnection)>> disconnectHandlers;
	std::map<std::string, std::vector<std::function<void(ClientConnection, const nlohmann::json&)>>> messageHandlers;
};

#endif