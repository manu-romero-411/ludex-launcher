
#include "launcher.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

int launchApp(
    const std::vector<std::string>& cmd,
    const LaunchHooks& hooks
) {
    if (cmd.empty()) {
        return -1;
    }

    if (hooks.before) {
        hooks.before();
    }

    pid_t pid = fork();

    if (pid < 0) {
        std::fprintf(
            stderr,
            "[ludex-launcher] fork falló: %s\n",
            std::strerror(errno)
        );

        if (hooks.after) {
            hooks.after();
        }

        return -1;
    }

    if (pid == 0) {
        std::vector<char*> argv;

        for (const auto& arg : cmd) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }

        argv.push_back(nullptr);

        execvp(argv[0], argv.data());

        std::fprintf(
            stderr,
            "[ludex-launcher] execvp falló para %s: %s\n",
            cmd[0].c_str(),
            std::strerror(errno)
        );

        _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    if (hooks.after) {
        hooks.after();
    }

    return status;
}
