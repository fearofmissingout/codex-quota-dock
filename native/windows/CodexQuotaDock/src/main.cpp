#include "win_app.h"

#include <Windows.h>
#include <Shellapi.h>

#include <string>
#include <vector>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1 && std::wstring(argv[1]) == L"--apply-update") {
        std::vector<std::wstring> args;
        args.reserve(static_cast<size_t>(argc));
        for (int i = 0; i < argc; ++i) args.emplace_back(argv[i]);
        LocalFree(argv);
        return cqd::runWindowsUpdateInstaller(args);
    }
    if (argv) LocalFree(argv);

    cqd::NativeWindowsApp app;
    return app.run(instance, showCommand);
}
