Objective: Create an order execution and management system to trade on
Deribit Test (https://test.deribit.com/).

Where to start:
•⁠ ⁠Create a new Deribit Test
account
•⁠ ⁠Generate a set of API Keys

Functions:
- Place order
- Cancel order
- Modify order
- Get orderbook
- View current positions

Scope:
•⁠ ⁠Spot, futures and options
•⁠ ⁠All supported symbols

First on Deribit test server I have created public key for OAuth2 authentication which are valid for 30 days need to validate again.

Focus:
- Full functionality of above functions with low latency

Advanced Bonus:
•⁠ ⁠Create a websocket server that clients can connect to and subscribe to a symbol by sending a message.
•⁠ ⁠The server should respond with a stream of messages with the orderbook updates for each symbol that is
subscribed to

Brief technical code flow:
==========================
This code is basically a Server (C++) Order management system which accepts subscriptions from clients for certain Deribit exchange test server instruments from various clients, then periodically information of these instruments is fetched from Deribit test server using its apis which need OAuth2 authentication for each api call, which is a time bound token authorization mechanism, which also provides refresh tokens to save redundency. Data fetched from Deribit crypto test server is then segregated and pushed to clients as per their subscribed instruments. 
Server uses websockets for long term connections and huge data transfers.
Server works in 3 threads, 
main thread to listen client connections,
second thread to register/unregister subscriptions from client, to validate if OAuth2 token is still valid
thread three, does a periodic execution of api calls in batch mode (i.e. for different instruments calls made in async mode so that server thread doesnt block itself waiting for response from Deribit exchange test server and response is segregated once all responses are acquired, used pplx library for this purpose which is industry proven and efficient than std::async) and segregated response is then pushed to corresponding clients.
API calls use json RPC mechanism version2 which is the latest used by industry for efficient/performance proven way to api transfer data

Coding done on debian(also compatible to Ubuntu).
Used Websocket++ library with async calling to allow max throughput for server while listening to connections. (basically depends on boost Asio libraries)
cpprestsdk for http api calls, pplx::task (parallel pattern libraries for concurrent tasks and achieving non-blocking asyn backend calls for server to reduce latency) , also used batch processing. 
and other standard c++ features like multi-threading, etc.
