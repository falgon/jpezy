// Copyright (C) 2017 roki
#ifndef INCLUDED_JPEZY_DECODER_HPP
#define INCLUDED_JPEZY_DECODER_HPP

#include <array>
#include <cassert>
#include <iostream>
#include <srook/string/string_view.hpp>
#include <type_traits>
#include <vector>

#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/generate.hpp>

#include <srook/algorithm/for_each.hpp>
#include <srook/config/attribute/fallthrough.hpp>
#include <srook/config/feature/if_constexpr.hpp>
#include <srook/config/feature/deduced_typename.hpp>
#include <srook/config/feature/decltype.hpp>
#include <srook/io/bifstream.hpp>
#include <srook/iterator/ostream_joiner.hpp>
#include <srook/iterator/range_access.hpp>
#include <srook/math/constants/algorithm/cos.hpp>
#include <srook/math/constants/algorithm/sqrt.hpp>
#include <srook/mpl/constant_sequence/math/make_costable.hpp>
#include <srook/optional.hpp>
#include <srook/type_traits/is_arithmetic.hpp>
#include <srook/type_traits/remove_reference.hpp>
#include <srook/type_traits/is_same.hpp>

#include "../jpezy.hpp"
#include "tables.hpp"

namespace jpezy {

template <class>
struct decoder;

template <class BuildMode = Release>
struct decoder {
    explicit decoder(const char *filename)
	: pr{
	      jpezy::make_property(
		  (
		      label::_width = 0,
		      label::_height = 0,
		      label::_dimension = 0,
		      label::_sample_precision = 0,
		      label::_comment = "",
		      label::_format = property::Format::undefined,
		      label::_major_rev = srook::byte(0),
		      label::_minor_rev = srook::byte(0),
		      label::_units = property::Units::undefined,
		      label::_width_density = 1,
		      label::_height_density = 1,
		      label::_width_thumbnail = 0,
		      label::_height_thumbnail = 0,
		      label::_extension_code = property::ExtensionCodes::undefined,
		      label::_decodable = property::AnalyzedResult::Yet))},
	  restart_interval{0},
	  rgb{},
	  comp{},
	  pred_dct{},
	  dct{},
	  block{},
	  ht{},
	  qt{},
	  bifs{filename},
	  hmax{},
	  vmax{},
	  enable{false}
    {}
    
    static SROOK_CONSTEXPR bool is_release_mode = srook::is_same<Release, BuildMode>();

    template <class MODE_TAG = COLOR_MODE>
    srook::optional<srook::array<std::vector<srook::byte>, 3>, srook::optionally::safe_optional_payload> decode()
    {
        raii_messenger mes("process started...");
        std::cout << '\n';

        try {
            analyze_header();
	    } catch (const std::runtime_error &er) {
	        return {};
	    }

        disp_info("\t");
        if (!(pr.get<property::At::Decodable>() & (property::AnalyzedResult::is_htable | property::AnalyzedResult::is_qtable | property::AnalyzedResult::is_start_data))) return {};
        std::unique_ptr<raii_messenger> mes_dec;
        SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>())
            mes_dec = std::make_unique<raii_messenger>("decoding started...", "\t");

        const std::size_t Vblock = get_blocks(pr.get<property::At::VSize>());
        const std::size_t Hblock = get_blocks(pr.get<property::At::HSize>());

        const std::size_t h_unit = (Hblock / hmax) + ((Hblock % hmax) ? 1 : 0);
        const std::size_t v_unit = (Vblock / vmax) + ((Vblock % vmax) ? 1 : 0);
        const std::size_t rgb_s = (v_unit * vmax) * block_size * (h_unit * hmax) * block_size;

        for (auto &&v : rgb) v.resize(rgb_s);

        const std::size_t unit_size = hmax * vmax * blocks_size;
	    comp[0].resize(unit_size);
        for (std::size_t i = 1; i < 3; ++i) comp[i].resize(unit_size, 0x80);

