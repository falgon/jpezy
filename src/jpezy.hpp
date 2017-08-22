#ifndef INCLUDED_JPEZY_JPEG_HPP
#define INCLUDED_JPEZY_JPEG_HPP
#include <array>
#include <chrono>
#include <iostream>
#include <string>
#include <tuple>

#include <boost/parameter/name.hpp>

#include <srook/array/make_array.hpp>
#include <srook/cstddef/byte.hpp>
#include <srook/mpl/variadic_player.hpp>
#include <srook/mpl/constant_sequence/math/make_costable.hpp>
#include <srook/optional.hpp>

namespace jpezy {

namespace detail{

template<class T>
struct GetCosTable{
	static constexpr decltype(auto) value() noexcept
	{
		return value(srook::constant_sequence::math::detail::make_costable_x<T::block_size,T::block_size>());
	}
private:
	template<std::int64_t... v>
	static constexpr decltype(auto) value(std::integer_sequence<std::int64_t, v...>) noexcept
	{
		return srook::make_array(srook::constant_sequence::math::detail::Realvalue()(v)...);
	}
};

} // namespace detail

void disp_logo()
{
    std::cout << "   _" << std::endl;
    std::cout << "  (_)_ __   ___ _____   _" << std::endl;
    std::cout << "  | | '_ \\ / _ \\_  / | | | " << std::endl;
    std::cout << "  | | |_) |  __// /| |_| |" << std::endl;
    std::cout << " _/ | .__/ \\___/___|\\__, |" << std::endl;
    std::cout << "|__/|_|             |___/\tby roki\n"
              << std::endl;
}

struct Release;
struct Debug;
struct COLOR_MODE;
struct GRAY_MODE;

const std::array<int, 64> ZZ{
    { 0, 1, 8, 16, 9, 2, 3, 10,
		17, 24, 32, 25, 18, 11, 4, 5,
        12, 19, 26, 33, 40, 48, 41, 34,
        27, 20, 13, 6, 7, 14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36,
        29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46,
        53, 60, 61, 54, 47, 55, 62, 63 }
};

enum class MARKER {
    SOF0 = 0xc0,
    SOF1 = 0xc1,
    SOF2 = 0xc2,
    SOF3 = 0xc3,
    SOF5 = 0xc5,
    SOF6 = 0xc6,
    SOF7 = 0xc7,

    JPG = 0xc8,

    SOF9 = 0xc9,
    SOF10 = 0xca,
    SOF11 = 0xcb,
    SOF13 = 0xcd,
    SOF14 = 0xce,
    SOF15 = 0xcf,

    DHT = 0xc4,

    DAC = 0xcc,

    RST0 = 0xd0,
    RST1 = 0xd1,
    RST2 = 0xd2,
    RST3 = 0xd3,
    RST4 = 0xd4,
    RST5 = 0xd5,
    RST6 = 0xd6,
    RST7 = 0xd7,

    SOI = 0xd8,
    EOI = 0xd9,
    SOS = 0xda,
    DQT = 0xdb,
    DNL = 0xdc,
    DRI = 0xdd,
    DHP = 0xde,
    EXP = 0xdf,
    COM = 0xfe,

    APP0 = 0xe0,
    APP1 = 0xe1,
    APP2 = 0xe2,
    APP3 = 0xe3,
    APP4 = 0xe4,
    APP5 = 0xe5,
    APP6 = 0xe6,
    APP7 = 0xe7,
    APP8 = 0xe8,
    APP9 = 0xe9,
    APP10 = 0xea,
    APP11 = 0xeb,
    APP12 = 0xec,
    APP13 = 0xed,
    APP14 = 0xee,
    APP15 = 0xef,

    JPG0 = 0xf0,
    JPG1 = 0xf1,
    JPG2 = 0xf2,
    JPG3 = 0xf3,
    JPG4 = 0xf4,
    JPG5 = 0xf5,
    JPG6 = 0xf6,
    JPG7 = 0xf7,
    JPG8 = 0xf8,
    JPG9 = 0xf9,
    JPG10 = 0xfa,
    JPG11 = 0xfb,
    JPG12 = 0xfc,
    JPG13 = 0xfd,

    TEM = 0x01,
    RESst = 0x02,
    RESnd = 0xbf,

