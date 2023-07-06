#include "saturn_imgui.h"

#include <string>
#include <iostream>
#include <algorithm>
#include <map>

#include "saturn/imgui/saturn_imgui_dynos.h"
#include "saturn/imgui/saturn_imgui_machinima.h"
#include "saturn/imgui/saturn_imgui_settings.h"
#include "saturn/imgui/saturn_imgui_chroma.h"
#include "saturn/libs/imgui/imgui.h"
#include "saturn/libs/imgui/imgui_internal.h"
#include "saturn/libs/imgui/imgui_impl_sdl.h"
#include "saturn/libs/imgui/imgui_impl_opengl3.h"
#include "saturn/libs/imgui/imgui_neo_sequencer.h"
#include "saturn/saturn.h"
#include "saturn/saturn_colors.h"
#include "saturn/saturn_textures.h"
#include "saturn/discord/saturn_discord.h"
#include "pc/controller/controller_keyboard.h"
#include "data/dynos.cpp.h"
#include "icons/IconsForkAwesome.h"

#include <SDL2/SDL.h>

#ifdef __MINGW32__
# define FOR_WINDOWS 1
#else
# define FOR_WINDOWS 0
#endif

#if FOR_WINDOWS || defined(OSX_BUILD)
# define GLEW_STATIC
# include <GL/glew.h>
#endif

#define GL_GLEXT_PROTOTYPES 1
#ifdef USE_GLES
# include <SDL2/SDL_opengles2.h>
# define RAPI_NAME "OpenGL ES"
#else
# include <SDL2/SDL_opengl.h>
# define RAPI_NAME "OpenGL"
#endif

#if FOR_WINDOWS
#define PLATFORM "Windows"
#define PLATFORM_ICON ICON_FK_WINDOWS
#elif defined(OSX_BUILD)
#define PLATFORM "Mac OS"
#define PLATFORM_ICON ICON_FK_APPLE
#else
#define PLATFORM "Linux"
#define PLATFORM_ICON ICON_FK_LINUX
#endif

extern "C" {
#include "pc/gfx/gfx_pc.h"
#include "pc/configfile.h"
#include "game/mario.h"
#include "game/game_init.h"
#include "game/camera.h"
#include "game/level_update.h"
#include "engine/level_script.h"
#include "game/object_list_processor.h"
}

using namespace std;

SDL_Window* window = nullptr;
ImGuiIO io;

Array<PackData *> &iDynosPacks = DynOS_Gfx_GetPacks();

// Variables

int currentMenu = 0;
bool showMenu = true;
bool showStatusBars = true;

bool windowStats = true;
bool windowCcEditor;
bool windowAnimPlayer;
bool windowSettings;
bool windowChromaKey;

bool chromaRequireReload;

int windowStartHeight;

bool has_copy_camera;
bool copy_relative = true;

bool paste_forever;

// Bundled Components

