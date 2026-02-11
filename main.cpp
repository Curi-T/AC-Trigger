#include <Windows.h>
#include <PowrProf.h>
#include <shellapi.h>
#include <cwchar>

#pragma comment(lib, "PowrProf.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "User32.lib")

// 定义托盘常量
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_TOGGLE 1002
#define ID_TRAY_AUTOSTART 1003
#define ID_TRAY_MODE_HIBERNATE 1004
#define ID_TRAY_MODE_SHUTDOWN  1005
#define ID_TRAY_MODE_FORCE_SHUTDOWN 1006

// 指令执行延迟时间常量 ID
#define ID_TRAY_DELAY_0S   1011
#define ID_TRAY_DELAY_5S   1012
#define ID_TRAY_DELAY_10S  1013
#define ID_TRAY_DELAY_30S  1014

enum class ShutdownMode {
    Hibernate,
    Shutdown,
    ForceShutdown
};

// 注册表路径
const char *REG_PATH = "Software\\AutoShutDownPC";

// 全局变量
NOTIFYICONDATAW nid = {0};
// 默认为休眠
ShutdownMode g_currentMode = ShutdownMode::Hibernate;
// 控制是否执行自动休眠
bool g_isActive = true;

DWORD g_delayTime = 0; // 延迟执行时间（秒），默认 0
ULONGLONG g_powerOffTime = 0; // 电源断开时间
ULONGLONG g_exeTime = 0; // 预计执行时间
bool g_isCountingDown = false; // 是否正在倒计时

UINT g_uMsgTaskbarCreated = 0;  // 任务栏重建消息ID


bool IsAutoStartEnabled();

void SetAutoStart(bool enable);

void SaveConfig(const char *name, DWORD value);

DWORD LoadConfig(const char *name, DWORD defaultValue);


