#include "saturn/saturn_textures.h"

#include <string>
#include <iostream>
#include <vector>
#include <SDL2/SDL.h>

#include "saturn/saturn.h"
#include "saturn/saturn_colors.h"
#include "saturn/imgui/saturn_imgui.h"
#include "saturn/imgui/saturn_imgui_chroma.h"

#include "saturn/libs/imgui/imgui.h"
#include "saturn/libs/imgui/imgui_internal.h"
#include "saturn/libs/imgui/imgui_impl_sdl.h"
#include "saturn/libs/imgui/imgui_impl_opengl3.h"

#include "pc/configfile.h"

extern "C" {
#include "game/mario.h"
#include "game/camera.h"
#include "game/level_update.h"
#include "sm64.h"
#include "pc/gfx/gfx_pc.h"
}

using namespace std;
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <assert.h>
#include <stdlib.h>
namespace fs = std::filesystem;
#include "pc/fs/fs.h"

#include <json/json.h>

bool is_replacing_exp;
bool is_replacing_eyes;

std::vector<string> eye_array;
string current_eye_pre_path;
string current_eye;
string current_eye_dir_path = "dynos/eyes/";
int current_eye_index;
bool model_eyes_enabled;

std::vector<string> mouth_array;
string current_mouth_pre_path;
string current_mouth;
string current_mouth_dir_path;
int current_mouth_index;
bool model_mouth_enabled;

int current_keybind_exp;
int current_exp_index[8];

bool show_vmario_emblem;

// Eye Folders, Non-Model

void saturn_load_eye_folder(std::string path) {
    eye_array.clear();
    fs::create_directory("res/gfx");

    // if eye folder is missing
    if (!fs::exists("dynos/eyes/"))
        return;

    // reset dir if we last used models or returned to root
    if (path == "../") path = "";
    if (model_eyes_enabled || path == "") {
        model_eyes_enabled = false;
        current_eye_dir_path = "dynos/eyes/";
    }

    // only update current path if folder exists
    if (fs::is_directory(current_eye_dir_path + path)) {
        current_eye_dir_path = current_eye_dir_path + path;
    }

    current_eye_pre_path = "../../" + current_eye_dir_path;

    if (current_eye_dir_path != "dynos/eyes/") {
        eye_array.push_back("../");
    }

    for (const auto & entry : fs::directory_iterator(current_eye_dir_path)) {
        if (fs::is_directory(entry.path())) {
            eye_array.push_back(entry.path().stem().u8string() + "/");
        } else {
            string entryPath = entry.path().filename().u8string();
            if (entryPath.find(".png") != string::npos) // only allow png files
                eye_array.push_back(entryPath);
        }
    }
    
    if (eye_array.size() > 0)
        saturn_set_eye_texture(0);
}

void saturn_eye_selectable(std::string name, int index) {
    if (name.find(".png") != string::npos) {
        // This is an eye
        saturn_set_eye_texture(index);
    } else {
        // This is a folder
        saturn_load_eye_folder(name);
    }
}

std::string current_mouth_folder;
std::string last_folder_name;

/*
    Sets an eye texture with an eye_array index.
*/
void saturn_set_eye_texture(int index) {
    if (eye_array[index].find(".png") == string::npos) {
        current_eye_index = -1;
        //current_eye = "actors/mario/mario_eyes_left_unused.rgba16";
        
        // Attempt to select the first actual PNG
        for (int i = 0; i < eye_array.size(); i++) {
            if (eye_array[i].find(".png") != string::npos) {
                saturn_set_eye_texture(i);
                break;
            }
        }

        return;
    } else {
        current_eye_index = index;
        current_eye = current_eye_pre_path + eye_array[index];
        current_eye = current_eye.substr(0, current_eye.size() - 4);
    }
}

// NEW SYSTEM, Model

string current_model_exp_tex[8];
bool using_model_eyes;

