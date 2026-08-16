#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rtos::model {

struct ModelConfig {
    std::string name;
    bool enabled{true};
    bool debugEnabled{};
    bool gdbEnabled{};
};

class ModelConfiguration {
public:
    [[nodiscard]] static ModelConfiguration load(const std::filesystem::path& path);
    [[nodiscard]] static ModelConfiguration parse(std::string_view xml);

    [[nodiscard]] const ModelConfig* find(std::string_view name) const noexcept;
    [[nodiscard]] const std::vector<ModelConfig>& models() const noexcept;

private:
    explicit ModelConfiguration(std::vector<ModelConfig> models);

    std::vector<ModelConfig> models_;
};

}  // namespace rtos::model
