#include "autosaver.h"
#include "gui.h"


std::wstring show_folder_dialog(HWND hwnd)
{
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (FAILED(hr)) return L"";

    DWORD dw_options;
    pfd->GetOptions(&dw_options);
    pfd->SetOptions(dw_options | FOS_PICKFOLDERS);

    // デフォルトフォルダを設定
    PIDLIST_ABSOLUTE pidl_default = nullptr;
    hr = SHParseDisplayName(get_autosave_dir().wstring().c_str(), nullptr, &pidl_default, 0, nullptr); // 空文字を渡す
    if (SUCCEEDED(hr) && pidl_default)
    {
        IShellItem* psi_folder = nullptr;
        hr = SHCreateShellItem(nullptr, nullptr, pidl_default, &psi_folder);
        if (SUCCEEDED(hr))
        {
            pfd->SetFolder(psi_folder);
            psi_folder->Release();
        }
        CoTaskMemFree(pidl_default);
    }

    std::wstring selected_path;

    // ダイアログを表示
    hr = pfd->Show(hwnd);
    if (SUCCEEDED(hr))
    {
        IShellItem* psi_result = nullptr;
        hr = pfd->GetResult(&psi_result);
        if (SUCCEEDED(hr))
        {
            PWSTR psz_file_path = nullptr;
            hr = psi_result->GetDisplayName(SIGDN_FILESYSPATH, &psz_file_path);
            if (SUCCEEDED(hr))
            {
                selected_path = psz_file_path;
                CoTaskMemFree(psz_file_path);
            }
            psi_result->Release();
        }
    }
    pfd->Release();

    return selected_path;
}

BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp) {
    auto& setting = get_setting();
    auto& state = get_state();

    switch (message) {
    case AviUtl::detail::FilterPluginWindowMessage::Init:
    {
        // フォント指定
        HFONT font = CreateFont(12, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
            SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            PROOF_QUALITY, DEFAULT_PITCH | FF_MODERN, L"ＭＳ Ｐゴシック");

        // ボタン配置
        HWND button_open_backup_file = CreateWindowA(
            "BUTTON",
            "バックアップファイルから開く",
            WS_CHILD | WS_GROUP | WS_VISIBLE | BS_PUSHBUTTON | BS_VCENTER,
            10, 10,
            200, 30,
            hwnd,
            (HMENU)ID_OPEN_BACKUP_FILE,
            fp->dll_hinst,
            NULL
        );
        SendMessage(button_open_backup_file, WM_SETFONT, (WPARAM)font, 0);

        HWND label_backup_interval = CreateWindowA(
            "STATIC",
            "バックアップの間隔(分)",
            WS_CHILD | WS_VISIBLE | ES_LEFT,
            15, 53,
            150, 20,
            hwnd,
            NULL,
            fp->dll_hinst,
            NULL
        );
        SendMessage(label_backup_interval, WM_SETFONT, (WPARAM)font, 0);

        HWND edit_backup_interval = CreateWindowExA(
            WS_EX_CLIENTEDGE,
            "EDIT",
            std::to_string((static_cast<int>(setting.duration.count()) + 59) / 60).c_str(),
            WS_CHILD | WS_TABSTOP | WS_VISIBLE | ES_LEFT | ES_NUMBER,
            140, 50,
            70, 20,
            hwnd,
            (HMENU)ID_EDIT_BACKUP_INTERVAL,
            fp->dll_hinst,
            NULL
        );
        SendMessage(edit_backup_interval, WM_SETFONT, (WPARAM)font, 0);

        HWND label_backup_limit = CreateWindowA(
            "STATIC",
            "バックアップの最大数",
            WS_CHILD | WS_VISIBLE | ES_LEFT,
            15, 73,
            150, 20,
            hwnd,
            NULL,
            fp->dll_hinst,
            NULL
        );
        SendMessage(label_backup_limit, WM_SETFONT, (WPARAM)font, 0);

        HWND edit_backup_limit = CreateWindowExA(
            WS_EX_CLIENTEDGE,
            "EDIT",
            std::to_string(setting.max_autosaves).c_str(),
            WS_CHILD | WS_TABSTOP | WS_VISIBLE | ES_LEFT | ES_NUMBER,
            140, 70,
            70, 20,
            hwnd,
            (HMENU)ID_EDIT_BACKUP_LIMIT,
            fp->dll_hinst,
            NULL
        );
        SendMessage(edit_backup_limit, WM_SETFONT, (WPARAM)font, 0);

        HWND label_backup_save_path = CreateWindowA(
            "STATIC",
            "バックアップの保存場所",
            WS_CHILD | WS_VISIBLE | ES_LEFT,
            15, 96,
            150, 20,
            hwnd,
            NULL,
            fp->dll_hinst,
            NULL
        );
        SendMessage(label_backup_save_path, WM_SETFONT, (WPARAM)font, 0);

        HWND button_backup_save_path = CreateWindowA(
            "BUTTON",
            "参照...",
            WS_CHILD | WS_TABSTOP | WS_VISIBLE | BS_PUSHBUTTON | BS_VCENTER,
            170, 93,
            40, 17,
            hwnd,
            (HMENU)ID_BUTTON_BACKUP_SAVE_PATH,
            fp->dll_hinst,
            NULL
        );
        SendMessage(button_backup_save_path, WM_SETFONT, (WPARAM)font, 0);

        HWND edit_backup_save_path = CreateWindowExA(
            WS_EX_CLIENTEDGE,
            "EDIT",
            wstr_to_sjis(setting.save_path).c_str(),
            WS_CHILD | WS_TABSTOP | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
            15, 110,
            195, 20,
            hwnd,
            (HMENU)ID_EDIT_BACKUP_SAVE_PATH,
            fp->dll_hinst,
            NULL
        );
        SendMessage(edit_backup_save_path, WM_SETFONT, (WPARAM)font, 0);

        HWND label_file_name = CreateWindowA(
            "STATIC",
            "保存ファイル名形式",
            WS_CHILD | WS_VISIBLE | ES_LEFT,
            15, 133,
            150, 20,
            hwnd,
            NULL,
            fp->dll_hinst,
            NULL
        );
        SendMessage(label_file_name, WM_SETFONT, (WPARAM)font, 0);

        HWND edit_file_name = CreateWindowExA(
            WS_EX_CLIENTEDGE,
            "EDIT",
            wstr_to_sjis(setting.file_format).c_str(),
            WS_CHILD | WS_TABSTOP | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
            15, 145,
            195, 20,
            hwnd,
            (HMENU)ID_EDIT_FILE_NAME,
            fp->dll_hinst,
            NULL
        );
        SendMessage(edit_file_name, WM_SETFONT, (WPARAM)font, 0);

    }
    break;

    //項目操作
    case WM_COMMAND:
        switch (LOWORD(wparam)) {

        case ID_OPEN_BACKUP_FILE:
            fp->exfunc->edit_open(*state.adr_editp, const_cast<char*>((get_autosave_dir() / "*.aup_backup").string().c_str()),
                AviUtl::ExFunc::EditOpenFlag::Project | AviUtl::ExFunc::EditOpenFlag::Dialog);
            break;
        case ID_EDIT_BACKUP_INTERVAL:
            if (HIWORD(wparam) == EN_KILLFOCUS) {
                WCHAR buf[16];
                GetDlgItemText(hwnd, ID_EDIT_BACKUP_INTERVAL, buf, sizeof(buf) / sizeof(WCHAR));
                if (_wtoi(buf) < 1) {
                    MessageBeep(MB_OK);
                    setting.duration = std::chrono::minutes(1);
                    SetDlgItemText(hwnd, ID_EDIT_BACKUP_INTERVAL, L"1");
                }
                else if (_wtoi(buf) > 10000) {
                    MessageBeep(MB_OK);
                    setting.duration = std::chrono::minutes(10000);
                    SetDlgItemText(hwnd, ID_EDIT_BACKUP_INTERVAL, L"10000");
                }
                else {
                    setting.duration = std::chrono::minutes(_wtoi(buf));
                }
                setting.store(state.setting_path);
            }
            break;
        case ID_EDIT_BACKUP_LIMIT:
            if (HIWORD(wparam) == EN_KILLFOCUS) {
                WCHAR buf[16];
                const WCHAR* NO_LIMIT = L"上限なし";
                GetDlgItemText(hwnd, ID_EDIT_BACKUP_LIMIT, buf, sizeof(buf) / sizeof(WCHAR));
                if ((wcscmp(buf, NO_LIMIT) != 0 && _wtoi(buf) == 0) || _wtoi(buf) > 1000) {
                    MessageBeep(MB_OK);
                    SetDlgItemText(hwnd, ID_EDIT_BACKUP_LIMIT, NO_LIMIT);
                }
                setting.max_autosaves = _wtoi(buf);
                setting.store(state.setting_path);
            }
            break;
        case ID_BUTTON_BACKUP_SAVE_PATH:
        {
            auto before_save_path = setting.save_path;
            try {
                std::wstring new_path = show_folder_dialog(hwnd);
                if (!new_path.empty())
                {
                    setting.save_path = new_path;
                    SetDlgItemText(hwnd, ID_EDIT_BACKUP_SAVE_PATH, setting.save_path.c_str());
                    
                }
                generate_formatted_filename(setting.file_format);
            }
            catch (std::system_error) {
                MessageBeep(MB_OK);
                log("不正なファイル名形式が入力されました。");
                setting.save_path = before_save_path;
                SetDlgItemText(hwnd, ID_EDIT_BACKUP_SAVE_PATH, setting.save_path.c_str());
            }
            catch (const std::filesystem::filesystem_error) {
                MessageBeep(MB_OK);
                log("不正なファイル名形式が入力されました。");
                SetDlgItemText(hwnd, ID_EDIT_FILE_NAME, setting.file_format.c_str());
            }
            setting.store(state.setting_path);

        }
        break;

        case ID_EDIT_BACKUP_SAVE_PATH:
            if (HIWORD(wparam) == EN_KILLFOCUS) {
                WCHAR buf[MAX_PATH];
                GetDlgItemText(hwnd, ID_EDIT_BACKUP_SAVE_PATH, buf, sizeof(buf) / sizeof(WCHAR));

                auto before_save_path = setting.save_path;
                setting.save_path = buf;
                try {
                    generate_formatted_filename(setting.file_format);
                }
                catch (std::filesystem::filesystem_error) {
                    MessageBeep(MB_OK);
                    log("不正な保存場所が入力されました。");
                    setting.save_path = before_save_path;
                    SetDlgItemText(hwnd, ID_EDIT_BACKUP_SAVE_PATH, setting.save_path.c_str());
                }
                
                setting.store(state.setting_path);
            }
            break;
        case ID_EDIT_FILE_NAME:
            if (HIWORD(wparam) == EN_KILLFOCUS) {
                WCHAR buf[256];

                GetDlgItemText(hwnd, ID_EDIT_FILE_NAME, buf, sizeof(buf) / sizeof(WCHAR));

                if (*buf == L'\0') {
                    setting.file_format = DEFAULT_DATE_FORMAT;
                    SetDlgItemText(hwnd, ID_EDIT_FILE_NAME, DEFAULT_DATE_FORMAT.c_str());
                }

                try {
                    log("保存ファイル名: \"" + generate_formatted_filename(buf) + "\"");
                    setting.file_format = buf;
                }
                catch (const std::format_error) {
                    MessageBeep(MB_OK);
                    log("不正なファイル名形式が入力されました。");
                    SetDlgItemText(hwnd, ID_EDIT_FILE_NAME, setting.file_format.c_str());
                }
                catch (const std::filesystem::filesystem_error) {
                    MessageBeep(MB_OK);
                    log("不正なファイル名形式が入力されました。");
                    SetDlgItemText(hwnd, ID_EDIT_FILE_NAME, setting.file_format.c_str());
                }


                setting.store(state.setting_path);
            }

            break;

        }
    }
    return FALSE;
}