void imgui_bundled_tooltip(const char* text) {
    if (!configEditorShowTips) return;

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(450.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void imgui_bundled_help_marker(const char* desc) {
    if (!configEditorShowTips) {
        ImGui::TextDisabled("");
        return;
    }

    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void imgui_bundled_space(float size, const char* title = NULL, const char* help_marker = NULL) {
    ImGui::Dummy(ImVec2(0, size/2));
    ImGui::Separator();
    if (title == NULL) {
        ImGui::Dummy(ImVec2(0, size/2));
    } else {
        ImGui::Dummy(ImVec2(0, size/4));
        ImGui::Text(title);
        if (help_marker != NULL) {
            ImGui::SameLine(); imgui_bundled_help_marker(help_marker);
        }
        ImGui::Dummy(ImVec2(0, size/4));
    }
}

void imgui_bundled_window_reset(const char* windowTitle, int width, int height, int x, int y) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::Selectable(ICON_FK_UNDO " Reset Window Pos")) {
            ImGui::SetWindowSize(ImGui::FindWindowByName(windowTitle), ImVec2(width, height));
            ImGui::SetWindowPos(ImGui::FindWindowByName(windowTitle), ImVec2(x, y));
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

ImGuiWindowFlags imgui_bundled_window_corner(int corner, int width, int height, float alpha) {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
    if (corner != -1) {
        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos;
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = (corner & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
        window_pos.y = (corner & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
        window_pos_pivot.x = (corner & 1) ? 1.0f : 0.0f;
        window_pos_pivot.y = (corner & 2) ? 1.0f : 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        if (width != 0) ImGui::SetNextWindowSize(ImVec2(width, height));
        window_flags |= ImGuiWindowFlags_NoMove;
        ImGui::SetNextWindowBgAlpha(alpha);
    }

    return window_flags;
}

void imgui_update_theme() {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle* style = &ImGui::GetStyle();

    float SCALE = 1.f;

    ImFontConfig defaultConfig;
    defaultConfig.SizePixels = 13.0f * SCALE;
    io.Fonts->AddFontDefault(&defaultConfig);

    ImFontConfig symbolConfig;
    symbolConfig.MergeMode = true;
    symbolConfig.SizePixels = 13.0f * SCALE;
    symbolConfig.GlyphMinAdvanceX = 13.0f * SCALE; // Use if you want to make the icon monospaced
    static const ImWchar icon_ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 };
    io.Fonts->AddFontFromFileTTF("fonts/forkawesome-webfont.ttf", symbolConfig.SizePixels, &symbolConfig, icon_ranges);

    if (configEditorTheme == 0) {
        ImGui::StyleColorsDark();
    } else if (configEditorTheme == 1) {
        ImGui::StyleColorsMoon();
    } else if (configEditorTheme == 2) {
        ImGui::StyleColorsHalfLife();
    } else if (configEditorTheme == 3) {
        ImGui::StyleColorsM64MM();
    } else if (configEditorTheme == 4) {
        ImGui::StyleColorsClassic();
    }

    style->ScaleAllSizes(SCALE);
}

// Set up ImGui

void saturn_imgui_init(SDL_Window * sdl_window, SDL_GLContext ctx) {
    window = sdl_window;

    const char* glsl_version = "#version 120";
    ImGuiContext* imgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui);
    io = ImGui::GetIO(); (void)io;
    io.WantSetMousePos = false;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    imgui_update_theme();

    ImGui_ImplSDL2_InitForOpenGL(window, ctx);
    ImGui_ImplOpenGL3_Init(glsl_version);

    sdynos_imgui_init();
    smachinima_imgui_init();
    ssettings_imgui_init();

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, configWindowState?"1":"0");
}

void saturn_imgui_handle_events(SDL_Event * event) {
    ImGui_ImplSDL2_ProcessEvent(event);
    switch (event->type){
        case SDL_KEYDOWN:
            if(event->key.keysym.sym == SDLK_F4) {
                limit_fps = !limit_fps;
                configWindow.fps_changed = true;
            }

            if(event->key.keysym.sym == SDLK_F5) {
                imgui_update_theme();
            }
        
        break;
    }
    smachinima_imgui_controls(event);
}

void saturn_imgui_update() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
    
    windowStartHeight = (showStatusBars) ? 48 : 30;

    camera_savestate_mult = 1.f;

    if (showMenu) {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Menu")) {
                windowCcEditor = false;

                if (ImGui::MenuItem(ICON_FK_WINDOW_MAXIMIZE " Show UI",      translate_bind_to_name(configKeyShowMenu[0]), showMenu)) {
                    showMenu = !showMenu;
                    if (!showMenu) accept_text_input = true;
                }
                if (ImGui::MenuItem(ICON_FK_WINDOW_MINIMIZE " Show Status Bars",  NULL, showStatusBars)) showStatusBars = !showStatusBars;
                ImGui::Separator();
                if (ImGui::MenuItem("Stats",        NULL, windowStats == true)) windowStats = !windowStats;
                if (ImGui::MenuItem(ICON_FK_COG " Settings",     NULL, windowSettings == true)) {
                    windowSettings = !windowSettings;
                    k_popout_open = false;
                }
                //if (windowStats) imgui_bundled_window_reset("Stats", 250, 125, 10, windowStartHeight);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Camera")) {
                windowCcEditor = false;

                ImGui::Checkbox("Freeze", &camera_frozen);
                if (camera_frozen) {
                    saturn_keyframe_camera_popout("Position", "k_c_camera");
                    ImGui::SameLine(200); ImGui::TextDisabled(translate_bind_to_name(configKeyFreeze[0]));

                    if (ImGui::BeginMenu("Options###camera_options")) {
                        camera_savestate_mult = 0.f;
                        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
                        ImGui::BeginChild("###model_metadata", ImVec2(200, 90), true, ImGuiWindowFlags_NoScrollbar);
                        ImGui::TextDisabled("pos %.f, %.f, %.f", gCamera->pos[0], gCamera->pos[1], gCamera->pos[2]);
                        ImGui::TextDisabled("foc %.f, %.f, %.f", gCamera->focus[0], gCamera->focus[1], gCamera->focus[2]);
                        if (ImGui::Button(ICON_FK_FILES_O " Copy###copy_camera")) {
                            saturn_copy_camera(copy_relative);
                            if (copy_relative) saturn_paste_camera();
                            has_copy_camera = 1;
                        } ImGui::SameLine();
                        if (!has_copy_camera) ImGui::BeginDisabled();
                        if (ImGui::Button(ICON_FK_CLIPBOARD " Paste###paste_camera")) {
                            if (has_copy_camera) saturn_paste_camera();
                        }
                        /*ImGui::Checkbox("Loop###camera_paste_forever", &paste_forever);
                        if (paste_forever) {
                            saturn_paste_camera();
                        }*/
                        if (!has_copy_camera) ImGui::EndDisabled();
                        ImGui::Checkbox("Relative to Mario###camera_copy_relative", &copy_relative);

                        ImGui::EndChild();
                        ImGui::PopStyleVar();

                        ImGui::Text(ICON_FK_VIDEO_CAMERA " Speed");
                        ImGui::PushItemWidth(150);
                        ImGui::SliderFloat("Move", &camVelSpeed, 0.0f, 2.0f);
                        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { camVelSpeed = 1.f; }
                        ImGui::SliderFloat("Rotate", &camVelRSpeed, 0.0f, 2.0f);
                        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { camVelRSpeed = 1.f; }
                        ImGui::PopItemWidth();
                        ImGui::Text(ICON_FK_KEYBOARD_O " Control Mode");
                        const char* mCameraSettings[] = { "Keyboard", "Keyboard/Gamepad (Old)", "Mouse (Experimental)" };
                        ImGui::PushItemWidth(200);
                        ImGui::Combo("###camera_mode", (int*)&configMCameraMode, mCameraSettings, IM_ARRAYSIZE(mCameraSettings));
                        ImGui::PopItemWidth();
                        if (configMCameraMode == 2) {
                            imgui_bundled_tooltip("Move Camera -> LShift + Mouse Buttons");
                        } else if (configMCameraMode == 1) {
                            imgui_bundled_tooltip("Pan Camera -> R + C-Buttons\nRaise/Lower Camera -> L + C-Buttons\nRotate Camera -> L + Crouch + C-Buttons");
                        } else if (configMCameraMode == 0) {
                            imgui_bundled_tooltip("Move Camera -> Y/G/H/J\nRaise/Lower Camera -> T/U\nRotate Camera -> R + Y/G/H/J");
                        }
                        ImGui::EndMenu();
                    }
                }
                ImGui::Separator();
                ImGui::PushItemWidth(100);
                ImGui::SliderFloat("FOV", &camera_fov, 0.0f, 100.0f);
                if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { camera_fov = 50.f; }
                imgui_bundled_tooltip("Controls the FOV of the in-game camera; Default is 50, or 40 in Project64.");
                saturn_keyframe_float_popout(&camera_fov, "FOV", "k_fov");
                ImGui::SameLine(200); ImGui::TextDisabled("N/M");
                ImGui::PopItemWidth();
                ImGui::Checkbox("Smooth###fov_smooth", &camera_fov_smooth);

                ImGui::Separator();
                ImGui::PushItemWidth(100);
                ImGui::SliderFloat("Follow", &camera_focus, 0.0f, 1.0f);
                if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { camera_focus = 1.f; }
                saturn_keyframe_float_popout(&camera_focus, "Follow", "k_focus");
                ImGui::PopItemWidth();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Appearance")) {
                sdynos_imgui_menu();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Game")) {
                imgui_machinima_quick_options();
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem(ICON_FK_EYEDROPPER " CHROMA KEY", NULL, autoChroma)) {
                schroma_imgui_init();
                autoChroma = !autoChroma;
                windowCcEditor = false;
                windowAnimPlayer = false;

                // Auto-chroma
                // Allows any level to become a customizable chroma key stage
                for (int i = 0; i < 960; i++) {
                    if (!autoChroma) gObjectPool[i].header.gfx.node.flags &= ~GRAPH_RENDER_INVISIBLE;
                    else gObjectPool[i].header.gfx.node.flags |= GRAPH_RENDER_INVISIBLE;
                }
            }
            ImGui::EndMainMenuBar();
        }

        ImGuiViewportP* viewport = (ImGuiViewportP*)(void*)ImGui::GetMainViewport();
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
        float height = ImGui::GetFrameHeight();

        if (showStatusBars) {
            if (ImGui::BeginViewportSideBar("##SecondaryMenuBar", viewport, ImGuiDir_Up, height, window_flags)) {
                if (ImGui::BeginMenuBar()) {
                    if (configFps60) ImGui::TextDisabled("%.1f FPS (%.3f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
                    else ImGui::TextDisabled("%.1f FPS (%.3f ms/frame)", ImGui::GetIO().Framerate / 2, 1000.0f / (ImGui::GetIO().Framerate / 2));
                    ImGui::EndMenuBar();
                }
                ImGui::End();
            }

            if (ImGui::BeginViewportSideBar("##MainStatusBar", viewport, ImGuiDir_Down, height, window_flags)) {
                if (ImGui::BeginMenuBar()) {
                    ImGui::Text(PLATFORM_ICON " ");
#ifdef GIT_BRANCH
#ifdef GIT_HASH
                    ImGui::SameLine(20);
                    ImGui::TextDisabled(ICON_FK_GITHUB " " GIT_BRANCH " " GIT_HASH);
#endif
#endif
                    ImGui::EndMenuBar();
                }
                ImGui::End();
            }
        }

        if (windowStats) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGuiWindowFlags stats_flags = imgui_bundled_window_corner(2, 0, 0, 0.64f);
            ImGui::Begin("Stats", &windowStats, stats_flags);

#ifdef DISCORDRPC
            if (has_discord_init && gCurUser.username != "") {
                if (gCurUser.username == NULL || gCurUser.username == "") {
                    ImGui::Text(ICON_FK_DISCORD " Loading...");
                } else {
                    std::string disc = gCurUser.discriminator;
                    if (disc == "0") ImGui::Text(ICON_FK_DISCORD " @%s", gCurUser.username);
                    else ImGui::Text(ICON_FK_DISCORD " %s#%s", gCurUser.username, gCurUser.discriminator);
                }
                ImGui::Separator();
            }
#endif
            ImGui::Text(ICON_FK_FOLDER_OPEN " %s", model_details.c_str());
            ImGui::Text(ICON_FK_FILE_TEXT " %s", cc_details.c_str());
            ImGui::TextDisabled(ICON_FK_PICTURE_O " %i textures loaded", preloaded_textures_count);

            ImGui::End();
            ImGui::PopStyleColor();
        }
        if (windowCcEditor) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGuiWindowFlags cce_flags = imgui_bundled_window_corner(0, 0, 0, 1.f);
            ImGui::Begin("Color Code Editor", &windowCcEditor, cce_flags);
            imgui_dynos_cc_editor();
            ImGui::End();
            ImGui::PopStyleColor();

#ifdef DISCORDRPC
            discord_state = "In-Game // Editing a CC";
#endif
        }
        if (windowAnimPlayer && mario_exists) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGuiWindowFlags anim_flags = imgui_bundled_window_corner(0, 0, 0, 1.f);
            ImGui::Begin("Animation Mixtape", &windowAnimPlayer, anim_flags);
            imgui_machinima_animation_player();
            ImGui::End();
            ImGui::PopStyleColor();
        }
        if (windowSettings) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGuiWindowFlags settings_flags = imgui_bundled_window_corner(1, 0, 0, 0.64f);
            ImGui::Begin("Settings", &windowSettings, settings_flags);
            ssettings_imgui_update();
            ImGui::End();
            ImGui::PopStyleColor();
        }
        if (autoChroma && mario_exists) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGuiWindowFlags chroma_flags = imgui_bundled_window_corner(0, 0, 0, 1.f);
            if (chromaRequireReload) chroma_flags |= ImGuiWindowFlags_UnsavedDocument;
            ImGui::Begin("Chroma Key Settings", &autoChroma, chroma_flags);
            schroma_imgui_update();
            ImGui::End();
            ImGui::PopStyleColor();
        }

        //ImGui::ShowDemoWindow();
    }

    is_cc_editing = windowCcEditor;

    ImGui::Render();
    GLint last_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    glUseProgram(0);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glUseProgram(last_program);
}

