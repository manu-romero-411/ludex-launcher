#pragma once

#include <memory>
#include <optional>
#include <string>

struct IrEvent {
    std::string code;
};

class IrInput {
public:
    virtual ~IrInput() = default;

    virtual bool init() {
        return false;
    }

    virtual void shutdown() {
    }

    virtual std::optional<IrEvent> poll() {
        return std::nullopt;
    }
};

std::unique_ptr<IrInput> createDefaultIrInput();
