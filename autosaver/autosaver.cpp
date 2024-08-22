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
		s.save_path = state.unsaved_dir;
	}

	return s;
}

void log(const std::string message) {
	printf("[autosaver] %s\n", message.c_str());
}

std::wstring str_to_wstr(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

std::string wstr_to_utf8(const std::wstring& wstr) {
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
	std::string str(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], size_needed, nullptr, nullptr);
	return str;
}

std::string wstr_to_sjis(const std::wstring& wstr) {
	int size_needed = WideCharToMultiByte(932, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
	std::string str(size_needed, 0);
	WideCharToMultiByte(932, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], size_needed, nullptr, nullptr);
	return str;
}

// ファイル名内の無効文字を削除する
std::string sanitize_filename(const std::string& input) {
	std::string invalidChars = "<>:\"/\\|?*";
	std::string result;
	result.reserve(input.size());

	std::transform(input.begin(), input.end(), std::back_inserter(result),
		[&invalidChars](char ch) {
			return (invalidChars.find(ch) != std::string::npos) ? '-' : ch;
		});

	return result;
}

// 現在読み込んでるプロジェクト名を返す 新規なら"無題"
std::wstring get_project_name() {
	auto project_name = get_state().si.project_name;
	if (project_name && project_name[0] != '\0') {
		return std::filesystem::path{ project_name }.stem().wstring();
	}
	else {
		return L"無題";
	}
}

// 自動保存するディレクトリを取得 置換処理も行う
std::filesystem::path get_autosave_dir() {
	auto& setting = get_setting();
	auto& state = get_state();
	std::filesystem::path project_dir;
	std::wstring project_name = get_project_name();
	std::string path_str = setting.save_path.string();

	// `%PROJECTNAME%` を置き換える
	size_t projectNamePos = path_str.find("%PROJECTNAME%");
	if (projectNamePos != std::string::npos) {
		path_str.replace(projectNamePos, std::strlen("%PROJECTNAME%"), wstr_to_sjis(project_name));
	}

	// `%PROJECTDIR%` を置き換える 新規プロジェクトならAviUtl/autosaves直下
	if (state.si.project_name && state.si.project_name[0] != '\0') {
		project_dir = std::filesystem::path{ state.si.project_name }.parent_path();
	}
	else {
		project_dir = state.unsaved_dir;
	}
	
	std::size_t pos = path_str.find("%PROJECTDIR%");
	int projdir_len = std::strlen("%PROJECTDIR%");
	if (pos == 0) {
		path_str.replace(pos, projdir_len, project_dir.string());

		// バックアップフォルダの中を参照している場合、同フォルダにする
		if (project_dir.string().ends_with(path_str.substr(pos + projdir_len))) {
			path_str = project_dir.string();
		}
	}

	// 相対パスの場合は `state.aviutl_dir` を基準にする
	std::filesystem::path path = std::filesystem::path(path_str);
	if (!path.is_absolute()) {
		path = state.aviutl_dir / path;
	}

	// 実際に作ってみる (有効なパスでなければfilesystem_errorを投げる)
	std::filesystem::create_directories(path);

	return path;

}

std::string generate_formatted_filename(std::wstring format) {
	// %PROJECTNAME% を置換
	size_t projectNamePos = format.find(L"%PROJECTNAME%");
	if (projectNamePos != std::wstring::npos) {
		format.replace(projectNamePos, std::wcslen(L"%PROJECTNAME%"), get_project_name());
	}

	// formatが%を含んでいればフォーマットする
	std::string filename;
	auto pos = format.find(L'%');
	if (pos != std::wstring::npos) {
		std::string before_percent = wstr_to_sjis(format.substr(0, pos));
		std::string after_percent = wstr_to_sjis(format.substr(pos));
		std::string format_str = before_percent + "{:L" + after_percent + "}";

		// 現在の時刻を取得
		const auto now = std::chrono::system_clock::now();
		auto timezone = std::chrono::current_zone();
		auto timestamp = std::chrono::zoned_time{ timezone, now };

		// vformatを使用してフォーマットを適用
		filename = std::vformat(std::locale("ja_JP.utf8"), format_str, std::make_format_args(timestamp));
	}
	// formatに%が含まれていない場合、そのままfilenameに渡す
	else {
		filename = wstr_to_sjis(format);
	}

	auto autosave_dir = get_autosave_dir();
	filename = sanitize_filename(filename);

	// ファイルが既に存在する場合、末尾にナンバリングを付加
	int counter = 1;
	std::string base_filename = filename;
	while (std::filesystem::exists(autosave_dir / (filename + ".aup_backup"))) {
		filename = base_filename + "-" + std::to_string(counter++);
	}
	auto fullpath = autosave_dir / (filename + ".aup_backup");
	if (wstr_to_sjis(fullpath).size() > 260) {
		throw std::filesystem::filesystem_error("Path exceeds 260 characters.", std::error_code());
	}
	return fullpath.string();
}

