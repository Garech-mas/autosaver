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
		s.save_path = "autosaver";
	}

	return s;
}

void log(const string message) {
	printf("[" PLUGIN_NAME "] %s\n", message.c_str());
}

string wstr_to_sjis(const wstring& wstr) {
	int size_needed = WideCharToMultiByte(932, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
	string str(size_needed, 0);
	WideCharToMultiByte(932, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], size_needed, nullptr, nullptr);
	return str;
}

wstring sjis_to_wstr(const string& str) {
	int size_needed = MultiByteToWideChar(932, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
	wstring wstr(size_needed, 0);
	MultiByteToWideChar(932, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], size_needed);
	return wstr;
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

wstring sanitize_filename(const wstring& input) {
	const wstring invalidChars = L"<>:\"/\\|?*";
	wstring result;
	result.reserve(input.size());

	transform(input.begin(), input.end(), back_inserter(result),
		[&invalidChars](wchar_t ch) {
			return (invalidChars.find(ch) != wstring::npos) ? L'-' : ch;
		});

	return result;
}

// 現在読み込んでるプロジェクト名を返す 新規なら"無題"
string get_project_name() {
	auto project_name = get_state().si.project_name;
	if (project_name && project_name[0] != '\0') {
		string filename = project_name;
		const auto last_slash = filename.find_last_of("\\/");
		if (last_slash != string::npos) {
			filename = filename.substr(last_slash + 1);
		}

		const auto last_dot = filename.find_last_of('.');
		if (last_dot != string::npos) {
			filename = filename.substr(0, last_dot);
		}

		return filename;
	}
	else {
		return "無題";
	}
}

// 自動保存するディレクトリを取得 置換処理も行う
path get_autosave_dir(bool IsCheck) {
	auto& setting = get_setting();
	auto& state = get_state();
	string path_str = setting.save_path.string();
	string project_name = get_project_name();
	string project_dir;

	if (state.si.project_name && state.si.project_name[0] != '\0') {
		string project_path = state.si.project_name;
		size_t last_slash = project_path.find_last_of("\\/");
		project_dir = (last_slash != string::npos) ? project_path.substr(0, last_slash) : "";
	}
	else {
		project_dir = state.default_dir.string();
	}

	size_t pos;

	// %PROJECTNAME%
	pos = path_str.find("%PROJECTNAME%");
	if (IsCheck && pos != string::npos) {
		path_str.erase(pos);
	} else if (pos != string::npos) {
		path_str.replace(pos, strlen("%PROJECTNAME%"), project_name);
	}

	// %PROJECTDIR%
	pos = path_str.find("%PROJECTDIR%");
	if (pos != string::npos) {
		path_str.replace(pos, strlen("%PROJECTDIR%"), project_dir);
	}

	// 相対パス処理
	if (PathIsRelative(path_str.c_str())) {
		CHAR buf[MAX_PATH] = {};
		strcpy_s(buf, state.aviutl_dir.string().c_str());
		PathAppend(buf, path_str.c_str());
		path_str = buf;
	}

	// ディレクトリ作成（CreateDirectoryは親ディレクトリがなければ失敗する）
	if (GetFileAttributes(path_str.c_str()) == INVALID_FILE_ATTRIBUTES) {
		CreateDirectory(path_str.c_str(), nullptr);
	}

	return path_str;


}

string generate_filepath(string format) {
	wstring format_w = sjis_to_wstr(format);
	const wstring project_name_w = sjis_to_wstr(get_project_name());

	// %PROJECTNAME% を置換
	size_t projectNamePos = format_w.find(L"%PROJECTNAME%");
	if (projectNamePos != wstring::npos) {
		format_w.replace(projectNamePos, wcslen(L"%PROJECTNAME%"), project_name_w);
	}

	// 日時文字列を作成
	time_t t = time(nullptr);
	tm local_tm;
	localtime_s(&local_tm, &t);

	wchar_t datetime[256];
	if (format_w.find(L'%') != wstring::npos) {
		wcsftime(datetime, _countof(datetime), format_w.c_str(), &local_tm);
	}
	else {
		wcscpy_s(datetime, format_w.c_str());
	}

	wstring filename = datetime;
	filename = sanitize_filename(filename);

	const wstring autosave_dir_w = sjis_to_wstr(get_autosave_dir().string());
	string fullpath_sjis;
	int counter = 1;
	const wstring base = filename;
	do {
		wstring trial = base + ((counter > 1) ? (L"-" + to_wstring(counter)) : L"") + L".aup";
		wstring full = autosave_dir_w + L"\\" + trial;
		fullpath_sjis = wstr_to_sjis(full);
		counter++;
	} while (GetFileAttributesA(fullpath_sjis.c_str()) != INVALID_FILE_ATTRIBUTES);

	return fullpath_sjis;

}

bool is_valid_save_filepath(const string& path) {
	return path.size() < MAX_PATH;
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
		key.erase(remove(key.begin(), key.end(), '"'), key.end());
		val.erase(remove(val.begin(), val.end(), '"'), val.end());
		val.erase(remove(val.begin(), val.end(), ','), val.end());

		if (key == "duration") {
			duration = chrono::seconds{ stoll(val) };
		}
		else if (key == "savePath") {
			save_path = val;
		}
		else if (key == "fileFormat") {
			file_format = val;
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
	ofs << "  \"savePath\": \"" << save_path.string() << "\",\n";
	ofs << "  \"fileFormat\": \"" << file_format << "\",\n";
	ofs << "  \"maxAutosaves\": " << max_autosaves << "\n";
	ofs << "}\n";
}


void save_project(const string& path) {
	auto& state = get_state();
	state.save_project(*state.adr_editp, path.c_str());
}


void delete_old_project(const path& path, size_t maxAutosaves) {
	// 保存されているファイルを取得して、ファイル数を制限
	vector<::path> files;
	
	for (const auto& entry : directory_iterator(path)) {
		std::string ext = entry.path().extension().string();
		if (ext.starts_with(".aup")) {
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
		MessageBox(fp->hwnd_parent, "autosaverを動作させるためには、バージョン1.10のAviUtlが必要です。", PLUGIN_NAME, MB_ICONINFORMATION);
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
	const path filename = string(PLUGIN_NAME) + ".json";
	state.setting_path = self_dir / filename;
	state.default_dir = state.aviutl_dir / PLUGIN_NAME;
	
	// 各アドレスの取得
	uintptr_t aviutl_base = reinterpret_cast<uintptr_t>(::GetModuleHandle(nullptr));
	state.adr_editp = reinterpret_cast<decltype(state.adr_editp)>(aviutl_base + 0x08717c);
	state.save_project = reinterpret_cast<decltype(state.save_project)>(aviutl_base + 0x024160);
	uintptr_t new_project_flag_adr = reinterpret_cast<uintptr_t>(*state.adr_editp) + 0x20c;
	state.new_project_flag = reinterpret_cast<uintptr_t*>(new_project_flag_adr);

	try {
		if (exists(state.setting_path)) {
			setting.load(state.setting_path);
		}
		else {
			setting.store(state.setting_path);
		}
	}
	catch (const std::system_error& e) {
		MessageBox(fp->hwnd_parent, "設定ファイル（autosaver.json）を読み込めませんでした。\n初期値にリセットされるため、必要な場合は再度設定をやり直してください。", PLUGIN_NAME, MB_ICONWARNING);
		std::filesystem::remove(state.setting_path);
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
		auto save_path = generate_filepath(setting.file_format);
		if (!is_valid_save_filepath(save_path)) {
			MessageBoxA(fp->hwnd_parent, "ファイルパスが長すぎるため保存できませんでした。\n保存場所またはプロジェクト名を見直してください。", PLUGIN_NAME, MB_ICONWARNING | MB_TOPMOST);
			return FALSE;
		}
		save_project(save_path);

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
		MessageBox(fp->hwnd_parent, "バックアップの保存中にエラーが発生しました。", PLUGIN_NAME, MB_ICONWARNING);
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
