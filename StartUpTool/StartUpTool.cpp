#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

bool IsProcessRunning(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe;
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

struct Config {
    std::wstring watchExe;
    std::wstring launchExe;
    int interval = 1000;
};

Config LoadConfig(const std::wstring& path) {
    Config cfg;
    std::wifstream file(path);
    file.imbue(std::locale(file.getloc(), new std::codecvt_utf8_utf16<wchar_t>));

    if (!file.is_open()) {
        std::wcerr << L"設定ファイルが開けません: " << path << L"\n";
        return cfg;
    }

    std::wstring line;
    while (std::getline(file, line)) {
        // コメントや空行はスキップ
        if (line.empty() || line[0] == L';' || line[0] == L'#') continue;

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

int wmain() {
    Config cfg = LoadConfig(L"config.ini");

    if (cfg.watchExe.empty() || cfg.launchExe.empty()) {
        std::wcerr << L"watch または launch が設定されていません。\n";
        return 1;
    }

    std::wcout << L"[" << cfg.watchExe << L"] の監視を開始します...\n";

    bool launched = false;

    while (true) {
        bool isRunning = IsProcessRunning(cfg.watchExe);

        if (isRunning && !launched) {
            std::wcout << cfg.watchExe << L" が起動しました → " << cfg.launchExe << L" を起動します。\n";

            STARTUPINFOW si;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi;
            ZeroMemory(&pi, sizeof(pi));

            std::wstring cmd = cfg.launchExe;
            if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                std::wcout << cfg.launchExe << L" を起動しました。\n";
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            } else {
                std::wcerr << cfg.launchExe << L" の起動に失敗しました。(エラー: " << GetLastError() << L")\n";
            }

            launched = true;
        }
        else if (!isRunning && launched) {
            std::wcout << cfg.watchExe << L" が終了しました → " << cfg.launchExe << L" を終了します。\n";
            std::wstring cmd = L"taskkill /IM \"" + cfg.launchExe + L"\" /F >nul 2>&1";
            _wsystem(cmd.c_str());
            launched = false;
        }

        Sleep(cfg.interval);
    }

    return 0;
}
