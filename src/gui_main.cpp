#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>   // drags in system OpenGL headers
#include <nfd.hpp>

#include "batch.hpp"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#  include <windows.h>
#endif
#ifdef _WIN32
#  include <windows.h>
#endif

namespace fs = std::filesystem;

// Forward-declare single-file converters (defined in rom.cpp / cfg.cpp)
std::string convert_rom(const std::string& rom_path,
                        const std::string& output_stem, bool quiet);
std::string convert_cfg(const std::string& bin_path, const std::string& cfg_path,
                        const std::string& output_stem, bool quiet);

// ── Application state ─────────────────────────────────────────────────────

struct AppState {
    // Batch tab
    char batch_source[1024] = {};
    char batch_output[1024] = {};
    bool batch_dry_run      = false;
    bool batch_force        = false;
    BatchResult batch_result{0, 0, 0};
    bool        batch_has_result = false;

    // Single file tab
    int  single_fmt = 0;          // 0 = ROM, 1 = BIN + CFG
    char single_rom[1024]  = {};
    char single_bin[1024]  = {};
    char single_cfg[1024]  = {};
    char single_stem[1024] = {};
    std::string single_status;
    bool        single_ok = true;

    // Shared worker
    std::mutex               log_mutex;
    std::vector<std::string> log_lines;
    std::atomic<bool>        running{false};
    std::thread              worker;
};

// ── Helpers ───────────────────────────────────────────────────────────────

static void browse_folder(char* buf, size_t buf_size) {
    NFD::UniquePathU8 path;
    if (NFD::PickFolder(path) == NFD_OKAY)
        strncpy(buf, path.get(), buf_size - 1);
}

static void browse_file(char* buf, size_t buf_size,
                        const nfdu8filteritem_t* filters, nfdfiltersize_t nfilters) {
    NFD::UniquePathU8 path;
    if (NFD::OpenDialog(path, filters, nfilters) == NFD_OKAY)
        strncpy(buf, path.get(), buf_size - 1);
}

// Auto-fill the output stem from an input file path (strips extension).
static void auto_stem(const char* filepath, char* stem_buf, size_t stem_size) {
    if (filepath[0] == '\0') return;
    fs::path p(filepath);
    std::string s = (p.parent_path() / p.stem()).string();
    strncpy(stem_buf, s.c_str(), stem_size - 1);
}

// ── Batch tab ─────────────────────────────────────────────────────────────

static void draw_batch_tab(AppState& s) {
    const float browse_w = 72.0f;
    const float avail    = ImGui::GetContentRegionAvail().x;
    const float field_w  = avail - browse_w - ImGui::GetStyle().ItemSpacing.x;

    ImGui::Text("Source folder:");
    ImGui::SetNextItemWidth(field_w);
    ImGui::InputText("##batch_src", s.batch_source, sizeof(s.batch_source));
    ImGui::SameLine();
    if (ImGui::Button("Browse##bsrc", {browse_w, 0}))
        browse_folder(s.batch_source, sizeof(s.batch_source));

    ImGui::Text("Output folder:");
    ImGui::SetNextItemWidth(field_w);
    ImGui::InputText("##batch_out", s.batch_output, sizeof(s.batch_output));
    ImGui::SameLine();
    if (ImGui::Button("Browse##bout", {browse_w, 0}))
        browse_folder(s.batch_output, sizeof(s.batch_output));

    ImGui::Spacing();
    ImGui::Checkbox("Dry run", &s.batch_dry_run);
    ImGui::SameLine(0, 24);
    ImGui::Checkbox("Force reconvert", &s.batch_force);
    ImGui::Spacing();

    bool can_go = !s.running
               && s.batch_source[0] != '\0'
               && s.batch_output[0] != '\0';
    if (!can_go) ImGui::BeginDisabled();
    const float btn_w = 160.0f;
    ImGui::SetCursorPosX((avail - btn_w) * 0.5f);
    if (ImGui::Button(s.running ? "Working..." : "Convert", {btn_w, 0})) {
        {
            std::lock_guard<std::mutex> lk(s.log_mutex);
            s.log_lines.clear();
        }
        s.batch_has_result = false;
        s.running          = true;
        if (s.worker.joinable()) s.worker.join();
        s.worker = std::thread([&s]() {
            auto log_fn = [&s](const std::string& line) {
                std::lock_guard<std::mutex> lk(s.log_mutex);
                s.log_lines.push_back(line);
            };
            s.batch_result     = run_batch(s.batch_source, s.batch_output,
                                           s.batch_dry_run, s.batch_force, log_fn);
            s.batch_has_result = true;
            s.running          = false;
        });
    }
    if (!can_go) ImGui::EndDisabled();

    ImGui::Spacing();

    // Log panel — fills remaining vertical space
    const float summary_h = s.batch_has_result
                            ? (ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y)
                            : 0.0f;
    const float log_h = ImGui::GetContentRegionAvail().y - summary_h;

    ImGui::BeginChild("##log", {0, log_h}, true, ImGuiWindowFlags_HorizontalScrollbar);
    {
        std::lock_guard<std::mutex> lk(s.log_mutex);
        for (const auto& line : s.log_lines) {
            if (line.rfind("  OK ", 0) == 0)
                ImGui::TextColored({0.4f, 1.0f, 0.4f, 1.0f}, "%s", line.c_str());
            else if (line.rfind("  FAIL", 0) == 0)
                ImGui::TextColored({1.0f, 0.45f, 0.45f, 1.0f}, "%s", line.c_str());
            else
                ImGui::TextUnformatted(line.c_str());
        }
        // Auto-scroll to bottom only when already near the bottom
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0f)
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    if (s.batch_has_result) {
        ImGui::Text("Done: %d converted, %d failed, %d skipped.",
                    s.batch_result.converted,
                    s.batch_result.failed,
                    s.batch_result.skipped);
    }
}