        for (std::size_t uy = 0, restart_counter = 0; uy < v_unit; ++uy) {
            for (std::size_t ux = 0; ux < h_unit; ++ux) {
                try {
                    decode_mcu();
                } catch (const std::runtime_error &er) {
                    std::cerr << "decode_mcu(): throw exception from " << er.what() << std::endl;
                    return {};
                }
                try {
                    make_rgb<MODE_TAG>(ux, uy);
                } catch (const std::runtime_error &er) {
                    std::cerr << "make_rgb(): throw exception from " << er.what() << std::endl;
                    return {};
                }
		   
                try {
                    restart_interval_check(restart_counter);
                } catch (const std::runtime_error &er) {
                    std::cerr << "restart_interval_check(): throw exception from " << er.what() << std::endl;
                } catch (const std::out_of_range &er) {
                    std::cerr << "restart_interval_check(): throw exception from " << er.what() << std::endl;
                }
            }
        }
        for (auto &&v : comp) v.clear();
        
        return { rgb };
    }
    
    property pr;

private:
    inline void disp_info(const char *indent = "")
    {
        std::cout << indent << "Loaded JPEG: ";
        std::cout << pr.get<jpezy::property::At::HSize>() << "x" << pr.get<jpezy::property::At::VSize>() << ", ";
        std::cout << "presicion " << pr.get<jpezy::property::At::SamplePrecision>() << ", ";
        std::cout << "\"" << pr.get<jpezy::property::At::Comment>() << "\"" << ", ";
        std::cout << ((pr.get<jpezy::property::At::Format>() == jpezy::property::Format::JFIF) ? "JFIF" : (pr.get<jpezy::property::At::Format>() == jpezy::property::Format::JFXX) ? "JFXX" : "undefined") << " standart ";
        std::cout << srook::to_integer<unsigned int>(pr.get<jpezy::property::At::MajorRevisions>()) << ".0" << srook::to_integer<unsigned int>(pr.get<jpezy::property::At::MinorRevisions>()) << ", ";
        std::cout << ((pr.get<jpezy::property::At::Units>() == jpezy::property::Units::dots_inch) ? "dots inch" : (pr.get<jpezy::property::At::Units>() == jpezy::property::Units::dots_cm) ? "dots cm" : "undefined") << ", ";
        std::cout << "frames " << pr.get<jpezy::property::At::Dimension>() << ", ";
        std::cout << "density " << pr.get<jpezy::property::At::HDensity>() << "x" << pr.get<jpezy::property::At::VDensity>() << "\n" << std::endl;
    }

    void restart_interval_check(std::size_t &restart_counter)
	SROOK_NOEXCEPT(get_marker())
    {
        if (restart_interval) {
            if (++restart_counter >= restart_interval) {
                restart_counter = 0;
                const MARKER mark = get_marker();
                if (mark >= MARKER::RST0 && mark <= MARKER::RST7)
                    pred_dct[0] = pred_dct[1] = pred_dct[2] = 0;
            }
        }
    }

    template <class T, SROOK_REQUIRES(srook::is_arithmetic<T>::value)>
    static SROOK_CONSTEXPR T get_blocks(T x) SROOK_NOEXCEPT_TRUE
    {
        return (x >> 3) + ((x & 0x07) > 0);
    }

    void analyze_header()
    {
        std::unique_ptr<raii_messenger> mes;
        SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>())
            mes = std::make_unique<raii_messenger>("analyzing header...", "\t");
	
        do {
            if (get_marker() == MARKER::SOI)
                enable = true;
        } while (!enable);
	
