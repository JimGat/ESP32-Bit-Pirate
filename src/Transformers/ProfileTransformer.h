#pragma once
#include <string>
#include <map>
#include "States/GlobalState.h"
#include "Enums/InfraredProtocolEnum.h"
#include "Enums/TerminalTypeEnum.h"

class ProfileTransformer {
public:
    static std::string toProfileText();
    static bool fromProfileText(const std::string& text, std::string& error);

private:
    static std::map<std::string, std::string> parse(const std::string& text);
};
