#pragma once

#include <Arduino.h>
#include <cstring>
#include <string>
#include <esp_http_server.h>
#include <mutex>
#include "Services/LittleFsService.h"
#include "Servers/WebSocketServer.h"
#include "Transformers/JsonTransformer.h"
#include "../webui/index.h"
#include "../webui/scripts.h"
#include "../webui/style.h"

class HttpServer {
public:
    HttpServer(httpd_handle_t sharedServer, LittleFsService& fsService, JsonTransformer& jsonTransformer,
               WebSocketServer* webSocketServer = nullptr)
        : server(sharedServer), littleFsService(fsService), jsonTransformer(jsonTransformer), wsServer(webSocketServer) {} ;
    void setupRoutes();

private:
    httpd_handle_t server;
    LittleFsService& littleFsService;
    JsonTransformer& jsonTransformer;
    WebSocketServer* wsServer;
    std::mutex apiCommandMutex;

    esp_err_t handleRootRequest(httpd_req_t *req);
    esp_err_t handleCssRequest(httpd_req_t *req);
    esp_err_t handleJsRequest(httpd_req_t *req);
    esp_err_t handleLittlefsList(httpd_req_t *req);
    esp_err_t handleLittlefsDelete(httpd_req_t *req);
    esp_err_t handleLittlefsDownload(httpd_req_t* req);
    esp_err_t handleLittlefsUpload(httpd_req_t* req);
    esp_err_t handleApiStatus(httpd_req_t* req);
    esp_err_t handleApiCommand(httpd_req_t* req);

    bool isApiAuthorized(httpd_req_t* req);
    esp_err_t sendJson(httpd_req_t* req, const std::string& payload, const char* status = nullptr);
    std::string readRequestBody(httpd_req_t* req, size_t maxBytes);
    std::string urlDecode(const char* s);
    std::string sanitizeUploadFilename(const char* raw);
};
