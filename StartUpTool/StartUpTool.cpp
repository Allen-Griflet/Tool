#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <string>
#include <fstream>
#include <locale>
#include <codecvt>
#include <iostream>

// ---------------------- 設定ファイル読み込み ----------------------
struct Config {
    std::wstring watchExe;
    std::wstring launchExe;
    int interval = 1000;
};

Config LoadConfig(const std::wstring& path) {
    Config cfg;
    std::wifstream file(path);
    file.imbue(std::locale(file.getloc(), new std::codecvt_utf8_utf16<wchar_t>));

    if (!file.is_open()) return cfg;

    std::wstring line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == L'#' || line[0] == L';') continue;
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring key = line.substr(0, eq);
        std::wstring value = line.substr(eq + 1);
        if (key == L"watch") cfg.watchExe = value;
        else if (key == L"launch") cfg.launchExe = value;
        else if (key == L"interval") cfg.interval = std::stoi(value);
    }
    return cfg;
}

// ---------------------- プロセス監視 ----------------------
bool IsProcessRunning(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    bool found = false;

    if (Process32First(snapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0) {
                found = true;
                break;
            }
        } while (Process32Next(snapshot, &pe));
    }
    CloseHandle(snapshot);
    return found;
}

// ---------------------- タスクトレイ通知 ----------------------
void ShowNotification(const std::wstring& title, const std::wstring& msg) {
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = NULL; // コンソールなしでも動作
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    wcscpy_s(nid.szInfo, msg.c_str());
    wcscpy_s(nid.szInfoTitle, title.c_str());
    nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_ADD, &nid);
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

// ---------------------- メイン ----------------------
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {

    // 設定ファイル読み込み
    Config cfg = LoadConfig(L"config.ini");
    if (cfg.watchExe.empty() || cfg.launchExe.empty()) {
        MessageBoxW(NULL, L"config.ini が見つからないか、watch または launch が未設定です。", L"StartUpTool", MB_ICONERROR);
        return 1;
    }

    bool launched = false;

    while (true) {
        bool isRunning = IsProcessRunning(cfg.watchExe);

        if (isRunning && !launched) {
            // 起動
            STARTUPINFOW si;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi;
            ZeroMemory(&pi, sizeof(pi));

            std::wstring cmd = cfg.launchExe;
            if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                ShowNotification(L"StartUpTool", L"WTRTI.exe を起動しました");
            }
            else {
                ShowNotification(L"StartUpTool", L"WTRTI.exe の起動に失敗しました");
            }

            launched = true;
        }
        else if (!isRunning && launched) {
            // WTRTI.exe をファイル名だけ取り出す
            std::wstring targetExe = cfg.launchExe;
            size_t pos = targetExe.find_last_of(L"\\/");
            if (pos != std::wstring::npos) targetExe = targetExe.substr(pos + 1);

            std::wstring cmd = L"taskkill /IM \"" + targetExe + L"\" /F >nul 2>&1";
            _wsystem(cmd.c_str());
            ShowNotification(L"StartUpTool", L"WTRTI.exe を終了しました");
            launched = false;
        }

        Sleep(cfg.interval);
    }

    return 0;
}
