#include <windows.h>
#include <iostream>

int main() {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    // --- 1つ目のEXEを起動 ---
    if (CreateProcess(
        L"C:\\Path\\To\\FirstApp.exe",  // 1つ目のEXEのパス
        NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        std::wcout << L"FirstApp.exe を起動しました。起動完了を待っています...\n";

        // GUIアプリがメッセージを処理可能になるまで待機（最大10秒）
        DWORD waitResult = WaitForInputIdle(pi.hProcess, 10000);

        if (waitResult == 0) {
            std::wcout << L"FirstApp.exe の起動を確認しました。\n";

            // --- 2つ目のEXEを起動 ---
            STARTUPINFO si2 = { sizeof(si2) };
            PROCESS_INFORMATION pi2;

            if (CreateProcess(
                L"C:\\Path\\To\\SecondApp.exe",  // 2つ目のEXEのパス
                NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si2, &pi2))
            {
                std::wcout << L"SecondApp.exe を起動しました。\n";
                CloseHandle(pi2.hProcess);
                CloseHandle(pi2.hThread);
            }
            else {
                std::wcerr << L"SecondApp.exe の起動に失敗しました。(エラーコード: " 
                           << GetLastError() << L")\n";
            }
        }
        else if (waitResult == WAIT_TIMEOUT) {
            std::wcerr << L"FirstApp.exe が10秒以内に起動しませんでした。\n";
        }
        else {
            std::wcerr << L"FirstApp.exe の起動確認中にエラーが発生しました。(コード: " 
                       << GetLastError() << L")\n";
        }

        // ハンドルを閉じる
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        std::wcerr << L"FirstApp.exe の起動に失敗しました。(エラーコード: " 
                   << GetLastError() << L")\n";
    }

    return 0;
}
