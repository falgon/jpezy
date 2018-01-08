#ifndef INCLUDED_JPEZY_ENCODE_IO_HPP
#define INCLUDED_JPEZY_ENCODE_IO_HPP

#include <algorithm>
#include <srook/array.hpp>
#include <srook/config/feature/constexpr.hpp>
#include <srook/config/feature/decltype.hpp>
#include <srook/config/feature/deduced_typename.hpp>
#include <experimental/iterator>
#include <fstream>
#include <iostream>
#include <list>
#include <srook/optional.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/transform.hpp>

#include <srook/algorithm/for_each.hpp>

#include "../jpezy.hpp"
#include "../pnm_stream.hpp"
#include "jpezy_encoder.hpp"
#include "jpezy_writer.hpp"

namespace jpezy {

struct to_jpeg {
    explicit SROOK_CONSTEXPR to_jpeg(const char* file_)
        : file(file_)
    {
    }
    const char* file;
};

const auto gray_scale = [] {};

struct encode_io : pnm_stream {
    encode_io(const char* file_name)
        : pnm_stream(true, 0, 0, 0)
    {
        using namespace std::string_literals;

        const auto jump_comment =
            [](std::ifstream& ifs) {
                std::string str;
                while (std::getline(ifs, str) and str.find("#") != std::string::npos)
                    ;
                return str;
            };

        std::ifstream ifs(file_name);

        if (!ifs) {
            initializing_succeed = false;
        } else {
            std::string format = jump_comment(ifs);
            if (format != "P3"s) {
                initializing_succeed = false;
            } else {
                format = jump_comment(ifs);
                std::list<std::string> wh;
                boost::split(wh, format, boost::is_space());
                if (wh.size() != 2) {
                    initializing_succeed = false;
                } else {
                    width = std::stoi(*std::begin(wh));
                    height = std::stoi(*std::next(std::begin(wh), 1));

                    format = jump_comment(ifs);
                    max_color = std::stoi(format);

                    std::list<value_type> img;
                    for (std::string line = jump_comment(ifs); !ifs.eof(); line = jump_comment(ifs)) {
                        std::list<std::string> spl;
                        boost::split(spl, line, boost::is_space());
                        if (spl.back() == ""s)
                            spl.pop_back();
                        boost::transform(spl, std::back_inserter(img), [](const std::string& str) { return srook::byte(std::stoi(str)); });
                    }

                    rgb_img.resize(img.size() / 3);
                    SROOK_DEDUCED_TYPENAME SROOK_DECLTYPE(rgb_img)::value_type::value_type r, g, b;
                    for (auto& v : rgb_img) {
                        for (SROOK_DEDUCED_TYPENAME SROOK_DECLTYPE(rgb_img)::value_type::value_type& cl : { std::ref(r), std::ref(g), std::ref(b) }) {
                            cl = img.front();
                            img.pop_front();
                        }
                        v = srook::make_array(r, g, b);
                    }
                }
                std::cout << "width: " << width << " height: " << height << std::endl;
            }
        }
    }

private:
    template <class T, std::enable_if_t<std::is_same_v<T, std::ostream> or std::is_same_v<T, std::ofstream>, std::nullptr_t> = nullptr>
    friend T& operator<<(T& os, const encode_io& pnm) noexcept(false)
    {
        using p3_ascii_type = std::size_t;

        pnm.report_error(__func__);
        os << "P3\n"
           << static_cast<p3_ascii_type>(pnm.width) << " " << static_cast<p3_ascii_type>(pnm.height) << "\n";
        os << static_cast<p3_ascii_type>(pnm.max_color) << "\n";

        for (const auto& rgb : pnm.rgb_img) {
            boost::transform(rgb, std::experimental::make_ostream_joiner(os, " "), [](const auto& v) { return static_cast<p3_ascii_type>(v); });
            os << '\n';
        }
        return os;
    }

    std::tuple<std::vector<rgb_type>, std::vector<rgb_type>, std::vector<rgb_type>>
    split_rgb() const noexcept
    {
        std::vector<rgb_type> r(rgb_img.size()), g(rgb_img.size()), b(rgb_img.size());

        srook::for_each(
            srook::make_counter({ std::ref(r), std::ref(g), std::ref(b) }),
            [this](std::vector<rgb_type>& element, std::size_t i) {
                boost::transform(rgb_img, std::begin(element), [&i](const srook::array<rgb_type, 3>& ar) { return srook::byte(ar[i]); });
            });

        return std::make_tuple(r, g, b);
    }

    friend std::ofstream&
    operator<<(std::ofstream& ofs, const std::pair<const to_jpeg, const encode_io&>& pnm) noexcept(false)
    {
        ofs.close();
        pnm.second.report_error(__func__);

        using namespace jpezy::label;
        using namespace srook::literals::byte_literals;

        const property pr{
            make_property((
                _width = pnm.second.width,
                _height = pnm.second.height,
                _dimension = 3,
                _sample_precision = 8,
                _comment = "Encoded by jpezy",
                _format = property::Format::JFIF,
                _major_rev = 1_byte,
                _minor_rev = 2_byte,
                _units = property::Units::dots_inch,
                _width_density = 96,
                _height_density = 96,
                _width_thumbnail = 0,
                _height_thumbnail = 0,
                _extension_code = property::ExtensionCodes::undefined,
                _decodable = property::AnalyzedResult::Yet))
        };

        auto && [ r, g, b ] = pnm.second.split_rgb();
        encoder enc(std::move(pr), std::move(r), std::move(g), std::move(b));
        const std::size_t size = enc.encode<jpezy::COLOR_MODE>(pnm.first.file);
        std::cout << "Output size: " << size << " byte" << std::endl;

        return ofs;
    }

    friend std::ofstream&
    operator<<(std::ofstream& ofs, const std::pair<SROOK_DECLTYPE(gray_scale), std::pair<const to_jpeg, const encode_io&>>& pnm)
    {
        ofs.close();
        pnm.second.second.report_error(__func__);

        property pr{
            pnm.second.second.width,
            pnm.second.second.height,
            3, 8,
            "Encoded by JPEZY",
            property::Format::JFIF,
            srook::byte(1), srook::byte(2),
            property::Units::dots_inch,
            96, 96,
            0, 0,
            property::ExtensionCodes::undefined
        };

        auto && [ r, g, b ] = pnm.second.second.split_rgb();
        encoder enc(std::move(pr), std::move(r), std::move(g), std::move(b));
        const std::size_t size = enc.encode<jpezy::GRAY_MODE>(pnm.second.first.file);
        std::cout << "Output size: " << size << " srook::byte" << std::endl;

        return ofs;
    }

    friend std::pair<const SROOK_DECLTYPE(gray_scale), std::pair<const to_jpeg, const encode_io&>>
    operator|(const std::pair<const to_jpeg, const encode_io&>& pnm, const SROOK_DECLTYPE(gray_scale) & gr)
    {
        return std::make_pair(gr, pnm);
    }

    friend std::pair<const to_jpeg, const encode_io&>
    operator|(const encode_io& pnm, const to_jpeg& jpeg_tag) noexcept
    {
        return std::make_pair(jpeg_tag, std::cref(pnm));
    }
};

} // namespace jpezy

#endif
