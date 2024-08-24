// autosaver.h
#pragma once

#include <fstream>
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>
#include <aviutl/filter.hpp>
#include <winwrap.hpp>

#define PLUGIN_NAME "autosaver"
#define PLUGIN_VERSION " r3_gr_1"
#define PLUGIN_INFO PLUGIN_NAME PLUGIN_VERSION " by Garech (original by ePi)"

using namespace std;
using namespace filesystem;
using namespace AviUtl;
using namespace nlohmann;

const wstring DEFAULT_DATE_FORMAT = L"%PROJECTNAME%_%F_%R";


struct Setting {
    chrono::seconds duration{ 60 };
    path save_path{};
    wstring file_format{ DEFAULT_DATE_FORMAT };
    wstring backup_file_ext = { L".aup_backup" };
    size_t max_autosaves{ 100 };

    void load(const path& path);
    void store(const path& path) const;
};
Setting& get_setting();

struct State {
    path setting_path;                                      // autosaver.jsonのパス
    path aviutl_dir;                                        // AviUtlのパス
    path default_dir;                                       // AviUtl/autosaver のパス
    chrono::system_clock::time_point last_saved;            // 最後に保存した時間
    SysInfo si;                                             // AviUtl::SysInfo
    bool last_is_saving = false;                            // 直前のfunc_procで得たis_savingの値
    EditHandle** adr_editp{};                               // エディットハンドル
    uintptr_t* new_project_flag{};                          // 0なら新規プロジェクトとして読み込む
                                                                // （.aup_backupに上書き保存させないための対処）
	BOOL(__fastcall* save_project)(EditHandle*, LPCSTR) {}; // プロジェクト保存関数へのポインタ
};
State& get_state();

void log(string message);
string generate_filepath(wstring format);
wstring str_to_wstr(const string& str);
string wstr_to_utf8(const wstring& wstr);
string wstr_to_sjis(const wstring& wstr);
string sanitize_filename(const string& input);
path get_autosave_dir();
wstring get_project_name();