#ifndef INCLUDED_JPEG_WRITER_HPP
#define INCLUDED_JPEG_WRITER_HPP

#include "huffman_table.hpp"
#include <array>
#include <cstring>
#include <fstream>
#include <srook/config/noexcept_detection.hpp>
#include <srook/io/bofstream.hpp>
#include <type_traits>

namespace jpezy {

struct jpezy_writer {
    jpezy_writer(const std::size_t buffer_size, const property& pr_, const char* output_name)
        : ofps(buffer_size, output_name, std::ios::out | std::ios::trunc | std::ios::binary)
        , pr(pr_)
    {}

    void write_header()
    {
        if (!ofps)
            throw std::runtime_error(__func__);

        // SOI
        (ofps | srook::io::jpeg::bofstream::Byte) << MARKER::Marker << MARKER::SOI;

        // JFIF
        (ofps | srook::io::jpeg::bofstream::Byte) << MARKER::Marker << MARKER::APP0;
        (ofps | srook::io::jpeg::bofstream::Word) << 16;
        (ofps | srook::io::jpeg::bofstream::Byte_n(5)) << "JFIF";
        (ofps | srook::io::jpeg::bofstream::Word) << 0x0102;
        (ofps | srook::io::jpeg::bofstream::Byte) << pr.get<property::At::Units>();
        (ofps | srook::io::jpeg::bofstream::Word) << pr.get<property::At::HDensity>();
        (ofps | srook::io::jpeg::bofstream::Word) << pr.get<property::At::VDensity>();
        (ofps | srook::io::jpeg::bofstream::Byte) << pr.get<property::At::HThumbnail>();
        (ofps | srook::io::jpeg::bofstream::Byte) << pr.get<property::At::VThumbnail>();

        // Comment
        if (!pr.get<property::At::Comment>().empty()) {
            (ofps | srook::io::jpeg::bofstream::Byte) << MARKER::Marker << MARKER::COM;
            (ofps | srook::io::jpeg::bofstream::Word) << static_cast<unsigned int>(pr.get<property::At::Comment>().size() + 3);
            (ofps | srook::io::jpeg::bofstream::Byte_n(static_cast<unsigned int>(pr.get<property::At::Comment>().size() + 1))) << pr.get<property::At::Comment>().data();
        }

        // DQT
        (ofps | srook::io::jpeg::bofstream::Byte) << MARKER::Marker << MARKER::DQT;
        (ofps | srook::io::jpeg::bofstream::Word) << 67;
        (ofps | srook::io::jpeg::bofstream::Byte) << 0;
        for (std::size_t i = 0; i < YQuantumTb.size(); ++i) {
            (ofps | srook::io::jpeg::bofstream::Byte) << static_cast<srook::byte>(YQuantumTb[ZZ[i]]);
        }
        (ofps | srook::io::jpeg::bofstream::Byte) << MARKER::Marker << MARKER::DQT;
        (ofps | srook::io::jpeg::bofstream::Word) << 67;
        (ofps | srook::io::jpeg::bofstream::Byte) << 1;
        for (std::size_t i = 0; i < CQuantumTb.size(); ++i) {
            (ofps | srook::io::jpeg::bofstream::Byte) << static_cast<srook::byte>(CQuantumTb[ZZ[i]]);
        }

        // DHT
        (ofps | srook::io::jpeg::bofstream::Bytes) << YDcDht;
        (ofps | srook::io::jpeg::bofstream::Bytes) << CDcDht;
        (ofps | srook::io::jpeg::bofstream::Bytes) << YAcDht;
        (ofps | srook::io::jpeg::bofstream::Bytes) << CAcDht;

        // Frame header
        (ofps | srook::io::jpeg::bofstream::Byte) << MARKER::Marker << MARKER::SOF0;
        (ofps | srook::io::jpeg::bofstream::Word) << (3 * pr.get<property::At::Dimension>() + 8);
        (ofps | srook::io::jpeg::bofstream::Byte) << pr.get<property::At::SamplePrecision>();
        (ofps | srook::io::jpeg::bofstream::Word) << pr.get<property::At::VSize>();
        (ofps | srook::io::jpeg::bofstream::Word) << pr.get<property::At::HSize>();
        (ofps | srook::io::jpeg::bofstream::Byte) << pr.get<property::At::Dimension>();

        (ofps | srook::io::jpeg::bofstream::Byte) << 0;
        (ofps | srook::io::jpeg::bofstream::Byte) << 0x22;
        (ofps | srook::io::jpeg::bofstream::Byte) << 0;
        for (std::size_t i = 1; i < 3; ++i) {
            (ofps | srook::io::jpeg::bofstream::Byte) << i;
            (ofps | srook::io::jpeg::bofstream::Byte) << 0x11;
            (ofps | srook::io::jpeg::bofstream::Byte) << 1;
        }

        // Scan Header
        (ofps | srook::io::jpeg::bofstream::Byte) << MARKER::Marker << MARKER::SOS;
        (ofps | srook::io::jpeg::bofstream::Word) << (2 * pr.get<property::At::Dimension>() + 6);
        (ofps | srook::io::jpeg::bofstream::Byte) << pr.get<property::At::Dimension>();
        for (int i = 0; i < pr.get<property::At::Dimension>(); ++i) {
            (ofps | srook::io::jpeg::bofstream::Byte) << i;
            (ofps | srook::io::jpeg::bofstream::Byte) << (i == 0 ? 0 : 0x11);
        }
        (ofps | srook::io::jpeg::bofstream::Byte) << 0;
        (ofps | srook::io::jpeg::bofstream::Byte) << 63;
        (ofps | srook::io::jpeg::bofstream::Byte) << 0;
    }

    srook::io::jpeg::bofstream& get_stream() SROOK_NOEXCEPT_TRUE
    {
        return ofps;
    }

    void write_eoi()
        SROOK_NOEXCEPT((ofps | srook::io::jpeg::bofstream::Byte) << MARKER::Marker << MARKER::EOI)
    {
        (ofps | srook::io::jpeg::bofstream::Byte) << MARKER::Marker << MARKER::EOI;
    }

    void output_file()
    {
        ofps.output_file();
    }

private:
    srook::io::jpeg::bofstream ofps;
    const property& pr;
};

} // namespace jpezy

#endif
