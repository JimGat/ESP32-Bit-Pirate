#pragma once

#include <string>
#include <esp_http_server.h>
#include <DNSServer.h>

class DnsServer {
public:
	static void configureCaptiveDns(httpd_config_t& config);
	static void startCaptiveDns(const std::string& ip);

private:
	static void captiveDnsTask(void* parameter);
	static bool setupCaptiveDnsServer(DNSServer& dnsServer, const std::string& ip);
};

