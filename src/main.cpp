#include "application.h"
#include <clocale>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    std::setlocale(LC_ALL, "");

    Application app;
    if (!app.init()) {
        return 1;
    }
    
    int exit_code = app.run();
    
    app.shutdown();
    return exit_code;
}