/**
 * @file
 * @brief Declares the public ModelConfiguration framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rtos::model {

/** @brief Configuration flags for one named application model. */
struct ModelConfig {
    /** Model identifier used by the simulator and configuration lookup. */
    std::string name;
    /** Whether the model worker is constructed and scheduled. */
    bool enabled{true};
    /** Whether component-level debug logging is allowed. */
    bool debugEnabled{};
    /** Whether the host should attach an interactive GDB session. */
    bool gdbEnabled{};
};

/** @brief Validated collection of model settings loaded from XML. */
class ModelConfiguration {
public:
    /**
     * @brief Loads and parses a model configuration file.
     * @param path XML file to read.
     * @return Validated model configuration.
     * @throws std::runtime_error If the file cannot be read or is invalid.
     */
    [[nodiscard]] static ModelConfiguration load(const std::filesystem::path& path);
    /**
     * @brief Parses model configuration XML from memory.
     * @param xml Complete XML document.
     * @return Validated model configuration.
     * @throws std::runtime_error If required elements or attributes are invalid.
     */
    [[nodiscard]] static ModelConfiguration parse(std::string_view xml);

    /**
     * @brief Finds a model by its exact configured name.
     * @param name Model name to locate.
     * @return Pointer into this configuration, or `nullptr` when absent.
     */
    [[nodiscard]] const ModelConfig* find(std::string_view name) const noexcept;
    /** @brief Returns all models in source-document order. */
    [[nodiscard]] const std::vector<ModelConfig>& models() const noexcept;

private:
    explicit ModelConfiguration(std::vector<ModelConfig> models);

    std::vector<ModelConfig> models_;
};

}  // namespace rtos::model
