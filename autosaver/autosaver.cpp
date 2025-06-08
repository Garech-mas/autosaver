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
	wstring path_str = setting.save_path;
	wstring project_name = get_project_name();
	wstring project_dir;

	if (state.si.project_name && state.si.project_name[0] != L'\0') {
		wstring project_path = str_to_wstr(state.si.project_name);
		size_t last_slash = project_path.find_last_of(L"\\/");
		project_dir = (last_slash != wstring::npos) ? project_path.substr(0, last_slash) : L"";
	}
	else {
		project_dir = state.default_dir;
	}

	size_t pos;

	// %PROJECTNAME%
	pos = path_str.find(L"%PROJECTNAME%");
	if (pos != wstring::npos) {
		path_str.replace(pos, wcslen(L"%PROJECTNAME%"), project_name);
	}

	// %PROJECTDIR%
	pos = path_str.find(L"%PROJECTDIR%");
	if (pos != wstring::npos) {
		path_str.replace(pos, wcslen(L"%PROJECTDIR%"), project_dir);
	}

	// 相対パス処理
	if (PathIsRelativeW(path_str.c_str())) {
		WCHAR buf[MAX_PATH] = {};
		wcscpy_s(buf, state.aviutl_dir.c_str());
		PathAppendW(buf, path_str.c_str());
		path_str = buf;
	}

	// ディレクトリ作成（CreateDirectoryW は親ディレクトリがなければ失敗する）
	if (GetFileAttributesW(path_str.c_str()) == INVALID_FILE_ATTRIBUTES) {
		CreateDirectoryW(path_str.c_str(), nullptr);
	}

	return path_str;


}

string generate_filepath(wstring format) {
	// %PROJECTNAME% を置換
	size_t projectNamePos = format.find(L"%PROJECTNAME%");
	if (projectNamePos != wstring::npos) {
		format.replace(projectNamePos, wcslen(L"%PROJECTNAME%"), get_project_name());
	}

	// 日時文字列を作成
	time_t t = time(nullptr);
	tm local_tm;
	localtime_s(&local_tm, &t);

	wchar_t datetime[256];
	if (format.find(L'%') != wstring::npos) {
		wcsftime(datetime, sizeof(datetime) / sizeof(wchar_t), format.c_str(), &local_tm);
	}
	else {
		wcscpy_s(datetime, format.c_str());
	}

	string filename = wstr_to_sjis(datetime);
	filename = sanitize_filename(filename);

	wstring autosave_dir = get_autosave_dir();
	string fullpath_sjis;
	int counter = 1;
	string base = filename;
	do {
		string trial = base + ((counter > 1) ? ("-" + to_string(counter)) : "") + ".aup";
		wstring full = autosave_dir + L"\\" + str_to_wstr(trial);
		fullpath_sjis = wstr_to_sjis(full);
		counter++;
	} while (GetFileAttributesA(fullpath_sjis.c_str()) != INVALID_FILE_ATTRIBUTES);

	return fullpath_sjis;

}

void Setting::load(const path& path) {
	ifstream ifs(path);
	if (!ifs) {
		log("設定ファイルを開けません。");
		return;
	}

	string line;
	while (getline(ifs, line)) {
		auto pos = line.find(':');
		if (pos == string::npos) continue;

		string key = line.substr(0, pos);
		string val = line.substr(pos + 1);

		// 前後の空白・引用符などを除去
		key.erase(remove_if(key.begin(), key.end(), ::isspace), key.end());
		val.erase(remove_if(val.begin(), val.end(), ::isspace), val.end());
		key.erase(remove(key.begin(), key.end(), '\"'), key.end());
		val.erase(remove(val.begin(), val.end(), '\"'), val.end());
		val.erase(remove(val.begin(), val.end(), ','), val.end());

		if (key == "duration") {
			duration = chrono::seconds{ stoll(val) };
		}
		else if (key == "savePath") {
			save_path = str_to_wstr(val);
		}
		else if (key == "fileFormat") {
			file_format = str_to_wstr(val);
		}
		else if (key == "maxAutosaves") {
			max_autosaves = stoull(val);
		}
	}
}

void Setting::store(const path& path) const {
	ofstream ofs(path);
	if (!ofs) {
		log("設定ファイルを書き込めません。");
		return;
	}

	ofs << "{\n";
	ofs << "  \"duration\": " << duration.count() << ",\n";
	ofs << "  \"savePath\": \"" << wstr_to_utf8(save_path) << "\",\n";
	ofs << "  \"fileFormat\": \"" << wstr_to_utf8(file_format) << "\",\n";
	ofs << "  \"maxAutosaves\": " << max_autosaves << "\n";
	ofs << "}\n";
}


void save_project(const path& path) {
	auto& state = get_state();
	state.save_project(*state.adr_editp, path.string().c_str());
}


void delete_old_project(const path& path, size_t maxAutosaves) {
	// 保存されているファイルを取得して、ファイル数を制限
	vector<::path> files;
	
	for (const auto& entry : directory_iterator(path)) {
		std::wstring ext = entry.path().extension().wstring();
		if (ext.starts_with(L".aup")) {
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
		MessageBoxW(fp->hwnd_parent, L"autosaverを動作させるためには、バージョン1.10のAviUtlが必要です。", str_to_wstr(PLUGIN_NAME).c_str(), MB_ICONINFORMATION);
		return FALSE;
	}

	// AviUtlのディレクトリの取得
	char path_str[MAX_PATH];
	GetModuleFileNameA(NULL, path_str, MAX_PATH);
	path aviutl_path{ path_str };

	state.aviutl_dir = aviutl_path.parent_path();
	wchar_t path_buf[MAX_PATH]{};
	::GetModuleFileNameW(fp->dll_hinst, path_buf, MAX_PATH);
	auto self_dir = std::filesystem::path{ path_buf }.parent_path();
	state.setting_path = self_dir / (str_to_wstr(PLUGIN_NAME) + L".json");
	state.default_dir = state.aviutl_dir / PLUGIN_NAME;
	
	// 各アドレスの取得
	uintptr_t aviutl_base = reinterpret_cast<uintptr_t>(::GetModuleHandle(nullptr));
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
			return FALSE;
		}
	}
	catch (...) {
		MessageBoxW(fp->hwnd_parent, L"バックアップの保存中にエラーが発生しました。", str_to_wstr(PLUGIN_NAME).c_str(), MB_ICONWARNING);
	}
	return TRUE;
}
BOOL __cdecl func_proc(FilterPlugin* fp, FilterProcInfo* fpip) {
	auto& state = get_state();
	fp->exfunc->get_sys_info(fpip->editp, &state.si);
	if (!fp->exfunc->is_editing(fpip->editp)) return FALSE;
	auto& setting = get_setting();
	const auto now = chrono::system_clock::now();

	if (now - state.last_saved > setting.duration) run(fp);

	return TRUE;
}

BOOL func_save_start(FilterPlugin* fp, int32_t s, int32_t e, EditHandle* editp) {
	run(fp);
	return TRUE;
}

using Flag = FilterPluginDLL::Flag;
FilterPluginDLL filter{
	.flag = Flag::AlwaysActive | Flag::DispFilter | Flag::ExInformation | Flag::WindowSize | Flag::PriorityHighest,
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
