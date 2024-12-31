//
// Created by Rajesh on 20/10/24.
//

#include "OAuth2Client.h"

OAuth2Client::OAuth2Client(const std::string& auth_url, /*const std::string& refresh_url,*/ const std::string& client_id, const std::string& client_secret)
        : auth_url_(auth_url), refresh_url_(auth_url), client_id_(client_id), client_secret_(client_secret), token_expiry_time_(std::chrono::system_clock::time_point::min()) {}

    // Authenticate method to obtain access and refresh tokens using client_id and client_secret
	// call when you don't have a refresh token (e.g., during the initial login or if the refresh token has expired)
pplx::task<std::string> OAuth2Client::authenticate() {
  try{
    rapidjson::Document requestBody;
    requestBody.SetObject();
    rapidjson::Document::AllocatorType& allocator = requestBody.GetAllocator();

    requestBody.AddMember("id", 0, allocator);
    requestBody.AddMember("method", "public/auth", allocator);

    rapidjson::Value params(rapidjson::kObjectType);
    params.AddMember("grant_type", "client_credentials", allocator);
    params.AddMember("scope", "spot futures options", allocator);
    params.AddMember("client_id", rapidjson::Value().SetString(client_id_.c_str(), allocator), allocator);
    params.AddMember("client_secret", rapidjson::Value().SetString(client_secret_.c_str(), allocator), allocator);

    requestBody.AddMember("params", params, allocator);
    requestBody.AddMember("jsonrpc", "2.0", allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    requestBody.Accept(writer);

    // Asynchronous send_json_rpc_request
    return send_json_rpc_request(auth_url_, buffer.GetString())
        .then([this](std::string response) {
            rapidjson::Document json_response;
            if (json_response.Parse(response.c_str()).HasParseError()) {
                std::cerr << "JSON Parse Error: " << rapidjson::GetParseError_En(json_response.GetParseError()) << std::endl;
                return std::string("");
            }

			if (json_response.HasMember("result")) {
                const rapidjson::Value& result = json_response["result"];
                if (result.HasMember("access_token")) {
                    access_token_ = result["access_token"].GetString();
                }
                if (result.HasMember("refresh_token")) {
                    refresh_token_ = result["refresh_token"].GetString();
                }
                if (result.HasMember("expires_in")) {
                    expires_in_ = result["expires_in"].GetInt();
                    token_expiry_time_ = std::chrono::system_clock::now() + std::chrono::seconds(expires_in_);
                }
              return access_token_;
            }
    std::cerr << "Failed to authenticate." << std::endl;
    return std::string("");
    });
  }
  catch (const std::exception& e) {
            std::cerr << "Failed to authenticate. " << e.what() << std::endl;
    }
    return pplx::task_from_result<std::string>("");
}


    // Refresh token if access token has expired
void OAuth2Client::refresh_token_if_expired() {
     try{
        auto current_time = std::chrono::system_clock::now();//.time_since_epoch().count();
        if (current_time >= token_expiry_time_) {
            std::cout << "Access token has expired. Refreshing..." << std::endl;
            refresh_access_token();
        } else {
            std::cout << "Access token is still valid." << std::endl;
        }
     }
  catch (const std::exception& e) {
            std::cerr << "Failed to authenticate. " << e.what() << std::endl;
    }
}

// Get the current access token
std::string OAuth2Client::get_access_token() const {
        return access_token_;
}

void OAuth2Client::register_observer(Observer* observr) {
        observers_.push_back(observr);
}

void OAuth2Client::unregister_observer(Observer* observer) {
        observers_.erase(std::remove(observers_.begin(), observers_.end(), observer), observers_.end());
}

// Helper function to send JSON-RPC requests
pplx::task<std::string> OAuth2Client::send_json_rpc_request(const std::string& url, const std::string& request_body, const std::string& access_token) {
  try{
    // Create an HTTP client
    http_client client(U(url));

    // Create a request
    http_request request(methods::POST);
	//request.headers().remove(U("Content-Length"));
	//request.headers().add(U("Transfer-Encoding"), U("chunked"));
	request.headers().clear(); // Clear existing headers
	request.headers().add(U("Content-Type"), U("application/json"));
//    request.set_version(http::http_version::HTTP_1_1);  // Explicitly set HTTP version

    // Set Authorization header if access token is provided
    if (!access_token.empty()) {
        request.headers().add(U("Authorization"), U("Bearer ") + U(access_token));
        std::cout << "Authorization: Bearer " << access_token << std::endl;  // Print to verify
    }

    // Set the request body as a json value (optional)
    request.set_body(utility::conversions::to_string_t(request_body));  // Convert string to utility::string_t

///  //Debug statements to check HTTP request
///    // Print Request URL
///    std::cout << "Request URL: " << url << std::endl;
///
///    // Print Headers
///    std::cout << "Request Headers:" << std::endl;
///    for (const auto& header : request.headers()) {
///        std::cout << header.first << ": " << header.second << std::endl;
///    }
///
///    // Print Request Body
///    std::cout << "Request Body: " << request_body << std::endl;
///
    // Send the request asynchronously
    return client.request(request)
        .then([](http_response response) {
            // Handle the response when it's received
            if (response.status_code() == status_codes::OK) {
                return response.extract_string(); // Extract the response body as a string
            } else {
                std::cerr << "Error: " << response.status_code() << " - " << response.reason_phrase() << std::endl;
                return pplx::task_from_result(std::string("")); // Return empty string on error
            }
        })
        .then([](pplx::task<std::string> previous_task) {
            try {
                return previous_task.get(); // Return the actual response
            } catch (const std::exception& e) {
                std::cerr << "Exception: " << e.what() << std::endl;
                return std::string("");
            }
        });
   }
   catch (const std::exception& e) {
            std::cerr << "Request to url "<< url << "Failed with "<< e.what() << std::endl;
   }
   return pplx::task_from_result<std::string>("");
}

/*
// Helper function to send JSON-RPC requests below is curl implementation
// works fine, but is commented as curl api calls work in synchronous way, which wait for http response
// which block and introduce latency with high volume of requests/connections.
std::string send_json_rpc_request(const std::string& url, const std::string& request_body, const std::string& access_token = "") {
        CURL* curl;
        CURLcode res;
        std::string readBuffer;
     try{
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();

        if (curl) {
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");

            if (!access_token.empty()) {
                std::string authHeader = "Authorization: Bearer " + access_token;
                headers = curl_slist_append(headers, authHeader.c_str());
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OAuth2Client::WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            res = curl_easy_perform(curl);

            if (res != CURLE_OK) {
                std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            }

            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
        }
        curl_global_cleanup();
    } catch (const std::exception& e) {
            std::cerr << "Exception in refresh_access_token: " << e.what() << std::endl;
    }
    return readBuffer;
}
*/

// Function to handle the response from the server and write it into a string
// This function is only requried when we use curl in send_json_rpc_request as a callback comment it if REST is used
size_t OAuth2Client::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
}

