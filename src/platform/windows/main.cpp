#include <cstdio>
#include <string>
#include <algorithm>
#include <SDL.h>
#include "renderer/opengl/opengl.h"
#include "utils/string.h"
#include "mips.h"
#include <imgui.h>
#include "imgui/imgui_impl_sdl_gl3.h"
#include "utils/cue/Cue.h"

#undef main

const int CPU_CLOCK = 33868500;
const int GPU_CLOCK_NTSC = 53690000;

device::controller::DigitalController &getButtonState(SDL_Event &event) {
    static device::controller::DigitalController buttons;
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_UP) buttons.up = true;
        if (event.key.keysym.sym == SDLK_DOWN) buttons.down = true;
        if (event.key.keysym.sym == SDLK_LEFT) buttons.left = true;
        if (event.key.keysym.sym == SDLK_RIGHT) buttons.right = true;
        if (event.key.keysym.sym == SDLK_KP_2) buttons.cross = true;
        if (event.key.keysym.sym == SDLK_KP_8) buttons.triangle = true;
        if (event.key.keysym.sym == SDLK_KP_4) buttons.square = true;
        if (event.key.keysym.sym == SDLK_KP_6) buttons.circle = true;
        if (event.key.keysym.sym == SDLK_KP_3) buttons.start = true;
        if (event.key.keysym.sym == SDLK_KP_1) buttons.select = true;
        if (event.key.keysym.sym == SDLK_KP_7) buttons.l1 = true;
        if (event.key.keysym.sym == SDLK_KP_DIVIDE) buttons.l2 = true;
        if (event.key.keysym.sym == SDLK_KP_9) buttons.r1 = true;
        if (event.key.keysym.sym == SDLK_KP_MULTIPLY) buttons.r2 = true;
    }
    if (event.type == SDL_KEYUP) {
        if (event.key.keysym.sym == SDLK_UP) buttons.up = false;
        if (event.key.keysym.sym == SDLK_DOWN) buttons.down = false;
        if (event.key.keysym.sym == SDLK_LEFT) buttons.left = false;
        if (event.key.keysym.sym == SDLK_RIGHT) buttons.right = false;
        if (event.key.keysym.sym == SDLK_KP_2) buttons.cross = false;
        if (event.key.keysym.sym == SDLK_KP_8) buttons.triangle = false;
        if (event.key.keysym.sym == SDLK_KP_4) buttons.square = false;
        if (event.key.keysym.sym == SDLK_KP_6) buttons.circle = false;
        if (event.key.keysym.sym == SDLK_KP_3) buttons.start = false;
        if (event.key.keysym.sym == SDLK_KP_1) buttons.select = false;
        if (event.key.keysym.sym == SDLK_KP_7) buttons.l1 = false;
        if (event.key.keysym.sym == SDLK_KP_DIVIDE) buttons.l2 = false;
        if (event.key.keysym.sym == SDLK_KP_9) buttons.r1 = false;
        if (event.key.keysym.sym == SDLK_KP_MULTIPLY) buttons.r2 = false;
    }

    return buttons;
}

const char *mapIo(uint32_t address) {
    address -= 0x1f801000;

#define IO(begin, end, periph)                   \
    if (address >= (begin) && address < (end)) { \
        return periph;                           \
    }

    IO(0x00, 0x24, "memoryControl");
    IO(0x40, 0x50, "controller");
    IO(0x50, 0x60, "serial");
    IO(0x60, 0x64, "memoryControl");
    IO(0x70, 0x78, "interrupt");
    IO(0x80, 0x100, "dma");
    IO(0x100, 0x110, "timer0");
    IO(0x110, 0x120, "timer1");
    IO(0x120, 0x130, "timer2");
    IO(0x800, 0x804, "cdrom");
    IO(0x810, 0x818, "gpu");
    IO(0x820, 0x828, "mdec");
    IO(0xC00, 0x1000, "spu");
    IO(0x1000, 0x1043, "exp2");
    return "";
}