/*
    Handles texture replacement. Called from gfx_pc.c
*/
const void* saturn_bind_texture(const void* input) {
    const char* inputTexture = static_cast<const char*>(input);
    const char* outputTexture;

    string texName = string(inputTexture);

    if (current_model_data.name != "") {
        for (int i = 0; i < current_model_data.expressions.size(); i++) {

            // Could be either "saturn_eye" or "saturn_eyes", check for both
            string pos_name1 = "saturn_" + current_model_data.expressions[i].name;
            string pos_name2 = pos_name1.substr(0, pos_name1.size() - 1);

            if (texName.find(pos_name1) != string::npos || texName.find(pos_name2) != string::npos) {
                outputTexture = current_model_exp_tex[i].c_str();
                //std::cout << current_model_exp_tex[i] << std::endl;
                const void* output = static_cast<const void*>(outputTexture);
                return output;
            }

        }
    }

    if (eye_array.size() > 0 && is_replacing_eyes) {
        if (texName.find("saturn_eye") != string::npos ||
            texName == "actors/mario/mario_eyes_left_unused.rgba16" ||
            texName == "actors/mario/mario_eyes_right_unused.rgba16" ||
            texName == "actors/mario/mario_eyes_up_unused.rgba16" ||
            texName == "actors/mario/mario_eyes_down_unused.rgba16") {
                outputTexture = current_eye.c_str();
                const void* output = static_cast<const void*>(outputTexture);
                return output;
        }
    }

    if (show_vmario_emblem) {
        if (texName == "actors/mario/no_m.rgba16")
            return "actors/mario/mario_logo.rgba16";
    }

    if (gCurrLevelNum == LEVEL_SA && use_color_background) {
        if (texName.find("textures/skybox_tiles/") != string::npos)
            return "textures/saturn/white.rgba16";
    }

    return input;
}

struct ModelData current_model_data;

void saturn_set_model_texture(int expIndex, string path) {
    current_model_exp_tex[expIndex] = "../../" + path;
    current_model_exp_tex[expIndex] = current_model_exp_tex[expIndex].substr(0, current_model_exp_tex[expIndex].size() - 4);
    //std::cout << current_model_exp_tex[expIndex] << std::endl;
}

void saturn_load_model_expression_entry(string folder_name, string expression_name) {
    Expression ex_entry;

    // Folder path, could be either something like "eye" OR "eyes"
    string path = "";
    string pos_path1 = "dynos/packs/" + folder_name + "/expressions/" + expression_name + "/";
    string pos_path2 = "dynos/packs/" + folder_name + "/expressions/" + expression_name + "s/";

    // Prefer "eye" over "eyes"
    if (fs::is_directory(pos_path2)) { path = pos_path2; ex_entry.name = (expression_name + "s"); }
    if (fs::is_directory(pos_path1)) { path = pos_path1; ex_entry.name = (expression_name + ""); }
    // If both don't exist, cancel
    if (path == "") { return; }
    if (fs::is_empty(path)) { return; }

    ex_entry.path = path;

    // Load each .png in the path
    for (const auto & entry : fs::directory_iterator(path)) {
        if (fs::is_directory(entry.path())) {
            // Ignore, this is a folder
        } else {
            string entryName = entry.path().filename().u8string();
            if (entryName.find(".png") != string::npos) // Only allow .png files
                ex_entry.textures.push_back(entryName);
        }
    }

    if (ex_entry.textures.size() > 0)
        current_model_data.expressions.push_back(ex_entry);
}

/*
    Loads an expression with a given number - helpful for keybinds.
*/
void saturn_load_expression_number(char number) {
    // For models without expression support
    for (int n = 0; n < eye_array.size(); n++) {
        if (eye_array[n].front() == number) {
            saturn_set_eye_texture(n);
            break;
        }
    }
    
    for (int i = 0; i < current_model_data.expressions.size(); i++) {
        // For every expression
        Expression expression = current_model_data.expressions[i];
        for (int j = 0; j < expression.textures.size(); j++) {
            // For every texture in that expression
            if (expression.textures[j].front() == number) {
                // We found a matching expression
                std::cout << (expression.path + expression.textures[j]) << std::endl;
                saturn_set_model_texture(i, expression.path + expression.textures[j]);  
                current_exp_index[i] = j;
                break;
            }
        }
    }
}

string current_folder_name;

string saturn_load_search(std::string folder_name) {
    // Load the json file
    std::ifstream file("dynos/packs/" + folder_name + "/model.json");
    if (file.good()) {
        // Begin reading
        Json::Value root;
        file >> root;

        return folder_name + " " + root["name"].asString() + " " + root["author"].asString();
    }
    return folder_name;
}

