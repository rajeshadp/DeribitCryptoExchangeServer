# Makefile for the WebSocket server application

# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17

# Libraries to link against
LIBS = -lboost_system -lboost_thread -lpthread -lcpprest -lssl -lcrypto
#CURL_LIB = -lcurl  # Comment this line if you want to omit -lcurl

# If you want to include it, uncomment the line above and change to:
#LIBS += $(CURL_LIB)

# Source files
SRC = DeribitWebSocketMain.cpp OAuth2Client.cpp WebSocketServer.cpp

# Output binary
TARGET = wss_rest

# Default target
all: $(TARGET)

# Build target
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Clean up generated files
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all clean