uint32_t startFrame = 0;
uint32_t endFrame = 60;
int endFrameText = 60;

void saturn_keyframe_window(string value_name, string id) {
#ifdef DISCORDRPC
    discord_state = "In-Game // Keyframing";
#endif

    if (windowSettings) windowSettings = false;

    ImGuiWindowFlags timeline_flags = imgui_bundled_window_corner(1, 0, 0, 0.64f);
    string windowLabel = "Timeline###kw_" + id;
    ImGui::Begin(windowLabel.c_str(), &k_popout_open, timeline_flags);
    if (!keyframe_playing) {
        if (k_frame_keys.size() < 2) ImGui::BeginDisabled();
        if (ImGui::Button(ICON_FK_PLAY " Play###k_t_play")) {
            saturn_play_keyframe(active_data_type);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Loop###k_t_loop", &k_loop);
        if (k_frame_keys.size() < 2) ImGui::EndDisabled();
        ImGui::SameLine(180);
        if (ImGui::Button(ICON_FK_TRASH_O " Clear All###k_t_clear")) {
            k_last_placed_frame = 0;
            k_frame_keys = {0};
            k_v_float_keys = {0.f};
            k_v_bool_keys = {0};
            
            k_c_pos1_keys = {0.f};
            k_c_pos2_keys = {0.f};
            k_c_foc0_keys = {0.f};
            k_c_foc1_keys = {0.f};
            k_c_foc2_keys = {0.f};
            k_c_rot0_keys = {0.f};
            k_c_rot1_keys = {0.f};
        }
    } else {
        if (ImGui::Button(ICON_FK_STOP " Stop###k_t_stop")) {
            keyframe_playing = false;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Loop###k_t_loop", &k_loop);
    }

    ImGui::Separator();

    ImGui::PushItemWidth(35);
    if (ImGui::InputInt("Frames", &endFrameText, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (endFrameText >= 60) {
            endFrame = (uint32_t)endFrameText;
        } else {
            endFrame = 60;
            endFrameText = 60;
        }
    }
    ImGui::PopItemWidth();
            
    // Popout
    if (ImGui::BeginNeoSequencer("Sequencer###k_sequencer", (uint32_t*)&k_current_frame, &startFrame, &endFrame, ImVec2(endFrame * 6, 0), ImGuiNeoSequencerFlags_HideZoom)) {
        if (ImGui::BeginNeoTimeline(value_name.c_str(), k_frame_keys)) { ImGui::EndNeoTimeLine(); }
        if (active_data_type == KEY_CAMERA) if(ImGui::BeginNeoTimeline("Rotation", k_frame_keys)) { ImGui::EndNeoTimeLine(); }
        ImGui::EndNeoSequencer();
    }

    // UI Controls
    if (!keyframe_playing) {
        if (k_current_frame != 0) {
            if (std::find(k_frame_keys.begin(), k_frame_keys.end(), k_current_frame) != k_frame_keys.end()) {
                // We are hovering over a keyframe
                auto it = std::find(k_frame_keys.begin(), k_frame_keys.end(), k_current_frame);
                if (it != k_frame_keys.end()) {
                    int key_index = it - k_frame_keys.begin();
                    if (ImGui::Button(ICON_FK_MINUS_SQUARE " Delete Keyframe###k_d_frame")) {
                        // Delete Keyframe
                        k_frame_keys.erase(k_frame_keys.begin() + key_index);
                        if (active_data_type == KEY_FLOAT || active_data_type == KEY_CAMERA) k_v_float_keys.erase(k_v_float_keys.begin() + key_index);
                        if (active_data_type == KEY_BOOL) k_v_bool_keys.erase(k_v_bool_keys.begin() + key_index);

                        if (active_data_type == KEY_CAMERA) {
                            k_c_pos1_keys.erase(k_c_pos1_keys.begin() + key_index);
                            k_c_pos2_keys.erase(k_c_pos2_keys.begin() + key_index);
                            k_c_foc0_keys.erase(k_c_foc0_keys.begin() + key_index);
                            k_c_foc1_keys.erase(k_c_foc1_keys.begin() + key_index);
                            k_c_foc2_keys.erase(k_c_foc2_keys.begin() + key_index);
                            k_c_rot0_keys.erase(k_c_rot0_keys.begin() + key_index);
                            k_c_rot1_keys.erase(k_c_rot1_keys.begin() + key_index);
                        }

                        k_last_placed_frame = k_frame_keys[k_frame_keys.size() - 1];
                    } ImGui::SameLine(); ImGui::Text("at %i", (int)k_current_frame);
                }
            } else {
                // No keyframe here
                if (ImGui::Button(ICON_FK_PLUS_SQUARE " Create Keyframe###k_c_frame")) {
                    if (k_last_placed_frame > k_current_frame) {
                        
                    } else {
                        k_frame_keys.push_back(k_current_frame);
                        if (active_data_type == KEY_FLOAT || active_data_type == KEY_CAMERA) k_v_float_keys.push_back(floor(*active_key_float_value));
                        if (active_data_type == KEY_BOOL) k_v_bool_keys.push_back(*active_key_bool_value);

                        if (active_data_type == KEY_CAMERA) {
                            f32 dist;
                            s16 pitch, yaw;
                            vec3f_get_dist_and_angle(gCamera->pos, gCamera->focus, &dist, &pitch, &yaw);
                            k_c_pos1_keys.push_back(floor(gCamera->pos[1]));
                            k_c_pos2_keys.push_back(floor(gCamera->pos[2]));
                            k_c_foc0_keys.push_back(floor(gCamera->focus[0]));
                            k_c_foc1_keys.push_back(floor(gCamera->focus[1]));
                            k_c_foc2_keys.push_back(floor(gCamera->focus[2]));
                            k_c_rot0_keys.push_back(floor(yaw));
                            k_c_rot1_keys.push_back(floor(pitch));
                            //std::cout << pitch << std::endl;
                        }
                        k_last_placed_frame = k_current_frame;
                    }
                } ImGui::SameLine(); ImGui::Text("at %i", (int)k_current_frame);
            }
        } else {
            // On frame 0
            if (active_data_type == KEY_CAMERA)
                ImGui::Text("(Setting initial value)");
        }
    }
    ImGui::End();

    // Auto focus (use controls without clicking window first)
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && accept_text_input == false) {
        ImGui::SetWindowFocus(windowLabel.c_str());
    }
}

/*
    ======== SATURN ========
    Advanced Keyframe Engine
    ========================
*/

void saturn_keyframe_float_popout(float* edit_value, string value_name, string id) {
    string buttonLabel = ICON_FK_LINK "###kb_" + id;

    ImGui::SameLine();
    if (ImGui::Button(buttonLabel.c_str())) {
        if (active_key_float_value != edit_value) {
            keyframe_playing = false;
            k_last_placed_frame = 0;
            k_frame_keys = {0};
            k_v_float_keys = {0.f};
        }

        k_popout_open = true;
        active_key_float_value = edit_value;
        active_data_type = KEY_FLOAT;
    }
    imgui_bundled_tooltip("Animate");

    if (active_key_float_value == edit_value && active_data_type == KEY_FLOAT) {
        if (k_popout_open) saturn_keyframe_window(value_name, id);
    }
}
void saturn_keyframe_bool_popout(bool* edit_value, string value_name, string id) {
    string buttonLabel = ICON_FK_LINK "###kb_" + id;

    ImGui::SameLine();
    if (ImGui::Button(buttonLabel.c_str())) {
        if (active_key_bool_value != edit_value) {
            keyframe_playing = false;
            k_last_placed_frame = 0;
            k_frame_keys = {0};
            k_v_bool_keys = {false};
        }

        k_popout_open = true;
        active_key_bool_value = edit_value;
        active_data_type = KEY_BOOL;
    }
    imgui_bundled_tooltip("Animate");

    if (active_key_bool_value == edit_value && active_data_type == KEY_BOOL) {
        if (k_popout_open) saturn_keyframe_window(value_name, id);
    }
}
void saturn_keyframe_camera_popout(string value_name, string id) {
    float* edit_value = &gCamera->pos[0];
    string buttonLabel = ICON_FK_LINK "###kb_" + id;

    ImGui::SameLine();
    if (ImGui::Button(buttonLabel.c_str())) {
        if (active_key_float_value != edit_value) {
            keyframe_playing = false;
            k_last_placed_frame = 0;
            k_frame_keys = {0};
            k_v_float_keys = {0.f};
        }

        k_popout_open = true;
        active_key_float_value = edit_value;
        active_data_type = KEY_CAMERA;
    }
    imgui_bundled_tooltip("Animate");

    if (active_key_float_value == edit_value && active_data_type == KEY_CAMERA) {
        if (k_popout_open) saturn_keyframe_window(value_name, id);
    }
}

template <typename T>
void saturn_keyframe_popout(const T &edit_value, s32 data_type, string value_name, string id) {
    /*string buttonLabel = ICON_FK_LINK "###kb_" + id;
    string windowLabel = "Timeline###kw_" + id;

#ifdef DISCORDRPC
    discord_state = "In-Game // Keyframing";
#endif

    ImGui::SameLine();
    if (ImGui::Button(buttonLabel.c_str())) {
        if (active_key_float_value != edit_value) {
            keyframe_playing = false;
            k_last_placed_frame = 0;
            k_frame_keys = {0};
            k_v_float_keys = {0.f};

            k_c_pos1_keys = {0.f};
            k_c_pos2_keys = {0.f};
            k_c_foc0_keys = {0.f};
            k_c_foc1_keys = {0.f};
            k_c_foc2_keys = {0.f};
            k_c_rot0_keys = {0.f};
            k_c_rot1_keys = {0.f};
        }

        k_popout_open = true;
        active_key_float_value = edit_value;
        active_data_type = data_type;
    }

    if (k_popout_open && active_key_float_value == edit_value) {
        if (windowSettings) windowSettings = false;

        ImGuiWindowFlags timeline_flags = imgui_bundled_window_corner(1, 0, 0, 0.64f);
        ImGui::Begin(windowLabel.c_str(), &k_popout_open, timeline_flags);
        if (!keyframe_playing) {
            if (ImGui::Button(ICON_FK_PLAY " Play###k_t_play")) {
                saturn_play_keyframe();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Loop###k_t_loop", &k_loop);
            ImGui::SameLine(180);
            if (ImGui::Button(ICON_FK_TRASH_O " Clear All###k_t_clear")) {
                k_last_placed_frame = 0;
                k_frame_keys = {0};
                k_v_float_keys = {0.f};
                k_v_int_keys = {0};
                
                k_c_pos1_keys = {0.f};
                k_c_pos2_keys = {0.f};
                k_c_foc0_keys = {0.f};
                k_c_foc1_keys = {0.f};
                k_c_foc2_keys = {0.f};
                k_c_rot0_keys = {0.f};
                k_c_rot1_keys = {0.f};
            }
        } else {
            if (ImGui::Button(ICON_FK_STOP " Stop###k_t_stop")) {
                keyframe_playing = false;
            }
            ImGui::SameLine();
            ImGui::Checkbox("Loop###k_t_loop", &k_loop);
        }

        ImGui::Separator();

        ImGui::PushItemWidth(35);
        if (ImGui::InputInt("Frames", &endFrameText, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (endFrameText >= 60) {
                endFrame = (uint32_t)endFrameText;
            } else {
                endFrame = 60;
                endFrameText = 60;
            }
        }
        ImGui::PopItemWidth();
                
        // Popout
        if (ImGui::BeginNeoSequencer("Sequencer###k_sequencer", (uint32_t*)&k_current_frame, &startFrame, &endFrame, ImVec2(endFrame * 6, 0), ImGuiNeoSequencerFlags_HideZoom)) {
            if (ImGui::BeginNeoTimeline(value_name.c_str(), k_frame_keys)) { ImGui::EndNeoTimeLine(); }
            if (data_type == KEY_CAMERA) if(ImGui::BeginNeoTimeline("Rotation", k_frame_keys)) { ImGui::EndNeoTimeLine(); }
            ImGui::EndNeoSequencer();
        }

        // UI Controls
        if (!keyframe_playing) {
            if (k_current_frame != 0) {
                if (std::find(k_frame_keys.begin(), k_frame_keys.end(), k_current_frame) != k_frame_keys.end()) {
                    // We are hovering over a keyframe
                    auto it = std::find(k_frame_keys.begin(), k_frame_keys.end(), k_current_frame);
                    if (it != k_frame_keys.end()) {
                        int key_index = it - k_frame_keys.begin();
                        if (ImGui::Button(ICON_FK_MINUS_SQUARE " Delete Keyframe###k_d_frame")) {
                            // Delete Keyframe
                            k_frame_keys.erase(k_frame_keys.begin() + key_index);
                            if (data_type == KEY_FLOAT || data_type == KEY_CAMERA) k_v_float_keys.erase(k_v_float_keys.begin() + key_index);
                            if (data_type == KEY_INT) k_v_int_keys.erase(k_v_int_keys.begin() + key_index);

                            if (data_type == KEY_CAMERA) {
                                k_c_pos1_keys.erase(k_c_pos1_keys.begin() + key_index);
                                k_c_pos2_keys.erase(k_c_pos2_keys.begin() + key_index);
                                k_c_foc0_keys.erase(k_c_foc0_keys.begin() + key_index);
                                k_c_foc1_keys.erase(k_c_foc1_keys.begin() + key_index);
                                k_c_foc2_keys.erase(k_c_foc2_keys.begin() + key_index);
                                k_c_rot0_keys.erase(k_c_rot0_keys.begin() + key_index);
                                k_c_rot1_keys.erase(k_c_rot1_keys.begin() + key_index);
                            }

                            k_last_placed_frame = k_frame_keys[k_frame_keys.size() - 1];
                        } ImGui::SameLine(); ImGui::Text("at %i", (int)k_current_frame);
                    }
                } else {
                    // No keyframe here
                    if (ImGui::Button(ICON_FK_PLUS_SQUARE " Create Keyframe###k_c_frame")) {
                        if (k_last_placed_frame > k_current_frame) {
                            
                        } else {
                            k_frame_keys.push_back(k_current_frame);
                            if (data_type == KEY_FLOAT || data_type == KEY_CAMERA) k_v_float_keys.push_back(*edit_value);
                            if (data_type == KEY_INT) k_v_int_keys.push_back(*edit_value);

                            if (data_type == KEY_CAMERA) {
                                f32 dist;
                                s16 pitch, yaw;
                                vec3f_get_dist_and_angle(gCamera->pos, gCamera->focus, &dist, &pitch, &yaw);
                                k_c_pos1_keys.push_back(gCamera->pos[1]);
                                k_c_pos2_keys.push_back(gCamera->pos[2]);
                                k_c_foc0_keys.push_back(gCamera->focus[0]);
                                k_c_foc1_keys.push_back(gCamera->focus[1]);
                                k_c_foc2_keys.push_back(gCamera->focus[2]);
                                k_c_rot0_keys.push_back(yaw);
                                k_c_rot1_keys.push_back(pitch);
                                std::cout << pitch << std::endl;
                            }
                            k_last_placed_frame = k_current_frame;
                        }
                    } ImGui::SameLine(); ImGui::Text("at %i", (int)k_current_frame);
                }
            } else {
                // On frame 0
                if (data_type == KEY_CAMERA)
                    ImGui::Text("(Setting initial value)");
            }
        }
        ImGui::End();

        // Auto focus (use controls without clicking window first)
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && accept_text_input == false) {
            ImGui::SetWindowFocus(windowLabel.c_str());
        }
    }*/
}
