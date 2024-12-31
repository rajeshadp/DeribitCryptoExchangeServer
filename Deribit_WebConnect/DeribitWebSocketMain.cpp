//
// Created by Rajesh on 20/10/24.
//

#include "DeribitWebSocketMain.h"

/*void signal_handler(int signum) {
    if (server_instance) {
        server_instance->stop();  // Call stop on the server instance
    }
    //    running = false;  // Set running to false
}*/

int main() {
//    signal(SIGINT, signal_handler); // Register signal handler
//    signal(SIGTERM, signal_handler);  // Handle termination signal
     try{
        std::string client_id = "edHJ8UTX";
        std::string client_secret = "eBV7vaP0YinTIB3nsqoh5F7BQpOu-NMnRH6A9VanBzI";

        // Create and run the WebSocket server
        WebSocketServer server ("https://test.deribit.com/api/v2/public/auth",client_id, client_secret);
        std::thread server_thread([&]() {
            server.run(9002);  // Port for the WebSocket server
        });

        // Start sending updates in a separate thread
        std::thread update_thread([&server]() { server.start_sending_updates(); });

        // Join the server thread (block main thread)
        server_thread.join();
        update_thread.join();
    }
    catch (const std::exception& e) {
          std::cerr << "From main() caugh a generic exception: " << e.what() << std::endl;
    }
    catch (...) {
         std::cerr << "From main() caught a universal exception " << std::endl;
    }

    std::cout << "Server stopped. Exiting..." << std::endl;
    return 0;
}
