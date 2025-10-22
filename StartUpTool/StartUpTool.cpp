#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;

// UTF-8 → UTF-16
std::wstring utf8_to_wstring(const std::string& str)
{
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

// UTF-16 → UTF-8
std::string wstring_to_utf8(const std::wstring& wstr)
{
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

// 設定ファイル
struct Config {
    std::wstring watchExe;
    std::wstring launchExe;
};

// 設定読み込み（UTF-8 対応）
Config LoadConfig(const std::wstring& filename) {
    Config cfg;
    std::ifstream file(filename);
    if (!file) {
        MessageBoxW(NULL, L"設定ファイル StartUpTool.ini が見つかりません。", L"StartUpTool", MB_ICONERROR);
        exit(1);
    }

    std::string line;
    while (std::getline(file, line)) {
        std::wstring wline = utf8_to_wstring(line);
        if (wline.rfind(L"watch=", 0) == 0)
            cfg.watchExe = wline.substr(6);
        else if (wline.rfind(L"launch=", 0) == 0)
            cfg.launchExe = wline.substr(7);
    }
    return cfg;
}

// 通知バルーン
void ShowNotification(const std::wstring& title, const std::wstring& message) {
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = NULL;
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, message.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &nid);
    Shell_NotifyIconW(NIM_SETVERSION, &nid);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

// プロセス実行中チェック
bool IsProcessRunning(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32 pe = {};
    pe.dwSize = sizeof(PROCESSENTRY32);

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

// 特定プロセス終了
void KillProcessByName(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return;

    PROCESSENTRY32 pe = {};
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                }
            }
        } while (Process32Next(snapshot, &pe));
    }

    CloseHandle(snapshot);
}

// エントリポイント
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    fs::path baseDir = fs::path(exePath).parent_path();
    fs::path iniPath = baseDir / L"StartUpTool.ini";

    Config cfg = LoadConfig(iniPath.wstring());
    std::wstring launchName = fs::path(cfg.launchExe).filename().wstring();

    ShowNotification(L"StartUpTool", L"監視を開始しました。");

    bool launched = false;

    while (true) {
        bool watchRunning = IsProcessRunning(cfg.watchExe);

        if (watchRunning && !launched) {
            ShowNotification(L"StartUpTool", L"監視対象が起動しました。WTRTIを起動します。");

            STARTUPINFOW si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            if (CreateProcessW(cfg.launchExe.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, baseDir.c_str(), &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                launched = true;
            }
            else {
                DWORD err = GetLastError();
                std::wstring msg = L"WTRTI の起動に失敗しました。エラーコード: " + std::to_wstring(err);
                MessageBoxW(NULL, msg.c_str(), L"StartUpTool", MB_ICONERROR);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
