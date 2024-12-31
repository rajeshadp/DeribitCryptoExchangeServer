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

I did the coding on debian VM.
Used Websocket++ (basically depends on boost Asio libraries)
cpprestsdk for http api calls, pplx::task (parallel pattern libraries for concurrent tasks and achieving non-blocking asyn backend calls for server to reduce latency) , also used batch processing. 
and other standard c++ features like multi-threading, etc.
