// Copyright (C) 2017 roki
#include "decode_io.hpp"
#include "jpezy_decoder.hpp"
#include <iostream>
#include <memory>
#include <srook/config/feature/decltype.hpp>
#include <srook/optional.hpp>
#include <srook/string/string_view.hpp>

SROOK_DECLTYPE(EXIT_FAILURE) disp_error() noexcept(false)
{
    std::cerr << "Usage: jpezy_decoder <input.(jpg | jpeg)> ( <output.ppm | [OPT: --gray]> | -v )" << std::endl;
    return EXIT_FAILURE;
}

enum class Mode {
    Release,
    Verbose,
    UD
};
enum class Color {
    enable,
    unable,
    UD
};

template <class CL, class T>
SROOK_DECLTYPE(EXIT_FAILURE) output(jpezy::decoder<T>& dec, const char* output)
{
    auto raw_op = dec.template decode<CL>();

    if (!raw_op) {
        std::cerr << "decode failed" << std::endl;
        return EXIT_FAILURE;
    }

    const auto raw = std::move(raw_op.value());
    const auto& [r, g, b] = raw;

    jpezy::decode_io dec_io(dec.pr.template get<jpezy::property::At::HSize>(), dec.pr.template get<jpezy::property::At::VSize>(), r, g, b);
    std::ofstream ofs(output, std::ios_base::out | std::ios_base::trunc);
    ofs << dec_io;

    std::cout << "Decoded image: "
              << "Netpbm image data, size = " << dec.pr.template get<jpezy::property::At::HSize>() << " x " << dec.pr.template get<jpezy::property::At::VSize>() << ", pixmap, ASCII text" << std::endl;
    return EXIT_SUCCESS;
}

SROOK_DECLTYPE(auto) exec(const char* input_file, const char* output_file, Mode m, Color c)
{
    jpezy::disp_logo();

    switch (m) {
    case Mode::Release: {
        jpezy::decoder<jpezy::Release> dec(input_file);
        switch (c) {
        case Color::enable: {
            return output<jpezy::COLOR_MODE>(dec, output_file);
        }
        case Color::unable: {
            return output<jpezy::GRAY_MODE>(dec, output_file);
        }
        case Color::UD:
            [[fallthrough]];
        default:
            throw std::invalid_argument(__func__);
        }
    }
    case Mode::Verbose: {
        jpezy::decoder<jpezy::Debug> dec(input_file);
        switch (c) {
        case Color::enable: {
            return output<jpezy::COLOR_MODE>(dec, output_file);
        }
        case Color::unable: {
            return output<jpezy::GRAY_MODE>(dec, output_file);
        }
        case Color::UD:
            [[fallthrough]];
        default:
            throw std::invalid_argument(__func__);
        }
    }
    case Mode::UD:
        [[fallthrough]];
    default:
        throw std::invalid_argument(__func__);
    }
}

int main(const int argc, const char* argv[])
{
    if (argc > 5 or argc < 2)
        return disp_error();

    Mode m = Mode::UD;
    Color c = Color::UD;

    const srook::string_view sv0 = argv[1], sv1 = argv[2];
    srook::optional<srook::string_view, srook::optionally::safe_optional_payload> sv2 = srook::nullopt, sv3 = srook::nullopt;
    if (argc > 2)
        sv2.emplace(argv[3]);
    if (argc > 3)
        sv3.emplace(argv[4]);

    if ((sv0.find("jpeg", sv0.find_first_of('.')) != srook::string_view::npos or sv0.find("jpg", sv0.find_first_of('.')) != srook::string_view::npos) and (sv1.find("ppm", sv1.find_first_of('.')) != srook::string_view::npos)) {

        c = Color::enable;
        m = Mode::Release;


        if (sv2) {
            if ((sv2.value().find("--gray") != srook::string_view::npos)) {
                c = Color::unable;
            } else if (sv3) {
                if (sv3.value().find("--gray") != srook::string_view::npos)
                    c = Color::unable;
            }

            if ((sv2.value().find("-v") != srook::string_view::npos) or (sv3 && sv3.value().find("-v") != srook::string_view::npos)) {
                m = Mode::Verbose;
            } else if (sv3) {
                if (sv3.value().find("-v") != srook::string_view::npos) {
                    m = Mode::Verbose;
                }
            }
        }
    } else {
        return disp_error();
    }

    switch (m) {
    case Mode::UD:
        return disp_error();
    case Mode::Release:
        [[fallthrough]];
    case Mode::Verbose: {
        switch (c) {
        case Color::enable:
            [[fallthrough]];
        case Color::unable:
            return exec(argv[1], sv1.data(), m, c);
        case Color::UD:
            [[fallthrough]];
        default:
            return disp_error();
        }
    }
    default:
        return disp_error();
    }
}