// ── Single file tab ────────────────────────────────────────────────────────

static void draw_single_tab(AppState& s) {
    const float browse_w = 72.0f;
    const float avail    = ImGui::GetContentRegionAvail().x;
    const float field_w  = avail - browse_w - ImGui::GetStyle().ItemSpacing.x;

    ImGui::Text("Format:");
    ImGui::SameLine(0, 12);
    ImGui::RadioButton("ROM", &s.single_fmt, 0);
    ImGui::SameLine(0, 24);
    ImGui::RadioButton("BIN + CFG", &s.single_fmt, 1);
    ImGui::Spacing();

    static const nfdu8filteritem_t rom_filter[] = { {"ROM files",    "rom"    } };
    static const nfdu8filteritem_t bin_filter[] = { {"Binary files", "bin,int"} };
    static const nfdu8filteritem_t cfg_filter[] = { {"Config files", "cfg"    } };

    if (s.single_fmt == 0) {
        // ROM mode
        ImGui::Text("ROM file:");
        ImGui::SetNextItemWidth(field_w);
        ImGui::InputText("##srom", s.single_rom, sizeof(s.single_rom));
        ImGui::SameLine();
        if (ImGui::Button("Browse##srom", {browse_w, 0})) {
            browse_file(s.single_rom, sizeof(s.single_rom), rom_filter, 1);
            auto_stem(s.single_rom, s.single_stem, sizeof(s.single_stem));
        }
    } else {
        // BIN + CFG mode
        ImGui::Text("BIN file:");
        ImGui::SetNextItemWidth(field_w);
        ImGui::InputText("##sbin", s.single_bin, sizeof(s.single_bin));
        ImGui::SameLine();
        if (ImGui::Button("Browse##sbin", {browse_w, 0})) {
            browse_file(s.single_bin, sizeof(s.single_bin), bin_filter, 1);
            auto_stem(s.single_bin, s.single_stem, sizeof(s.single_stem));
        }

        ImGui::Text("CFG file:");
        ImGui::SetNextItemWidth(field_w);
        ImGui::InputText("##scfg", s.single_cfg, sizeof(s.single_cfg));
        ImGui::SameLine();
        if (ImGui::Button("Browse##scfg", {browse_w, 0}))
            browse_file(s.single_cfg, sizeof(s.single_cfg), cfg_filter, 1);
    }

    ImGui::Spacing();
    ImGui::Text("Output stem:");
    ImGui::SetNextItemWidth(avail);
    ImGui::InputText("##sstem", s.single_stem, sizeof(s.single_stem));
    ImGui::TextDisabled("  The stem is the output path without extension, e.g. /path/to/MyGame");
    ImGui::Spacing();

    bool can_go = !s.running && s.single_stem[0] != '\0'
               && (s.single_fmt == 0
                   ? s.single_rom[0] != '\0'
                   : (s.single_bin[0] != '\0' && s.single_cfg[0] != '\0'));

    if (!can_go) ImGui::BeginDisabled();
    const float btn_w = 160.0f;
    ImGui::SetCursorPosX((avail - btn_w) * 0.5f);
    if (ImGui::Button(s.running ? "Working..." : "Convert##single", {btn_w, 0})) {
        s.single_status.clear();
        s.running = true;
        if (s.worker.joinable()) s.worker.join();
        s.worker = std::thread([&s]() {
            std::string err;
            if (s.single_fmt == 0)
                err = convert_rom(s.single_rom, s.single_stem, true);
            else
                err = convert_cfg(s.single_bin, s.single_cfg, s.single_stem, true);

            s.single_ok = err.empty();
            if (err.empty()) {
                std::string base = fs::path(s.single_stem).filename().string();
                s.single_status = "OK — wrote " + base + "-nt-noir.intv"
                                  " and " + base + "-pocket.intv";
            } else {
                s.single_status = "Error: " + err;
            }
            s.running = false;
        });
    }
    if (!can_go) ImGui::EndDisabled();

    if (!s.single_status.empty()) {
        ImGui::Spacing();
        if (s.single_ok)
            ImGui::TextColored({0.4f, 1.0f, 0.4f, 1.0f}, "%s", s.single_status.c_str());
        else
            ImGui::TextWrapped("%s", s.single_status.c_str());
    }
}

