#include "rtos/model/ModelConfiguration.hpp"

#include <fstream>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace rtos::model {

namespace {

bool parseBoolean(const std::string_view value, const std::string_view attribute)
{
    if (value == "true" || value == "1") {
        return true;
    }
    if (value == "false" || value == "0") {
        return false;
    }
    throw std::runtime_error{
        "Invalid boolean value for model attribute '" + std::string{attribute} + "'"
    };
}

}  // namespace

ModelConfiguration::ModelConfiguration(std::vector<ModelConfig> models)
    : models_{std::move(models)}
{
}

ModelConfiguration ModelConfiguration::load(const std::filesystem::path& path)
{
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error{"Unable to open model configuration: " + path.string()};
    }

    const std::string xml{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };
    return parse(xml);
}

ModelConfiguration ModelConfiguration::parse(const std::string_view xml)
{
    const std::string document{xml};
    if (document.find("<models") == std::string::npos) {
        throw std::runtime_error{"Model configuration is missing the <models> root"};
    }

    const std::regex modelPattern{R"(<model\s+([^>]*)/?>)"};
    const std::regex attributePattern{
        R"xml(([A-Za-z][A-Za-z0-9_-]*)\s*=\s*"([^"]*)")xml"
    };
    std::vector<ModelConfig> models;
    std::unordered_set<std::string> modelNames;

    for (
        auto model = std::sregex_iterator{document.begin(), document.end(), modelPattern};
        model != std::sregex_iterator{};
        ++model
    ) {
        const std::string attributesText = (*model)[1].str();
        std::unordered_map<std::string, std::string> attributes;
        for (
            auto attribute = std::sregex_iterator{
                attributesText.begin(),
                attributesText.end(),
                attributePattern
            };
            attribute != std::sregex_iterator{};
            ++attribute
        ) {
            attributes.emplace((*attribute)[1].str(), (*attribute)[2].str());
        }

        const auto name = attributes.find("name");
        if (name == attributes.end() || name->second.empty()) {
            throw std::runtime_error{"Every <model> requires a non-empty name"};
        }
        if (!modelNames.insert(name->second).second) {
            throw std::runtime_error{"Duplicate model configuration: " + name->second};
        }

        ModelConfig config;
        config.name = name->second;
        if (const auto enabled = attributes.find("enabled"); enabled != attributes.end()) {
            config.enabled = parseBoolean(enabled->second, "enabled");
        }
        if (const auto debug = attributes.find("debug"); debug != attributes.end()) {
            config.debugEnabled = parseBoolean(debug->second, "debug");
        }
        if (const auto gdb = attributes.find("gdb"); gdb != attributes.end()) {
            config.gdbEnabled = parseBoolean(gdb->second, "gdb");
        }
        models.push_back(std::move(config));
    }

    if (models.empty()) {
        throw std::runtime_error{"Model configuration does not contain any models"};
    }
    return ModelConfiguration{std::move(models)};
}

const ModelConfig* ModelConfiguration::find(const std::string_view name) const noexcept
{
    for (const auto& model : models_) {
        if (model.name == name) {
            return &model;
        }
    }
    return nullptr;
}

const std::vector<ModelConfig>& ModelConfiguration::models() const noexcept
{
    return models_;
}

}  // namespace rtos::model
