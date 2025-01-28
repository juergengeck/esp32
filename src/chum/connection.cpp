#include "connection.h"
#include "websocket_wrapper.h"
#include "message_serializer.h"
#include <WiFi.h>
#include "websocket_types.h"

namespace chum {

struct Connection::WebSocketImpl {
    std::unique_ptr<WebSocketWrapper> server;
    std::unique_ptr<WebSocketWrapper> client;
    
    WebSocketImpl() : server(nullptr), client(nullptr) {}
    ~WebSocketImpl() {
        if (server) server->disconnect();
        if (client) client->disconnect();
    }
};

Connection::Connection() 
    : impl_(std::make_unique<WebSocketImpl>())
    , state_(ConnectionState::NotConnected)
    , lastHeartbeat_(0)
    , securityInitialized_(false) {}

Connection::~Connection() = default;

bool Connection::initServer(uint16_t port) {
    if (impl_->server) {
        return false;  // Already initialized
    }

    impl_->server = std::make_unique<WebSocketWrapper>();
    impl_->server->begin("0.0.0.0", port, "/");
    
    // Create a strongly-typed event handler
    WebSocketWrapper::EventHandler serverHandler = 
        [this](chum::WSEventType event, const uint8_t* payload, size_t length) {
            this->handleWebSocketEvent(event, payload, length);
        };
    impl_->server->onEvent(serverHandler);

    setState(ConnectionState::Connected);  // Server is always in Connected state when running
    startHeartbeat();
    return true;
}

void Connection::stopServer() {
    if (impl_->server) {
        impl_->server->disconnect();
        impl_->server.reset();
        setState(ConnectionState::NotConnected);
        stopHeartbeat();
    }
}

bool Connection::connectToPeer(const char* host, uint16_t port) {
    if (impl_->client) {
        return false;  // Already connected
    }

    impl_->client = std::make_unique<WebSocketWrapper>();
    impl_->client->begin(host, port, "/");
    
    // Create a strongly-typed event handler
    WebSocketWrapper::EventHandler clientHandler = 
        [this](chum::WSEventType event, const uint8_t* payload, size_t length) {
            this->handleWebSocketEvent(event, payload, length);
        };
    impl_->client->onEvent(clientHandler);

    setState(ConnectionState::Connecting);
    return true;
}

void Connection::disconnect() {
    if (impl_->client) {
        impl_->client->disconnect();
        impl_->client.reset();
    }
    setState(ConnectionState::NotConnected);
    stopHeartbeat();
}

bool Connection::sendMessage(MessageType type, const uint8_t* data, size_t length) {
    if (!isConnected()) {
        // Queue message for later
        messageQueue_.push({type, data, length});
        return false;
    }

    std::vector<uint8_t> encrypted;
    if (securityInitialized_) {
        auto signature = security_->sign(data, length);
        if (signature.empty()) {
            return false;
        }
        
        Message msg{
            .type = type,
            .payload = std::vector<uint8_t>(data, data + length),
            .signature = signature
        };
        
        auto serialized = MessageSerializer::serializeMessage(msg);
        data = serialized.data();
        length = serialized.size();
    }

    bool sent = false;
    if (impl_->client) {
        sent = impl_->client->sendBIN(data, length);
    } else if (impl_->server) {
        sent = impl_->server->sendBIN(data, length);
    }

    return sent;
}

void Connection::setMessageCallback(MessageCallback callback) {
    messageCallback_ = std::move(callback);
}

void Connection::setStateCallback(StateCallback callback) {
    stateCallback_ = std::move(callback);
}

void Connection::update() {
    // WebSocket events are handled by ESP32's event loop
    // No need to call loop() explicitly

    // Handle heartbeat
    if (isConnected() && millis() - lastHeartbeat_ >= HEARTBEAT_INTERVAL) {
        sendMessage(MessageType::Data, nullptr, 0);  // Use Data type for heartbeat
        lastHeartbeat_ = millis();
    }

    processMessageQueue();
}

bool Connection::isConnected() const {
    return state_ == ConnectionState::Connected;
}

ConnectionState Connection::getState() const {
    return state_;
}

void Connection::setState(ConnectionState newState) {
    if (state_ != newState) {
        state_ = newState;
        if (stateCallback_) {
            stateCallback_(newState);
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
    while (!messageQueue_.empty() && isConnected()) {
        const auto& msg = messageQueue_.front();
        if (sendMessage(msg.type, msg.data, msg.length)) {
            messageQueue_.pop();
        } else {
            break;  // Failed to send, try again later
        }
    }
}

void Connection::handleWebSocketEvent(WSEventType type, const uint8_t* payload, size_t length) {
    switch (type) {
        case WEBSOCKET_EVENT_CONNECTED:
            setState(ConnectionState::Connected);
            if (securityInitialized_) {
                // Send our public key
                const auto& keyPair = security_->getKeyPair();
                sendMessage(MessageType::KeyExchange,
                          keyPair.publicKey.data(),
                          keyPair.publicKey.size());
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            setState(ConnectionState::NotConnected);
            break;

        case WEBSOCKET_EVENT_DATA:
            if (length > 0) {
                auto msg = MessageSerializer::deserializeMessage(payload, length);
                if (msg) {
                    if (msg->type == MessageType::KeyExchange) {
                        handleKeyExchange(msg->payload.data(), msg->payload.size());
                    } else if (messageCallback_) {
                        if (securityInitialized_ && !msg->signature.empty()) {
                            if (security_->verify(msg->payload.data(), msg->payload.size(),
                                               msg->signature.data(), msg->signature.size(),
                                               peerPublicKey_)) {
                                messageCallback_(*msg);
                            }
                        } else {
                            messageCallback_(*msg);
                        }
                    }
                }
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            Serial.println("WebSocket error occurred");
            break;

        default:
            break;
    }
}

bool Connection::initSecurity() {
    if (!security_) {
        security_ = std::make_unique<Security>();
    }
    securityInitialized_ = security_->generateKeyPair();
    return securityInitialized_;
}

const KeyPair& Connection::getKeyPair() const {
    static const KeyPair empty;
    return security_ ? security_->getKeyPair() : empty;
}

bool Connection::handleKeyExchange(const uint8_t* data, size_t length) {
    if (!security_ || !securityInitialized_) {
        return false;
    }

    peerPublicKey_.assign(data, data + length);
    return true;
}

bool Connection::verifyMessage(const Message& msg) {
    if (!security_ || !securityInitialized_ || msg.signature.empty()) {
        return false;
    }
    return security_->verify(msg.payload.data(), msg.payload.size(),
                           msg.signature.data(), msg.signature.size(),
                           peerPublicKey_);
}

} // namespace chum 