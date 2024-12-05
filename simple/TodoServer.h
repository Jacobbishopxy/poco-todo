/**
 * @file:	TodoServer.h
 * @author:	Jacob Xie
 * @date:	2024/12/04 13:13:50 Wednesday
 * @brief:
 **/

#ifndef __TODOSERVER__H__
#define __TODOSERVER__H__

#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

#include "Poco/JSON/Object.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/WebSocket.h"

struct Todo
{
    int id;
    std::string title;
    std::string description;
    bool completed;
};

class JsonUtil
{
public:
    static Poco::JSON::Object::Ptr serializeTodo(const Todo& todo);

    static Poco::JSON::Array::Ptr serializeTodos(const std::unordered_map<int, Todo>& todos);

    static void deserializeCreateTodo(const Poco::JSON::Object::Ptr& json, std::string& title, std::string& description);

    static void deserializeModifyTodo(const Poco::JSON::Object::Ptr& json, std::string& title, std::string& description, bool& completed);
};

class TodoStorage
{
public:
    TodoStorage();
    int createTodo(const std::string& title, const std::string& description);
    Todo getTodo(int id);
    std::unordered_map<int, Todo> getAllTodos();
    void modifyTodo(int id, const std::string& title, const std::string& description, bool completed);
    void deleteTodo(int id);

private:
    std::unordered_map<int, Todo> todos_;
    int nextId_;
    std::mutex mutex_;
};

class WebSocketHandler : public Poco::Net::HTTPRequestHandler
{
public:
    WebSocketHandler() = default;
    ~WebSocketHandler();

    WebSocketHandler(const WebSocketHandler& sharedHandler) = delete;  // Share state between instances
    WebSocketHandler& operator=(const WebSocketHandler&) = delete;

    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

    void broadcastMessage(const std::string& message);

private:
    void addClient(Poco::Net::WebSocket* ws);
    void removeClient(Poco::Net::WebSocket* ws);

private:
    Poco::Mutex clientsMutex_;                 // Mutex for thread-safe access to clients
    std::set<Poco::Net::WebSocket*> clients_;  // Set of active WebSocket clients
};

class WebSocketConnectionHandler : public Poco::Net::HTTPRequestHandler {
public:
    explicit WebSocketConnectionHandler(WebSocketHandler& sharedHandler)
        : sharedHandler_(sharedHandler) {}

    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
        sharedHandler_.handleRequest(request, response);
    }

private:
    WebSocketHandler& sharedHandler_; // Reference to the shared WebSocketHandler
};

class TodoHandler : public Poco::Net::HTTPRequestHandler
{
public:
    TodoHandler(TodoStorage& storage, WebSocketHandler& wsHandler);

    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    TodoStorage& storage_;
    WebSocketHandler& wsHandler_;
};

#endif  //!__TODOSERVER__H__
