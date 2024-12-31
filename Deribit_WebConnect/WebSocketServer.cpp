//
// Created by Rajesh on 20/10/24.
//

#include "WebSocketServer.h"

using namespace web;
using namespace web::http;
using namespace web::http::client;

std::atomic<bool> sending_updates_running(true); // Flag for status update thread, useful to gracefully stop the server

WebSocketServer::WebSocketServer(const std::string& auth_url,const std::string& client_id, const std::string& client_secret)
        : oauth2_(auth_url, client_id,client_secret), authenticated(false) {
        server_.set_open_handler(bind(&WebSocketServer::on_open, this, std::placeholders::_1));
        server_.set_close_handler(bind(&WebSocketServer::on_close, this, std::placeholders::_1));
        server_.set_fail_handler(bind(&WebSocketServer::on_fail, this, std::placeholders::_1));
        server_.set_message_handler(bind(&WebSocketServer::on_message, this, std::placeholders::_1, std::placeholders::_2));

       oauth2_.register_observer(this); // Register as an observer
    }

WebSocketServer::~WebSocketServer() {
        oauth2_.unregister_observer(this); // Unregister upon destruction
    }

//server after socket creation, start running by calling this callback,
//server starts to listen to port and accept new connections
void WebSocketServer::run(uint16_t port) {
        server_.init_asio();
        server_.listen(port);
        server_.start_accept();
        server_.run();
}

