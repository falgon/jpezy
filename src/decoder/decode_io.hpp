// JPEZY Copyright (C) 2017 Roki
#ifndef INCLUDED_JPEZY_DECODE_DECODE_IO_HPP
#define INCLUDED_JPEZY_DECODE_DECODE_IO_HPP

#include "../pnm_stream.hpp"
#include <fstream>
#include <srook/config/feature/deduced_typename.hpp>
#include <srook/config/require.hpp>
#include <srook/utility/void_t.hpp>
#include <type_traits>
#include <vector>

namespace jpezy {

namespace detail {

template <class, class = srook::void_t<>>
struct has_30_7_5_1_output_stream_type : std::false_type {};

template <class T>
struct has_30_7_5_1_output_stream_type<
    T, srook::void_t<SROOK_DEDUCED_TYPENAME T::char_type, SROOK_DEDUCED_TYPENAME T::int_type, SROOK_DEDUCED_TYPENAME T::pos_type, SROOK_DEDUCED_TYPENAME T::pos_type, SROOK_DEDUCED_TYPENAME T::off_type, SROOK_DEDUCED_TYPENAME T::traits_type>> : std::true_type {
};

} // namespace detail

template <class Range>
struct decode_io : pnm_stream {
    decode_io(std::size_t w, std::size_t h, const Range &r, const Range &g, const Range &b)
	: pnm_stream(true, w, h, 255), r_(r), g_(g), b_(b)
    {
        if (!(r.size() == g.size() && g.size() == b.size())) initializing_succeed = false;
    }

private:
    template <class OutputStream, REQUIRES(detail::has_30_7_5_1_output_stream_type<std::decay_t<OutputStream>>::value)>
    friend OutputStream &operator<<(OutputStream &&ofs, const decode_io &io)
    {
        if (!io.initializing_succeed) io.report_error(__func__);
        
        ofs << "P3\n";
        ofs << "# Decoded by jpezy\n";
        ofs << io.width << " " << io.height << "\n";
        ofs << io.max_color << "\n";
        for (
                SROOK_DEDUCED_TYPENAME std::decay_t<Range>::const_iterator r_iter = std::begin(io.r_), g_iter = std::begin(io.g_), b_iter = std::begin(io.b_);
                r_iter != std::next(std::begin(io.r_), (io.width * io.height)) && g_iter != std::next(std::begin(io.g_), (io.width * io.height)) && b_iter != std::next(std::begin(io.b_), (io.width * io.height)) && ofs;
                ++r_iter, ++g_iter, ++b_iter) {
            ofs << srook::to_integer<unsigned int>(*r_iter) << " "
                << srook::to_integer<unsigned int>(*g_iter) << " "
                << srook::to_integer<unsigned int>(*b_iter) << "\n";
        }
        return ofs;
    }

    const Range &r_, g_, b_;
};

template <class Range>
decode_io(std::size_t, std::size_t, const Range &) -> decode_io<Range>;

} // namespace jpezy

#endif
