#pragma once
#include <string>
#include <vector>

#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Managers/UserInputManager.h"
#include "Services/LittleFsService.h"
#include "Transformers/ProfileTransformer.h"

class ProfileShell {
public:
    ProfileShell(ITerminalView& tv,
                 IInput& in,
                 UserInputManager& uim,
                 LittleFsService& lfs,
                 ProfileTransformer& profileTransformer);

    void run();

private:
    ITerminalView& terminalView;
    IInput& terminalInput;
    UserInputManager& userInputManager;
    LittleFsService& littleFsService;
    ProfileTransformer& profileTransformer;

    inline static constexpr const char* actions[] = {
        " 📥 Load profile",
        " 💾 Save profile",
        " 🚪 Exit"
    };
    inline static constexpr size_t actionsCount = sizeof(actions) / sizeof(actions[0]);

    inline static constexpr const char* PROFILE_EXT = ".profile";

    void cmdLoad();
    void cmdSave();

    // helpers
    bool ensureMounted();
    std::vector<std::string> listProfiles();
    std::string buildProfilePath(const std::string& baseName);
};
