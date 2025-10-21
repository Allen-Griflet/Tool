#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

bool IsProcessRunning(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

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

int wmain() {
    std::wstring surveillanceExe = L"aces.exe"; // 監視対象
    std::wstring relativeLaunchPath = L"WTRTI\\WTRTI.exe"; // ← 相対パス

    // 実行ファイルのあるフォルダを取得
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    fs::path baseDir = fs::path(exePath).parent_path();

    // 相対パスを結合
    fs::path fullLaunchPath = baseDir / relativeLaunchPath;

    std::wcout << L"[" << surveillanceExe << L"] の起動を監視中...\n";

    bool launched = false;

    while (true) {
        if (IsProcessRunning(surveillanceExe)) {
            if (!launched) {
                std::wcout << surveillanceExe << L" が起動しました！ → " 
                           << fullLaunchPath << L" を起動します。\n";

                STARTUPINFO si = { sizeof(si) };
                PROCESS_INFORMATION pi;
                std::wstring cmd = fullLaunchPath.wstring(); // 書き換え可能バッファにする

                if (CreateProcessW(
                    NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
                {
                    std::wcout << L"WTRTI.exe を起動しました。\n";
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                } else {
                    std::wcerr << L"WTRTI.exe の起動に失敗しました。(エラー: "
                               << GetLastError() << L")\n";
                }

                launched = true;
            }
        } else {
            launched = false;
        }

        Sleep(1000);
    }

    return 0;
}