void WebSocketServer::start_sending_updates() {
    try {
        while (sending_updates_running) {
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Send updates every 5 seconds

            std::lock_guard<std::mutex> lock(subscriptions_mutex); // Protect subscriptions_ during access

            // Simulate an order book update
            for (const auto& subscription : subscriptions_) {
            const auto& symbol = subscription.first;   // Capture symbol separately
            const auto& clients = subscription.second; // Capture clients separately
                for (auto client_hdl : clients) {
                    try {
                        // Get the connection object from the handle
                  //      server::connection_ptr con = server_.get_con_from_hdl(client_hdl);

                        // Batch response accumulation to reduce WebSocket overhead
                        std::string batch_response = "";

                        // Initiate parallel async tasks to fetch order book and positions
                        pplx::task<std::string> orderbook_task = get_orderbook(symbol);
                        pplx::task<std::string> positions_task = view_positions();

                        pplx::task<std::string> plceorder_task = place_order(symbol, 1.0, 40000);
                        pplx::task<std::string> mdifyorder_task = modify_order(symbol, 45000);
                        pplx::task<std::string> cancelorder_task = cancel_order(symbol);
                        // Place order and modify order immediately
                        //batch_response += "Order placed= " + place_order(symbol, 1.0, 40000).get() + "\n";
                        //batch_response += "Modify Order= " + modify_order(symbol, 45000).get() + "\n";

                        // Fetch results in parallel
                        std::vector<pplx::task<std::string>> tasks = {orderbook_task, positions_task, plceorder_task, mdifyorder_task, cancelorder_task};
                        pplx::when_all(tasks.begin(), tasks.end()).then([this, client_hdl, batch_response, symbol](pplx::task<std::vector<std::string>> all_tasks) mutable {
                            try {
                               // Retrieve the result from each task
                                std::vector<std::string> results = all_tasks.get();  // Get the results from the tasks

                                // Build the batch response using the retrieved values
                                batch_response += "Order book = " + results[0] + "\n";  // First task: order book
                                batch_response += "Positions = " + results[1] + "\n";   // Second task: positions
                                batch_response += "placed order = " + results[2] + "\n";   // third task: placed order
                                batch_response += "modify order = " + results[3] + "\n";   // fourth task: modify order
                                batch_response += "cancel order = " + results[4] + "\n";   // fifth task: cancel order

                                // Send batch response over WebSocket
                                server_.send(client_hdl, batch_response, websocketpp::frame::opcode::text);
                            } catch (const std::exception& e) {
                                std::cerr << "Error processing batch response: " << e.what() << std::endl;
                                handle_disconnection(client_hdl);
                            }
                        });

                    } catch (const websocketpp::exception& e) {
                        std::cerr << "WebSocket++ exception while sending updates: " << e.what() << std::endl;
                        handle_disconnection(client_hdl);
                    } catch (const std::exception& e) {
                        handle_disconnection(client_hdl);
                        std::cerr << "Exception while sending updates: " << e.what() << std::endl;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception while sending updates: " << e.what() << std::endl;
    }
}

pplx::task<std::string> WebSocketServer::getAccessToken() const
{
   return access_token_;
}
// Function to get the order book
pplx::task<std::string> WebSocketServer::get_orderbook(const std::string& instrument_name) {
    rapidjson::Document requestBody;
    requestBody.SetObject();
    rapidjson::Document::AllocatorType& allocator = requestBody.GetAllocator();
    requestBody.AddMember("id", 4, allocator);
    requestBody.AddMember("method", "public/get_order_book", allocator);
    rapidjson::Value params(rapidjson::kObjectType);
    params.AddMember("instrument_name", rapidjson::Value().SetString(instrument_name.c_str(), allocator), allocator);
    requestBody.AddMember("params", params, allocator);
    requestBody.AddMember("jsonrpc", "2.0", allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    requestBody.Accept(writer);

    oauth2_.refresh_token_if_expired();
    return oauth2_.send_json_rpc_request("https://test.deribit.com/api/v2/public/get_order_book", buffer.GetString());
}

// Function to place an order, calls Deribit buy endpoint
pplx::task<std::string> WebSocketServer::place_order(const std::string& instrument_name, double amount, double price) {
    rapidjson::Document requestBody;
    requestBody.SetObject();
    rapidjson::Document::AllocatorType& allocator = requestBody.GetAllocator();
    requestBody.AddMember("id", 1, allocator);
    requestBody.AddMember("method", "private/buy", allocator);  // For selling, use "private/sell"
    rapidjson::Value params(rapidjson::kObjectType);
    params.AddMember("instrument_name", rapidjson::Value().SetString(instrument_name.c_str(), allocator), allocator);
    params.AddMember("amount", amount, allocator);
    params.AddMember("price", price, allocator);
    requestBody.AddMember("params", params, allocator);
    requestBody.AddMember("jsonrpc", "2.0", allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    requestBody.Accept(writer);

    oauth2_.refresh_token_if_expired();
    return oauth2_.send_json_rpc_request("https://test.deribit.com/api/v2/private/buy", buffer.GetString(), getAccessToken().get());
}

// Function to cancel an order
pplx::task<std::string> WebSocketServer::cancel_order(const std::string& order_id) {
    rapidjson::Document requestBody;
    requestBody.SetObject();
    rapidjson::Document::AllocatorType& allocator = requestBody.GetAllocator();

    requestBody.AddMember("id", 2, allocator);
    requestBody.AddMember("method", "private/cancel", allocator);

    rapidjson::Value params(rapidjson::kObjectType);
    params.AddMember("order_id", rapidjson::Value().SetString(order_id.c_str(), allocator), allocator);

    requestBody.AddMember("params", params, allocator);
    requestBody.AddMember("jsonrpc", "2.0", allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    requestBody.Accept(writer);

    oauth2_.refresh_token_if_expired();
    return oauth2_.send_json_rpc_request("https://test.deribit.com/api/v2/private/cancel", buffer.GetString(), getAccessToken().get());
}

// Function to modify an order
pplx::task<std::string> WebSocketServer::modify_order(const std::string& order_id, double price) {
    rapidjson::Document requestBody;
    requestBody.SetObject();
    rapidjson::Document::AllocatorType& allocator = requestBody.GetAllocator();
    requestBody.AddMember("id", 3, allocator);
    requestBody.AddMember("method", "private/edit", allocator);
    rapidjson::Value params(rapidjson::kObjectType);
    params.AddMember("order_id", rapidjson::Value().SetString(order_id.c_str(), allocator), allocator);
    params.AddMember("price", price, allocator);
    requestBody.AddMember("params", params, allocator);
    requestBody.AddMember("jsonrpc", "2.0", allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    requestBody.Accept(writer);
    oauth2_.refresh_token_if_expired();
    return oauth2_.send_json_rpc_request("https://test.deribit.com/api/v2/private/edit", buffer.GetString(), getAccessToken().get());
}

// Function to view current positions
pplx::task<std::string> WebSocketServer::view_positions() {
    rapidjson::Document requestBody;
    requestBody.SetObject();
    rapidjson::Document::AllocatorType& allocator = requestBody.GetAllocator();
    requestBody.AddMember("id", 5, allocator);
    requestBody.AddMember("method", "private/get_positions", allocator);
    rapidjson::Value params(rapidjson::kObjectType);
    params.AddMember("currency", "BTC", allocator);  // Modify to desired currency if needed
    requestBody.AddMember("params", params, allocator);
    requestBody.AddMember("jsonrpc", "2.0", allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    requestBody.Accept(writer);

    oauth2_.refresh_token_if_expired();
    return oauth2_.send_json_rpc_request("https://test.deribit.com/api/v2/private/get_positions", buffer.GetString(), getAccessToken().get());
}

//method to process server stop gracefully
void WebSocketServer::stop() {
        server_.stop(); // Stop the WebSocket server
        sending_updates_running=false;
        std::lock_guard<std::mutex> lock(subscriptions_mutex); // Lock to protect shared resources
        for (auto& [symbol, clients] : subscriptions_) {
            for (auto client_hdl : clients) {
                server_.close(client_hdl, websocketpp::close::status::normal, "Server shutting down"); // Notify clients and close connections
            }
        }
    }

//handler called when a new client connects to server.
void WebSocketServer::on_open(connection_hdl hdl) {
        std::cout<<"Client connected!"<<std::endl;
        //Instead of getting OAuth2 authorization in constructor, get token only when we get first client connection
        if (!authenticated)
        {
           // Authenticate and store access token
           authenticate_and_store_token();
           //we dont need to do this, as subsequent token are got through refresh
           authenticated=true;
         }
}

//invoked when a client gracefully closes the connection
// (e.g., the client sends a close frame or terminates the connection cleanly)
void WebSocketServer::on_close(connection_hdl hdl) {
      handle_disconnection(hdl); // Handle client disconnection
        std::cout << "Client disconnected!" << std::endl;
}

//invoked when the connection fails due to a network error or an unexpected disconnect
//(e.g., client crashes, network issues, etc.), we can inspect the error_code for specific details on why the connection failed.
void WebSocketServer::on_fail(websocketpp::connection_hdl hdl) {
        websocketpp::server<websocketpp::config::asio>::connection_ptr con = server_.get_con_from_hdl(hdl);
        websocketpp::lib::error_code ec = con->get_ec();

        std::cout << "Connection failed. Error: " << ec.message() << std::endl;

       // Compare error code values for specific conditions
        if (ec.value() == websocketpp::transport::error::eof) {
            std::cout << "Client closed the connection (EOF)." << std::endl;
        } /*else if (ec == websocketpp::error::operation_aborted) {
            std::cout << "Operation aborted, likely due to shutdown." << std::endl;
        } else if (ec == websocketpp::error::connection_reset) {
            std::cout << "Connection reset by peer (client lost connection)." << std::endl;
        } */else {
            std::cout << "Unknown error: " << ec.message() << std::endl;
        }

       handle_disconnection(hdl); // Handle client disconnection
}

//when a client disconnects, clean its subscriptions and its handle.
void WebSocketServer::handle_disconnection(websocketpp::connection_hdl hdl) {
        std::cout << "Client disconnected" << std::endl;
        // Remove the client from all subscription lists
        for (auto& [symbol, clients] : subscriptions_) {
            clients.erase(hdl);
        }
    }

//this is invoked when any client communications are received
//subscribe request from client is received and processed by this handler
//Below implementation expects client send json message like
//  ex:       {  "symbol": "AAPL"},
//  or        {  "symbol": "BTC-PERPETUAL"}
void WebSocketServer::on_message(connection_hdl hdl, server::message_ptr msg) {
        std::string payload = msg->get_payload();
        std::cout << "Received message: " << payload << std::endl;

        // Parse JSON message and process subscription
        rapidjson::Document doc;

        if (doc.Parse(payload.c_str()).HasParseError() || !doc.HasMember("symbol")) {
            server_.send(hdl, "Invalid subscription message", websocketpp::frame::opcode::text);
            return;
        }

        std::string symbol = doc["symbol"].GetString();
        subscribe(symbol, hdl);

        // Respond to the client
        server_.send(hdl, "Subscribed to " + symbol, websocketpp::frame::opcode::text);

/*      //Below code when we want client to just send Cryptocurrency for subscription like: "BTC-USD"
        std::string instrument_name = msg->get_payload();
        std::cout << "Received subscription for instrument: " << instrument_name << std::endl;

        // Fetch order book details
        std::string orderbook_response = get_orderbook(access_token, instrument_name);

        // Send order book details to the client
        server_.send(hdl, orderbook_response, websocketpp::frame::opcode::text); */

    }

// Subscribe a client to a symbol
void WebSocketServer::subscribe(const std::string& symbol, websocketpp::connection_hdl& hdl) {
        std::lock_guard<std::mutex> lock(subscriptions_mutex);  // Protect subscriptions_ during access
        subscriptions_[symbol].insert(hdl);
//        std::string temp("Entered subscribe :subscriptions_.size()=" + std::to_string(subscriptions_.size()) + " for symbol: "+symbol);
//        server_.send(hdl, temp, websocketpp::frame::opcode::text);  //Debug st
}

// Unsubscribe a client from a symbol
void WebSocketServer::unsubscribe(const std::string& symbol, websocketpp::connection_hdl& hdl) {
        std::lock_guard<std::mutex> lock(subscriptions_mutex);  // Protect subscriptions_ during access
        subscriptions_[symbol].erase(hdl);
        if (subscriptions_[symbol].empty()) {
            subscriptions_.erase(symbol);
        }
}

// Function to authenticate and store the access token
void WebSocketServer::authenticate_and_store_token() {
        access_token_ = oauth2_.authenticate();
        if (access_token_.get().empty()) {
            std::cerr << "Failed to authenticate and get access token!" << std::endl;
        } else {
            std::cout << "Access token acquired: " << access_token_.get() << std::endl;
        }
}

// Implementation of the Observer interface
void WebSocketServer::on_token_updated(const std::string& new_token) {
        // Update internal state or perform any necessary actions with the new token
        std::cout << "Access token updated: " << new_token << std::endl;
        access_token_ = pplx::task_from_result(new_token); // Store the new access token
}
