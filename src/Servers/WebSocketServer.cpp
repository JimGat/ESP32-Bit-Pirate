#include "WebSocketServer.h"


static const char* TAG = "WebSocketServer";

WebSocketServer::WebSocketServer(httpd_handle_t sharedServer)
    : server(sharedServer) {}

void WebSocketServer::setupRoutes() {
    static httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = WebSocketServer::wsHandler,
        .user_ctx = this,
        .is_websocket = true
    };

    httpd_register_uri_handler(server, &ws_uri);
}

esp_err_t WebSocketServer::wsHandler(httpd_req_t *req) {
    WebSocketServer* self = static_cast<WebSocketServer*>(req->user_ctx);
    
    if (req->method == HTTP_GET) {
        int newClientFd = httpd_req_to_sockfd(req);
        if (clientFd >= 0 && clientFd != newClientFd) {
            closeClient(self->server, clientFd);
        }
        clientFd = newClientFd;
        return ESP_OK;
    }
    
    
    httpd_ws_frame_t frame = {};
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = nullptr;

    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    frame.payload = (uint8_t*)malloc(frame.len + 1);
    if (!frame.payload) return ESP_ERR_NO_MEM;

    ret = httpd_ws_recv_frame(req, &frame, frame.len);
    if (ret != ESP_OK) {
        free(frame.payload);
        if (clientFd == httpd_req_to_sockfd(req)) {
            clientFd = -1;
        }
        return ret;
    }
    frame.payload[frame.len] = '\0';
    
    // Push chars one by one into buffer
    {
        std::lock_guard<std::mutex> lock(ioMutex);
        for (size_t i = 0; i < frame.len; ++i) {
            self->buffer.push_back(((char*)frame.payload)[i]);
        }
    }

    free(frame.payload);
    return ESP_OK;
}

char WebSocketServer::readCharBlocking() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(ioMutex);
            if (!buffer.empty()) {
                char c = buffer.front();
                buffer.pop_front();
                return c;
            }
        }
        delay(10);
    }
}

char WebSocketServer::readCharNonBlocking() {
    std::lock_guard<std::mutex> lock(ioMutex);
    if (buffer.empty()) return KEY_NONE;

    char c = buffer.front();
    buffer.pop_front();

    return c;
}

void WebSocketServer::sendText(const std::string& msg) {
    // Sanitize UTF8
    std::string safeMsg = sanitizeUtf8(msg);

    {
        std::lock_guard<std::mutex> lock(ioMutex);
        appendOutputLocked(safeMsg);
    }

    if (clientFd < 0) return;

    httpd_ws_frame_t ws_pkt = {};
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t*) safeMsg.c_str();
    ws_pkt.len = safeMsg.length();

    esp_err_t err = httpd_ws_send_frame_async(server, clientFd, &ws_pkt);
    if (err != ESP_OK) {
        closeClient(server, clientFd);
    }
}

std::string WebSocketServer::sanitizeUtf8(const std::string& input) {
    std::string output;
    size_t i = 0;

    while (i < input.size()) {
        unsigned char c = input[i];

        if (c <= 0x7F) {  // ASCII
            output += c;
            i++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < input.size() &&
                   (input[i+1] & 0xC0) == 0x80) {
            output += input.substr(i, 2);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < input.size() &&
                   (input[i+1] & 0xC0) == 0x80 &&
                   (input[i+2] & 0xC0) == 0x80) {
            output += input.substr(i, 3);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < input.size() &&
                   (input[i+1] & 0xC0) == 0x80 &&
                   (input[i+2] & 0xC0) == 0x80 &&
                   (input[i+3] & 0xC0) == 0x80) {
            output += input.substr(i, 4);
            i += 4;
        } else {
            // Invalid byte or sequence, skip it
            i++;
        }
    }

    return output;
}

void WebSocketServer::closeClient(httpd_handle_t server, int fd) {
    if (fd < 0) return;
    httpd_sess_trigger_close(server, fd);
    if (clientFd == fd) {
        std::lock_guard<std::mutex> lock(ioMutex);
        clientFd = -1;
        buffer.clear();
    }
}

void WebSocketServer::appendOutputLocked(const std::string& msg) {
    outputRing += msg;
    if (outputRing.size() > OUTPUT_RING_MAX) {
        outputRing.erase(0, outputRing.size() - OUTPUT_RING_MAX);
    }
    outputSeq += 1;
}

void WebSocketServer::injectInput(const std::string& input) {
    std::lock_guard<std::mutex> lock(ioMutex);
    for (char c : input) {
        buffer.push_back(c);
    }
}

uint32_t WebSocketServer::getOutputSeq() const {
    std::lock_guard<std::mutex> lock(ioMutex);
    return outputSeq;
}

void WebSocketServer::clearCapturedOutput() {
    std::lock_guard<std::mutex> lock(ioMutex);
    outputRing.clear();
    outputSeq += 1;
}

std::string WebSocketServer::getOutputSince(uint32_t seq, size_t maxBytes) const {
    std::lock_guard<std::mutex> lock(ioMutex);
    if (seq == outputSeq) return "";
    std::string out = outputRing;
    if (out.size() > maxBytes) {
        out = out.substr(out.size() - maxBytes);
    }
    return out;
}

bool WebSocketServer::hasClient() const {
    std::lock_guard<std::mutex> lock(ioMutex);
    return clientFd >= 0;
}
