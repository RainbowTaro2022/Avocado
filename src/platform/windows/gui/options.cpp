#include "options.h"
#include <imgui.h>
#include "config.h"
#include "gui.h"
#include "images.h"
#include "platform/windows/input/sdl_input_manager.h"
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
    const std::array<const char*, 4> resolutions = {{"1x", "2x", "3x", "4x"}};
    int internalResolution = config["options"]["graphics"]["resolution"];
    bool filtering = config["options"]["graphics"]["filtering"];
    bool widescreen = config["options"]["graphics"]["widescreen"];
    ImGui::Begin("Graphics", &showGraphicsOptionsWindow, ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::Combo("Internal resolution", &internalResolution, resolutions.data(), resolutions.size())) {
        config["options"]["graphics"]["resolution"] = internalResolution;
        configObserver.notify(Event::Graphics);
    }
    if (ImGui::Checkbox("Filtering", &filtering)) {
        config["options"]["graphics"]["filtering"] = filtering;
        configObserver.notify(Event::Graphics);
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

void drawImage(const optional<Image> image, float w = 0.f, float h = 0.f) {
    if (auto img = image) {
        ImVec2 size;

        if (w != 0.f && h != 0.f) {
            size = ImVec2(w, h);
        } else if (w != 0.f) {
            float aspect = static_cast<float>(img->h) / static_cast<float>(img->w);
            size = ImVec2(w, w * aspect);
        } else if (h != 0.f) {
            float aspect = static_cast<float>(img->w) / static_cast<float>(img->h);
            size = ImVec2(h * aspect, h);
        } else {
            size = ImVec2((float)img->w, (float)img->h);
        }
        ImGui::Image((ImTextureID)(uintptr_t)img->id, size);
    }
}

void button(int controller, std::string button, const char* tooltip = nullptr) {
    auto inputManager = static_cast<SdlInputManager*>(InputManager::getInstance());
    static std::string currentButton = "";
    if (controller < 1 || controller > 4) return;
    std::string ctrl = std::to_string(controller);

    if (button == currentButton && inputManager->lastPressedKey.type != Key::Type::None) {
        config["controller"][ctrl]["keys"][button] = inputManager->lastPressedKey.to_string();
        inputManager->lastPressedKey = Key();
    }

    std::string key = config["controller"][ctrl]["keys"][button];

    const float iconSize = 20.f;
    drawImage(getImage(button, "data/assets/buttons"), iconSize);
    if (ImGui::IsItemHovered() && tooltip != nullptr) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(tooltip);
        ImGui::EndTooltip();
    }
    ImGui::SameLine();

    optional<Image> image = {};
    std::string deviceIndex = "";

    if (key.rfind("keyboard") == 0) {
        image = getImage("ic_keyboard");
    } else if (key.rfind("mouse") == 0) {
        image = getImage("ic_mouse");
    } else if (key.rfind("controller") == 0) {
        image = getImage("ic_controller");
        deviceIndex = key.substr(10, key.find("|") - 10);
    }

    if (auto pos = key.find("|"); pos != std::string::npos) {
        key = key.substr(pos + 1);
    }

    if (ImGui::Button(string_format("%s##%s", key.c_str(), button.c_str()).c_str(), ImVec2(-iconSize * 3, 0.f))) {
        currentButton = button;
        inputManager->waitingForKeyPress = true;
        ImGui::OpenPopup("Waiting for key...");

        // TODO: Detect duplicates
    }

    if (image) {
        ImGui::SameLine();
        drawImage(image, iconSize);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(deviceIndex.c_str());
}

void controllerSetupWindow() {
    const int controllerCount = 2;
    static int selectedController = 1;
    static std::string comboString = string_format("Controller %d", selectedController);

    const std::array<const char*, 4> types
        = {{ControllerType::NONE.c_str(), ControllerType::DIGITAL.c_str(), ControllerType::ANALOG.c_str(), ControllerType::MOUSE.c_str()}};

    const auto find = [&](std::string selectedType) -> int {
        for (auto i = 0; i < (int)types.size(); i++) {
            std::string type = types[i];
            if (type == selectedType) return i;
        }
        return 0;
    };

    const std::array<const char*, 5> defaults = {{"Defaults ...", "Clear", "Keyboard (Numpad)", "Mouse", "Controller 1"}};

    ImGui::Begin("Controller", &showControllerSetupWindow, ImVec2(500.f, 320.f), ImGuiWindowFlags_NoScrollbar);

    ImGui::PushItemWidth(-1);
    if (ImGui::BeginCombo("##combo_controller", comboString.c_str())) {
        for (int i = 1; i <= controllerCount; i++) {
            bool isSelected = i == selectedController;
            std::string label = string_format("Controller %d", i);
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                selectedController = i;
                comboString = label;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    int typePos = find(config["controller"][std::to_string(selectedController)]["type"]);
    auto currentType = types[typePos];

    const float leftGroupWidth = 256.f;
    ImGui::BeginGroup();
    ImGui::BeginChild("##controller_image", ImVec2(leftGroupWidth, -ImGui::GetFrameHeightWithSpacing()));
    ImGui::Text("Type");
    ImGui::SameLine();
    ImGui::PushItemWidth(-1);
    if (ImGui::Combo("##type", &typePos, types.data(), (int)types.size())) {
        config["controller"][std::to_string(selectedController)]["type"] = types[typePos];
        configObserver.notify(Event::Controller);
    }
    ImGui::PopItemWidth();

    if (currentType != ControllerType::NONE) {
        drawImage(getImage(currentType), leftGroupWidth);
    }
    ImGui::EndChild();

    if (currentType != ControllerType::NONE) {
        ImGui::PushItemWidth(leftGroupWidth);
        if (int pos = 0; ImGui::Combo("##defaults", &pos, defaults.data(), (int)defaults.size())) {
            auto& keysConfig = config["controller"][std::to_string(selectedController)]["keys"];
            switch (pos) {
                case 1: keysConfig = DefaultKeyBindings::none(); break;
                case 2: keysConfig = DefaultKeyBindings::keyboard_numpad(); break;
                case 3: keysConfig = DefaultKeyBindings::mouse(); break;
                case 4: keysConfig = DefaultKeyBindings::controller(); break;
            }
        }
        ImGui::PopItemWidth();
    }
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginChild("Buttons", ImVec2(0.f, 0.f));
    if (currentType == ControllerType::MOUSE) {
        button(selectedController, "l_up", "Move Up");
        button(selectedController, "l_right", "Move Right");
        button(selectedController, "l_down", "Move Down");
        button(selectedController, "l_left", "Move Left");
        button(selectedController, "l1", "Left Button");
        button(selectedController, "r1", "Right Button");
    }
    if (currentType == ControllerType::DIGITAL || currentType == ControllerType::ANALOG) {
        button(selectedController, "dpad_up", "D-Pad Up");
        button(selectedController, "dpad_down", "D-Pad Down");
        button(selectedController, "dpad_left", "D-Pad Left");
        button(selectedController, "dpad_right", "D-Pad Right");

        button(selectedController, "triangle", "Triange");
        button(selectedController, "cross", "Cross");
        button(selectedController, "square", "Square");
        button(selectedController, "circle", "Circle");

        button(selectedController, "l1", "L1");
        button(selectedController, "r1", "R1");
        button(selectedController, "l2", "L2");
        button(selectedController, "r2", "R2");

        button(selectedController, "select", "Select");
        button(selectedController, "start", "Start");

        if (currentType == ControllerType::ANALOG) {
            button(selectedController, "analog", "Analog mode toggle");
            button(selectedController, "l3", "Left Stick Press");
            button(selectedController, "l_up", "Left Stick Up");
            button(selectedController, "l_right", "Left Stick Right");
            button(selectedController, "l_down", "Left Stick Down");
            button(selectedController, "l_left", "Left Stick Left");
            button(selectedController, "r3", "Right Stick Press");
            button(selectedController, "r_up", "Right Stick Up");
            button(selectedController, "r_right", "Right Stick Right");
            button(selectedController, "r_down", "Right Stick Down");
            button(selectedController, "r_left", "Right Stick Left");
        }
    }

    auto inputManager = static_cast<SdlInputManager*>(InputManager::getInstance());
    if (ImGui::BeginPopupModal("Waiting for key...", &inputManager->waitingForKeyPress, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Press any key (ESC to cancel).");
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::End();
}