        while (enable) {
            pr.get<property::At::Decodable>() |= analyze_marker();
            if (pr.get<property::At::Decodable>() & property::AnalyzedResult::is_start_data)
                return;
        }
        throw std::runtime_error(__func__);
    }

    void analyze_dht(std::size_t size)
    {
        std::unique_ptr<raii_messenger> mes;
        SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) mes = std::make_unique<raii_messenger>("analyzing DHT...", "\t\t\t");
        
        const srook::byte *end_add = bifs.next_address();
        if (!end_add) throw std::out_of_range(__func__);
        end_add += size;
        
        do {
            std::underlying_type_t<srook::byte> uc{};
            (bifs | srook::io::jpeg::bifstream::Byte) >> uc;
            const std::size_t tc = uc >> 4, th = uc & 0x0f;
	    
            if (tc > 1) throw std::runtime_error("DC format error");
            if (th > 3) throw std::runtime_error("AC format error");
	    
            huffman_table &ht_ = ht[tc][th];
            srook::array<std::underlying_type_t<srook::byte>, block_size * 2> cc{};
            std::size_t n_ = 0;
            
            boost::generate(cc, [this, &n_]() {std::underlying_type_t<srook::byte> b{}; (bifs | srook::io::jpeg::bifstream::Byte) >> b; n_ += b; return b; });
            SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>())
                std::cout << " size: " << n_ << std::endl;
            const std::size_t &n = n_;

            ht_.sizeTP.clear();
            ht_.codeTP.clear();
            ht_.valueTP.clear();
            ht_.sizeTP.resize(n);
            ht_.codeTP.resize(n);
            ht_.valueTP.resize(n);
	    
            for (std::size_t i = 1, k = 0; i <= (block_size * 2); ++i) {
                for (std::size_t j = 1; j <= cc[i - 1]; ++j, ++k) {
                    if (k >= n) throw std::out_of_range("invalid size table");
                    ht_.sizeTP[k] = i;
                }
            }
	   
            for (auto[k, code, si] = std::make_tuple(0u, 0, ht_.sizeTP[0]);;) {
                for (; ht_.sizeTP[k] == si; ++k, ++code)
                    ht_.codeTP[k] = code;
            
                if (k >= n) break;
                do {
                    code <<= 1;
                    ++si;
                } while (ht_.sizeTP[k] != si);
            }
            for (std::size_t k = 0; k < n; ++k) (bifs | srook::io::jpeg::bifstream::Byte) >> ht_.valueTP[k];
	   
            SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) {
                switch (n) {
                    case 12:
                        std::cout << "found DC Huffman Table... ";
                        break;
                    case 162:
                        std::cout << "found AC Huffman Table... ";
			            break;
                    default:
                        std::cerr << n_ << std::endl;
                        throw std::out_of_range("invalid size table");
                }
            }
        } while (bifs.next_address() < end_add);
    }

    void analyze_dqt(std::size_t size)
    {
        std::unique_ptr<raii_messenger> mes;
        SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) mes = std::make_unique<raii_messenger>("\t\t\tanalyzing DQT...");
        const srook::byte *const end_add = bifs.next_address() + size;
        std::underlying_type_t<srook::byte> c{};
	
        do {
            (bifs | srook::io::jpeg::bifstream::Byte) >> c;
            if (SROOK_DEDUCED_TYPENAME SROOK_DECLTYPE(qt)::value_type::iterator iter = srook::begin(qt[c & 0x3]); !(c >> 4)) {
                std::underlying_type_t<srook::byte> t{};
                for (std::size_t i = 0; i < blocks_size; ++i) {
                    (bifs | srook::io::jpeg::bifstream::Byte) >> t;
                    iter[ZZ[i]] = int(t);
                }
            } else {
                for (std::size_t i = 0; i < blocks_size; ++i) (bifs | srook::io::jpeg::bifstream::Word) >> iter[ZZ[i]];
            }
        } while (bifs.next_address() < end_add);
    }

    void analyze_frame()
    {
        std::unique_ptr<raii_messenger> mes;
        SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) mes = std::make_unique<raii_messenger>("\t\t\tanalyzing frames...");

        using namespace srook::literals::byte_literals;
	
        (bifs | srook::io::jpeg::bifstream::Byte) >> pr.get<property::At::SamplePrecision>();
        (bifs | srook::io::jpeg::bifstream::Word) >> pr.get<property::At::VSize>();
        (bifs | srook::io::jpeg::bifstream::Word) >> pr.get<property::At::HSize>();
        (bifs | srook::io::jpeg::bifstream::Byte) >> pr.get<property::At::Dimension>();
        if (pr.get<property::At::Dimension>() != 3 && pr.get<property::At::Dimension>() != 1) throw std::runtime_error("Sorry, this dimension size is not supported");

        SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) std::cout << "VSize: " << pr.get<property::At::VSize>() << " HSize: " << pr.get<property::At::HSize>() << " ";
	
        for (auto [i, c] = std::make_tuple(0u, 0_byte); i < std::size_t(pr.get<property::At::Dimension>()); ++i) {
            (bifs | srook::io::jpeg::bifstream::Byte) >> fcomp[i].C;
            (bifs | srook::io::jpeg::bifstream::Byte) >> c;
	    
            fcomp[i].H = SROOK_DEDUCED_TYPENAME get_character<srook::byte>::type(c >> 4);
	    
            if (fcomp[i].H > hmax) hmax = fcomp[i].H;
            fcomp[i].V = (std::underlying_type_t<srook::byte>(c) & 0xf);
            if (fcomp[i].V > vmax) vmax = fcomp[i].V;
            (bifs | srook::io::jpeg::bifstream::Byte) >> fcomp[i].Tq;
        }
    }

    void analyze_scan()
    {
        std::unique_ptr<raii_messenger> mes;
        SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) mes = std::make_unique<raii_messenger>("\t\t\tanalyzing scan data...");
	
        srook::byte c{};
        (bifs | srook::io::jpeg::bifstream::Byte) >> s_header.scan_comp;
        for (std::size_t i = 0; i < srook::to_integer<std::size_t>(s_header.scan_comp); ++i) {
            (bifs | srook::io::jpeg::bifstream::Byte) >> scomp[i].Cs;
            (bifs | srook::io::jpeg::bifstream::Byte) >> c;
	    
            scomp[i].Td = srook::to_integer<std::underlying_type_t<srook::byte>>(c >> 4);
            if (scomp[i].Td > 2) throw std::out_of_range(__func__);
	    
            scomp[i].Ta = (srook::to_integer<std::underlying_type_t<srook::byte>>(c) & 0xf);
            if (scomp[i].Ta > 2) throw std::out_of_range(__func__);
	
        }

        // unused for DCT
        {
            (bifs | srook::io::jpeg::bifstream::Byte) >> s_header.spectral_begin;
            (bifs | srook::io::jpeg::bifstream::Byte) >> s_header.spectral_end;
            (bifs | srook::io::jpeg::bifstream::Byte) >> c;
            s_header.Ah = srook::to_integer<SROOK_DEDUCED_TYPENAME get_character<srook::byte>::type>(c >> 4);
            s_header.Al = srook::to_integer<std::underlying_type_t<srook::byte>>(c) & 0xf;
        }
    }

    void analyze_jfif()
    {
        std::unique_ptr<raii_messenger> mes;
        SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) mes = std::make_unique<raii_messenger>("\t\t\tanalyzing jfif...");
	
        pr.get<property::At::Format>() = property::Format::JFIF;
        (bifs | srook::io::jpeg::bifstream::Byte) >> pr.get<property::At::MajorRevisions>();
        (bifs | srook::io::jpeg::bifstream::Byte) >> pr.get<property::At::MinorRevisions>();
        (bifs | srook::io::jpeg::bifstream::Byte) >> pr.get<property::At::Units>();
        (bifs | srook::io::jpeg::bifstream::Word) >> pr.get<property::At::HDensity>();
        (bifs | srook::io::jpeg::bifstream::Word) >> pr.get<property::At::VDensity>();
        (bifs | srook::io::jpeg::bifstream::Byte) >> pr.get<property::At::HThumbnail>();
        (bifs | srook::io::jpeg::bifstream::Byte) >> pr.get<property::At::VThumbnail>();
        pr.get<property::At::Decodable>() |= property::AnalyzedResult::is_jfif;
    }

    void analyze_jfxx()
    {
        std::unique_ptr<raii_messenger> mes;
        SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) mes = std::make_unique<raii_messenger>("\t\t\tanalyzing jfxx...");
        pr.get<property::At::Format>() = property::Format::JFXX;
        (bifs | srook::io::jpeg::bifstream::Byte) >> pr.get<property::At::ExtensionCode>();
    }

    int analyze_marker()
    {
        int length{};
        MARKER mark = get_marker();
        const auto param_offset = [](int &len_) { len_ -= 2; };
	
        switch (mark) {
	    
            case MARKER::SOF0:
                SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) std::cout << "\t\tfound marker: [SOF0]" << std::endl;
                (bifs | srook::io::jpeg::bifstream::Word) >> length;
                analyze_frame();
                break;
            case MARKER::DHT:
                SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) std::cout << "\t\tfound marker: [DHT]" << std::endl;
                (bifs | srook::io::jpeg::bifstream::Word) >> length;
                param_offset(length);
                analyze_dht(length);
                return property::AnalyzedResult::is_htable;
            case MARKER::DNL:
		
                SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) std::cout << "\t\tfound marker: [DNL]" << std::endl;
                (bifs | srook::io::jpeg::bifstream::Word) >> length;
                (bifs | srook::io::jpeg::bifstream::Word) >> pr.get<property::At::VSize>();
                break;
            case MARKER::DQT:
                SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) std::cout << "\t\tfound marker: [DQT]" << std::endl;
                (bifs | srook::io::jpeg::bifstream::Word) >> length;
                param_offset(length);
                analyze_dqt(length);
                return property::AnalyzedResult::is_qtable;
            case MARKER::EOI:
                SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) std::cout << "\t\tfound marker: [EOI]" << std::endl;
                enable = false;
                break;
            case MARKER::SOS:
                SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) std::cout << "\t\tfound marker: [SOS]" << std::endl;
                (bifs | srook::io::jpeg::bifstream::Word) >> length;
                analyze_scan();
                return property::AnalyzedResult::is_start_data;
            case MARKER::DRI:
                SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) std::cout << "\t\tfound marker: [DRI]" << std::endl;
                (bifs | srook::io::jpeg::bifstream::Word) >> length;
                (bifs | srook::io::jpeg::bifstream::Word) >> restart_interval;
                break;
            case MARKER::COM:
                SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) std::cout << "\t\tfound marker: [COM]" << std::endl;
                (bifs | srook::io::jpeg::bifstream::Word) >> length;
                param_offset(length);
                (bifs | srook::io::jpeg::bifstream::Byte_n(length)) >> pr.get<property::At::Comment>();
                return property::AnalyzedResult::is_comment;
            // Not supported frames
            case MARKER::SOF1: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::SOF2: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::SOF3: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::SOF5: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::SOF6: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::SOF7: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::SOF9: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::SOF10: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::SOF11: SROOK_ATTRIBUTE_FALLTHROUGH;    case MARKER::SOF13: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::SOF14: SROOK_ATTRIBUTE_FALLTHROUGH;    case MARKER::SOF15: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::EXP: SROOK_ATTRIBUTE_FALLTHROUGH;      case MARKER::DAC: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::DHP:
                std::runtime_error("Not supported");
                break;
            case MARKER::APP0: {
                SROOK_IF_CONSTEXPR (srook::is_same<BuildMode, Debug>()) std::cout << "\n\t\tfound marker: [APP0]" << std::endl;
                (bifs | srook::io::jpeg::bifstream::Word) >> length;
                param_offset(length);
		
                if (static SROOK_CONSTEXPR char JFIF[] = "JFIF", JFXX[] = "JFXX"; srook::string_view(JFIF).size() == srook::string_view(JFXX).size()) {
                    if (const std::size_t size = srook::string_view(JFIF).size(); length >= signed(size)) {
                        std::string id;
                        id.resize(size + 1);
                        (bifs | srook::io::jpeg::bifstream::Bytes) >> id;
                        id.pop_back();
			
                        if (id == JFIF) {
                            analyze_jfif();
                            bifs.skip_byte(length - 14);
                        } else if (JFXX == id) {
                            analyze_jfxx();
                            bifs.skip_byte(length - 1);
                        } else {
                            bifs.skip_byte(length - size);
                        }
                    } else {
                        bifs.skip_byte(length);
                    }
                }
                break;
            }

            // Not supported markers
            case MARKER::APP1: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::APP2: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::APP3: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::APP4: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::APP5: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::APP6: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::APP7: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::APP8: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::APP9: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::APP10: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::APP11: SROOK_ATTRIBUTE_FALLTHROUGH;    case MARKER::APP12: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::APP13: SROOK_ATTRIBUTE_FALLTHROUGH;    case MARKER::APP14: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::APP15:
                (bifs | srook::io::jpeg::bifstream::Word) >> length;
                param_offset(length);
                bifs.skip_byte(length);
                break;

	        // reserves and other
            case MARKER::JPG: SROOK_ATTRIBUTE_FALLTHROUGH;      case MARKER::JPG0: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::JPG1: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::JPG2: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::JPG3: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::JPG4: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::JPG5: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::JPG6: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::JPG7: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::JPG8: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::JPG9: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::JPG10: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::JPG11: SROOK_ATTRIBUTE_FALLTHROUGH;    case MARKER::JPG12: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::JPG13: SROOK_ATTRIBUTE_FALLTHROUGH;    case MARKER::TEM: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::Error: SROOK_ATTRIBUTE_FALLTHROUGH;    case MARKER::RST0: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::RST1: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::RST2: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::RST3: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::RST4: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::RST5: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::RST6: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::RST7: SROOK_ATTRIBUTE_FALLTHROUGH;     case MARKER::SOI: SROOK_ATTRIBUTE_FALLTHROUGH;
            case MARKER::RESst: SROOK_ATTRIBUTE_FALLTHROUGH;    case MARKER::RESnd: SROOK_ATTRIBUTE_FALLTHROUGH;
	    
            default:
                throw std::runtime_error("Marker error");
        }
        return property::AnalyzedResult::Yet;
    }

    MARKER get_marker()
    {
        for (std::underlying_type_t<srook::byte> c{};;) {
            (bifs | srook::io::jpeg::bifstream::Byte) >> c;
            if (c == std::underlying_type_t<srook::byte>(MARKER::Marker)) {
                (bifs | srook::io::jpeg::bifstream::Byte) >> c;
                if (c) {
                    if ((c > std::underlying_type_t<srook::byte>(MARKER::RESst)) && (c < std::underlying_type_t<srook::byte>(MARKER::SOF0))) {
                        return MARKER::Error;
                    } else {
                        return MARKER(c);
                    }
                }
            }
        }
        return MARKER::Error;
    }

    void decode_mcu()
    {
        for (std::size_t sc = 0; sc < std::size_t(pr.get<property::At::Dimension>()); ++sc) {
            const std::size_t num_v = fcomp[sc].V;
            const std::size_t num_h = fcomp[sc].H;
            const std::size_t dupc_y = vmax / num_v;
            const std::size_t dupc_x = hmax / num_h;
            const std::size_t v_step = hmax * block_size;
	    
            for (std::size_t ky = 0; ky < num_v; ++ky) {
                for (std::size_t kx = 0; kx < num_h; ++kx) {
                    decode_huffman(sc);
                    inverse_quantization(sc);
                    inverse_dct();

                    SROOK_DEDUCED_TYPENAME srook::remove_reference_t<SROOK_DECLTYPE(comp)>::value_type::iterator tp = std::next(srook::begin(comp[sc]), ky * v_step * block_size + kx * block_size);
                    for (std::size_t y_u = 0; y_u < block_size * dupc_y; ++y_u) {
                        for (std::size_t x_u = 0; x_u < block_size * dupc_x; ++x_u) {
                            tp[y_u * v_step + x_u] = block[(y_u / dupc_y) * block_size + (x_u / dupc_x)];
                        }
                    }
                }
            }
        }
    }


    template <class MODE_TAG>
    void make_rgb(const std::size_t ux, const std::size_t uy) noexcept
    {
        SROOK_DEDUCED_TYPENAME srook::remove_reference_t<SROOK_DECLTYPE(comp)>::value_type::iterator yp = srook::begin(comp[0]), up = srook::begin(comp[1]), vp = srook::begin(comp[2]);
        const std::size_t line = uy * vmax * block_size;
        const std::size_t offset_v = line * pr.get<property::At::HSize>();
        const std::size_t offset_h = ux * hmax * block_size;
        const std::size_t offset = offset_v + offset_h;
	
        std::vector<srook::byte>::iterator rp = std::next(srook::begin(rgb[0]), offset), gp = std::next(srook::begin(rgb[1]), offset), bp = std::next(srook::begin(rgb[2]), offset);
        const std::size_t end_x = (hmax * block_size);
        const std::size_t end_y = (vmax * block_size);
	
        for (std::size_t pic_y = 0; pic_y < end_y; ++pic_y) {
            for (std::size_t pic_x = 0; pic_x < end_x; ++pic_x) {
                if (pic_x + offset_h >= pr.get<property::At::HSize>()) {
                    yp += end_x - pic_x;
                    up += end_x - pic_x;
                    vp += end_x - pic_x;
                    break;
                }

                SROOK_IF_CONSTEXPR (const std::size_t index = pic_y * pr.get<property::At::HSize>() + pic_x; srook::is_same<MODE_TAG, COLOR_MODE>()) {
                    rp[index] = revise_value(to_r(*yp, *vp));
                    gp[index] = revise_value(to_g(*yp, *up, *vp));
                    bp[index] = revise_value(to_b(*yp, *up));
                    ++yp;
                    ++vp;
                    ++up;
                } else {
                    gp[index] = bp[index] = rp[index] = revise_value(*yp++);
                }
            }
        }
    }

    SROOK_CONSTEXPR double to_r(const double yp, const double vp) const noexcept
    {
        return yp + (vp - 0x80) * 1.4020;
    }
    SROOK_CONSTEXPR double to_g(const double yp, const double up, const double vp) const noexcept
    {
        return yp - (up - 0x80) * 0.3441 - (vp - 0x80) * 0.7139;
    }
    SROOK_CONSTEXPR double to_b(const double yp, const double up) const noexcept
    {
        return yp + (up - 0x80) * 1.7718;
    }

    struct DC;
    struct AC;

    void decode_huffman(std::size_t sc)
    {
        // DC
        int dc_diff = 0;
        int category = decode_huffman_impl<DC>(sc);
        if (category > 0) {
            (bifs | srook::io::jpeg::bifstream::Bits(category)) >> dc_diff;
            if ((dc_diff & (1 << (category - 1))) == 0) {
                dc_diff -= (1 << category) - 1;
            }
        } else if (category < 0) {
            throw std::runtime_error(__func__);
        }
        pred_dct[sc] += dc_diff;
        dct[0] = pred_dct[sc];
	
        // AC
        for (std::size_t k = 1; k < blocks_size;) {
            category = decode_huffman_impl<AC>(sc);
            if (!category) {
                for (; k < blocks_size; ++k) dct[ZZ[k]] = 0;
                break;
            } else if (category < 0) {
                throw std::runtime_error(__func__);
            }

            int run = category >> 4, acv = 0;
            category &= 0x0f;
            if (category) {
                (bifs | srook::io::jpeg::bifstream::Bits(category)) >> acv;
                if (!(acv & (1 << (category - 1)))) {
                    acv -= (1 << category) - 1;
                }
            } else if (run != 15) {
                std::runtime_error(__func__);
            }
            if ((run + k) > int(blocks_size - 1)) throw std::runtime_error(__func__);
	    
            for (; run-- > 0; ++k) dct[ZZ[k]] = 0;
            dct[ZZ[k++]] = acv;
        }
    }

    template <class TC>
    int decode_huffman_impl(std::size_t sc)
    {
        SROOK_ST_ASSERT(srook::is_same<TC, DC>() or srook::is_same<TC, AC>());
        huffman_table &ht_ = ht[srook::is_same<TC, AC>()][scomp[sc].Td];
        for (auto[code, length, next, k] = std::make_tuple(0, 0u, 0, 0u); k < ht_.size() && length < (block_size * 2);) {
            ++length;
            code <<= 1;
            (bifs | srook::io::jpeg::bifstream::Bits(1)) >> next;
            if (next < 0) return next;
            code |= next;
            for (; ht_.sizeTP[k] == length; ++k) {
                if (ht_.codeTP[k] == code) return ht_.valueTP[k];
            }
        }
        throw std::runtime_error(__func__);
    }


    void inverse_quantization(std::size_t sc) noexcept
    {
        SROOK_DEDUCED_TYPENAME srook::remove_reference_t<SROOK_DECLTYPE(dct)>::iterator p = srook::begin(dct);
        SROOK_DEDUCED_TYPENAME srook::remove_reference_t<SROOK_DECLTYPE(qt)>::value_type::const_iterator iter = srook::begin(qt[fcomp[sc].Tq]);
        for (std::size_t i = 0; i < blocks_size; ++i) *p++ *= *iter++;
    }

    void inverse_dct() noexcept
    {
        const int sl = pr.get<property::At::SamplePrecision>() == block_size ? 128 : 2048;
        SROOK_CONSTEXPR double disqrt2 = (1.0 / srook::sqrt(2));
	
        for (std::size_t y = 0; y < block_size; ++y) {
            for (std::size_t x = 0; x < block_size; ++x) {
                double sum = 0;
                for (std::size_t v = 0; v < block_size; ++v) {
                    const double cv = (!v) ? disqrt2 : 1.0;
                    for (std::size_t u = 0; u < block_size; ++u) {
                        const double cu = (!u) ? disqrt2 : 1.0;
                        sum += cu * cv * dct[v * block_size + u] * cos_table[u * block_size + x] * cos_table[v * block_size + y];
                    }
                }
                block[y * block_size + x] = int(sum / 4 + sl);
            }
        }
    }

    SROOK_CONSTEXPR srook::byte revise_value(double v) const noexcept
    {
        using namespace srook::literals::byte_literals;
        return (v < 0.0) ? 0_byte : (v > 255.0) ? 255_byte : srook::byte(v);
    }

