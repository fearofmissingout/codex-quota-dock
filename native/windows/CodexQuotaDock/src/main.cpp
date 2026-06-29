#include "win_app.h"

#include <Windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    cqd::NativeWindowsApp app;
    return app.run(instance, showCommand);
}