    Error = 0xff,
    Marker = 0xff
};

// ISO/IEC 10918-1:1993(E)
// Table K.1 - Luminance quantization table
static const std::array<int, 64> YQuantumTb{
    { 16, 11, 10, 16, 24, 40, 51, 61,
        12, 12, 14, 19, 26, 58, 60, 55,
        14, 13, 16, 24, 40, 57, 69, 56,
        14, 17, 22, 29, 51, 87, 80, 62,
        18, 22, 37, 56, 68, 109, 103, 77,
        24, 35, 55, 64, 81, 104, 113, 92,
        49, 64, 78, 87, 103, 121, 120, 101,
        72, 92, 95, 98, 112, 100, 103, 99 }
};

// Table K.2 - Chrominance quantization table
static const std::array<int, 64> CQuantumTb{
    { 17, 18, 24, 47, 99, 99, 99, 99,
        18, 21, 26, 66, 99, 99, 99, 99,
        24, 26, 56, 99, 99, 99, 99, 99,
        47, 66, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99 }
};

struct property {
    enum class Format {
        undefined,
        JFIF,
        JFXX
    };
    enum class Units {
        undefined,
        dots_inch,
        dots_cm
    };
    enum class ExtensionCodes {
        undefined = 0,
        JPEG = 0x10,
        oneByte_pixel = 0x11,
        threeByte_pixel = 0x13
    };
    enum AnalyzedResult {
        Yet = 0,
        is_htable = 0x01,
        is_qtable = 0x02,
        is_jfif = 0x04,
        is_comment = 0x08,
        is_start_data = 0x10
    };
    enum class At {
        HSize,
        VSize,
        Dimension,
        SamplePrecision,
        Comment,
        Format,
        MajorRevisions,
        MinorRevisions,
        Units,
        HDensity,
        VDensity,
        HThumbnail,
        VThumbnail,
        ExtensionCode,
        Decodable,
        ELEMENT_SIZE
    };

    std::size_t width, height;
    int dimension, sample_precision;
    std::string comment;
    Format format;
    srook::byte major_rev, minor_rev;
    Units uni;
    int width_density, height_density, width_thumbnail, height_thumbnail;
    ExtensionCodes ext;
    int decodable;

    using property_member = srook::pack<std::size_t, std::size_t, int, int, std::string, Format, srook::byte, srook::byte, Units, int, int, int, int, ExtensionCodes, int>;

    explicit property(std::size_t w, std::size_t h, int dim, int sample_pre, std::string com, Format form, srook::byte marev, srook::byte mirev, Units u, int wd, int hd, int wt, int ht, ExtensionCodes e, int decflag = AnalyzedResult::Yet)
        : width(std::move(w))
        , height(std::move(h))
        , dimension(std::move(dim))
        , sample_precision(std::move(sample_pre))
        , comment(std::move(com))
        , format(std::move(form))
        , major_rev(std::move(marev))
        , minor_rev(std::move(mirev))
        , uni(std::move(u))
        , width_density(std::move(wd))
        , height_density(std::move(hd))
        , width_thumbnail(std::move(wt))
        , height_thumbnail(std::move(ht))
        , ext(std::move(e))
        , decodable(decflag)
    {
    }
    explicit property() = default;
    property(const property&) = default;

    template <At at>
    const auto& get() const noexcept
    {
        if constexpr (at == At::HSize) {
            return width;
        } else if constexpr (at == At::VSize) {
            return height;
        } else if constexpr (at == At::Dimension) {
            return dimension;
        } else if constexpr (at == At::SamplePrecision) {
            return sample_precision;
        } else if constexpr (at == At::Comment) {
            return comment;
        } else if constexpr (at == At::Format) {
            return format;
        } else if constexpr (at == At::MajorRevisions) {
            return major_rev;
        } else if constexpr (at == At::MinorRevisions) {
            return minor_rev;
        } else if constexpr (at == At::Units) {
            return uni;
        } else if constexpr (at == At::HDensity) {
            return width_density;
        } else if constexpr (at == At::VDensity) {
            return height_density;
        } else if constexpr (at == At::HThumbnail) {
            return width_thumbnail;
        } else if constexpr (at == At::VThumbnail) {
            return height_thumbnail;
        } else if constexpr (at == At::ExtensionCode) {
            return ext;
        } else if constexpr (at == At::Decodable) {
            return decodable;
        }
    }

