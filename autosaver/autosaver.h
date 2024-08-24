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
    path setting_path;                                      // autosaver.json�̃p�X
    path aviutl_dir;                                        // AviUtl�̃p�X
    path default_dir;                                       // AviUtl/autosaver �̃p�X
    chrono::system_clock::time_point last_saved;            // �Ō�ɕۑ���������
    SysInfo si;                                             // AviUtl::SysInfo
    bool last_is_saving = false;                            // ���O��func_proc�œ���is_saving�̒l
    EditHandle** adr_editp{};                               // �G�f�B�b�g�n���h��
    uintptr_t* new_project_flag{};                          // 0�Ȃ�V�K�v���W�F�N�g�Ƃ��ēǂݍ���
                                                                // �i.aup_backup�ɏ㏑���ۑ������Ȃ����߂̑Ώ��j
	BOOL(__fastcall* save_project)(EditHandle*, LPCSTR) {}; // �v���W�F�N�g�ۑ��֐��ւ̃|�C���^
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