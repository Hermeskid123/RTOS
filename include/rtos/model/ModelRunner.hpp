/**
 * @file
 * @brief Declares the public ModelRunner framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include "rtos/model/BaseModel.hpp"

#include <string>
#include <vector>

namespace rtos::model {

/** @brief Named lifecycle result returned by ModelRunner operations. */
struct ModelStatusReport {
    /** Registered model name. */
    std::string name;
    /** State returned by the lifecycle operation. */
    ControlStatus status;
};

/** @brief Executes lifecycle operations over models in registration order. */
class ModelRunner {
public:
    /** @brief Registers a non-owning model reference under a diagnostic name. */
    void add(std::string name, BaseModel& model);

    /** @brief Initializes every registered model. */
    [[nodiscard]] std::vector<ModelStatusReport> initialize();
    /** @brief Begins every registered model. */
    [[nodiscard]] std::vector<ModelStatusReport> begin();
    /** @brief Freezes every registered model. */
    [[nodiscard]] std::vector<ModelStatusReport> freeze();
    /** @brief Operates every registered model once. */
    [[nodiscard]] std::vector<ModelStatusReport> operate();
    /** @brief Terminates every registered model. */
    [[nodiscard]] std::vector<ModelStatusReport> terminate();
    /** @brief Returns current states without invoking lifecycle operations. */
    [[nodiscard]] std::vector<ModelStatusReport> statuses() const;
    /** @brief Returns the number of registered models. */
    [[nodiscard]] std::size_t size() const noexcept;

private:
    struct Entry {
        std::string name;
        BaseModel* model;
    };

    std::vector<Entry> models_;
};

}  // namespace rtos::model
