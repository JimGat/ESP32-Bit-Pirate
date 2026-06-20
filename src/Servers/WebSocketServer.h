#pragma once
#include <deque>
#include <esp_http_server.h>
#include <vector>
#include <string>
#include <Inputs/InputKeys.h>
#include <Arduino.h>
#include <esp_log.h>
#include <cstring>
#include <mutex>

class WebSocketServer {
public:
    WebSocketServer(httpd_handle_t sharedServer);
    void setupRoutes();

    char readCharBlocking();
    char readCharNonBlocking();
    void sendText(const std::string& msg);
    std::string sanitizeUtf8(const std::string& input);

    // Automation API helpers. These let HTTP API handlers inject a command into
    // the same terminal input queue used by the Web CLI, then read captured
    // terminal output without scraping a browser session.
    void injectInput(const std::string& input);
    uint32_t getOutputSeq() const;
    void clearCapturedOutput();
    std::string getOutputSince(uint32_t seq, size_t maxBytes) const;
    bool hasClient() const;

private:
    static esp_err_t wsHandler(httpd_req_t *req);
    static void closeClient(httpd_handle_t server, int fd);

    httpd_handle_t server;
    static inline std::deque<char> buffer;
    static inline int clientFd = -1;
    static inline std::mutex ioMutex;
    static inline std::string outputRing;
    static inline uint32_t outputSeq = 0;
    static constexpr size_t OUTPUT_RING_MAX = 8192;

    static void appendOutputLocked(const std::string& msg);
};
