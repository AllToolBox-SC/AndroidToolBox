#include "launch.h"
#include <string>
#include <vector>

const std::wstring RUN_BAT_CMD = L"main.exe";

#ifdef _WIN32
BOOL IsRunAsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0,
                                 &adminGroup))
    {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin;
}
#endif

void Message()
{
    MessageBoxW(NULL, (LPCWSTR)L"请以管理员身份运行！", (LPCWSTR)L"授权失败", MB_OK | MB_ICONERROR);
}

void ElevatePrivileges()
{
    TCHAR szPath[MAX_PATH];
    if (GetModuleFileName(NULL, szPath, MAX_PATH))
    {
        SHELLEXECUTEINFO sei = { sizeof(sei) };
        sei.lpVerb = TEXT("runas");
        sei.lpFile = szPath;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;

        if (!ShellExecuteEx(&sei))
        {
            DWORD dwError = GetLastError();
            if (dwError == ERROR_CANCELLED)
            {
#if DEBUG
                std::cerr << "User declined the elevation." << std::endl;
#endif
                Message();
            }
            else
            {
#if DEBUG
                std::cerr << "ShellExecuteEx failed with error: " << dwError << std::endl;
#endif
                Message();
            }
            exit(EXIT_FAILURE);
        }
        else
        {
            exit(EXIT_SUCCESS);
        }
    }
    else
    {
        Message();
        exit(EXIT_FAILURE);
    }
}

void RunMainBat(int argc, wchar_t* argv[])
{
    printf("\033[94m[信息]\033[97m正在启动中...\033[0m\n");
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    SetCurrentDirectoryW(L".\\bin\\");
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    DWORD flags = 0;

    // Build command line: main.exe [args...]
    std::wstring cmd = RUN_BAT_CMD;
    for (int i = 1; i < argc; ++i)
    {
        cmd.push_back(L' ');
        cmd.push_back(L'"');
        cmd.append(argv[i]);
        cmd.push_back(L'"');
    }

    std::vector<wchar_t> cmdline(cmd.begin(), cmd.end());
    cmdline.push_back(L'\0');

    BOOL isOk = CreateProcessW(NULL, cmdline.data(), NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi);
    if (!isOk)
    {
        std::cerr << "CreateProcessW failed with error: " << GetLastError() << std::endl;
        exit(EXIT_FAILURE);
    }
}

int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(936);
    if (!IsRunAsAdmin())
    {
        ElevatePrivileges();
    }
#if DEBUG
    printf("[INFO] 程序提权成功");
#endif
    RunMainBat(argc, argv);

    return 0;
}