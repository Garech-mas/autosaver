#include "autosaver.h"
#include "gui.h"

State& get_state() {
	static State s;
	return s;
}

Setting& get_setting() {
	static Setting s;

	auto& state = get_state();
	if (s.save_path.empty()) {
		s.save_path = state.default_dir;
	}

	return s;
}

void log(const string message) {
	printf("[" PLUGIN_NAME "] %s\n", message.c_str());
}

wstring str_to_wstr(const string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

string wstr_to_utf8(const wstring& wstr) {
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
	string str(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], size_needed, nullptr, nullptr);
	return str;
}

string wstr_to_sjis(const wstring& wstr) {
	int size_needed = WideCharToMultiByte(932, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
	string str(size_needed, 0);
	WideCharToMultiByte(932, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], size_needed, nullptr, nullptr);
	return str;
}

string sanitize_filename(const string& input) {
	string invalidChars = "<>:\"/\\|?*";
	string result;
	result.reserve(input.size());

	transform(input.begin(), input.end(), back_inserter(result),
		[&invalidChars](char ch) {
			return (invalidChars.find(ch) != string::npos) ? '-' : ch;
		});

	return result;
}

// 現在読み込んでるプロジェクト名を返す 新規なら"無題"
wstring get_project_name() {
	auto project_name = get_state().si.project_name;
	if (project_name && project_name[0] != '\0') {
		return ::path{ project_name }.stem().wstring();
	}
	else {
		return L"無題";
	}
}

// 自動保存するディレクトリを取得 置換処理も行う
path get_autosave_dir() {
	auto& setting = get_setting();
	auto& state = get_state();
	path project_dir;
	wstring project_name = get_project_name();
	string path_str = setting.save_path.string();

	// `%PROJECTNAME%` を置き換える
	size_t projectNamePos = path_str.find("%PROJECTNAME%");
	if (projectNamePos != string::npos) {
		path_str.replace(projectNamePos, strlen("%PROJECTNAME%"), wstr_to_sjis(project_name));
	}

	// `%PROJECTDIR%` を置き換える 新規プロジェクトならAviUtl/autosaves直下
	if (state.si.project_name && state.si.project_name[0] != '\0') {
		project_dir = path{ state.si.project_name }.parent_path();
	}
	else {
		project_dir = state.default_dir;
	}
	
	size_t pos = path_str.find("%PROJECTDIR%");
	int projdir_len = strlen("%PROJECTDIR%");
	if (pos == 0) {
		path_str.replace(pos, projdir_len, project_dir.string());

		// バックアップフォルダの中を参照している場合、同フォルダにする
		if (project_dir.string().ends_with(path_str.substr(pos + projdir_len))) {
			path_str = project_dir.string();
		}
	}

	// 相対パスの場合は `state.aviutl_dir` を基準にする
	path path = ::path(path_str);
	if (!path.is_absolute()) {
		path = state.aviutl_dir / path;
	}

	// 実際に作ってみる (有効なパスでなければfilesystem_errorを投げる)
	create_directories(path);

	return path;

}

string generate_filepath(wstring format) {
	auto backup_file_ext = wstr_to_sjis(get_setting().backup_file_ext);
	// %PROJECTNAME% を置換
	size_t projectNamePos = format.find(L"%PROJECTNAME%");
	if (projectNamePos != wstring::npos) {
		format.replace(projectNamePos, wcslen(L"%PROJECTNAME%"), get_project_name());
	}

	// %を含んでいる時だけフォーマットする
	string filename;
	auto pos = format.find(L'%');
	if (pos != wstring::npos) {
		string before_percent = wstr_to_sjis(format.substr(0, pos));
		string after_percent = wstr_to_sjis(format.substr(pos));
		string format_str = before_percent + "{:L" + after_percent + "}";

		const auto now = chrono::system_clock::now();
		auto timezone = chrono::current_zone();
		auto timestamp = chrono::zoned_time{ timezone, now };

		filename = vformat(locale("ja_JP.utf8"), format_str, make_format_args(timestamp));
	}
	// %が含まれていない場合、そのままfilenameに渡す
	else {
		filename = wstr_to_sjis(format);
	}

	auto autosave_dir = get_autosave_dir();
	filename = sanitize_filename(filename);

	// ファイルが既に存在する場合、末尾にナンバリングを付加
	int counter = 1;
	string base_filename = filename;
	while (exists(autosave_dir / (filename + backup_file_ext))) {
		filename = base_filename + "-" + to_string(counter++);
	}
	auto fullpath = autosave_dir / (filename + backup_file_ext);
	if (wstr_to_sjis(fullpath).size() > 260) {
		throw filesystem_error("Path exceeds 260 characters.", error_code());
	}
	return fullpath.string();
}

