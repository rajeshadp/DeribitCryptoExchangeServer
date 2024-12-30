//
// Created by Rajesh on 20/10/24.
//

#ifndef WEBSOCKETSERVER_H
#define WEBSOCKETSERVER_H
#include <iostream>
#include "OAuth2Client.h"
#include "WebSocketServer.h"

// Type aliases for convenience
using server = websocketpp::server<websocketpp::config::asio>;
using websocketpp::connection_hdl;

// Flag for status update thread, useful to gracefully stop the server
extern std::atomic<bool> sending_updates_running;

// WebSocket server
class WebSocketServer : public Observer {
public:
    WebSocketServer(const std::string& auth_url,const std::string& client_id, const std::string& client_secret);
    ~WebSocketServer();
    void run(uint16_t port);
    void start_sending_updates();
    pplx::task<std::string> getAccessToken() const;
    pplx::task<std::string> get_orderbook(const std::string& instrument_name);
    pplx::task<std::string> place_order(const std::string& instrument_name, double amount, double price);
    pplx::task<std::string> cancel_order(const std::string& order_id);
    pplx::task<std::string> modify_order(const std::string& order_id, double price);
    pplx::task<std::string> view_positions();
    void stop();
private:

    //reference not used as cppsdk bind doesnt accept &
    void on_open(connection_hdl hdl);
    void on_close(connection_hdl hdl);
    void on_fail(websocketpp::connection_hdl hdl);
    void handle_disconnection(websocketpp::connection_hdl hdl);
    void on_message(connection_hdl hdl, server::message_ptr msg);
    void subscribe(const std::string& symbol, websocketpp::connection_hdl& hdl);
    void unsubscribe(const std::string& symbol, websocketpp::connection_hdl& hdl);
    void on_token_updated(const std::string& new_token) override;
    void authenticate_and_store_token();

private:
    // Oauth2 is a member of WebSocketServer (Composition)
    OAuth2Client oauth2_;
    pplx::task<std::string> access_token_;

    // Mutex to protect the subscriptions_
    std::mutex subscriptions_mutex;

    server server_;
    std::map<std::string, std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>>> subscriptions_;
    //flag to OAuth2 authentication with first client connection
    // this is not handled in constructor to avoid unnecessary network call and any subsequent refresh tokens without active client connections.
    bool authenticated;
};


#endif //WEBSOCKETSERVER_H
