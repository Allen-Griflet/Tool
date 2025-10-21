#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>

bool IsProcessRunning(const std::wstring& processName) {
    bool found = false;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(snapshot, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0) {
                    found = true;
                    break;
                }
            } while (Process32Next(snapshot, &pe));
        }
        CloseHandle(snapshot);
    }
    return found;
}

int main() {
    std::wstring targetExe = L"FirstApp.exe";   // 監視対象のEXE名（拡張子込み）
    std::wstring secondExe = L"C:\\Path\\To\\SecondApp.exe"; // 起動するEXE

    std::wcout << L"[" << targetExe << L"] の起動を監視中...\n";

    bool launched = false;

    while (true) {
        if (IsProcessRunning(targetExe)) {
            if (!launched) {
                std::wcout << targetExe << L" が起動しました！ → SecondApp.exe を起動します。\n";

                STARTUPINFO si = { sizeof(si) };
                PROCESS_INFORMATION pi;

                if (CreateProcess(
                    secondExe.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
                {
                    std::wcout << L"SecondApp.exe を起動しました。\n";
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                } else {
                    std::wcerr << L"SecondApp.exe の起動に失敗しました。(エラー: " 
                               << GetLastError() << L")\n";
                }

                launched = true;
            }
        } else {
            launched = false; // プロセスが終了したら再度監視に戻る
        }

        Sleep(1000); // 1秒ごとにチェック
    }

    return 0;
}