void Setting::load(const path& path) {
	ifstream ifs(path, ios::binary);
	if (!ifs) {
		log("設定ファイルを開けません。");
		return;
	}
	string json_data((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
	json j = json::parse(json_data);

	if (j.contains("duration")) {
		duration = chrono::seconds{ j["duration"].get<long long>() };
	}

	if (j.contains("savePath")) {
		save_path = str_to_wstr(j["savePath"].get<string>());
	}

	if (j.contains("fileFormat")) {
		file_format = str_to_wstr(j["fileFormat"].get<string>());
	}

	if (j.contains("maxAutosaves")) {
		max_autosaves = j["maxAutosaves"].get<size_t>();
	}

	if (j.contains("extension")) {
		backup_file_ext = str_to_wstr(j["extension"].get<string>());
	}
}

void Setting::store(const path& path) const {
	json j;
	j["duration"] = duration.count();
	j["savePath"] = wstr_to_utf8(save_path);
	j["fileFormat"] = wstr_to_utf8(file_format);
	j["maxAutosaves"] = max_autosaves;
	j["extension"] = wstr_to_utf8(backup_file_ext);

	ofstream ofs{ path };
	if (ofs) {
		ofs << j.dump(5);
	}
	else {
		log("設定ファイルを書き込めません。");
	}
}

void save_project(const path& path) {
	auto& state = get_state();
	state.save_project(*state.adr_editp, path.string().c_str());
}


void delete_old_project(const path& path, size_t maxAutosaves) {
	// 保存されているファイルを取得して、ファイル数を制限
	vector<::path> files;
	for (const auto& entry : directory_iterator(path)) {
		if (entry.is_regular_file() && entry.path().extension() == get_setting().backup_file_ext) {
			files.push_back(entry.path());
		}
	}
	// ファイルが最大数を超えた場合、古いファイルから削除
	if (files.size() > maxAutosaves) {
		sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
			return ::last_write_time(a) < ::last_write_time(b);
			});

		for (size_t i = 0; i < files.size() - maxAutosaves; ++i) {
			remove(files[i]);
		}
	}
}

BOOL __cdecl func_init(FilterPlugin* fp) {
	auto& state = get_state();
	fp->exfunc->get_sys_info(nullptr, &state.si);
	auto& setting = get_setting();
	state.last_saved = chrono::system_clock::now();

	if (state.si.build != 11003) {
		MessageBoxW(fp->hwnd_parent, L"バージョン1.10のAviUtlが必要です。", str_to_wstr(PLUGIN_NAME).c_str(), MB_ICONINFORMATION);
		return FALSE;
	}

	// AviUtlのディレクトリの取得
	char path_str[MAX_PATH];
	GetModuleFileNameA(NULL, path_str, MAX_PATH);
	path aviutl_path{ path_str };

	state.aviutl_dir = aviutl_path.parent_path();
	auto self_dir = path{ WinWrap::Module{ fp->dll_hinst }.getFileNameW() }.parent_path();
	state.setting_path = self_dir / (str_to_wstr(PLUGIN_NAME) + L".json");
	state.default_dir = state.aviutl_dir / PLUGIN_NAME;
	
	// 各アドレスの取得
	WinWrap::Module aviutl{};
	auto aviutl_base = reinterpret_cast<uintptr_t>(aviutl.getHandle());
	state.adr_editp = reinterpret_cast<decltype(state.adr_editp)>(aviutl_base + 0x08717c);
	state.save_project = reinterpret_cast<decltype(state.save_project)>(aviutl_base + 0x024160);
	uintptr_t new_project_flag_adr = reinterpret_cast<uintptr_t>(*state.adr_editp) + 0x20c;
	state.new_project_flag = reinterpret_cast<uintptr_t*>(new_project_flag_adr);

	if (exists(state.setting_path)) {
		setting.load(state.setting_path);
	}
	else {
		setting.store(state.setting_path);
	}

	return TRUE;
}

BOOL run(FilterPlugin* fp) {
	auto& state = get_state();
	auto& setting = get_setting();

	const auto now = chrono::system_clock::now();
	state.last_saved = now;
	bool retry = false;

	TRY_START:
	try {
		log("保存します");
		auto autosave_dir = get_autosave_dir();
		save_project(generate_filepath(setting.file_format));

		if (setting.max_autosaves > 0) {
			delete_old_project(autosave_dir, setting.max_autosaves);
		}

	}
	catch (const format_error) {
		// 不正なdate_format
		setting.file_format = DEFAULT_DATE_FORMAT;
	}
	catch (const filesystem_error) {
		setting.save_path = "";
		if (!retry) {
			retry = true;
			goto TRY_START;
		}
		else {
			throw;
		}
	}
	catch (...) {
		MessageBoxW(fp->hwnd_parent, L"バックアップの保存中にエラーが発生しました。", str_to_wstr(PLUGIN_NAME).c_str(), MB_ICONWARNING);
	}
}
BOOL __cdecl func_proc(FilterPlugin* fp, FilterProcInfo* fpip) {
	auto& state = get_state();
	fp->exfunc->get_sys_info(fpip->editp, &state.si);
	auto& setting = get_setting();
	
	bool is_saving = fp->exfunc->is_saving(fpip->editp);
	const auto now = chrono::system_clock::now();

	if (!is_saving && now - state.last_saved > setting.duration) run(fp);

	return TRUE;
}

BOOL func_save_start(FilterPlugin* fp, int32_t s, int32_t e, EditHandle* editp) {
	run(fp);
	return TRUE;
}

using Flag = FilterPluginDLL::Flag;
FilterPluginDLL filter{
	.flag = Flag::AlwaysActive | Flag::DispFilter | Flag::ExInformation | Flag::WindowSize,
	.x = 235,
	.y = 210,
	.name = PLUGIN_NAME,
	.func_proc = func_proc,
	.func_init = func_init,
	.func_WndProc = func_WndProc,
	.information = PLUGIN_INFO,
	.func_save_start = func_save_start,
};

auto __stdcall GetFilterTable() {
	return &filter;
}
