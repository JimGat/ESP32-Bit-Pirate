#include "ProfileShell.h"
#include <algorithm>

ProfileShell::ProfileShell(ITerminalView& tv,
                           IInput& in,
                           UserInputManager& uim,
                           LittleFsService& lfs,
                           ProfileTransformer& pt)
    : terminalView(tv),
      terminalInput(in),
      userInputManager(uim),
      littleFsService(lfs),
      profileTransformer(pt) {}

void ProfileShell::run() {
    terminalView.println("\n=== Profiles ===");
    terminalView.println("Load and save your pin states.");

    while (true) {
        int choice = userInputManager.readValidatedChoiceIndex("Choose an action", actions, actionsCount, actionsCount-1);

        if (choice == 0) cmdLoad();
        else if (choice == 1) cmdSave(); 
        break;
    }

    terminalView.println("Exiting profile shell...\n");
}

void ProfileShell::cmdSave() {
    if (!ensureMounted()) return;

    constexpr size_t MIN_FREE_BYTES = 2 * 1024;
    size_t free = littleFsService.freeBytes();
    if (free < MIN_FREE_BYTES) {
        terminalView.println("\n❌ Not enough LittleFS space.");
        return;
    }

    std::string defName = "profile_" + std::to_string(millis() % 1000000);
    std::string base = userInputManager.readSanitizedString("Profile name", defName, false);
    if (base.empty()) base = defName;

    std::string path = buildProfilePath(base);
    std::string text = profileTransformer.toProfileText();

    if (!littleFsService.write(path, text)) {
        terminalView.println("\n❌ Failed to write: " + path);
        return;
    }

    terminalView.println("\n✅ Saved " + path);
}

void ProfileShell::cmdLoad() {
    if (!ensureMounted()) return;

    auto profiles = listProfiles();
    if (profiles.empty()) {
        terminalView.println("\n❌ No .profile files found.");
        return;
    }
    
    profiles.push_back("Cancel");
    terminalView.println("\n=== Load ===");
    int idx = userInputManager.readValidatedChoiceIndex("Choose the profile", profiles, profiles.size() - 1);
    
    if (idx < 0 || idx >= (int)profiles.size() - 1) {
        terminalView.println("\n❌ Load cancelled.");
        return;
    }

    std::string path = profiles[(size_t)idx];

    std::string content;
    if (!littleFsService.readAll(path, content)) {
        terminalView.println("\n❌ Failed to read: " + path);
        return;
    }

    std::string err;
    if (!profileTransformer.fromProfileText(content, err)) {
        terminalView.println("\n❌ Invalid profile: " + err);
        return;
    }

    terminalView.println("\n✅ Loaded " + path);
}

bool ProfileShell::ensureMounted() {
    if (!littleFsService.mounted()) {
        littleFsService.begin();
        if (!littleFsService.mounted()) {
            terminalView.println("\n ❌ LittleFS not mounted.");
            return false;
        }
    }
    return true;
}

std::vector<std::string> ProfileShell::listProfiles() {
    std::vector<std::string> out;
    std::vector<std::string> files = littleFsService.listFiles("/", ".profile");

    for (auto& f : files) {
        if (f.size() >= strlen(PROFILE_EXT) &&
            f.substr(f.size() - strlen(PROFILE_EXT)) == PROFILE_EXT) {
            out.push_back(f);
        }
    }

    std::sort(out.begin(), out.end());
    return out;
}

std::string ProfileShell::buildProfilePath(const std::string& baseName) {
    std::string name = baseName;
    if (name.empty()) name = "profile";

    std::string path = "/" + name;
    if (path.size() < strlen(PROFILE_EXT) + 1 ||
        path.substr(path.size() - strlen(PROFILE_EXT)) != PROFILE_EXT) {
        path += PROFILE_EXT;
    }
    return path;
}
