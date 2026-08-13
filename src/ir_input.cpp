#include "ir_input.h"

#ifdef HAVE_LIRC

#include <fcntl.h>
#include <lirc/lirc_client.h>
#include <unistd.h>

#include <iostream>

class LircInput final : public IrInput {
public:
    bool init() override {
        fd_ = lirc_init("tenfoot", 0);

        if (fd_ < 0) {
            std::cerr << "[tenfoot-shell] lirc_init falló" << std::endl;
            return false;
        }

        if (lirc_readconfig(nullptr, &config_, nullptr) != 0) {
            std::cerr << "[tenfoot-shell] lirc_readconfig falló" << std::endl;
            lirc_deinit();
            fd_ = -1;
            return false;
        }

        int flags = fcntl(fd_, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        }

        std::cout << "[tenfoot-shell] LIRC inicializado" << std::endl;

        return true;
    }

    void shutdown() override {
        if (config_) {
            lirc_freeconfig(config_);
            config_ = nullptr;
        }

        if (fd_ >= 0) {
            lirc_deinit();
            fd_ = -1;
        }
    }

    std::optional<IrEvent> poll() override {
        if (fd_ < 0 || !config_) {
            return std::nullopt;
        }

        char* code = nullptr;

        while (lirc_nextcode(&code) == 0 && code != nullptr) {
            char* ir_code = nullptr;

            if (lirc_code2char(config_, code, &ir_code) == 0 &&
                ir_code != nullptr) {
                std::string value = ir_code;
                free(code);
                return IrEvent{value};
            }

            free(code);
        }

        return std::nullopt;
    }

private:
    int fd_ = -1;
    struct lirc_config* config_ = nullptr;
};

std::unique_ptr<IrInput> createDefaultIrInput() {
    return std::make_unique<LircInput>();
}

#else

std::unique_ptr<IrInput> createDefaultIrInput() {
    return std::make_unique<IrInput>();
}

#endif
