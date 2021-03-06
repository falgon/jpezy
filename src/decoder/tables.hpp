// Copyright (C) 2017 roki
#ifndef INCLUDED_JPEZY_DECODE_TABLES_HPP
#define INCLUDED_JPEZY_DECODE_TABLES_HPP

#include <srook/cstddef/byte.hpp>
#include <srook/config/feature/final.hpp>
#include <srook/config/feature/noexcept.hpp>
#include <srook/type_traits/type_constant.hpp>
#include <cassert>
#include <type_traits>
#include <vector>

namespace jpezy {

template <class T>
struct get_character : public srook::type_constant<std::make_signed_t<std::underlying_type_t<T>>> {};

struct Frame_component SROOK_FINAL : get_character<srook::byte> {
    srook::byte C;
    type H, V, Tq;
};

struct Scan_component SROOK_FINAL : get_character<srook::byte> {
    srook::byte Cs;
    type Td, Ta;
};

struct Scan_header SROOK_FINAL : get_character<srook::byte> {
    srook::byte scan_comp, spectral_begin, spectral_end;
    type Ah, Al;
};

struct huffman_table {
    std::vector<std::size_t> sizeTP;
    std::vector<int> codeTP, valueTP;

    std::size_t size() const SROOK_NOEXCEPT_TRUE
    {
		assert(sizeTP.size() == codeTP.size() && codeTP.size() == valueTP.size());
		return sizeTP.size();
    }
};

} // namespace jpezy

#endif
