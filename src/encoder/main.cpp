#include "encode_io.hpp"
#include <srook/string/string_view.hpp>

SROOK_DECLTYPE(EXIT_FAILURE) disp_error()
{
    std::cerr << "Usage: jpezy_encoder <input.ppm> ( <ouput.(jpeg | jpg) [OPT: --gray]> | <output.ppm> | --debug )" << std::endl;
    return EXIT_FAILURE;
}

enum class Mode { JPEG,
    GRAY,
    PPM,
    DEBUG,
    UD };

template <class... Modes>
auto exec(const jpezy::encode_io& pnm, const char* ofile, Modes&&... m)
-> std::enable_if_t<std::conjunction_v<std::is_same<Mode, std::decay_t<Modes>>...> and (sizeof...(m) > 0 and sizeof...(m) < 3), void>
{
    const std::tuple<std::decay_t<Modes>...> ms{ std::forward<Modes>(m)... };

    SROOK_IF_CONSTEXPR (sizeof...(m) == 2) {
        if (std::get<0>(ms) == Mode::JPEG and std::get<1>(ms) == Mode::GRAY) {
            std::ofstream ofs(ofile, std::ios::binary);
            ofs << (pnm | jpezy::to_jpeg(ofile) | jpezy::gray_scale);
        } else {
            SROOK_THROW std::invalid_argument("2nd Mode parameter must be GRAY");
        }
    } else {
        switch (std::get<0>(ms)) {
        case Mode::JPEG: {
            std::ofstream ofs(ofile, std::ios::binary);
            ofs << (pnm | jpezy::to_jpeg(ofile));
            break;
        }
        case Mode::PPM: {
            std::ofstream ofs(ofile);
            ofs << pnm;
            break;
        }
        case Mode::DEBUG:
            std::cout << pnm << std::endl;
            break;
        case Mode::GRAY:
            [[fallthrough]];
        case Mode::UD:
            [[fallthrough]];
        default:
            SROOK_THROW std::runtime_error("Maybe broken memory");
        };
    }
}

int main(const int argc, const char* argv[])
{
    if (argc < 3) {
        return disp_error();
    }

    Mode m1 = Mode::UD, m2 = Mode::UD;

    const srook::string_view sv1 = argv[2];
    srook::optional<srook::string_view> sv2 = srook::nullopt;

    if (argc > 2) {
        sv2 = argv[3];
    }

    if (sv1.find("jpeg", sv1.find_first_of('.')) != srook::string_view::npos or sv1.find("jpg", sv1.find_first_of('.')) != srook::string_view::npos) {
        m1 = Mode::JPEG;
        if (sv2) {
            if (sv2.value().find("--gray") != srook::string_view::npos) {
                m2 = Mode::GRAY;
            }
        }
    } else if (sv1.find("ppm", sv1.find_first_of('.')) != srook::string_view::npos) {
        m1 = Mode::PPM;
    } else if (!sv1.compare("--debug")) {
        m1 = Mode::DEBUG;
    } else {
        return disp_error();
    }

    jpezy::disp_logo();

    jpezy::raii_messenger section("Reading the input file...");
    jpezy::encode_io pnm(argv[1]);
    if (!pnm) {
        std::cerr << "The file is not found or the formatting error" << std::endl;
        return disp_error();
    }

    auto t1 = section.stop();
    section.restart("Start encoding and writing ...");

    SROOK_TRY {
        if (m2 == Mode::GRAY) {
            exec(pnm, argv[2], m1, m2);
        } else {
            exec(pnm, argv[2], m1);
        }
    } SROOK_CATCH (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    auto t2 = section.stop();

    if (t1 and t2) {
        std::cout << "Total processing time: " << t1.value() + t2.value() << std::endl;
    } else {
        SROOK_THROW std::runtime_error("Timer error");
    }
}
