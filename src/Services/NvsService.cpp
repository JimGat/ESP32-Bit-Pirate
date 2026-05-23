#include "NvsService.h"

NvsService::~NvsService() {
    preferences.end(); // Close namespace
}

void NvsService::open() {
    // Open nvs namespace
    preferences.begin(globalState.getNvsNamespace(), false);
}

void NvsService::close() {
    preferences.end(); // Close namespace
}

bool NvsService::hasKey(const std::string& key) {
    return preferences.isKey(key.c_str());
}

void NvsService::saveString(const std::string& key, const std::string& value) {
    preferences.putString(key.c_str(), value.c_str());
}

std::string NvsService::getString(const std::string& key, const std::string& defaultValue) {
    if (hasKey(key)) {
        return preferences.getString(key.c_str(), defaultValue.c_str()).c_str();
    }
    return defaultValue;
}

void NvsService::saveInt(const std::string& key, int value) {
    preferences.putInt(key.c_str(), value);
}

int NvsService::getInt(const std::string& key, int defaultValue) {
    return preferences.getInt(key.c_str(), defaultValue);
}

void NvsService::remove(const std::string& key) {
    preferences.remove(key.c_str());
}

void NvsService::clearNamespace() {
    preferences.clear();
}
