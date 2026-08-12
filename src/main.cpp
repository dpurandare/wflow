#include "app/App.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    wflow::App app(instance);
    return app.Run();
}
