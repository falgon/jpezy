#ifndef INCLUDED_JPEZY_ENCODER_HPP
#define INCLUDED_JPEZY_ENCODER_HPP
#define _USE_MATH_DEFINES
#include "../jpezy.hpp"
#include "huffman_table.hpp"
#include "jpezy_writer.hpp"
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/fill.hpp>
#include <chrono>
#include <experimental/iterator>
#include <iostream>
#include <srook/algorithm/for_each.hpp>
#include <srook/array.hpp>
#include <srook/io/bofstream.hpp>
#include <srook/math/constants/algorithm/sqrt.hpp>
#include <srook/mpl/constant_sequence/math/make_costable.hpp>
#include <vector>

namespace jpezy {

template <class T>
struct encoder {
    constexpr encoder(const property& pr_, const std::vector<T>& r_, const std::vector<T>& g_, const std::vector<T>& b_)
        : pr(pr_)
        , r{ r_ }
        , g{ g_ }
        , b{ b_ }
        , Y_block{}
        , Cb_block{}
        , Cr_block{}
        , DCT_data{}
        , pre_DC{}
    {
        static_assert(rgb_size == static_cast<std::size_t>(RGB::ELEMENT_SIZE));
    }

    template <class MODE_TAG = COLOR_MODE>
    std::size_t encode(const char* output_file) noexcept(false)
    {
        int size = pr.get<property::At::HSize>() * pr.get<property::At::VSize>() * 3;
        if (size < 10240)
            size = 10240;

        jpezy_writer jpeg(size, pr, output_file);

        {
            raii_messenger mes("Write JPEG Header ...");
            jpeg.write_header();
        }

        {
            raii_messenger mes("Encoding ...");

            const int VUnits = (pr.get<property::At::VSize>() / 16) + ((pr.get<property::At::VSize>() % 16) ? 1 : 0);
            const int HUnits = (pr.get<property::At::HSize>() / 16) + ((pr.get<property::At::HSize>() % 16) ? 1 : 0);

            for (int y = 0; y < VUnits; ++y) {
                for (int x = 0; x < HUnits; ++x) {
                    make_YCC(x, y);
                    if constexpr (std::is_same_v<MODE_TAG, GRAY_MODE>) {
                        boost::fill(Cb_block, 0);
                        boost::fill(Cr_block, 0);
                    }
                    make_MCU(jpeg.get_stream());
                }
            }
        }

        {
            raii_messenger mes("Write EOI ...");
            jpeg.write_eoi();
        }

        jpeg.output_file();
        return jpeg.get_stream().wrote_size();
    }

private:
    using value_type = int;

    static constexpr value_type block = 8;
public: 
	static constexpr value_type block_size = block;
private:
    static constexpr value_type blocks_size = block * block;
    static constexpr value_type rgb_size = 3;
    static constexpr value_type mcu_size = 4;

    void make_YCC(int ux, int uy) noexcept
    {
        decltype(Y_block) Crblock{}, Cbblock{};

        for (std::size_t i = 0; i < mcu_size; ++i) {
            typename decltype(Y_block)::value_type::iterator
                yp
                = std::begin(Y_block[i]),
                cbp = std::begin(Cbblock[i]), crp = std::begin(Crblock[i]);

            for (std::size_t sy = uy * (block * 2) + ((i > 1) ? block : 0), sy_t = sy + block; sy < sy_t; ++sy) {
                const std::size_t ii = sy < pr.get<property::At::VSize>() ? sy : pr.get<property::At::VSize>() - 1;

                for (std::size_t sx = ux * (block * 2) + ((i & 1) ? block : 0), sx_t = sx + block; sx < sx_t; ++sx) {
                    const std::size_t jj = sx < pr.get<property::At::HSize>() ? sx : pr.get<property::At::HSize>() - 1;
                    const std::size_t index = ii * pr.get<property::At::HSize>() + jj;

                    const srook::byte &rv = r[index], gv = g[index], bv = b[index];

                    *yp++ = RGB::Y(rv, gv, bv);
                    *cbp++ = RGB::Cb(rv, gv, bv);
                    *crp++ = RGB::Cr(rv, gv, bv);
                }
            }
        }

        int n = 0;
        for (int i = 0; i < mcu_size; ++i) {
            switch (i) {
            case 0:
                n = 0;
                break;
            case 1:
                n = 4;
                break;
            case 2:
                n = 32;
                break;
            case 3:
                n = 36;
                break;
            default:
                break;
            }
            for (int y = 0; y < block; y += 2) {
                for (int x = 0; x < block; x += 2) {
                    const int index = y * block + x;
                    Cr_block[n] = Crblock[i][index];
                    Cb_block[n] = Cbblock[i][index];
                    ++n;
                }
                n += mcu_size;
            }
        }
    }

