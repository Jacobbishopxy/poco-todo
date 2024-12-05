/**
 * @file:	Main.cpp
 * @author:	Jacob Xie
 * @date:	2024/12/04 10:03:10 Wednesday
 * @brief:
 **/

#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Util/ServerApplication.h"
#include "TodoServer.h"

class RequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory
{
public:
    RequestHandlerFactory(TodoStorage& storage, WebSocketHandler& wsHandler)
        : storage_(storage), wsHandler_(wsHandler) {}

    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override
    {
        if (request.getURI() == "/ws")
        {
            return new WebSocketConnectionHandler(wsHandler_);
        }
        return new TodoHandler(storage_, wsHandler_);
    }

private:
    TodoStorage& storage_;
    WebSocketHandler& wsHandler_;
};

class TodoServerApp : public Poco::Util::ServerApplication
{
protected:
    void initialize(Application& self) override
    {
        loadConfiguration();  // Load default configuration files.
        ServerApplication::initialize(self);
        std::cout << "Server initialized" << std::endl;
    }

    void uninitialize() override
    {
        std::cout << "Server shutting down" << std::endl;
        ServerApplication::uninitialize();
    }

    int main(const std::vector<std::string>& args) override
    {
        auto port = 9001;

        // Initialize storage and WebSocket manager
        TodoStorage storage;
        WebSocketHandler wsHandler;

        // Configure server parameters
        Poco::Net::HTTPServerParams::Ptr params = new Poco::Net::HTTPServerParams;
        params->setMaxQueued(100);
        params->setMaxThreads(16);

        // Set up HTTP server
        Poco::Net::HTTPServer server(
            new RequestHandlerFactory(storage, wsHandler),
            Poco::Net::ServerSocket(port),
            params);

        std::cout << "Starting server on port " << port << "..." << std::endl;
        server.start();

        waitForTerminationRequest();  // Wait for CTRL+C or termination signal.

        std::cout << "Stopping server..." << std::endl;
        server.stop();

        return Application::EXIT_OK;
    }
};

int main(int argc, char** argv)
{
    TodoServerApp app;
    return app.run(argc, argv);
}