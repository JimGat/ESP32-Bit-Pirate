#include "DnsServer.h"
#include <Arduino.h>

void DnsServer::captiveDnsTask(void* parameter) {
    auto* dnsServer = static_cast<DNSServer*>(parameter);
    while (true) {
        dnsServer->processNextRequest();
        delay(10);
    }
}

bool DnsServer::setupCaptiveDnsServer(DNSServer& dnsServer, const std::string& ip) {
    IPAddress apIp;
    apIp.fromString(ip.c_str());
    dnsServer.setTTL(300);
    dnsServer.start(53, "*", apIp);
    if (xTaskCreate(captiveDnsTask, "captive_dns", 1024, &dnsServer, 1, nullptr) != pdPASS) {
        dnsServer.stop();
        return false;
    }
    return true;
}

void DnsServer::configureCaptiveDns(httpd_config_t& config) {
    config.max_uri_handlers = 9;
    config.uri_match_fn = httpd_uri_match_wildcard;
}

void DnsServer::startCaptiveDns(const std::string& ip) {
    DNSServer* captiveDnsServer = new DNSServer();
    if (!setupCaptiveDnsServer(*captiveDnsServer, ip)) {
        delete captiveDnsServer;
    }
}
