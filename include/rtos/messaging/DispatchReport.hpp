#pragma once

#include <cstddef>

namespace rtos::messaging {

struct DispatchReport {
    std::size_t messagesDispatched{};
    std::size_t callbacksInvoked{};
    std::size_t messagesWithoutSubscribers{};

    [[nodiscard]] std::size_t messagesProcessed() const noexcept
    {
        return messagesDispatched + messagesWithoutSubscribers;
    }
};

}  // namespace rtos::messaging