void saturn_load_model_data(std::string folder_name) {
    // Reset current model data
    ModelData blank;
    current_model_data = blank;
    using_model_eyes = false;

    // Load the json file
    std::ifstream file("dynos/packs/" + folder_name + "/model.json");
    if (file.good()) {
        // Begin reading
        Json::Value root;
        file >> root;

        current_model_data.name = root["name"].asString();
        current_model_data.author = root["author"].asString();
        current_model_data.version = root["version"].asString();

        // Description (optional)
        if (root.isMember("description")) {
            current_model_data.description = root["description"].asString();
            // 500 character limit
            int characterLimit = 500;
            if (current_model_data.description.length() > characterLimit) {
                current_model_data.description = current_model_data.description.substr(0, characterLimit - 4);
                current_model_data.description += " ...";
            }
        }

        // Type (optional)
        if (root.isMember("type")) {
            current_model_data.type = root["type"].asString();
        }

        // CC support is enabled by default, SPARK is disabled
        // This is just in case it wasn't defined in the model.json
        current_model_data.cc_support = true;
        current_model_data.spark_support = false;

        if (root.isMember("cc_support")) {
            current_model_data.cc_support = root["cc_support"].asBool();
            cc_model_support = current_model_data.cc_support;
        }
        
        if (root.isMember("spark_support")) {
            current_model_data.spark_support = root["spark_support"].asBool();
            cc_spark_support = current_model_data.spark_support;

            // If SPARK is enabled, enable CC support too (it needs it to work)
            if (current_model_data.spark_support == true) {
                current_model_data.cc_support = true;
                cc_model_support = true;
            }
        }

        // CC Editor Labels (optional)
        if (root.isMember("colors")) {
            if (root["colors"].isMember("hat")) current_model_data.hat_label = root["colors"]["hat"].asString();
            if (root["colors"].isMember("overalls")) current_model_data.overalls_label = root["colors"]["overalls"].asString();
            if (root["colors"].isMember("gloves")) current_model_data.gloves_label = root["colors"]["gloves"].asString();
            if (root["colors"].isMember("shoes")) current_model_data.shoes_label = root["colors"]["shoes"].asString();
            if (root["colors"].isMember("skin")) current_model_data.skin_label = root["colors"]["skin"].asString();
            if (root["colors"].isMember("hair")) current_model_data.hair_label = root["colors"]["hair"].asString();
            if (root["colors"].isMember("shirt")) current_model_data.shirt_label = root["colors"]["shirt"].asString();
            if (root["colors"].isMember("shoulders")) current_model_data.shoulders_label = root["colors"]["shoulders"].asString();
            if (root["colors"].isMember("arms")) current_model_data.arms_label = root["colors"]["arms"].asString();
            if (root["colors"].isMember("pelvis")) current_model_data.pelvis_label = root["colors"]["pelvis"].asString();
            if (root["colors"].isMember("thighs")) current_model_data.thighs_label = root["colors"]["thighs"].asString();
            if (root["colors"].isMember("calves")) current_model_data.calves_label = root["colors"]["calves"].asString();
        }

        // Torso Rotations (optional)
        enable_torso_rotation = true;
        if (root.isMember("torso_rotations")) {
            current_model_data.torso_rotations = root["torso_rotations"].asBool();
            enable_torso_rotation = current_model_data.torso_rotations;
        }

        // Custom eyes - enabled by default, but authors can optionally disable the feature
        // If disabled, the "Custom Eyes" checkbox will be hidden from the menu
        current_model_data.eye_support = true;
        if (root.isMember("eye_support")) {
            current_model_data.eye_support = root["eye_support"].asBool();
        }
    }

    // Set the current folder name
    current_folder_name = folder_name;

    string path = "dynos/packs/" + folder_name + "/expressions/";
    if (!fs::is_directory(path)) return;

    int i = 0;
    for (const auto & entry : fs::directory_iterator(path)) {
        if (fs::is_directory(entry.path())) {
            string expression_name = entry.path().filename().u8string();
            saturn_load_model_expression_entry(folder_name, expression_name);
            
            // Choose first texture as default
            current_model_exp_tex[i] = "../../" + current_model_data.expressions[i].path + current_model_data.expressions[i].textures[0];
            current_model_exp_tex[i] = current_model_exp_tex[i].substr(0, current_model_exp_tex[i].size() - 4);

            // Toggle model eyes
            if (expression_name.find("eye") != string::npos) using_model_eyes = true;

            i++;
        } else {
            // Ignore, these are files
        }
    }
}

void saturn_copy_file(string from, string to) {
    fs::path from_path(from);
    string final = "" + fs::current_path().generic_string() + "/" + to + from_path.filename().generic_string();

    fs::path final_path(final);
    // Convert TXT files to GS
    if (final_path.extension() == ".txt") {
        final = "" + fs::current_path().generic_string() + "/" + to + from_path.stem().generic_string() + ".gs";
    }

    std::cout << from << " - " << final << std::endl;

    // Skip existing files
    if (fs::exists(final))
        return;

    fs::copy(from, final, fs::copy_options::skip_existing);
}

void saturn_delete_file(string file) {
    remove(file.c_str());
}

std::size_t number_of_files_in_directory(std::filesystem::path path) {
    return (std::size_t)std::distance(std::filesystem::directory_iterator{path}, std::filesystem::directory_iterator{});
}