    template <class U, std::size_t s>
    void DCT(srook::array<U, s>& pic) noexcept
    {
        constexpr double dis_sqrt = 1.0 / srook::sqrt(2.0);

        for (int i = 0; i < block; ++i) {
            const double cv = i ? 1.0 : dis_sqrt;

            for (int j = 0; j < block; ++j) {
                const double cu = j ? 1.0 : dis_sqrt;
                double sum = 0;

                for (int y = 0; y < block; ++y) {
                    for (int x = 0; x < block; ++x) {
                        sum += pic[y * block + x] * cos_table[j * block_size + x] * cos_table[i * block_size + y];
                    }
                }
                DCT_data[i * block + j] = int(sum * cu * cv / 4);
            }
        }
    }

    void quantization(int cs) noexcept
    {
        const srook::array<int, 64>& qt = cs ? CQuantumTb : YQuantumTb;
        srook::for_each(srook::make_counter(DCT_data), [&qt](auto& v, std::size_t i) { v /= qt[i]; });
    }

    template <class U, std::size_t dcs, std::size_t acs>
    void encode_huffman(int cs, srook::io::jpeg::bofstream& ofs, const huffmanCode_Tb<U, dcs>& dcT, const huffmanCode_Tb<U, acs>& acT, int eob_idx, int zrl_idx) noexcept(false)
    {
        const decltype(DCT_data)& dct_data = DCT_data;

        // DC
        const int diff = dct_data[0] - pre_DC[cs];
        pre_DC[cs] = dct_data[0];

        std::size_t di = 0;
        for (int abs_diff = std::abs(diff); abs_diff > 0; abs_diff >>= 1, ++di)
            ;
        if (di > static_cast<signed>(dcT.size()))
            throw std::runtime_error(__func__);

        (ofs | srook::io::jpeg::bofstream::Bits(dcT.size_tb[di])) << dcT.code_tb[di];
        if (di)
            (ofs | srook::io::jpeg::bofstream::Bits(di)) << (diff < 0 ? diff - 1 : diff);

        // AC
        for (std::size_t n = 1, run = 0; n < blocks_size; ++n) {
            int abs_coefficient = std::abs(dct_data[ZZ[n]]);

            if (abs_coefficient) {
                while (run > 15) {
                    (ofs | srook::io::jpeg::bofstream::Bits(acT.size_tb[zrl_idx])) << acT.code_tb[zrl_idx];
                    run -= 16;
                }
                int s = 0;
                for (; abs_coefficient > 0; abs_coefficient >>= 1, ++s)
                    ;

                const int a_di = run * 10 + s + (run == 15);
                if (a_di >= static_cast<signed>(acT.size()))
                    throw std::runtime_error(__func__);

                (ofs | srook::io::jpeg::bofstream::Bits(acT.size_tb[a_di])) << acT.code_tb[a_di];

                int v = dct_data[ZZ[n]];
                if (v < 0)
                    --v;

                (ofs | srook::io::jpeg::bofstream::Bits(s)) << v;
                run = 0;
            } else {
                if (n == 63)
                    (ofs | srook::io::jpeg::bofstream::Bits(acT.size_tb[eob_idx])) << acT.code_tb[eob_idx];
                else
                    ++run;
            }
        }
    }

    void make_MCU(srook::io::jpeg::bofstream& ofps) noexcept(false)
    {
        for (auto& v : Y_block) {
            DCT(v);
            quantization(0);
            encode_huffman(0, ofps, YDcHuffmanT, YAcHuffmanT, YEOBidx, YZRLidx);
        }

        DCT(Cb_block);
        quantization(1);
        encode_huffman(1, ofps, CDcHuffmanT, CAcHuffmanT, CEOBidx, CZRLidx);

        DCT(Cr_block);
        quantization(2);
        encode_huffman(2, ofps, CDcHuffmanT, CAcHuffmanT, CEOBidx, CZRLidx);
    }

    struct RGB {
        static constexpr int Y(srook::byte r, srook::byte g, srook::byte b)
        {
            return int((0.2990 * srook::to_integer<int>(r)) + (0.5870 * srook::to_integer<int>(g)) + (0.1140 * srook::to_integer<int>(b)) - 128);
        }
        static constexpr int Cb(srook::byte r, srook::byte g, srook::byte b)
        {
            return int(-(0.1687 * srook::to_integer<int>(r)) - (0.3313 * srook::to_integer<int>(g)) + (0.5000 * srook::to_integer<int>(b)));
        }
        static constexpr int Cr(srook::byte r, srook::byte g, srook::byte b)
        {
            return int((0.5000 * srook::to_integer<int>(r)) - (0.4187 * srook::to_integer<int>(g)) - (0.0813 * srook::to_integer<int>(b)));
        }
        enum { R,
            G,
            B,
            ELEMENT_SIZE };
    };

    const property& pr;
    const std::vector<T> r, g, b;

    srook::array<srook::array<value_type, blocks_size>, mcu_size> Y_block;
    srook::array<value_type, blocks_size> Cb_block, Cr_block;

    static constexpr srook::array<const double, block * block> cos_table = srook::constant_sequence::math::unwrap_costable::array<srook::constant_sequence::math::make_costable_t<8,8>>::value;

    srook::array<value_type, blocks_size> DCT_data;
    srook::array<value_type, rgb_size> pre_DC;
};

template <class T>
encoder(const property&, const std::vector<T>&, const std::vector<T>&, const std::vector<T>&)->encoder<T>;

} // namespace jpezy

#endif
