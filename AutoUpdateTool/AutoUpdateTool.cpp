#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <thread>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <mutex>

namespace fs = std::filesystem;

struct Config {
    std::wstring watchExe;
    std::wstring updateExe;
    int intervalSeconds = 5;
    int targetHour = 3;
    int targetMinute = 0;
};

std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

Config LoadConfig(const std::wstring& filename) {
    Config cfg;
    std::ifstream file(filename);
    if (!file) {
        MessageBoxW(NULL, L"StartUpTool.ini が見つかりません。", L"StartUpTool", MB_ICONERROR);
        exit(1);
    }

    std::string line;
    while (std::getline(file, line)) {
        std::wstring wline = utf8_to_wstring(line);
        if (wline.rfind(L"watch=", 0) == 0)
            cfg.watchExe = wline.substr(6);
        else if (wline.rfind(L"update=", 0) == 0)
            cfg.updateExe = wline.substr(7);
        else if (wline.rfind(L"interval=", 0) == 0)
            cfg.intervalSeconds = std::stoi(wline.substr(9));
        else if (wline.rfind(L"time=", 0) == 0) {
            int h = 0, m = 0;
            swscanf_s(wline.substr(5).c_str(), L"%d:%d", &h, &m);
            cfg.targetHour = h;
            cfg.targetMinute = m;
        }
    }
    return cfg;
}

bool IsProcessRunning(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe{ sizeof(pe) };
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

void KillProcessByName(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe{ sizeof(pe) };
    if (Process32First(snapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0) {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProc) {
                    TerminateProcess(hProc, 0);
                    CloseHandle(hProc);
                }
            }
        } while (Process32Next(snapshot, &pe));
    }
    CloseHandle(snapshot);
}

struct SharedState {
    HWND hwnd;
    Config cfg;
    bool todayExecuted = false;
    std::mutex mtx;
};

void LaunchProcess(const std::wstring& exePath) {
    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(exePath.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

void WatchThread(SharedState* state) {
    auto lastDay = -1;

    while (true) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        bool watchRunning = IsProcessRunning(state->cfg.watchExe);

        // GUI更新
        std::wstring status = L"監視対象: " + state->cfg.watchExe +
            L"\n現在時刻: " + std::to_wstring(st.wHour) + L":" +
            (st.wMinute < 10 ? L"0" : L"") + std::to_wstring(st.wMinute) +
            L"\n状態: " + (watchRunning ? L"起動中" : L"停止中");
        SetWindowTextW(state->hwnd, status.c_str());

        // 日付が変わったらフラグリセット
        if (st.wDay != lastDay) {
            state->todayExecuted = false;
            lastDay = st.wDay;
        }

        // 指定時刻に実行
        if (!state->todayExecuted &&
            st.wHour == state->cfg.targetHour &&
            st.wMinute == state->cfg.targetMinute) {

            state->todayExecuted = true; // 実行フラグ

            // Step 1: 対象アプリを終了
            if (watchRunning) {
                KillProcessByName(state->cfg.watchExe);
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }

            // Step 2: update.exe を起動（待機付き）
            STARTUPINFOW si{ sizeof(si) };
            PROCESS_INFORMATION pi{};
            if (CreateProcessW(state->cfg.updateExe.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, INFINITE); // update.exeが終了するまで待機
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }

            // Step 3: 再起動
            std::this_thread::sleep_for(std::chrono::seconds(2));
            LaunchProcess(state->cfg.watchExe);
        }

        std::this_thread::sleep_for(std::chrono::seconds(state->cfg.intervalSeconds));
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    fs::path baseDir = fs::path(exePath).parent_path();
    fs::path iniPath = baseDir / L"StartUpTool.ini";
    Config cfg = LoadConfig(iniPath.wstring());

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"StartUpToolClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"StartUpToolClass", L"StartUpTool",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 340, 160,
        NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    SharedState state{ hwnd, cfg };
    std::thread watcher(WatchThread, &state);
    watcher.detach();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