    template <std::size_t at>
    const auto& get() const noexcept
    {
        if constexpr (at == 0) {
            return width;
        } else if constexpr (at == 1) {
            return height;
        } else if constexpr (at == 2) {
            return dimension;
        } else if constexpr (at == 3) {
            return sample_precision;
        } else if constexpr (at == 4) {
            return comment;
        } else if constexpr (at == 5) {
            return format;
        } else if constexpr (at == 6) {
            return major_rev;
        } else if constexpr (at == 7) {
            return minor_rev;
        } else if constexpr (at == 8) {
            return uni;
        } else if constexpr (at == 9) {
            return width_density;
        } else if constexpr (at == 10) {
            return height_density;
        } else if constexpr (at == 11) {
            return width_thumbnail;
        } else if constexpr (at == 12) {
            return height_thumbnail;
        } else if constexpr (at == 13) {
            return ext;
        } else if constexpr (at == 14) {
            return decodable;
        }
    }

    template <At at>
    auto& get() noexcept
    {
        if constexpr (at == At::HSize) {
            return width;
        } else if constexpr (at == At::VSize) {
            return height;
        } else if constexpr (at == At::Dimension) {
            return dimension;
        } else if constexpr (at == At::SamplePrecision) {
            return sample_precision;
        } else if constexpr (at == At::Comment) {
            return comment;
        } else if constexpr (at == At::Format) {
            return format;
        } else if constexpr (at == At::MajorRevisions) {
            return major_rev;
        } else if constexpr (at == At::MinorRevisions) {
            return minor_rev;
        } else if constexpr (at == At::Units) {
            return uni;
        } else if constexpr (at == At::HDensity) {
            return width_density;
        } else if constexpr (at == At::VDensity) {
            return height_density;
        } else if constexpr (at == At::HThumbnail) {
            return width_thumbnail;
        } else if constexpr (at == At::VThumbnail) {
            return height_thumbnail;
        } else if constexpr (at == At::ExtensionCode) {
            return ext;
        } else if constexpr (at == At::Decodable) {
            return decodable;
        }
    }
};

//using namespace boost::parameter;

namespace label {
    BOOST_PARAMETER_NAME(width)
    BOOST_PARAMETER_NAME(height)
    BOOST_PARAMETER_NAME(dimension)
    BOOST_PARAMETER_NAME(sample_precision)
    BOOST_PARAMETER_NAME(comment)
    BOOST_PARAMETER_NAME(format)
    BOOST_PARAMETER_NAME(major_rev)
    BOOST_PARAMETER_NAME(minor_rev)
    BOOST_PARAMETER_NAME(units)
    BOOST_PARAMETER_NAME(width_density)
    BOOST_PARAMETER_NAME(height_density)
    BOOST_PARAMETER_NAME(width_thumbnail)
    BOOST_PARAMETER_NAME(height_thumbnail)
    BOOST_PARAMETER_NAME(extension_code)
    BOOST_PARAMETER_NAME(decodable)
} // namespace label

namespace detail {

    property make_property_impl(
        std::size_t width, std::size_t height, int dimension, int sample_precision, std::string comment, property::Format form, srook::byte major_rev, srook::byte minor_rev,
        property::Units units, int width_density, int height_density, int width_thumbnail, int height_thumbnail, property::ExtensionCodes extension_code, int decflag = property::AnalyzedResult::Yet)
    {
        return property{
            width, height, dimension, sample_precision, comment, form, major_rev, minor_rev, units, width_density, height_density,
            height_thumbnail, width_thumbnail, extension_code, decflag
        };
    }

} // namespace detail

template <class ArgPack>
property make_property(const ArgPack& args)
{
    using namespace label;

    return detail::make_property_impl(
        args[_width], args[_height], args[_dimension], args[_sample_precision], args[_comment], args[_format], args[_major_rev], args[_minor_rev],
        args[_units], args[_width_density], args[_height_density], args[_width_thumbnail], args[_height_thumbnail], args[_extension_code], args[_decodable]);
}

struct raii_messenger {
    raii_messenger(const char* message, const char* ind = "")
        : mes(message)
        , indent(ind)
        , stoped(false)
    {
        std::cout << indent << mes << " ";
        start = std::chrono::system_clock::now();
    }

    void restart(const char* str = nullptr)
    {
        if (stoped) {
            if (str) {
                std::cout << str << std::endl;
            } else {
                std::cout << mes << " ";
            }
            start = std::chrono::system_clock::now();
            stoped = false;
        }
    }

    srook::optional<float> stop()
    {
        if (!stoped) {
            end = std::chrono::system_clock::now();
            const float time = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) / 1000;
            std::cout << indent << "Done! Processing time: " << time << "(sec)" << std::endl;
            stoped = true;
            return srook::make_optional(time);
        }
        return srook::nullopt;
    }

    ~raii_messenger()
    {
        stop();
    }

private:
    std::chrono::system_clock::time_point start, end;
    const char *mes, *indent;
    bool stoped;
};

} // namespace jpezy

#endif
