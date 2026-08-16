#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace rtos::messaging {

namespace detail {

struct SubscriptionSlot {
    std::size_t id{};
    bool active{true};
    std::function<void(const void*)> callback;
};

struct SubscriptionState {
    void remove(std::type_index type, std::size_t id) noexcept;

    std::unordered_map<
        std::type_index,
        std::vector<std::shared_ptr<SubscriptionSlot>>
    > subscribers;
    std::size_t nextId{1};
};

}  // namespace detail

class SubscriptionRegistry;

class SubscriptionHandle final {
public:
    SubscriptionHandle() = default;
    SubscriptionHandle(const SubscriptionHandle&) = delete;
    SubscriptionHandle& operator=(const SubscriptionHandle&) = delete;

    SubscriptionHandle(SubscriptionHandle&& other) noexcept;
    SubscriptionHandle& operator=(SubscriptionHandle&& other) noexcept;
    ~SubscriptionHandle();

    void reset() noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

private:
    friend class SubscriptionRegistry;

    SubscriptionHandle(
        std::weak_ptr<detail::SubscriptionState> state,
        std::type_index type,
        std::size_t id
    ) noexcept;

    std::weak_ptr<detail::SubscriptionState> state_;
    std::type_index type_{typeid(void)};
    std::size_t id_{};
};

}  // namespace rtos::messaging
