#include "rtos/logging/Logger.hpp"

#include <iostream>
#include <string_view>

namespace {

constexpr std::string_view version{"0.1.0"};

void printHelp()
{
    std::cout << "RTOS Model Simulator\n\n"
                 "Usage: rtos_sim [options]\n\n"
                 "Options:\n"
                 "  --help       Show this help message\n"
                 "  --version    Show the simulator version\n";
}

}  // namespace

int main(const int argc, char* argv[])
{
    if (argc > 1) {
        const std::string_view option{argv[1]};
        if (option == "--help") {
            printHelp();
            return 0;
        }
        if (option == "--version") {
            std::cout << "rtos_sim " << version << '\n';
            return 0;
        }

        std::cerr << "Unknown option: " << option << "\n"
                  << "Run rtos_sim --help for usage.\n";
        return 1;
    }

    rtos::logging::Logger logger{std::cout};

    std::cout << "RTOS Model Simulator\n";
    logger.log(rtos::logging::LogLevel::info, "Host", "START", "Simulator initialized");
    logger.log(rtos::logging::LogLevel::info, "Host", "STOP", "Simulator completed");

    return 0;
}
