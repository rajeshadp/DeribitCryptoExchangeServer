//
// Created by root1 on 20/10/24.
//

#include "DeribitWebSocketMain.h"

WebSocketServer *server_instance = nullptr;  // Global pointer to the server instance, to gracefully exit the server
std::atomic<bool> input_received(false); // Flag to signal input reception
std::mutex mtx;
std::condition_variable cv;

void input_thread() {
    std::string user_input;
    std::cout << "Enter something (you have 5 seconds): ";

    if (std::cin >> user_input) {
        std::lock_guard<std::mutex> lock(mtx);
        input_received = true;
        cv.notify_one(); // Notify that input has been received
        std::cout << "You entered: " << user_input << std::endl;
        if(user_input == "end")
        {
          server_instance->stop();
        }
    }
}
/*void signal_handler(int signum) {
    if (server_instance) {
        server_instance->stop();  // Call stop on the server instance
    }
    //    running = false;  // Set running to false
}*/

int main() {
//    signal(SIGINT, signal_handler); // Register signal handler
//    signal(SIGTERM, signal_handler);  // Handle termination signal
//    signal(SIGSEGV, signal_handler); // Not advicable, as to detect any crashes, Register signal handler

    std::string client_id = "edHJ8UTX";
    std::string client_secret = "eBV7vaP0YinTIB3nsqoh5F7BQpOu-NMnRH6A9VanBzI";

 /*   // Authenticate and get the access token
    access_token = authenticate(client_id, client_secret);
    if (access_token.empty()) {
        std::cerr << "Failed to authenticate." << std::endl;
        return 1;
    }*/

    // Create and run the WebSocket server
    WebSocketServer server ("https://test.deribit.com/api/v2/public/auth",client_id, client_secret);
    server_instance=&server;
    std::thread server_thread([&]() {
        server.run(9002);  // Port for the WebSocket server
    });

    // Start sending updates in a separate thread
    std::thread update_thread([&server]() { server.start_sending_updates(); });
    // Join the server thread (block main thread)

/*    // Wait for the running flag to be set to false
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }*/

//    server.stop(); // Stop the server gracefully
 ////////For performance testing --start here
   std::thread input(input_thread);
   std::atomic<bool> running(true);

while(running)
{
    std::unique_lock<std::mutex> lock(mtx);

    // Wait for 5 seconds or until input is received
    if (cv.wait_for(lock, std::chrono::seconds(5), []{ return input_received.load(); })) {
        std::cout << "Input received before timeout!" << std::endl;
        running=false;
    } else {
        std::cout << "\nTimeout! No input received, try again 10 sec" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

}
    input.join(); // Join the input thread
 ////////For performance testing --end here

    server_thread.join();

    update_thread.join();

    std::cout << "Server stopped. Exiting..." << std::endl;
    return 0;
}
