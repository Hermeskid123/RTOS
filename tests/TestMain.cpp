#include "TestFramework.hpp"

int main(const int argc, char* argv[])
{
    if (argc == 3 && std::string_view{argv[1]} == "--case") {
        return test::runNamed(argv[2]);
    }

    if (argc != 1) {
        std::cerr << "Usage: rtos_unit_tests [--case <name>]\n";
        return 2;
    }

    return test::runAll();
}
