/**
 * @file
 * @brief Declares the public ModelRunner framework API.
 */

#pragma once

#include "rtos/model/BaseModel.hpp"

#include <string>
#include <vector>

namespace rtos::model {

struct ModelStatusReport {
    std::string name;
    ControlStatus status;
};

class ModelRunner {
public:
    void add(std::string name, BaseModel& model);

    [[nodiscard]] std::vector<ModelStatusReport> initialize();
    [[nodiscard]] std::vector<ModelStatusReport> begin();
    [[nodiscard]] std::vector<ModelStatusReport> freeze();
    [[nodiscard]] std::vector<ModelStatusReport> operate();
    [[nodiscard]] std::vector<ModelStatusReport> terminate();
    [[nodiscard]] std::vector<ModelStatusReport> statuses() const;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    struct Entry {
        std::string name;
        BaseModel* model;
    };

    std::vector<Entry> models_;
};

}  // namespace rtos::model