bool gteRegistersEnabled = false;
bool ioLogEnabled = false;
bool gteLogEnabled = false;
bool gpuLogEnabled = false;
bool showVRAM = false;
bool singleFrame = false;
bool running = true;
bool skipRender = false;
bool showIo = false;

void replayCommands(mips::gpu::GPU *gpu, int to) {
    auto commands = gpu->gpuLogList;
    gpu->vram = gpu->prevVram;

    gpu->gpuLogEnabled = false;
    for (int i = 0; i <= to; i++) {
        auto cmd = commands.at(i);

        if (cmd.args.size() == 0) printf("Panic! no args");
        int mask = cmd.command << 24;

        for (auto arg : cmd.args) {
            gpu->write(0, mask | arg);
            mask = 0;
        }
    }
    gpu->gpuLogEnabled = true;
}

void dumpRegister(const char *name, uint32_t *reg) {
    ImGui::BulletText(name);
    ImGui::NextColumn();
    ImGui::InputInt("", (int *)reg, 1, 100, ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::NextColumn();
}

void renderImgui(mips::CPU *cpu) {
    auto gte = cpu->gte;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                running = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulation")) {
            if (ImGui::MenuItem("Soft reset", "F2")) {
                cpu->softReset();
            }

            const char *shellStatus = cpu->cdrom->getShell() ? "Shell opened" : "Shell closed";
            if (ImGui::MenuItem(shellStatus, "F3")) {
                cpu->cdrom->toggleShell();
            }

            if (ImGui::MenuItem("Single frame", "F7")) {
                singleFrame = true;
                cpu->state = mips::CPU::State::run;
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("IO", NULL, &showIo);
            ImGui::MenuItem("GTE registers", NULL, &gteRegistersEnabled);
            ImGui::MenuItem("BIOS log", "F5", &cpu->biosLog);
#ifdef ENABLE_IO_LOG
            ImGui::MenuItem("IO log", NULL, &ioLogEnabled);
#endif
            ImGui::MenuItem("GTE log", NULL, &gteLogEnabled);
            ImGui::MenuItem("GPU log", NULL, &gpuLogEnabled);
            if (ImGui::MenuItem("Show VRAM", NULL, &showVRAM)) {
            }
            ImGui::MenuItem("Disassembly (slow)", "F6", &cpu->disassemblyEnabled);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (gteRegistersEnabled) {
        ImGui::Begin("GTE registers", &gteRegistersEnabled);

        ImGui::Columns(3, 0, false);
        ImGui::Text("IR1:  %04hX", gte.ir[1]);
        ImGui::NextColumn();
        ImGui::Text("IR2:  %04hX", gte.ir[2]);
        ImGui::NextColumn();
        ImGui::Text("IR3:  %04hX", gte.ir[3]);
        ImGui::NextColumn();

        ImGui::Separator();
        ImGui::Text("MAC0: %08X", gte.mac[0]);

        ImGui::Separator();
        ImGui::Text("MAC1: %08X", gte.mac[1]);
        ImGui::NextColumn();
        ImGui::Text("MAC2: %08X", gte.mac[2]);
        ImGui::NextColumn();
        ImGui::Text("MAC3: %08X", gte.mac[3]);
        ImGui::NextColumn();

        ImGui::Separator();
        ImGui::Text("TRX:  %08X", gte.tr.x);
        ImGui::NextColumn();
        ImGui::Text("TRY:  %08X", gte.tr.y);
        ImGui::NextColumn();
        ImGui::Text("TRZ:  %08X", gte.tr.z);
        ImGui::NextColumn();

        ImGui::Separator();

        for (int i = 0; i < 4; i++) {
            ImGui::Text("S%dX:  %04hX", i, gte.s[i].x);
            ImGui::NextColumn();
            ImGui::Text("S%dY:  %04hX", i, gte.s[i].y);
            ImGui::NextColumn();
            ImGui::Text("S%dZ:  %04hX", i, gte.s[i].z);
            ImGui::NextColumn();
        }

        ImGui::Columns(3, 0, false);
        ImGui::Separator();

        ImGui::Text("RT11:  %04hX", gte.rt.v11);
        ImGui::NextColumn();
        ImGui::Text("RT12:  %04hX", gte.rt.v12);
        ImGui::NextColumn();
        ImGui::Text("RT13:  %04hX", gte.rt.v13);
        ImGui::NextColumn();

        ImGui::Text("RT21:  %04hX", gte.rt.v21);
        ImGui::NextColumn();
        ImGui::Text("RT22:  %04hX", gte.rt.v22);
        ImGui::NextColumn();
        ImGui::Text("RT23:  %04hX", gte.rt.v23);
        ImGui::NextColumn();

        ImGui::Text("RT31:  %04hX", gte.rt.v31);
        ImGui::NextColumn();
        ImGui::Text("RT32:  %04hX", gte.rt.v32);
        ImGui::NextColumn();
        ImGui::Text("RT33:  %04hX", gte.rt.v33);
        ImGui::NextColumn();

        ImGui::Separator();
        ImGui::Text("VX0:  %04hX", gte.v[0].x);
        ImGui::NextColumn();
        ImGui::Text("VY0:  %04hX", gte.v[0].y);
        ImGui::NextColumn();
        ImGui::Text("VZ0:  %04hX", gte.v[0].z);
        ImGui::NextColumn();

        ImGui::Separator();
        ImGui::Text("OFX:  %08X", gte.of[0]);
        ImGui::NextColumn();
        ImGui::Text("OFY:  %08X", gte.of[1]);
        ImGui::NextColumn();
        ImGui::Text("H:   %04hX", gte.h);
        ImGui::NextColumn();

        ImGui::Separator();
        ImGui::Text("DQA:  %04hX", gte.dqa);
        ImGui::NextColumn();
        ImGui::Text("DQB:  %08X", gte.dqb);
        ImGui::NextColumn();

        ImGui::End();
    }

#ifdef ENABLE_IO_LOG
    if (ioLogEnabled) {
        ImGui::Begin("IO Log", &ioLogEnabled);

        ImGui::BeginChild("IO Log", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        ImGuiListClipper clipper(cpu->ioLogList.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                auto ioEntry = cpu->ioLogList[i];
                ImGui::Text("%c %2d 0x%08x: 0x%0*x %*s %s", ioEntry.mode == mips::CPU::IO_LOG_ENTRY::MODE::READ ? 'R' : 'W', ioEntry.size,
                            ioEntry.addr, ioEntry.size / 4, ioEntry.data,
                            // padding
                            8 - ioEntry.size / 4, "", mapIo(ioEntry.addr));
            }
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();

        ImGui::End();
    }
#endif

    if (gteLogEnabled) {
        ImGui::Begin("GTE Log", &gteLogEnabled);

        ImGui::BeginChild("GTE Log", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        ImGuiListClipper clipper(cpu->gte.log.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                auto ioEntry = cpu->gte.log[i];
                if (ioEntry.mode == mips::gte::GTE::GTE_ENTRY::MODE::func) {
                    ImGui::Text("%c 0x%02x", 'F', ioEntry.n);
                } else {
                    ImGui::Text("%c %2d: 0x%08x", ioEntry.mode == mips::gte::GTE::GTE_ENTRY::MODE::read ? 'R' : 'W', ioEntry.n,
                                ioEntry.data);
                }
            }
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();

        ImGui::End();
    }

    if (gpuLogEnabled) {
        ImGui::Begin("GPU Log", &gpuLogEnabled);

        ImGui::BeginChild("GPU Log", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()), false);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        int renderTo = -1;
        //        ImGuiListClipper clipper(cpu->getGPU()->gpuLogList.size());
        //        while (clipper.Step()) {
        //            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
        for (int i = 0; i < cpu->getGPU()->gpuLogList.size(); i++) {
            auto entry = cpu->getGPU()->gpuLogList[i];

            bool nodeOpen = ImGui::TreeNode((void *)(intptr_t)i, "cmd: 0x%02x  %s", entry.command, device::gpu::CommandStr[(int)entry.cmd]);

            if (ImGui::IsItemHovered()) {
                printf("Render to cmd %d\n", i);
                renderTo = i;
            }

            if (nodeOpen) {
                // Render arguments
                for (auto arg : entry.args) {
                    ImGui::Text("- 0x%08x", arg);
                }
                ImGui::TreePop();
            }
        }
        //        }
        ImGui::PopStyleVar();
        ImGui::EndChild();

        ImGui::End();

        if (renderTo >= 0) {
            replayCommands(cpu->getGPU(), renderTo);
        }
    }

    if (showIo) {
        ImGui::Begin("IO", &showIo);

        ImGui::Columns(1, 0, false);
        ImGui::Text("Timer 0");

        ImGui::Columns(2, 0, false);
        dumpRegister("current", (uint32_t *)&cpu->timer0->current);
        dumpRegister("target", (uint32_t *)&cpu->timer0->target);
        dumpRegister("mode", (uint32_t *)&cpu->timer0->mode);

        ImGui::Columns(1, 0, false);
        ImGui::Text("Timer 1");

        ImGui::Columns(2, 0, false);
        dumpRegister("current", (uint32_t *)&cpu->timer1->current);
        dumpRegister("target", (uint32_t *)&cpu->timer1->target);
        dumpRegister("mode", (uint32_t *)&cpu->timer1->mode);

        ImGui::Columns(1, 0, false);
        ImGui::Text("Timer 2");

        ImGui::Columns(2, 0, false);
        dumpRegister("current", (uint32_t *)&cpu->timer2->current);
        dumpRegister("target", (uint32_t *)&cpu->timer2->target);
        dumpRegister("mode", (uint32_t *)&cpu->timer2->mode);

        ImGui::End();
    }

    ImGui::Render();
}

int start(int argc, char **argv) {
    std::string bios = "SCPH1001.bin";
    std::string iso = "D:/Games/!PSX/Doom/Doom (Track 1).bin";

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Cannot init SDL\n");
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Avocado", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, OpenGL::resWidth, OpenGL::resHeight,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (window == nullptr) {
        printf("Cannot create window (%s)\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (glContext == nullptr) {
        printf("Cannot initialize opengl\n");
        return 1;
    }

    OpenGL opengl;

    if (!opengl.init()) {
        printf("Cannot initialize OpenGL\n");
        return 1;
    }

    if (!opengl.setup()) {
        printf("Cannot setup graphics\n");
        return 1;
    }

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    ImGui_ImplSdlGL3_Init(window);

    std::unique_ptr<mips::CPU> cpu = std::make_unique<mips::CPU>();

    if (cpu->loadBios(bios)) {
        printf("Using bios %s\n", bios.c_str());
    }
    //    cpu->loadExpansion("data/bios/expansion.rom");

    utils::CueParser parser;
    //        auto cue = parser.parse("D:/Games/!PSX/Tekken (USA)/Tekken (USA).cue");
    auto cue = parser.parse("D:/Games/!PSX/Doom/Doom.cue");
    //    auto cue = parser.parse("D:/Games/!PSX/Vib_Ribbon_6/vib_ribbon_PAL_PSX.cue");

    cpu->cdrom->setShell(true);  // open shell
    if (fileExists(iso)) {
        bool success = cpu->dma->dma3.load(iso);
        cpu->cdrom->setShell(!success);
        cpu->cdrom->setCue(cue);
        if (!success) printf("Cannot load iso file: %s\n", iso.c_str());
    }

    float startTime = SDL_GetTicks() / 1000.f;
    float fps = 0.f;
    int deltaFrames = 0;

    SDL_Event event;
    while (running) {
        bool newEvent = false;
        if (cpu->state != mips::CPU::State::run) {
            SDL_WaitEvent(&event);
            newEvent = true;
        }

        while (newEvent || SDL_PollEvent(&event)) {
            newEvent = false;
            if (event.type == SDL_QUIT || (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)) running = false;
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                if (event.key.keysym.sym == SDLK_2) cpu->interrupt->trigger(device::interrupt::TIMER2);
                if (event.key.keysym.sym == SDLK_c) cpu->interrupt->trigger(device::interrupt::CDROM);
                if (event.key.keysym.sym == SDLK_d) cpu->interrupt->trigger(device::interrupt::DMA);
                if (event.key.keysym.sym == SDLK_s) cpu->interrupt->trigger(device::interrupt::SPU);
                if (event.key.keysym.sym == SDLK_TAB) skipRender = !skipRender;
                if (event.key.keysym.sym == SDLK_r) {
                    cpu->dumpRam();
                    cpu->spu->dumpRam();
                }
                if (event.key.keysym.sym == SDLK_F2) {
                    cpu->softReset();
                }
                if (event.key.keysym.sym == SDLK_F3) {
                    printf("Shell toggle\n");
                    cpu->cdrom->toggleShell();
                }
                if (event.key.keysym.sym == SDLK_F5) {
                    cpu->biosLog = !cpu->biosLog;
                }
                if (event.key.keysym.sym == SDLK_F6) {
                    cpu->disassemblyEnabled = !cpu->disassemblyEnabled;
                }
                if (event.key.keysym.sym == SDLK_F7) {
                    singleFrame = true;
                    cpu->state = mips::CPU::State::run;
                }
                if (event.key.keysym.sym == SDLK_SPACE) {
                    if (cpu->state == mips::CPU::State::pause)
                        cpu->state = mips::CPU::State::run;
                    else if (cpu->state == mips::CPU::State::run)
                        cpu->state = mips::CPU::State::pause;
                }
                if (event.key.keysym.sym == SDLK_q) {
                    showVRAM = !showVRAM;
                }
            }
            if (event.type == SDL_DROPFILE) {
                std::string path = event.drop.file;
                std::string ext = getExtension(path);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                SDL_free(event.drop.file);

                if (ext == "iso" || ext == "bin" || ext == "img") {
                    if (fileExists(path)) {
                        printf("Dropped .iso, loading... ");

                        bool success = cpu->dma->dma3.load(path);
                        cpu->cdrom->setShell(!success);

                        if (success)
                            printf("ok.\n");
                        else
                            printf("fail.\n");
                    }
                } else if (ext == "exe" || ext == "psexe") {
                    printf("Dropped .exe, currently not supported.\n");
                }
            }
            cpu->controller->setState(getButtonState(event));
            ImGui_ImplSdlGL3_ProcessEvent(&event);
        }

        if (showVRAM != opengl.getViewFullVram()) {
            if (showVRAM)
                SDL_SetWindowSize(window, 1024, 512);
            else
                SDL_SetWindowSize(window, OpenGL::resWidth, OpenGL::resHeight);
            opengl.setViewFullVram(showVRAM);
        }

        if (cpu->state == mips::CPU::State::run) {
            cpu->emulateFrame();
            if (singleFrame) {
                singleFrame = false;
                cpu->state = mips::CPU::State::pause;
            }
        }
        ImGui_ImplSdlGL3_NewFrame(window);
        if (!skipRender)
            opengl.render(cpu->getGPU());
        else
            cpu->getGPU()->render().clear();
        renderImgui(cpu.get());

        deltaFrames++;
        float currentTime = SDL_GetTicks() / 1000.f;
        if (currentTime - startTime > 0.25f) {
            fps = (float)deltaFrames / (currentTime - startTime);
            startTime = currentTime;
            deltaFrames = 0;
        }

        std::string title = string_format("Avocado: IMASK: %s, ISTAT: %s, frame: %d, FPS: %.0f", cpu->interrupt->getMask().c_str(),
                                          cpu->interrupt->getStatus().c_str(), cpu->getGPU()->frames, fps);
        SDL_SetWindowTitle(window, title.c_str());
        SDL_GL_SwapWindow(window);
    }
    ImGui_ImplSdlGL3_Shutdown();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

int main(int argc, char **argv) {
    int retval = start(argc, argv);
    if (retval != 0) {
        printf("\nPress enter to close");
        getchar();
    }
    return retval;
}