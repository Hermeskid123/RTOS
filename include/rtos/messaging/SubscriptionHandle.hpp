/**
 * @file
 * @brief Declares the public SubscriptionHandle framework API.
 */

#pragma once

#include <cstddef>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rtos::messaging {

namespace detail {

struct SubscriptionSlot {
    SubscriptionSlot(std::size_t id, std::function<void(const void*)> callback)
        : id{id}, callback{std::move(callback)}
    {
    }

    [[nodiscard]] bool invoke(const void* payload);
    void deactivateAndWait() noexcept;

    std::size_t id{};
    std::atomic<bool> active{true};
    std::function<void(const void*)> callback;
    std::mutex executionMutex;
    std::condition_variable executionComplete;
    std::size_t callbacksInFlight{};
};

struct SubscriptionState {
    void remove(std::type_index type, std::size_t id) noexcept;

    std::unordered_map<
        std::type_index,
        std::vector<std::shared_ptr<SubscriptionSlot>>
    > subscribers;
    std::size_t nextId{1};
    mutable std::mutex mutex;
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