void Setting::load(const std::filesystem::path& path) {
	std::ifstream ifs(path, std::ios::binary);
	if (!ifs) {
		log("設定ファイルを開けません。");
		return;
	}
	std::string json_data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	nlohmann::json j = nlohmann::json::parse(json_data);

	if (j.contains("duration")) {
		duration = std::chrono::seconds{ j["duration"].get<long long>() };
	}

	if (j.contains("savePath")) {
		std::string savePathStr = j["savePath"].get<std::string>();
		save_path = str_to_wstr(savePathStr);
	}

	if (j.contains("fileFormat")) {
		std::string fileFormatStr = j["fileFormat"].get<std::string>();
		file_format = str_to_wstr(fileFormatStr);
	}

	if (j.contains("maxAutosaves")) {
		max_autosaves = j["maxAutosaves"].get<size_t>();
	}
}

void Setting::store(const std::filesystem::path& path) const {
	nlohmann::json j;
	j["duration"] = duration.count();
	j["savePath"] = wstr_to_utf8(save_path);
	j["fileFormat"] = wstr_to_utf8(file_format);
	j["maxAutosaves"] = max_autosaves;

	std::ofstream ofs{ path };
	if (ofs) {
		ofs << j.dump(4);
	}
	else {
		log("設定ファイルを書き込めません。");
	}
}

void save_project(const std::filesystem::path& path) {
	auto& state = get_state();
	state.save_project(*state.adr_editp, path.string().c_str());
}


void delete_old_project(const std::filesystem::path& path, size_t maxAutosaves) {
	// 保存されているファイルを取得して、ファイル数を制限
	std::vector<std::filesystem::path> files;
	for (const auto& entry : std::filesystem::directory_iterator(path)) {
		if (entry.is_regular_file() && entry.path().extension() == L".aup_backup") {
			files.push_back(entry.path());
		}
	}
	// ファイルが最大数を超えた場合、古いファイルから削除
	if (files.size() > maxAutosaves) {
		std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
			return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
			});

		for (size_t i = 0; i < files.size() - maxAutosaves; ++i) {
			std::filesystem::remove(files[i]);
		}
	}
}

BOOL __cdecl func_init(AviUtl::FilterPlugin* fp) {
	auto& state = get_state();
	fp->exfunc->get_sys_info(nullptr, &state.si);
	auto& setting = get_setting();
	state.last_saved = std::chrono::system_clock::now();
	
	if (state.si.build != 11003) {
		MessageBoxW(fp->hwnd_parent, L"バージョン1.10のAviUtlが必要です。", str_to_wstr(PLUGIN_NAME).c_str(), MB_ICONINFORMATION);
		return FALSE;
	}

	// AviUtlのディレクトリの取得
	char path_str[MAX_PATH];
	::GetModuleFileNameA(NULL, path_str, MAX_PATH);
	std::filesystem::path aviutl_path{ path_str };

	state.aviutl_dir = aviutl_path.parent_path();
	auto self_dir = std::filesystem::path{ WinWrap::Module{ fp->dll_hinst }.getFileNameW() }.parent_path();
	state.setting_path = self_dir / (str_to_wstr(PLUGIN_NAME) + L".json");
	state.unsaved_dir = state.aviutl_dir / PLUGIN_NAME;

	WinWrap::Module aviutl{};
	auto aviutl_base = reinterpret_cast<uintptr_t>(aviutl.getHandle());
	state.adr_editp = reinterpret_cast<decltype(state.adr_editp)>(aviutl_base + 0x08717c);
	state.save_project = reinterpret_cast<decltype(state.save_project)>(aviutl_base + 0x024160);

	if (std::filesystem::exists(state.setting_path)) {
		setting.load(state.setting_path);
	}
	else {
		setting.store(state.setting_path);
	}

	return TRUE;
}

BOOL __cdecl func_proc(AviUtl::FilterPlugin* fp, AviUtl::FilterProcInfo* fpip) {
	TRY_START:
	auto& state = get_state();
	auto& setting = get_setting();
	fp->exfunc->get_sys_info(fpip->editp, &state.si);

	const auto now = std::chrono::system_clock::now();
	if (now - state.last_saved <= setting.duration) return TRUE;

	state.last_saved = now;
	bool retry = false;
	
	try {
		auto autosave_dir = get_autosave_dir();
		save_project(generate_formatted_filename(setting.file_format));

		if (setting.max_autosaves > 0) {
			delete_old_project(autosave_dir, setting.max_autosaves);
		}

	}
	catch (const std::format_error) {
		// 不正なdate_format
		setting.file_format = DEFAULT_DATE_FORMAT;
	}
	catch (const std::filesystem::filesystem_error) {
		setting.save_path = "";
		if (!retry) {
			retry = true;
			goto TRY_START;
		}
		else {
			throw;
		}

	}
	catch (const std::exception& e) {
		// 標準ライブラリ以外の例外
		log("エラー: " + std::string(e.what()));
	}
	catch (...) {
		log("バックアップの保存中にエラーが発生しました。");
	}

	return TRUE;
}

using Flag = AviUtl::FilterPluginDLL::Flag;
AviUtl::FilterPluginDLL filter{
	.flag = Flag::AlwaysActive | Flag::DispFilter | Flag::ExInformation | Flag::WindowSize,
	.x = 235, 
	.y = 210,
	.name = PLUGIN_NAME,
	.func_proc = func_proc,
	.func_init = func_init,
	.func_WndProc = func_WndProc,
	.information = PLUGIN_INFO,
};

auto __stdcall GetFilterTable() {
	return &filter;
}
