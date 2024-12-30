//
// Created by Rajesh on 20/10/24.
//

#ifndef OAUTH2CLIENT_H
#define OAUTH2CLIENT_H

#include <iostream>
#include <curl/curl.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <string>
#include <thread>
#include <mutex>
#include <set>
#include <unordered_set>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <csignal>
#include <atomic>
#include <cpprest/http_client.h>
#include <cpprest/json.h>
#include <iostream>

#include "Observer.h"

using namespace web;
using namespace web::http;
using namespace web::http::client;

class OAuth2Client {
public:
    OAuth2Client(const std::string& auth_url, /*const std::string& refresh_url,*/ const std::string& client_id, const std::string& client_secret);
    pplx::task<std::string> authenticate();
    void refresh_token_if_expired();
    void register_observer(Observer* observer);
    void unregister_observer(Observer* observer);
    pplx::task<std::string> send_json_rpc_request(const std::string& url, const std::string& request_body, const std::string& access_token = "");
    std::string get_access_token() const;
    // This WriteCallback is only requried when we use curl in send_json_rpc_request as a callback, comment it if REST is used
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

private:
    pplx::task<void> refresh_access_token();
    void notify_observers(const std::string& new_token);

    std::vector<Observer*> observers_;  //when Deribit API using this oauth2 authorization need to be updated regarding refreshed token
    std::string auth_url_;
    std::string refresh_url_;
    std::string client_id_;
    std::string client_secret_;
    std::string access_token_;
    std::string refresh_token_;
    int expires_in_; // in seconds
    //    int64_t token_expiry_time_; // time in microseconds since epoch when the token expires
    std::chrono::system_clock::time_point token_expiry_time_;

};



#endif //OAUTH2CLIENT_H
