/**
 * @file:	TodoServer.cpp
 * @author:	Jacob Xie
 * @date:	2024/12/04 13:17:33 Wednesday
 * @brief:
 **/

#include "TodoServer.h"

#include "Poco/Dynamic/Var.h"
#include "Poco/JSON/Parser.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/NetException.h"

// ================================================================================================
// JsonUtil
// ================================================================================================
#pragma region JsonUtil

Poco::JSON::Object::Ptr JsonUtil::serializeTodo(const Todo& todo)
{
    Poco::JSON::Object::Ptr jsonObj = new Poco::JSON::Object;
    jsonObj->set("id", todo.id);
    jsonObj->set("title", todo.title);
    jsonObj->set("description", todo.description);
    jsonObj->set("completed", todo.completed);
    return jsonObj;
}

Poco::JSON::Array::Ptr JsonUtil::serializeTodos(const std::unordered_map<int, Todo>& todos)
{
    Poco::JSON::Array::Ptr jsonArray = new Poco::JSON::Array;
    for (const auto& [id, todo] : todos)
    {
        jsonArray->add(serializeTodo(todo));
    }
    return jsonArray;
}

void JsonUtil::deserializeCreateTodo(const Poco::JSON::Object::Ptr& json, std::string& title, std::string& description)
{
    title = json->getValue<std::string>("title");
    description = json->getValue<std::string>("description");
}

void JsonUtil::deserializeModifyTodo(const Poco::JSON::Object::Ptr& json, std::string& title, std::string& description, bool& completed)
{
    title = json->getValue<std::string>("title");
    description = json->getValue<std::string>("description");
    completed = json->getValue<bool>("completed");
}

#pragma endregion JsonUtil

// ================================================================================================
// Storage
// ================================================================================================
#pragma region TodoStorage

TodoStorage::TodoStorage()
    : nextId_(1)
{
}

int TodoStorage::createTodo(const std::string& title, const std::string& description)
{
    std::lock_guard<std::mutex> lock(mutex_);
    int id = nextId_++;
    todos_[id] = {id, title, description, false};
    return id;
}

Todo TodoStorage::getTodo(int id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (todos_.find(id) == todos_.end())
    {
        throw std::runtime_error("Todo not found");
    }
    return todos_[id];
}

std::unordered_map<int, Todo> TodoStorage::getAllTodos()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return todos_;
}

void TodoStorage::modifyTodo(int id, const std::string& title, const std::string& description, bool completed)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (todos_.find(id) == todos_.end())
    {
        throw std::runtime_error("Todo not found");
    }
    todos_[id] = {id, title, description, completed};
}

void TodoStorage::deleteTodo(int id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    todos_.erase(id);
}

#pragma endregion TodoStorage

// ================================================================================================
// WebSocketHandler
// ================================================================================================
#pragma region WebSocketHandler

WebSocketHandler::~WebSocketHandler()
{
    Poco::Mutex::ScopedLock lock(clientsMutex_);
    for (auto* client : clients_)
    {
        delete client;  // Cleanup WebSocket pointers
    }
    clients_.clear();
}

void WebSocketHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    try
    {
        // Upgrade HTTP connection to WebSocket
        Poco::Net::WebSocket* ws = new Poco::Net::WebSocket(request, response);

        std::cout << "WebSocket connection established" << std::endl;

        addClient(ws);

        char buffer[1024];
        int flags;
        int n;

        // Continuously listen for messages from the client
        do
        {
            n = ws->receiveFrame(buffer, sizeof(buffer), flags);
            if (n > 0)
            {
                std::string receivedMessage(buffer, n);
                // std::cout << "Received message: " << receivedMessage << std::endl;

                // Echo the received message back to the client
                ws->sendFrame(buffer, n, flags);
            }
        } while (n > 0 && (flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE);

        std::cout << "WebSocket connection closed" << std::endl;
        removeClient(ws);
    }
    catch (const Poco::Net::WebSocketException& e)
    {
        std::cerr << "WebSocket exception: " << e.displayText() << std::endl;
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST, e.displayText());
        response.send();
    }
}

