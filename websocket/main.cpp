#include "WebsockServer.h"

int main(int argc, char* argv[])
{
	int PORT_NUMBER = 6080;
	//Create the event loop for the main thread, and the WebSocket server
	asio::io_service mainEventLoop;
	WebsockServer server;
	
	//Register our network callbacks, ensuring the logic is run on the main thread's event loop
	server.connect([&mainEventLoop, &server](ClientConnection conn)
	{
		mainEventLoop.post([conn, &server]()
		{
			std::clog << "Connection opened." << std::endl;
			std::clog << "There are now " << server.numConnections() << " open connections." << std::endl;

			//Send a hello message to the client
			server.sendMessage(conn, "hello", nlohmann::json());
		});
	});
	server.disconnect([&mainEventLoop, &server](ClientConnection conn)
	{
		mainEventLoop.post([conn, &server]()
		{
			std::clog << "Connection closed." << std::endl;
			std::clog << "There are now " << server.numConnections() << " open connections." << std::endl;
		});
	});
	server.message("message", [&mainEventLoop, &server](ClientConnection conn, const nlohmann::json& args)
	{
		mainEventLoop.post([conn, args, &server]()
		{
			std::clog << "message handler on the main thread" << std::endl;
			std::clog << "Message payload:" << std::endl;
			std::cout << args.dump() << "\n";
			//Echo the message pack to the client
			server.sendMessage(conn, "message", args);
		});
	});

	//Start the networking thread
	std::thread serverThread([&server, PORT_NUMBER]() {
		server.run(PORT_NUMBER);
	});

	//Start a keyboard input thread that reads from stdin
	std::thread inputThread([&server, &mainEventLoop]()
	{
		std::string input;
		while (1)
		{
			//Read user input from stdin
			std::getline(std::cin, input);

			//Broadcast the input to all connected clients (is sent on the network thread)
			nlohmann::json payload;
			payload["input"] = input;
			server.broadcastMessage("userInput", payload);

			//Debug output on the main thread
			mainEventLoop.post([]() {
				std::clog << "User input debug output on the main thread" << std::endl;
			});
		}
	});

	//Start the event loop for the main thread
	asio::io_service::work work(mainEventLoop);
	mainEventLoop.run();

	return 0;
}
