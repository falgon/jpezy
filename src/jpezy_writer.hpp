/*
 *
 * JPEZY
 * Copyright (c) 2017 Roki
 *
 */

#ifndef INCLUDED_JPEG_WRITER_HPP
#define INCLUDED_JPEG_WRITER_HPP

#include"ofpstream.hpp"
#include"huffman_table.hpp"
#include"noexcept_detection.hpp"
#include<fstream>
#include<string_view>
#include<type_traits>
#include<array>
#include<cstring>

namespace jpezy{

struct jpezy_writer{
	jpezy_writer(const std::size_t buffer_size,const property& pr,const char* output_name)
		:ofps(buffer_size,output_name,std::ios::out | std::ios::trunc | std::ios::binary),pr(pr){}

	void write_header()noexcept(false)
	{
		if(!ofps)throw std::runtime_error(__func__);

		// SOI
		(ofps | ofpstream::Byte) << MARKER::Marker << MARKER::SOI;

		// JFIF
		(ofps | ofpstream::Byte) << MARKER::Marker << MARKER::APP0;
		(ofps | ofpstream::Word) << 16;
		(ofps | ofpstream::Byte_n(5)) << "JFIF";
		(ofps | ofpstream::Word) << 0x0102;
		(ofps | ofpstream::Byte) << pr.get<property::At::Units>();
		(ofps | ofpstream::Word) << pr.get<property::At::HDensity>();
		(ofps | ofpstream::Word) << pr.get<property::At::VDensity>();
		(ofps | ofpstream::Byte) << pr.get<property::At::HThumbnail>();
		(ofps | ofpstream::Byte) << pr.get<property::At::VThumbnail>();

		// Comment
		if(pr.get<property::At::Comment>()){
			(ofps | ofpstream::Byte) << MARKER::Marker << MARKER::COM;
			(ofps | ofpstream::Word) << std::strlen(pr.get<property::At::Comment>()) + 3;
			(ofps | ofpstream::Byte_n(std::strlen(pr.get<property::At::Comment>()) + 1)) << pr.get<property::At::Comment>();
		}

		// DQT
		(ofps | ofpstream::Byte) << MARKER::Marker << MARKER::DQT;
		(ofps | ofpstream::Word) << 67;
		(ofps | ofpstream::Byte) << 0;
		for(std::size_t i=0; i<YQuantumTb.size(); ++i){
			(ofps | ofpstream::Byte) << static_cast<byte>( YQuantumTb[ ZZ[i] ] );
		}
		(ofps | ofpstream::Byte) << MARKER::Marker << MARKER::DQT;
		(ofps | ofpstream::Word) << 67;
		(ofps | ofpstream::Byte) << 1;
		for(std::size_t i=0; i<CQuantumTb.size(); ++i){
			(ofps | ofpstream::Byte) << static_cast<byte>( CQuantumTb[ ZZ[i] ] );
		}

		// DHT
		(ofps | ofpstream::Bytes) << YDcDht;
		(ofps | ofpstream::Bytes) << CDcDht;
		(ofps | ofpstream::Bytes) << YAcDht;
		(ofps | ofpstream::Bytes) << CAcDht;

		// Frame header
		(ofps | ofpstream::Byte) << MARKER::Marker << MARKER::SOF0;
		(ofps | ofpstream::Word) << (3 * pr.get<property::At::Dimension>() + 8);
		(ofps | ofpstream::Byte) << pr.get<property::At::SamplePrecision>();
		(ofps | ofpstream::Word) << pr.get<property::At::VSize>();
		(ofps | ofpstream::Word) << pr.get<property::At::HSize>();
		(ofps | ofpstream::Byte) << pr.get<property::At::Dimension>();

		(ofps | ofpstream::Byte) << 0;
		(ofps | ofpstream::Byte) << 0x22;
		(ofps | ofpstream::Byte) << 0;
		for(std::size_t i=1; i<3; ++i){
			(ofps | ofpstream::Byte) << i;
			(ofps | ofpstream::Byte) << 0x11;
			(ofps | ofpstream::Byte) << 1;
		}

		// Scan Header
		(ofps | ofpstream::Byte) << MARKER::Marker << MARKER::SOS;
		(ofps | ofpstream::Word) << (2 * pr.get<property::At::Dimension>() + 6);
		(ofps | ofpstream::Byte) << pr.get<property::At::Dimension>();
		for(int i=0; i<pr.get<property::At::Dimension>(); ++i){
			(ofps | ofpstream::Byte) << i;
			(ofps | ofpstream::Byte) << (i == 0 ? 0 : 0x11);
		}
		(ofps | ofpstream::Byte) << 0;
		(ofps | ofpstream::Byte) << 63;
		(ofps | ofpstream::Byte) << 0;
	}

	ofpstream& get_stream()noexcept
	{
		return ofps;
	}

	void write_eoi()
	JPEZY_NOEXCEPT((ofps | ofpstream::Byte) << MARKER::Marker << MARKER::EOI)
	{
		(ofps | ofpstream::Byte) << MARKER::Marker << MARKER::EOI;
	}

	void output_file()noexcept(false)
	{
		ofps.output_file();
	}
private:
	ofpstream ofps;
	const property& pr;
};

} // namespace jpezy

#endif
