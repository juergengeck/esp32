#include "connection.h"
#include <Arduino.h>

namespace chum {

Connection::Connection()
    : server_(nullptr)
    , client_(nullptr)
    , state_(ConnectionState::NotConnected)
    , lastHeartbeat_(0)
    , messageCallback_(nullptr)
    , stateCallback_(nullptr) {
}

Connection::~Connection() {
    stopServer();
    disconnect();
    delete server_;
    delete client_;
}

bool Connection::initServer(uint16_t port) {
    if (state_ != ConnectionState::NotConnected) {
        return false;
    }

    if (!server_) {
        server_ = new WebSocketsServer(port);
    }

    server_->onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        handleWebSocketEvent(type, payload, length);
    });

    server_->begin();
    setState(ConnectionState::Connecting);
    return true;
}

void Connection::stopServer() {
    if (server_) {
        server_->close();
        setState(ConnectionState::NotConnected);
    }
}

bool Connection::connectToPeer(const char* host, uint16_t port) {
    if (state_ != ConnectionState::NotConnected) {
        return false;
    }

    if (!client_) {
        client_ = new WebSocketsClient();
    }

    client_->onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        handleWebSocketEvent(type, payload, length);
    });

    client_->begin(host, port, "/");
    setState(ConnectionState::Connecting);
    return true;
}

void Connection::disconnect() {
    if (client_) {
        client_->disconnect();
    }
    setState(ConnectionState::NotConnected);
    stopHeartbeat();
}

bool Connection::sendMessage(MessageType type, const uint8_t* data, size_t length) {
    if (state_ != ConnectionState::Connected) {
        return false;
    }

    // Create message header (type + length)
    uint8_t header[3];
    header[0] = static_cast<uint8_t>(type);
    header[1] = length & 0xFF;
    header[2] = (length >> 8) & 0xFF;

    // Send header
    if (client_) {
        client_->sendBinary(header, 3);
        if (length > 0) {
            client_->sendBinary(data, length);
        }
    } else if (server_) {
        server_->broadcastBIN(header, 3);
        if (length > 0) {
            server_->broadcastBIN(data, length);
        }
    }

    return true;
}

void Connection::setMessageCallback(MessageCallback callback) {
    messageCallback_ = callback;
}

void Connection::setStateCallback(StateCallback callback) {
    stateCallback_ = callback;
}

void Connection::update() {
    if (server_) {
        server_->loop();
    }
    if (client_) {
        client_->loop();
    }

    // Handle heartbeat
    if (state_ == ConnectionState::Connected) {
        uint32_t now = millis();
        if (now - lastHeartbeat_ >= HEARTBEAT_INTERVAL) {
            sendMessage(MessageType::HEARTBEAT, nullptr, 0);
            lastHeartbeat_ = now;
        }
    }

    processMessageQueue();
}

bool Connection::isConnected() const {
    return state_ == ConnectionState::Connected;
}

ConnectionState Connection::getState() const {
    return state_;
}

void Connection::handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            setState(ConnectionState::Connected);
            startHeartbeat();
            break;

        case WStype_DISCONNECTED:
            setState(ConnectionState::NotConnected);
            stopHeartbeat();
            break;

        case WStype_BIN:
            if (length >= 3) { // Minimum length for header
                MessageType msgType = static_cast<MessageType>(payload[0]);
                uint16_t msgLength = (payload[2] << 8) | payload[1];
                
                if (length >= msgLength + 3) { // Full message received
                    Message msg = {
                        .type = msgType,
                        .length = msgLength,
                        .data = (msgLength > 0) ? &payload[3] : nullptr
                    };
                    
                    if (messageCallback_) {
                        messageCallback_(msg);
                    }
                }
            }
            break;

        case WStype_ERROR:
            // Handle error
            break;

        default:
            break;
    }
}

void Connection::setState(ConnectionState newState) {
    if (state_ != newState) {
        state_ = newState;
        if (stateCallback_) {
            stateCallback_(state_);
        }
    }
}

void Connection::startHeartbeat() {
    lastHeartbeat_ = millis();
}

void Connection::stopHeartbeat() {
    lastHeartbeat_ = 0;
}

void Connection::processMessageQueue() {
    while (!messageQueue_.empty() && state_ == ConnectionState::Connected) {
        const Message& msg = messageQueue_.front();
        if (sendMessage(msg.type, msg.data, msg.length)) {
            messageQueue_.pop();
        } else {
            break;
        }
    }
}

} // namespace chum 