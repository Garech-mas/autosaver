#pragma once

#include <windows.h>
#include <shlobj.h>

const int ID_OPEN_BACKUP_FILE = 1001;			// Aviutl.exeがあるフォルダを開く ボタン
const int ID_EDIT_BACKUP_INTERVAL = 1003;		// バックアップの間隔 (分) エディットボックス
const int ID_EDIT_BACKUP_LIMIT = 1005;			// バックアップの最大数 エディットボックス
const int ID_BUTTON_BACKUP_SAVE_PATH = 1007;	// バックアップ場所 参照 ボタン
const int ID_EDIT_BACKUP_SAVE_PATH = 1008;		// バックアップ場所 エディットボックス
const int ID_EDIT_FILE_NAME = 1010;				// 保存ファイル名形式 エディットボックス

BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, EditHandle* editp, FilterPlugin* fp);