// Refresh the access token using the refresh token
// When access token expires but we still have a valid refresh token, to get a new access token without needing to re-authenticate.
// it's a good idea to have a method instead of re-using authenticate(), it helps keep the authentication process efficient and well-structured.
// especially if we are handling token expiration and refresh cycles, this also notifies observers.
pplx::task<void> OAuth2Client::refresh_access_token() {
 try{
    if (refresh_token_.empty()) {
        std::cerr << "No refresh token available. Please authenticate first." << std::endl;
        return pplx::task_from_result();
    }

    // Construct the JSON-RPC request body for refreshing the token
    rapidjson::Document requestBody;
    requestBody.SetObject();
    rapidjson::Document::AllocatorType& allocator = requestBody.GetAllocator();
    requestBody.AddMember("id", 0, allocator);
    requestBody.AddMember("method", "public/auth", allocator);
    rapidjson::Value params(rapidjson::kObjectType);
    params.AddMember("grant_type", "refresh_token", allocator);  // Use refresh token grant type
    params.AddMember("refresh_token", rapidjson::Value().SetString(refresh_token_.c_str(), allocator), allocator);
    requestBody.AddMember("params", params, allocator);
    requestBody.AddMember("jsonrpc", "2.0", allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    requestBody.Accept(writer);

    // Asynchronous send_json_rpc_request
    return send_json_rpc_request(refresh_url_, buffer.GetString())
        .then([this](std::string response) {
            rapidjson::Document json_response;
            if (json_response.Parse(response.c_str()).HasParseError()) {
                std::cerr << "JSON Parse Error: " << rapidjson::GetParseError_En(json_response.GetParseError()) << std::endl;
                return;
            }

            if (json_response.HasMember("result")) {
                const rapidjson::Value& result = json_response["result"];
                if (result.HasMember("access_token")) {
                    access_token_ = result["access_token"].GetString();
                    std::cout << "Access token refreshed successfully! New Access Token: " << access_token_ << std::endl;
                }
                if (result.HasMember("refresh_token")) {
                    refresh_token_ = result["refresh_token"].GetString();
                    std::cout << "Refresh token updated: " << refresh_token_ << std::endl;
                }

                if (result.HasMember("expires_in")) {
                    expires_in_ = result["expires_in"].GetInt();
                    token_expiry_time_ = std::chrono::system_clock::now() + std::chrono::seconds(expires_in_);
                }

                // Notify observers after token update
                notify_observers(access_token_);
            } else {
                std::cerr << "Failed to refresh the access token." << std::endl;
            }
        });
   }
  catch (const std::exception& e) {
            std::cerr << "Failed to refresh the access token." << e.what() << std::endl;
  }
         return pplx::task_from_result();
}

//when new token is acquired, this need to be updated to all subscribed/ in this context websocketserver
void OAuth2Client::notify_observers(const std::string& new_token) {
            // Notify each observer
        for (auto observer : observers_) {
            observer->on_token_updated(new_token);
        }
}