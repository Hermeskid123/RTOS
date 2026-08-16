#pragma once

#include "rtos/model/ControlStatus.hpp"

namespace rtos::model {

class BaseModel {
public:
    BaseModel() = default;
    BaseModel(const BaseModel&) = delete;
    BaseModel& operator=(const BaseModel&) = delete;
    BaseModel(BaseModel&&) = delete;
    BaseModel& operator=(BaseModel&&) = delete;
    virtual ~BaseModel() = default;

    virtual ControlStatus initialize() = 0;
    virtual ControlStatus begin() = 0;
    virtual ControlStatus freeze() = 0;
    virtual ControlStatus operate() = 0;
    virtual ControlStatus terminate() = 0;
    [[nodiscard]] virtual ControlStatus status() const noexcept = 0;
};

}  // namespace rtos::model
