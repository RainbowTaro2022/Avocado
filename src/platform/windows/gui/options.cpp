#include "options.h"
#include <imgui.h>
#include "config.h"
#include "gui.h"
#include "renderer/opengl/opengl.h"
#include "utils/file.h"
#include "utils/string.h"

#ifdef _WIN32
#include <filesystem>
using namespace std::experimental::filesystem::v1;
#endif

bool showGraphicsOptionsWindow = false;
bool showBiosWindow = false;
bool showControllerSetupWindow = false;

void graphicsOptionsWindow() {
    bool filtering = config["options"]["graphics"]["filtering"];
    bool widescreen = config["options"]["graphics"]["widescreen"];
    ImGui::Begin("Graphics", &showGraphicsOptionsWindow, ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::Checkbox("Filtering", &filtering)) {
        config["options"]["graphics"]["filtering"] = filtering;
    }
    if (ImGui::Checkbox("Widescreen (16/9)", &widescreen)) {
        config["options"]["graphics"]["widescreen"] = widescreen;
    }

    ImGui::End();
}

void biosSelectionWindow() {
    static bool biosesFound = false;
    static std::vector<std::string> bioses;
    static int selectedBios = 0;

#ifdef _WIN32
    if (!biosesFound) {
        bioses.clear();
        auto dir = directory_iterator("data/bios");
        for (auto& e : dir) {
            if (!is_regular_file(e)) continue;

            auto path = e.path().string();
            auto ext = getExtension(path);
            std::transform(ext.begin(), ext.end(), ext.begin(), tolower);

            if (ext == "bin" || ext == "rom") {
                bioses.push_back(path);
            }
        }
        biosesFound = true;

        int i = 0;
        for (auto it = bioses.begin(); it != bioses.end(); ++it, ++i) {
            if (*it == config["bios"]) {
                selectedBios = i;
                break;
            }
        }
    }
#endif

    ImGui::Begin("BIOS", &showBiosWindow, ImGuiWindowFlags_AlwaysAutoResize);
    if (bioses.empty()) {
#ifdef _WIN32
        ImGui::Text(
            "BIOS directory is empty.\n"
            "You need one of BIOS files (eg. SCPH1001.bin) placed in data/bios directory.\n"
            ".bin and .rom extensions are recognised.");
#else
        ImGui::Text(
            "No filesystem support on macOS, sorry :(\n"
            "edit bios in config.json after closing this program");
#endif
    } else {
        ImGui::PushItemWidth(300.f);
        ImGui::ListBox("", &selectedBios,
                       [](void* data, int idx, const char** out_text) {
                           const std::vector<std::string>* v = (std::vector<std::string>*)data;
                           *out_text = v->at(idx).c_str();
                           return true;
                       },
                       (void*)&bioses, (int)bioses.size());
        ImGui::PopItemWidth();

        if (ImGui::Button("Select", ImVec2(-1, 0)) && selectedBios < (int)bioses.size()) {
            config["bios"] = bioses[selectedBios];

            biosesFound = false;
            showBiosWindow = false;
            doHardReset = true;
        }
    }
    ImGui::End();
}

void button(int controller, std::string button) {
    static std::string currentButton = "";
    if (controller < 1 || controller > 4) return;
    std::string ctrl = std::to_string(controller);

    if (button == currentButton && lastPressedKey != 0) {
        config["controller"][ctrl]["keys"][button] = SDL_GetKeyName(lastPressedKey);
        lastPressedKey = 0;
    }

    std::string key = config["controller"][ctrl]["keys"][button];

    ImGui::TextUnformatted(button.c_str());
    ImGui::NextColumn();
    if (ImGui::Button(key.c_str(), ImVec2(100.f, 0.f))) {
        currentButton = button;
        waitingForKeyPress = true;
        ImGui::OpenPopup("Waiting for key...");
    }
    ImGui::NextColumn();
}

void controllerSetupWindow() {
    const char* TYPE_NONE = "None";
    const char* TYPE_DIGITAL = "Digital";
    const char* TYPE_ANALOG = "Analog";
    const char* TYPE_MOUSE = "Mouse";
    const std::array<const char*, 4> types = {{
        TYPE_NONE, TYPE_DIGITAL, TYPE_ANALOG, TYPE_MOUSE
    }};

    const auto find = [&](std::string selectedType) -> int {
        std::transform(selectedType.begin(), selectedType.end(), selectedType.begin(), tolower);

        for (size_t i = 0; i<types.size(); i++) {
            std::string type = types[i];
            std::transform(type.begin(), type.end(), type.begin(), tolower);

            if (type == selectedType) return i;
        }
        return 0;
    };

    if (ImGui::BeginPopupModal("Waiting for key...", &waitingForKeyPress, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Press any key (ESC to cancel).");
        ImGui::EndPopup();
    }

    ImGui::Begin("Controller", &showControllerSetupWindow);

    for (int i = 1; i<=2; i++) {
        int flags = ImGuiTreeNodeFlags_CollapsingHeader;
        if (i == 1) flags |= ImGuiTreeNodeFlags_DefaultOpen;

        if (ImGui::TreeNodeEx(string_format("Controller %d", i).c_str(), flags)) {
            int currentType = find(config["controller"][std::to_string(i)]["type"]);

            ImGui::Text("Type");
            ImGui::SameLine();
            if (ImGui::Combo("##type", &currentType, types.data(), types.size())) {
                std::string type = types[currentType];
                std::transform(type.begin(), type.end(), type.begin(), tolower);

                config["controller"][std::to_string(i)]["type"] = type;
            }

            if (types[currentType] == TYPE_DIGITAL || types[currentType] == TYPE_ANALOG)  {
                if (types[currentType] == TYPE_ANALOG) {
                    ImGui::Text("Note: use game controller with analog sticks");
                }
                ImGui::Columns(2, nullptr, false);
                button(i, "up");
                button(i, "down");
                button(i, "left");
                button(i, "right");

                button(i, "triangle");
                button(i, "cross");
                button(i, "square");
                button(i, "circle");
        
                button(i, "l1");
                button(i, "r1");
                button(i, "l2");
                button(i, "r2");
        
                button(i, "select");
                button(i, "start");

                ImGui::Columns(1);

                if (ImGui::Button("Restore defaults")) {
                    config["controller"][std::to_string(i)]["keys"] = defaultConfig["controller"][std::to_string(i)]["keys"];
                }
            }
            ImGui::TreePop();
        }
    }

    ImGui::End();
}
