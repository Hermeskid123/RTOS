#include "rtos/model/ModelRunner.hpp"

#include <utility>

namespace rtos::model {

void ModelRunner::add(std::string name, BaseModel& model)
{
    models_.push_back(Entry{std::move(name), &model});
}

std::vector<ModelStatusReport> ModelRunner::initialize()
{
    std::vector<ModelStatusReport> reports;
    reports.reserve(models_.size());
    for (const auto& entry : models_) {
        reports.push_back(ModelStatusReport{entry.name, entry.model->initialize()});
    }
    return reports;
}

std::vector<ModelStatusReport> ModelRunner::begin()
{
    std::vector<ModelStatusReport> reports;
    reports.reserve(models_.size());
    for (const auto& entry : models_) {
        reports.push_back(ModelStatusReport{entry.name, entry.model->begin()});
    }
    return reports;
}

std::vector<ModelStatusReport> ModelRunner::freeze()
{
    std::vector<ModelStatusReport> reports;
    reports.reserve(models_.size());
    for (const auto& entry : models_) {
        reports.push_back(ModelStatusReport{entry.name, entry.model->freeze()});
    }
    return reports;
}

std::vector<ModelStatusReport> ModelRunner::operate()
{
    std::vector<ModelStatusReport> reports;
    reports.reserve(models_.size());
    for (const auto& entry : models_) {
        reports.push_back(ModelStatusReport{entry.name, entry.model->operate()});
    }
    return reports;
}

std::vector<ModelStatusReport> ModelRunner::terminate()
{
    std::vector<ModelStatusReport> reports;
    reports.reserve(models_.size());
    for (const auto& entry : models_) {
        reports.push_back(ModelStatusReport{entry.name, entry.model->terminate()});
    }
    return reports;
}

std::vector<ModelStatusReport> ModelRunner::statuses() const
{
    std::vector<ModelStatusReport> reports;
    reports.reserve(models_.size());
    for (const auto& entry : models_) {
        reports.push_back(ModelStatusReport{entry.name, entry.model->status()});
    }
    return reports;
}

std::size_t ModelRunner::size() const noexcept
{
    return models_.size();
}

}  // namespace rtos::model
