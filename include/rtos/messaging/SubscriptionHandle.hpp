/**
 * @file
 * @brief Declares the public SubscriptionHandle framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
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

/** @brief Shared callback record with cross-thread deactivation tracking. */
struct SubscriptionSlot {
    /** @brief Creates an active callback slot with a registry-unique ID. */
    SubscriptionSlot(std::size_t id, std::function<void(const void*)> callback)
        : id{id}, callback{std::move(callback)}
    {
    }

    /** @brief Invokes the callback if the slot is still active. */
    [[nodiscard]] bool invoke(const void* payload);
    /** @brief Prevents new invocation and waits for foreign callbacks to finish. */
    void deactivateAndWait() noexcept;

    std::size_t id{};
    std::atomic<bool> active{true};
    std::function<void(const void*)> callback;
    std::mutex executionMutex;
    std::condition_variable executionComplete;
    std::size_t callbacksInFlight{};
};

/** @brief Shared registry state that may outlive its owning registry. */
struct SubscriptionState {
    /** @brief Removes and deactivates one exact subscription. */
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

/**
 * @brief Move-only RAII token controlling one callback registration.
 * @details Destruction or reset unregisters the callback. A weak reference makes
 * handle destruction safe after the owning DispatchPort has been destroyed.
 */
class SubscriptionHandle final {
public:
    /** @brief Creates an inactive handle. */
    SubscriptionHandle() = default;
    SubscriptionHandle(const SubscriptionHandle&) = delete;
    SubscriptionHandle& operator=(const SubscriptionHandle&) = delete;

    /** @brief Transfers callback ownership from another handle. */
    SubscriptionHandle(SubscriptionHandle&& other) noexcept;
    /** @brief Resets this handle and transfers ownership from another handle. */
    SubscriptionHandle& operator=(SubscriptionHandle&& other) noexcept;
    /** @brief Unregisters the owned callback if still active. */
    ~SubscriptionHandle();

    /** @brief Unregisters the callback and makes this handle inactive. */
    void reset() noexcept;
    /** @brief Reports whether this handle still identifies a live subscription. */
    [[nodiscard]] bool active() const noexcept;
    /** @brief Equivalent to active(), enabling boolean tests. */
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
