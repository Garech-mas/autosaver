// autosaver.h
#pragma once

#include <fstream>
#include <chrono>
#include <filesystem>
#include <string>
#include <codecvt>

#include <nlohmann/json.hpp>
#include <aviutl/filter.hpp>
#include <winwrap.hpp>

#define PLUGIN_NAME "autosaver"
#define PLUGIN_VERSION " r3_gr_1"
#define PLUGIN_INFO PLUGIN_NAME PLUGIN_VERSION " by Garech (original by ePi)"

using namespace std;

const std::wstring DEFAULT_DATE_FORMAT = L"%F-%X";


struct Setting {
    std::chrono::seconds duration{ 300 };
    std::filesystem::path save_path{};
    std::wstring file_format{ DEFAULT_DATE_FORMAT };
    size_t max_autosaves{ 50 };

    void load(const std::filesystem::path& path);
    void store(const std::filesystem::path& path) const;
};
Setting& get_setting();

struct State {
	std::filesystem::path setting_path;
	std::filesystem::path aviutl_dir;
    std::filesystem::path unsaved_dir;
	std::chrono::system_clock::time_point last_saved;
    AviUtl::SysInfo si;
	AviUtl::EditHandle** adr_editp{};
	BOOL(__fastcall* save_project)(AviUtl::EditHandle*, LPCSTR) {};
};
State& get_state();

void log(std::string message);
std::string generate_formatted_filename(std::wstring format);
std::wstring str_to_wstr(const std::string& str);
std::string wstr_to_utf8(const std::wstring& wstr);
std::string wstr_to_sjis(const std::wstring& wstr);
std::string sanitize_filename(const std::string& input);
std::filesystem::path get_autosave_dir();
std::wstring get_project_name();