// ── CLI fallback (same subcommands as intv2convert) ───────────────────────

int cmd_rom(int argc, char* argv[]);
int cmd_cfg(int argc, char* argv[]);
int cmd_lst(int argc, char* argv[]);
int cmd_batch(int argc, char* argv[]);

static int run_cli(int argc, char* argv[]) {
#ifdef _WIN32
    // Re-attach to the parent console so printf/fprintf reach the terminal.
    // This is a no-op when there is no parent console (e.g. double-clicked).
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif
    if (argc < 2) {
        fprintf(stderr,
            "intv2convert -- Convert Intellivision ROMs to INTV2 format\n\n"
            "Usage:\n"
            "  intv2convert rom   <input.rom> <output_stem>\n"
            "  intv2convert cfg   <input.bin> <input.cfg> <output_stem>\n"
            "  intv2convert lst   <input.lst> <output.intv> [--pocket]\n"
            "  intv2convert batch <source_dir> <output_dir> [--dry-run] [--force]\n\n"
            "Run without arguments to open the GUI.\n");
        return 1;
    }
    std::string cmd = argv[1];
    if (cmd == "rom")                        return cmd_rom(argc - 1, argv + 1);
    if (cmd == "cfg")                        return cmd_cfg(argc - 1, argv + 1);
    if (cmd == "lst")                        return cmd_lst(argc - 1, argv + 1);
    if (cmd == "batch")                      return cmd_batch(argc - 1, argv + 1);
    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        // re-invoke with no args to print usage
        char* fake[] = { argv[0] };
        return run_cli(1, fake);
    }
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    return 1;
}

// ── main ──────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc > 1) return run_cli(argc, argv);
    glfwSetErrorCallback([](int, const char* desc) {
        fprintf(stderr, "GLFW Error: %s\n", desc);
    });
    if (!glfwInit()) return 1;

#ifdef __APPLE__
    // macOS requires forward-compat core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    const char* glsl_version = "#version 150";
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    const char* glsl_version = "#version 130";
#endif

    GLFWwindow* window = glfwCreateWindow(860, 560, "intv2convert", nullptr, nullptr);
    if (!window) {
#ifdef _WIN32
        MessageBoxA(nullptr,
            "intv2convert could not open an OpenGL 3.0 window.\n\n"
            "Your graphics driver may not support OpenGL 3.0, or it may need updating.\n\n"
            "You can still use intv2convert from the command line:\n"
            "  intv2convert rom   <input.rom> <output_stem>\n"
            "  intv2convert cfg   <input.bin> <input.cfg> <output_stem>\n"
            "  intv2convert lst   <input.lst> <output.intv> [--pocket]\n"
            "  intv2convert batch <source_dir> <output_dir> [--dry-run] [--force]",
            "intv2convert", MB_OK | MB_ICONERROR);
#endif
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);   // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // don't write imgui.ini
    ImGui::StyleColorsDark();

    // Slightly larger font for readability
    io.Fonts->AddFontDefault();
    ImGui::GetStyle().FramePadding  = {6, 4};
    ImGui::GetStyle().ItemSpacing   = {8, 6};
    ImGui::GetStyle().WindowPadding = {12, 12};

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    NFD::Guard nfd_guard;
    AppState state;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##root", nullptr,
                     ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize   |
                     ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

        if (ImGui::BeginTabBar("##tabs")) {
            if (ImGui::BeginTabItem("Batch")) {
                draw_batch_tab(state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Single File")) {
                draw_single_tab(state);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();

        ImGui::Render();
        int dw, dh;
        glfwGetFramebufferSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Wait for any in-progress conversion before cleanup
    if (state.worker.joinable()) state.worker.join();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