// 窗口过程：处理托盘交互
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    // 检查是否是任务栏重建消息
    if (message == g_uMsgTaskbarCreated) {
        // 重新添加图标（注意：这里用 NIM_ADD 而不是 NIM_MODIFY）
        Shell_NotifyIconW(NIM_ADD, &nid);
        return 0;
    }

    if (message == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP) {
            // 右键点击弹出菜单
            POINT lpClickPoint;
            GetCursorPos(&lpClickPoint);
            HMENU hPopMenu = CreatePopupMenu();

            // 添加菜单项
            wchar_t delayMenuText[64];
            if (g_delayTime == 0) {
                swprintf_s(delayMenuText, L"延迟执行 (立即)");
            } else {
                swprintf_s(delayMenuText, L"延迟执行 (%d秒)", g_delayTime);
            }
            bool isAuto = IsAutoStartEnabled();
            AppendMenuW(hPopMenu, MF_STRING | (isAuto ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_AUTOSTART, L"开机自启动");

            AppendMenuW(hPopMenu, MF_STRING | (g_currentMode == ShutdownMode::Hibernate ? MF_CHECKED : MF_UNCHECKED),
                        ID_TRAY_MODE_HIBERNATE, L"触发时：休眠（推荐）");
            AppendMenuW(hPopMenu, MF_STRING | (g_currentMode == ShutdownMode::Shutdown ? MF_CHECKED : MF_UNCHECKED),
                        ID_TRAY_MODE_SHUTDOWN, L"触发时：普通关机");
            AppendMenuW(
                hPopMenu, MF_STRING | (g_currentMode == ShutdownMode::ForceShutdown ? MF_CHECKED : MF_UNCHECKED),
                ID_TRAY_MODE_FORCE_SHUTDOWN, L"触发时：强制关机（慎用）");

            // “延迟执行”子菜单
            HMENU hDelayMenu = CreatePopupMenu();
            AppendMenuW(hDelayMenu, MF_STRING | (g_delayTime == 0 ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_DELAY_0S,
                        L"立即执行");
            AppendMenuW(hDelayMenu, MF_STRING | (g_delayTime == 5 ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_DELAY_5S,
                        L"5 秒");
            AppendMenuW(hDelayMenu, MF_STRING | (g_delayTime == 10 ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_DELAY_10S,
                        L"10 秒");
            AppendMenuW(hDelayMenu, MF_STRING | (g_delayTime == 30 ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_DELAY_30S,
                        L"30 秒");

            // 挂载到主菜单
            AppendMenuW(hPopMenu, MF_POPUP, (UINT_PTR) hDelayMenu, delayMenuText);

            AppendMenuW(hPopMenu, MF_STRING | (g_isActive ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_TOGGLE, L"断电休眠模式");
            AppendMenuW(hPopMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hPopMenu, MF_STRING, ID_TRAY_EXIT, L"退出程序");

            SetForegroundWindow(hWnd); // 确保菜单在失去焦点时消失
            TrackPopupMenu(hPopMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN, lpClickPoint.x, lpClickPoint.y,
                           0, hWnd, NULL);
            DestroyMenu(hPopMenu);
        }
    } else if (message == WM_COMMAND) {
        int wmId = LOWORD(wParam);
        switch (wmId) {
            case ID_TRAY_AUTOSTART: {
                // 是否开机自启
                bool current = IsAutoStartEnabled();
                SetAutoStart(!current); // 切换状态
                SaveConfig("SelfStarting", current ? 1 : 0);
                break;
            }
            case ID_TRAY_MODE_HIBERNATE: {
                // 选择休眠模式
                g_currentMode = ShutdownMode::Hibernate;
                SaveConfig("ShutdownMode", 0);
                break;
            }
            case ID_TRAY_MODE_SHUTDOWN: {
                // 选择关机模式
                g_currentMode = ShutdownMode::Shutdown;
                SaveConfig("ShutdownMode", 1);
                break;
            }
            case ID_TRAY_MODE_FORCE_SHUTDOWN: {
                // 选择强制关机模式
                g_currentMode = ShutdownMode::ForceShutdown;
                SaveConfig("ShutdownMode", 2);
                break;
            }
            case ID_TRAY_DELAY_0S:
                g_delayTime = 0;
                SaveConfig("Delay", g_delayTime);
                break;
            case ID_TRAY_DELAY_5S:
                g_delayTime = 5;
                SaveConfig("Delay", g_delayTime);
                break;
            case ID_TRAY_DELAY_10S:
                g_delayTime = 10;
                SaveConfig("Delay", g_delayTime);
                break;
            case ID_TRAY_DELAY_30S:
                g_delayTime = 30;
                SaveConfig("Delay", g_delayTime);
                break;
            case ID_TRAY_TOGGLE: {
                // 是否启用监控程序
                g_isActive = !g_isActive;
                if (!g_isActive && g_isCountingDown) {
                    g_isCountingDown = false;
                }
                lstrcpyW(nid.szTip, g_isActive ? L"自动休眠监控中" : L"暂停自动休眠监控");
                Shell_NotifyIconW(NIM_MODIFY, &nid);
                SaveConfig("IsActive", g_isActive ? 1 : 0);
                break;
            }
            case ID_TRAY_EXIT: {
                // 退出程序
                Shell_NotifyIconW(NIM_DELETE, &nid);
                PostQuitMessage(0);
                break;
            }
        }
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// 检查并设置开机自启的函数
void SetAutoStart(bool enable) {
    HKEY hKey;
    const char *runKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const char *appName = "AutoShutDownPC";

    // 获取当前程序的完整路径
    char szPath[MAX_PATH];
    GetModuleFileNameA(NULL, szPath, MAX_PATH);

    if (RegOpenKeyExA(HKEY_CURRENT_USER, runKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            // 写入注册表
            RegSetValueExA(hKey, appName, 0, REG_SZ, (const BYTE *) szPath, (DWORD) strlen(szPath) + 1);
        } else {
            // 从注册表中删除
            RegDeleteValueA(hKey, appName);
        }
        RegCloseKey(hKey);
    }
}

// 检查当前是否已经设置了自启
bool IsAutoStartEnabled() {
    HKEY hKey;
    const char *runKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const char *appName = "AutoShutDownPC";
    bool enabled = false;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, runKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, appName, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            enabled = true;
        }
        RegCloseKey(hKey);
    }
    return enabled;
}

// 写入配置
void SaveConfig(const char *name, DWORD value) {
    HKEY hKey;
    // 创建或打开属于本程序的注册表项
    if (RegCreateKeyExA(HKEY_CURRENT_USER, REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) ==
        ERROR_SUCCESS) {
        RegSetValueExA(hKey, name, 0, REG_DWORD, (const BYTE *) &value, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

// 读取配置
DWORD LoadConfig(const char *name, DWORD defaultValue) {
    HKEY hKey;
    DWORD value = defaultValue;
    DWORD size = sizeof(DWORD);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, name, NULL, NULL, (LPBYTE) &value, &size);
        RegCloseKey(hKey);
    }
    return value;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow) {
    HANDLE hMutex = CreateMutexA(NULL, TRUE, "AutoShutDownPC_Unique_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0; // 程序已在运行，直接退出
    }

    g_uMsgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    // 1. 注册一个隐藏窗口来接收托盘消息
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "AutoShutDownTrayClass";
    RegisterClass(&wc);
    HWND hWnd = CreateWindowEx(0, wc.lpszClassName, "AutoShutDownHiddenWin",
                               0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);

    // 2. 初始化托盘图标
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIconW(GetModuleHandle(NULL), L"IDI_ICON1");
    // 兜底逻辑
    if (!nid.hIcon) {
        nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
        DWORD error = GetLastError();
    }
    Shell_NotifyIconW(NIM_ADD, &nid);

    // 3. 混合消息循环与电源检查
    SYSTEM_POWER_STATUS powerStatus;
    MSG msg;
    // 上次记录的时间
    ULONGLONG lastCheck = 0;

    // 读取注册表
    // 0: Hibernate, 1: Shutdown, 2: ForceShutdown
    int modeInt = LoadConfig("ShutdownMode", 0);
    g_currentMode = static_cast<ShutdownMode>(modeInt);
    g_isActive = LoadConfig("IsActive", 1) == 1;
    lstrcpyW(nid.szTip, g_isActive ? L"自动休眠监控中" : L"暂停自动休眠监控");
    Shell_NotifyIconW(NIM_MODIFY, &nid);
    g_delayTime = LoadConfig("Delay", 5);

    while (true) {
        // 非阻塞式处理消息
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // 当前记录的时间
        ULONGLONG currentTime = GetTickCount64();
        if (currentTime - lastCheck > 1000) {
            // 更新记录时间
            lastCheck = currentTime;

            if (GetSystemPowerStatus(&powerStatus)) {
                // --- 情况 A: 发现断电且监控开启 ---
                if (g_isActive && powerStatus.ACLineStatus == AC_LINE_OFFLINE) {
                    if (!g_isCountingDown) {
                        // 初始化倒计时
                        g_isCountingDown = true;
                        g_powerOffTime = currentTime;
                        g_exeTime = currentTime + g_delayTime * 1000;
                    }

                    if (currentTime < g_exeTime) {
                        // 倒计时中：更新托盘文字
                        wchar_t buf[64];
                        wsprintfW(buf, L"电池供电！将在 %d 秒后执行操作", (g_exeTime - currentTime) / 1000);
                        lstrcpyW(nid.szTip, buf);
                        Shell_NotifyIconW(NIM_MODIFY, &nid);
                    } else {
                        // 倒计时结束：触发执行
                        g_isCountingDown = false;
                        g_isActive = false; // 临时关闭监控，防止重复执行指令
                        lstrcpyW(nid.szTip, L"临时关闭自动休眠监控");
                        Shell_NotifyIconW(NIM_MODIFY, &nid);

                        switch (g_currentMode) {
                            case ShutdownMode::Hibernate:
                                SetSuspendState(TRUE, FALSE, FALSE);
                                break;
                            case ShutdownMode::Shutdown:
                                ShellExecuteW(NULL, L"open", L"shutdown.exe", L"-s -t 0", NULL, SW_HIDE);
                                break;
                            case ShutdownMode::ForceShutdown:
                                ShellExecuteW(NULL, L"open", L"shutdown.exe", L"-s -f -t 0", NULL, SW_HIDE);
                                break;
                        }
                    }
                }
                // --- 情况 B: 电源正常 (插着电) ---
                else if (powerStatus.ACLineStatus == AC_LINE_ONLINE) {
                    bool stateChanged = false;

                    // 如果之前在倒计时，现在插回去了，立即恢复
                    if (g_isCountingDown) {
                        g_isCountingDown = false;
                        stateChanged = true;
                    }

                    // 如果之前因为执行了指令导致监控临时关闭，现在插电后自动恢复
                    if (!g_isActive && LoadConfig("IsActive", 1) == 1) {
                        g_isActive = true;
                        stateChanged = true;
                    }

                    // 只有状态确实发生变化了，才去刷新托盘，减少 API 调用
                    if (stateChanged) {
                        lstrcpyW(nid.szTip, g_isActive ? L"自动休眠监控中" : L"暂停自动休眠监控");
                        Shell_NotifyIconW(NIM_MODIFY, &nid);
                    }
                }
            }
        }
        Sleep(10); // 避免 CPU 占用过高
    }
}