public:
    static SROOK_CONSTEXPR std::size_t rgb_size = 3;
    static SROOK_CONSTEXPR std::size_t block_size = 8;
    static SROOK_CONSTEXPR std::size_t blocks_size = block_size * block_size;
    static SROOK_CONSTEXPR std::size_t mcu_size = 4;

private:
    Scan_header s_header;
    srook::array<Frame_component, 3> fcomp;
    srook::array<Scan_component, 3> scomp;

    std::size_t restart_interval;
    srook::array<std::vector<srook::byte>, rgb_size> rgb;
    srook::array<std::vector<int>, rgb_size> comp;
    srook::array<int, rgb_size> pred_dct;
    srook::array<int, blocks_size> dct;
    srook::array<int, blocks_size> block;

    srook::array<srook::array<huffman_table, 4>, 2> ht;
    srook::array<srook::array<int, blocks_size>, mcu_size> qt;
    static SROOK_CONSTEXPR srook::array<const double, block_size * block_size> cos_table = srook::constant_sequence::math::unwrap_costable::array<srook::constant_sequence::math::make_costable_t<8,8>>::value;

    srook::io::jpeg::bifstream bifs;
    std::make_signed_t<std::underlying_type_t<srook::byte>> hmax, vmax;
    bool enable;
};

} // namespace jpezy
#endif
