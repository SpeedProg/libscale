#include <cmath>
#include <cstdlib>
#include <iostream>

#include "libscale/calculator.hpp"
#include "libscale/types.hpp"
#include "libscale/units.hpp"

auto main() -> int {
    // 70 kg at 175 cm is BMI ~22.86.
    double const bmi =
        libscale::health_utils::calculate_bmi(libscale::units::Kg{70.0}, libscale::units::Cm{175});
    if (std::abs(bmi - 22.857) > 0.01) {
        std::cerr << "Unexpected BMI: " << bmi << '\n';
        return EXIT_FAILURE;
    }

    libscale::UserProfile user;
    user.name = "test";
    user.height = libscale::units::Cm{175};
    user.birthday = std::chrono::year(1990) / 1 / 1;

    auto const age = libscale::health_utils::calculate_age(user.birthday,
                                                           std::chrono::year(2026) / 1 / 1);
    if (!age || age->value != 36) {
        std::cerr << "Unexpected age calculation.\n";
        return EXIT_FAILURE;
    }

    std::cout << "libscale package consumed successfully (BMI " << bmi << ")\n";
    return EXIT_SUCCESS;
}
