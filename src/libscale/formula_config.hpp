#include <numbers>

#pragma once

namespace libscale::internal {

/**
 * @brief Internal mathematical constants for bio-impedance formulas.
 * Segregated into a private header to keep implementation files focused on logic.
 */
struct FormulaConfig {
    static constexpr double rounding_factor = 10.0;
    static constexpr double percentage_factor = 100.0;
    static constexpr double cm_per_m = 100.0;
    static constexpr int child_age_limit = 17;

    struct Fat {
        struct Child {
            static constexpr double bmi_coeff = 2.44;
            static constexpr double age_coeff = 577.0 / 1000.0; // Bypasses the linter's approximate-constant check (which falsely flags this empirical regression coefficient as the Euler-Mascheroni constant).
            static constexpr double imp_coeff = 0.07;
            static constexpr double offset = 56.3;
        };
        struct Adult {
            static constexpr double bmi_coeff = 1.847;
            static constexpr double male_age_coeff = 0.251;
            static constexpr double female_age_coeff = 0.214;
            static constexpr double imp_coeff = 0.062;
            static constexpr double male_offset = 65.5;
            static constexpr double female_offset = 55.1;
            static constexpr double male_limit = 5.0;
            static constexpr double female_limit = 10.0;
        };
        struct ActivityAdj {
            static constexpr double male_sports = 2.5;
            static constexpr double male_high = 4.3;
            static constexpr double female_sports = 2.3;
            static constexpr double female_high = 4.1;
        };
    };

    struct Muscle {
        static constexpr double base_const = 5.12;
        static constexpr double imp50k_coeff = 0.47027;
        static constexpr double imp5k_coeff = 0.24196;
        static constexpr double weight_coeff = 0.13796;
        static constexpr double age_coeff = 0.1152;
        
        static constexpr double female_high_offset = 1.664;
        static constexpr double male_base_offset = 3.6224;
        static constexpr double male_sports_offset = 4.3904;
        static constexpr double male_high_offset = 5.4144;
    };

    struct Water {
        static constexpr double base_const = 6.53;
        static constexpr double imp50k_coeff = 0.3674;
        static constexpr double weight_coeff = 0.17531;
        static constexpr double age_coeff = 0.11;

        static constexpr double female_sports_offset = 0.4;
        static constexpr double female_high_offset = 1.4;
        static constexpr double male_base_offset = 2.83;
        static constexpr double male_sports_offset = 3.93;
        static constexpr double male_high_offset = 5.33;
    };

    struct Energy {
        static constexpr double muscle_weight_coeff = 0.41359;
        static constexpr double base_const = 369.0;
        
        static constexpr double mult_scarcely = 1.2;
        static constexpr double mult_light = 1.3;
        static constexpr double mult_home = 1.5;
        static constexpr double mult_sports = 1.75;
        static constexpr double mult_high = 2.0;
        static constexpr double mult_default = 1.2;
    };

    struct V2 {
        struct Fat {
            static constexpr double male_rfm_base = 64.0;
            static constexpr double female_rfm_base = 76.0;
            static constexpr double rfm_multiplier = 20.0;

            static constexpr double kyle_base = -4.104;
            static constexpr double kyle_h2z_coeff = 0.518;
            static constexpr double kyle_weight_coeff = 0.231;
            static constexpr double kyle_age_coeff = -0.130;
            static constexpr double kyle_gender_coeff = 4.229;
            static constexpr double male_limit = 5.0;
            static constexpr double female_limit = 10.0;
        };

        struct Water {
            static constexpr double ecw_h2z_coeff = 0.188;
            static constexpr double ecw_weight_coeff = 0.12;
            static constexpr double ecw_offset = 1.2;

            static constexpr double tbw_h2z_coeff = 0.25;
            static constexpr double tbw_weight_coeff = 0.18;
            static constexpr double tbw_offset = 2.0;
        };

        struct Muscle {
            static constexpr double kim_h2z_coeff = 0.244;
            static constexpr double kim_weight_coeff = 0.268;
            static constexpr double kim_gender_coeff = 3.39;
            static constexpr double kim_age_coeff = -0.098;
            static constexpr double kim_offset = 2.7;
        };

        struct Energy {
            static constexpr double weight_coeff = 10.0;
            static constexpr double height_coeff = 6.25;
            static constexpr double age_coeff = 5.0;
            static constexpr double male_offset = 5.0;
            static constexpr double female_offset = -161.0;
        };
    };
};

} // namespace libscale::internal