void WebSocketHandler::broadcastMessage(const std::string& message)
{
    Poco::Mutex::ScopedLock lock(clientsMutex_);
    for (auto* client : clients_)
    {
        try
        {
            client->sendFrame(message.data(), static_cast<int>(message.size()), Poco::Net::WebSocket::FRAME_TEXT);
        }
        catch (const Poco::Exception& e)
        {
            std::cerr << "Error broadcasting to WebSocket client: " << e.displayText() << std::endl;
            removeClient(client);
        }
    }
}

void WebSocketHandler::addClient(Poco::Net::WebSocket* client)
{
    std::cout << "WebSocketHandler::addClient" << client->address() << std::endl;
    Poco::Mutex::ScopedLock lock(clientsMutex_);
    clients_.insert(client);
}

void WebSocketHandler::removeClient(Poco::Net::WebSocket* client)
{
    std::cout << "WebSocketHandler::removeClient" << client->address() << std::endl;
    Poco::Mutex::ScopedLock lock(clientsMutex_);
    clients_.erase(client);
    delete client;  // Clean up the WebSocket object
}

#pragma endregion WebSocketHandler

// ================================================================================================
// HttpHandler
// ================================================================================================
#pragma region TodoHandler

TodoHandler::TodoHandler(TodoStorage& storage, WebSocketHandler& wsHandler)
    : storage_(storage), wsHandler_(wsHandler)
{
}

void TodoHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
    response.setContentType("application/json");
    response.setChunkedTransferEncoding(true);

    auto path = request.getURI();
    auto method = request.getMethod();

    Poco::JSON::Object::Ptr jsonResponse = new Poco::JSON::Object;

    try
    {
        if (path == "/todos" && method == Poco::Net::HTTPRequest::HTTP_GET)
        {
            // Get all Todos
            auto todos = storage_.getAllTodos();
            jsonResponse->set("todos", JsonUtil::serializeTodos(todos));
        }
        else if (path.find("/todos/") == 0 && method == Poco::Net::HTTPRequest::HTTP_GET)
        {
            // Get a single Todo
            int id = std::stoi(path.substr(7));
            auto todo = storage_.getTodo(id);
            jsonResponse = JsonUtil::serializeTodo(todo);
        }
        else if (path == "/todos" && method == Poco::Net::HTTPRequest::HTTP_POST)
        {
            // Create a Todo
            Poco::JSON::Parser parser;
            auto parsedBody = parser.parse(request.stream()).extract<Poco::JSON::Object::Ptr>();

            std::string title, description;
            JsonUtil::deserializeCreateTodo(parsedBody, title, description);

            int id = storage_.createTodo(title, description);
            jsonResponse->set("id", id);

            wsHandler_.broadcastMessage("{\"action\":\"createTodo\",\"id\":" + std::to_string(id) + "}");
        }
        else if (path.find("/todos/") == 0 && method == Poco::Net::HTTPRequest::HTTP_PUT)
        {
            // Modify a Todo
            int id = std::stoi(path.substr(7));

            Poco::JSON::Parser parser;
            auto parsedBody = parser.parse(request.stream()).extract<Poco::JSON::Object::Ptr>();

            std::string title, description;
            bool completed;
            JsonUtil::deserializeModifyTodo(parsedBody, title, description, completed);

            storage_.modifyTodo(id, title, description, completed);
            jsonResponse->set("id", id);

            wsHandler_.broadcastMessage("{\"action\":\"modifyTodo\",\"id\":" + std::to_string(id) + "}");
        }
        else if (path.find("/todos/") == 0 && method == Poco::Net::HTTPRequest::HTTP_DELETE)
        {
            // Delete a Todo
            int id = std::stoi(path.substr(7));
            storage_.deleteTodo(id);

            jsonResponse->set("id", id);
            wsHandler_.broadcastMessage("{\"action\":\"deleteTodo\",\"id\":" + std::to_string(id) + "}");
        }
        else
        {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
            jsonResponse->set("error", "Invalid endpoint or method");
        }
    }
    catch (std::exception& ex)
    {
        response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        jsonResponse->set("error", ex.what());
    }

    std::ostream& responseStream = response.send();
    jsonResponse->stringify(responseStream);
}

#pragma endregion TodoHandler
