// Copyright (C) 2017 roki
#ifndef INCLUDED_JPEZY_DECODER_HPP
#define INCLUDED_JPEZY_DECODER_HPP

#include<type_traits>
#include<iostream>
#include<array>
#include<vector>
#include<cassert>
#include<cmath>
#include<string_view>

#include<boost/range/algorithm/generate.hpp>
#include<boost/range/algorithm/copy.hpp>

#include<srook/iterator/ostream_joiner.hpp>
#include<srook/math/constants/pi.hpp>
#include<srook/algorithm/for_each.hpp>
#include<srook/io/bifstream.hpp>
#include<srook/optional.hpp>

#include"../jpezy.hpp"
#include"tables.hpp"

namespace jpezy{

template<class BuildMode = Release>
struct decoder{
	explicit decoder(const char* filename)
		:pr{
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
					label::_decodable = property::AnalyzedResult::Yet
				)
			)
		},restart_interval{0},rgb{},comp{},pred_dct{},dct{},block{},ht{},qt{},cos_table{},bifs{filename},hmax{},vmax{},enable{false}
	{
		std::unique_ptr<raii_messenger> mes;
		if constexpr(std::is_same<BuildMode,Debug>())mes = std::make_unique<raii_messenger>("make a cos table...");

		srook::for_each(
				srook::make_counter(cos_table),
				[](auto& x,std::size_t n){
					srook::for_each(
							srook::make_counter(x),
							[&n](auto& y,std::size_t m){y = std::cos(double(2 * m + 1) * double(n) * (srook::math::pi<double> / (block_size * 2)));}
					);
				}
		);
	}

	static constexpr bool is_release_mode = std::is_same<Release,BuildMode>();

	template<class MODE_TAG = COLOR_MODE>
	srook::optional<std::array<std::vector<srook::byte>,3>> decode()
	{
		raii_messenger mes("process started...");
		std::cout << '\n';

		try{
			analyze_header();
		}catch(const std::runtime_error& er){
			return {};
		}

		disp_info("\t");
		if(!(pr.get<property::At::Decodable>() & (property::AnalyzedResult::is_htable | property::AnalyzedResult::is_qtable | property::AnalyzedResult::is_start_data)))return {};
		std::unique_ptr<raii_messenger> mes_dec;
		if constexpr(std::is_same<BuildMode,Debug>())mes_dec = std::make_unique<raii_messenger>("decoding started...","\t");
		

		const std::size_t Vblock = get_blocks(pr.get<property::At::VSize>());
		const std::size_t Hblock = get_blocks(pr.get<property::At::HSize>());

		const std::size_t h_unit = (Hblock / hmax) + ((Hblock % hmax) ? 1 : 0);
		const std::size_t v_unit = (Vblock / vmax) + ((Vblock % vmax) ? 1 : 0);
		const std::size_t rgb_s = (v_unit * vmax) * block_size * (h_unit * hmax) * block_size;

		for(auto&& v:rgb)v.resize(rgb_s);

		const std::size_t unit_size = hmax * vmax * blocks_size;
		comp[0].resize(unit_size);  comp[1].resize(unit_size,0x80); comp[2].resize(unit_size,0x80);

		for(std::size_t uy = 0,restart_counter = 0; uy < v_unit; ++uy){
			for(std::size_t ux = 0; ux < h_unit; ++ux){
				try{
					decode_mcu();
				}catch(const std::runtime_error& er){
					std::cerr << "decode_mcu(): throw exception from " << er.what() << std::endl;
					return {};
				}

				try{
					make_rgb<MODE_TAG>(ux,uy);
				}catch(const std::runtime_error& er){
					std::cerr << "make_rgb(): throw exception from " << er.what() << std::endl;
					return {};
				}

				try{
					restart_interval_check(restart_counter);
				}catch(const std::runtime_error& er){
					std::cerr << "restart_interval_check(): throw exception from " << er.what() << std::endl;
				}catch(const std::out_of_range& er){
					std::cerr << "restart_interval_check(): throw exception from " << er.what() << std::endl;
				}
			}
		}

		for(auto&& v:comp)v.clear();

		return {rgb};
	}

	property pr;
private:
	inline void disp_info(const char* indent="")
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

	void restart_interval_check(std::size_t& restart_counter)
	SROOK_NOEXCEPT(get_marker())
	{
		if(restart_interval){
			if(++restart_counter >= restart_interval){			
				restart_counter = 0;
				const MARKER mark = get_marker();
				if(mark >= MARKER::RST0 and mark <= MARKER::RST7)pred_dct[0] = pred_dct[1] = pred_dct[2] = 0;
			}
		}
	}

	template<class T>
	static constexpr T get_blocks(T x)
	SROOK_NOEXCEPT(((x >> 3) + ((x & 0x07) > 0)))
	{
		return (x >> 3) + ((x & 0x07) > 0);
	}

	void analyze_header()
	SROOK_NOEXCEPT(noexcept(get_marker()) and noexcept(analyze_marker()))
	{
		std::unique_ptr<raii_messenger> mes;
		if constexpr(std::is_same<BuildMode,Debug>())mes = std::make_unique<raii_messenger>("analyzing header...","\t");

		do{
			if(get_marker() == MARKER::SOI)enable = true;
		}while(!enable);

		while(enable){
			pr.get<property::At::Decodable>() |= analyze_marker();

			if(pr.get<property::At::Decodable>() & property::AnalyzedResult::is_start_data)return;
		}
		throw std::runtime_error(__func__);
	}

	void analyze_dht(std::size_t size)
	{
		std::unique_ptr<raii_messenger> mes;
		if constexpr(std::is_same<BuildMode,Debug>())mes = std::make_unique<raii_messenger>("analyzing DHT...","\t\t\t");

		const srook::byte* end_add = bifs.next_address();
		if(!end_add)throw std::out_of_range(__func__);
		end_add += size;

		do{
			std::underlying_type_t<srook::byte> uc{};
			(bifs | srook::bifstream::Byte) >> uc;
			
			const std::size_t tc = uc >> 4,th = uc & 0x0f;
			
			if(tc > 1){throw std::runtime_error("DC format error");} if(th > 3){throw std::runtime_error("AC format error");}
			
			huffman_table& ht_ = ht[tc][th];

			std::array<std::underlying_type_t<srook::byte>,block_size * 2> cc{};
			std::size_t n_ = 0;
			boost::generate(cc,[this,&n_](){std::underlying_type_t<srook::byte> b{}; (bifs | srook::bifstream::Byte) >> b; n_ += b; return b;});
			
			if constexpr(std::is_same<BuildMode,Debug>())std::cout << " size: " << n_ << std::endl;
			const std::size_t& n = n_;
			
			ht_.sizeTP.clear(); ht_.codeTP.clear(); ht_.valueTP.clear();
			ht_.sizeTP.resize(n); ht_.codeTP.resize(n); ht_.valueTP.resize(n);

			for(std::size_t i = 1,k = 0; i <= (block_size * 2); ++i){
				for(std::size_t j = 1; j <= cc[i-1]; ++j,++k){
					if(k >= n)throw std::out_of_range("invalid size table");
					ht_.sizeTP[k] = i;
				}
			}

			for(auto [k,code,si] = std::make_tuple(0u,0,ht_.sizeTP[0]);;){
				for(; ht_.sizeTP[k] == si; ++k,++code)ht_.codeTP[k] = code;
				if(k >= n)break;
				do{
					code <<= 1;
					++si;
				}while(ht_.sizeTP[k] != si);
			}
			for(std::size_t k = 0; k < n; ++k)(bifs | srook::bifstream::Byte) >> ht_.valueTP[k];
			

			if constexpr(std::is_same<BuildMode,Debug>()){
				switch(n){
					case 12:{
						std::cout << "found DC Huffman Table... ";
						break;
					}
					case 162:
						std::cout << "found AC Huffman Table... ";
						break;
					default:
						std::cerr << n_ << std::endl;
						throw std::out_of_range("invalid size table");
				}
			}
		}while(bifs.next_address() < end_add);
	}

	void analyze_dqt(std::size_t size)
	{
		std::unique_ptr<raii_messenger> mes;
		if constexpr(std::is_same<BuildMode,Debug>())mes = std::make_unique<raii_messenger>("\t\t\tanalyzing DQT...");

		const srook::byte* const end_add = bifs.next_address() + size;
		std::underlying_type_t<srook::byte> c{};
		do{
			(bifs | srook::bifstream::Byte) >> c;
			
			if(typename decltype(qt)::value_type::iterator iter = std::begin(qt[c & 0x3]); !(c >> 4)){
				std::underlying_type_t<srook::byte> t{};
				for(std::size_t i = 0; i < blocks_size; ++i){
					(bifs | srook::bifstream::Byte) >> t;
					iter[ZZ[i]] = int(t);
				}
			}else{
				for(std::size_t i = 0; i < blocks_size; ++i)(bifs | srook::bifstream::Word) >> iter[ZZ[i]];
			}
		}while(bifs.next_address() < end_add);
	}

	void analyze_frame()
	{
		std::unique_ptr<raii_messenger> mes;
		if constexpr(std::is_same<BuildMode,Debug>())mes = std::make_unique<raii_messenger>("\t\t\tanalyzing frames...");

		using namespace srook::literals::byte_literals;

		(bifs | srook::bifstream::Byte) >> pr.get<property::At::SamplePrecision>();
		(bifs | srook::bifstream::Word) >> pr.get<property::At::VSize>();
		(bifs | srook::bifstream::Word) >> pr.get<property::At::HSize>();
		(bifs | srook::bifstream::Byte) >> pr.get<property::At::Dimension>();
		if(pr.get<property::At::Dimension>() != 3 and pr.get<property::At::Dimension>() != 1)throw std::runtime_error("Sorry, this dimension size is not supported");

		if constexpr(std::is_same<BuildMode,Debug>()){
			std::cout << "VSize: " << pr.get<property::At::VSize>() << " HSize: " << pr.get<property::At::HSize>() << " ";
		}

		for(auto [i,c] = std::make_tuple(0u,0_byte); i < std::size_t(pr.get<property::At::Dimension>()); ++i){
			(bifs | srook::bifstream::Byte) >> fcomp[i].C;
			(bifs | srook::bifstream::Byte) >> c;

			fcomp[i].H = typename get_character<srook::byte>::type(c >> 4);
			if(fcomp[i].H > hmax)hmax = fcomp[i].H;
			fcomp[i].V = (std::underlying_type_t<srook::byte>(c) & 0xf);
			if(fcomp[i].V > vmax)vmax = fcomp[i].V;

			(bifs | srook::bifstream::Byte) >> fcomp[i].Tq;
		}
	}

	void analyze_scan()
	{
		std::unique_ptr<raii_messenger> mes;
		if constexpr(std::is_same<BuildMode,Debug>())mes = std::make_unique<raii_messenger>("\t\t\tanalyzing scan data...");

		srook::byte c{};
		(bifs | srook::bifstream::Byte) >> s_header.scan_comp;

		for(std::size_t i = 0; i < srook::to_integer<std::size_t>(s_header.scan_comp); ++i){
			(bifs | srook::bifstream::Byte) >> scomp[i].Cs;
			(bifs | srook::bifstream::Byte) >> c;

			scomp[i].Td = srook::to_integer<std::underlying_type_t<srook::byte>>(c >> 4);
			if(scomp[i].Td > 2)throw std::out_of_range(__func__);
			
			scomp[i].Ta = (srook::to_integer<std::underlying_type_t<srook::byte>>(c) & 0xf);
			if(scomp[i].Ta > 2)throw std::out_of_range(__func__);
		}

		// unused for DCT
		{
			(bifs | srook::bifstream::Byte) >> s_header.spectral_begin;
			(bifs | srook::bifstream::Byte) >> s_header.spectral_end;
			(bifs | srook::bifstream::Byte) >> c;
			s_header.Ah = srook::to_integer<typename get_character<srook::byte>::type>(c >> 4);
			s_header.Al = srook::to_integer<std::underlying_type_t<srook::byte>>(c) & 0xf;
		}
	}

	void analyze_jfif()
	{
		std::unique_ptr<raii_messenger> mes;
		if constexpr(std::is_same<BuildMode,Debug>())mes = std::make_unique<raii_messenger>("\t\t\tanalyzing jfif...");

		pr.get<property::At::Format>() = property::Format::JFIF;
		(bifs | srook::bifstream::Byte) >> pr.get<property::At::MajorRevisions>();
		(bifs | srook::bifstream::Byte) >> pr.get<property::At::MinorRevisions>();
		(bifs | srook::bifstream::Byte) >> pr.get<property::At::Units>();
		(bifs | srook::bifstream::Word) >> pr.get<property::At::HDensity>();
		(bifs | srook::bifstream::Word) >> pr.get<property::At::VDensity>();
		(bifs | srook::bifstream::Byte) >> pr.get<property::At::HThumbnail>();
		(bifs | srook::bifstream::Byte) >> pr.get<property::At::VThumbnail>();
		pr.get<property::At::Decodable>() |= property::AnalyzedResult::is_jfif;
	}

	void analyze_jfxx()
	{
		std::unique_ptr<raii_messenger> mes;
		if constexpr(std::is_same<BuildMode,Debug>())mes = std::make_unique<raii_messenger>("\t\t\tanalyzing jfxx...");

		pr.get<property::At::Format>() = property::Format::JFXX;
		(bifs | srook::bifstream::Byte) >> pr.get<property::At::ExtensionCode>();
	}

	int analyze_marker()
	{
		int length{};
		MARKER mark = get_marker();
		const auto param_offset = [](int& len_){len_ -= 2;};

		switch(mark){
			case MARKER::SOF0:
				if constexpr(std::is_same<BuildMode,Debug>())std::cout << "\t\tfound marker: [SOF0]" << std::endl;
				(bifs | srook::bifstream::Word) >> length;
				analyze_frame();
				break;
			case MARKER::DHT:
				if constexpr(std::is_same<BuildMode,Debug>())std::cout << "\t\tfound marker: [DHT]" << std::endl;
				(bifs | srook::bifstream::Word) >> length; param_offset(length);
				analyze_dht(length);
				return property::AnalyzedResult::is_htable;
			case MARKER::DNL:
				if constexpr(std::is_same<BuildMode,Debug>())std::cout << "\t\tfound marker: [DNL]" << std::endl;
				(bifs | srook::bifstream::Word) >> length;
				(bifs | srook::bifstream::Word) >> pr.get<property::At::VSize>();
				break;
			case MARKER::DQT:
				if constexpr(std::is_same<BuildMode,Debug>())std::cout << "\t\tfound marker: [DQT]" << std::endl;
				(bifs | srook::bifstream::Word) >> length; param_offset(length);
				analyze_dqt(length);
				return property::AnalyzedResult::is_qtable;
			case MARKER::EOI:
				if constexpr(std::is_same<BuildMode,Debug>())std::cout << "\t\tfound marker: [EOI]" << std::endl;
				enable = false;
				break;
			case MARKER::SOS:
				if constexpr(std::is_same<BuildMode,Debug>())std::cout << "\t\tfound marker: [SOS]" << std::endl;
				(bifs | srook::bifstream::Word) >> length;
				analyze_scan();
				return property::AnalyzedResult::is_start_data;
			case MARKER::DRI:
				if constexpr(std::is_same<BuildMode,Debug>())std::cout << "\t\tfound marker: [DRI]" << std::endl;
				(bifs | srook::bifstream::Word) >> length;
				(bifs | srook::bifstream::Word) >> restart_interval;
				break;
			case MARKER::COM:
				if constexpr(std::is_same<BuildMode,Debug>())std::cout << "\t\tfound marker: [COM]" << std::endl;
				(bifs | srook::bifstream::Word) >> length; param_offset(length);
				(bifs | srook::bifstream::Byte_n(length)) >> pr.get<property::At::Comment>();
				return property::AnalyzedResult::is_comment;
			
			// Not supported frames
			case MARKER::SOF1: [[fallthrough]]; case MARKER::SOF2: [[fallthrough]]; case MARKER::SOF3: [[fallthrough]];
			case MARKER::SOF5: [[fallthrough]]; case MARKER::SOF6: [[fallthrough]]; case MARKER::SOF7: [[fallthrough]];
			case MARKER::SOF9: [[fallthrough]]; case MARKER::SOF10: [[fallthrough]]; case MARKER::SOF11: [[fallthrough]];
			case MARKER::SOF13: [[fallthrough]]; case MARKER::SOF14: [[fallthrough]]; case MARKER::SOF15: [[fallthrough]];
			case MARKER::EXP: [[fallthrough]]; case MARKER::DAC: [[fallthrough]]; case MARKER::DHP: 
				std::runtime_error("Not supported");
				break;
			
			case MARKER::APP0:{
				if constexpr(std::is_same<BuildMode,Debug>())std::cout << "\n\t\tfound marker: [APP0]" << std::endl;
				(bifs | srook::bifstream::Word) >> length; param_offset(length);

				if(static constexpr char JFIF[]="JFIF",JFXX[]="JFXX"; std::string_view(JFIF).size() == std::string_view(JFXX).size()){
					if(const std::size_t size = std::string_view(JFIF).size(); length >= signed(size)){
						std::string id;
						id.resize(size + 1);
						(bifs | srook::bifstream::Bytes) >> id;
						id.pop_back();

						if(id == JFIF){
							analyze_jfif();
							bifs.skip_byte(length - 14);
						}else if(JFXX == id){
							analyze_jfxx();
							bifs.skip_byte(length - 1);
						}else{
							bifs.skip_byte(length - size);
						}
					}else{
						bifs.skip_byte(length);
					}
				}
				break;
			}

			// Not supported markers
			case MARKER::APP1: [[fallthrough]]; case MARKER::APP2: [[fallthrough]]; case MARKER::APP3: [[fallthrough]];
			case MARKER::APP4: [[fallthrough]]; case MARKER::APP5: [[fallthrough]]; case MARKER::APP6: [[fallthrough]];
			case MARKER::APP7: [[fallthrough]]; case MARKER::APP8: [[fallthrough]]; case MARKER::APP9: [[fallthrough]];
			case MARKER::APP10: [[fallthrough]]; case MARKER::APP11: [[fallthrough]]; case MARKER::APP12: [[fallthrough]];
			case MARKER::APP13: [[fallthrough]]; case MARKER::APP14: [[fallthrough]]; case MARKER::APP15:
				(bifs | srook::bifstream::Word) >> length; param_offset(length);
				bifs.skip_byte(length);
				break;

			// reserves and other
			case MARKER::JPG: [[fallthrough]]; case MARKER::JPG0: [[fallthrough]]; case MARKER::JPG1: [[fallthrough]];
			case MARKER::JPG2: [[fallthrough]]; case MARKER::JPG3: [[fallthrough]]; case MARKER::JPG4: [[fallthrough]];
			case MARKER::JPG5: [[fallthrough]]; case MARKER::JPG6: [[fallthrough]]; case MARKER::JPG7: [[fallthrough]];
			case MARKER::JPG8: [[fallthrough]]; case MARKER::JPG9: [[fallthrough]]; case MARKER::JPG10: [[fallthrough]];
			case MARKER::JPG11: [[fallthrough]]; case MARKER::JPG12: [[fallthrough]]; case MARKER::JPG13: [[fallthrough]]; 
			case MARKER::TEM: [[fallthrough]]; case MARKER::Error: [[fallthrough]]; case MARKER::RST0: [[fallthrough]];
			case MARKER::RST1: [[fallthrough]]; case MARKER::RST2: [[fallthrough]]; case MARKER::RST3: [[fallthrough]];
			case MARKER::RST4: [[fallthrough]]; case MARKER::RST5: [[fallthrough]]; case MARKER::RST6: [[fallthrough]];
			case MARKER::RST7: [[fallthrough]]; case MARKER::SOI: [[fallthrough]]; case MARKER::RESst: [[fallthrough]];
			case MARKER::RESnd: [[fallthrough]]; default:
				throw std::runtime_error("Marker error");
				break;
		}
		return property::AnalyzedResult::Yet;
	}

	MARKER get_marker()
	{
		for(std::underlying_type_t<srook::byte> c{};;){
			(bifs | srook::bifstream::Byte) >> c;
			if(c == std::underlying_type_t<srook::byte>(MARKER::Marker)){
				(bifs | srook::bifstream::Byte) >> c;
				if(c){
					if((c > std::underlying_type_t<srook::byte>(MARKER::RESst)) and (c < std::underlying_type_t<srook::byte>(MARKER::SOF0))){
						return MARKER::Error;
					}else{
						return MARKER(c);
					}
				}
			}
		}
		return MARKER::Error;
	}

	void decode_mcu()
	{
		for(std::size_t sc = 0; sc < std::size_t(pr.get<property::At::Dimension>()); ++sc){
			const std::size_t num_v = fcomp[sc].V;
			const std::size_t num_h = fcomp[sc].H;
			const std::size_t dupc_y = vmax / num_v;
			const std::size_t dupc_x = hmax / num_h;
			const std::size_t v_step = hmax * block_size;

			for(std::size_t ky = 0; ky < num_v; ++ky){
				for(std::size_t kx = 0; kx < num_h; ++kx){
					decode_huffman(sc);
					inverse_quantization(sc);
					inverse_dct();

					typename std::remove_reference_t<decltype(comp)>::value_type::iterator tp = std::next(std::begin(comp[sc]),ky * v_step * block_size + kx * block_size); 
					for(std::size_t y_u = 0; y_u < block_size * dupc_y; ++y_u){
						for(std::size_t x_u = 0; x_u < block_size * dupc_x; ++x_u){
							tp[y_u * v_step + x_u] = block[(y_u / dupc_y) * block_size + (x_u / dupc_x)];
						}
					}
				}
			}
		}
	}


	template<class MODE_TAG>
	void make_rgb(const std::size_t ux,const std::size_t uy)noexcept
	{
		typename std::remove_reference_t<decltype(comp)>::value_type::iterator yp = std::begin(comp[0]),up = std::begin(comp[1]),vp = std::begin(comp[2]);
		const std::size_t line = uy * vmax * block_size;
		const std::size_t offset_v = line * pr.get<property::At::HSize>();
		const std::size_t offset_h = ux * hmax * block_size;
		const std::size_t offset = offset_v + offset_h;

		std::vector<srook::byte>::iterator rp = std::next(std::begin(rgb[0]),offset),gp = std::next(std::begin(rgb[1]),offset),bp = std::next(std::begin(rgb[2]),offset);
		const std::size_t end_x = (hmax * block_size);
		const std::size_t end_y = (vmax * block_size);

		for(std::size_t pic_y = 0; pic_y < end_y; ++pic_y){
			for(std::size_t pic_x = 0; pic_x < end_x; ++pic_x){
				if(pic_x + offset_h >= pr.get<property::At::HSize>()){
					yp += end_x - pic_x;
					up += end_x - pic_x;
					vp += end_x - pic_x;
					break;
				}

				if constexpr(const std::size_t index = pic_y * pr.get<property::At::HSize>() + pic_x; std::is_same<MODE_TAG,COLOR_MODE>()){
					rp[index] = revise_value(to_r(*yp,*vp));
					gp[index] = revise_value(to_g(*yp,*up,*vp));
					bp[index] = revise_value(to_b(*yp,*up));
					++yp; ++vp; ++up;
				}else{
					gp[index] = bp[index] = rp[index] = revise_value(*yp++);
				}
			}
		}
	}

	constexpr double to_r(const double yp,const double vp)const noexcept
	{
		return yp + (vp - 0x80) * 1.4020;
	}
	constexpr double to_g(const double yp,const double up,const double vp)const noexcept
	{
		return yp - (up - 0x80) * 0.3441 - (vp - 0x80) * 0.7139;
	}
	constexpr double to_b(const double yp,const double up)const noexcept
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

		if(category > 0){
			(bifs | srook::bifstream::Bits(category)) >> dc_diff;
			if( ( dc_diff & (1 << (category - 1) )) == 0 ){
				dc_diff -= (1 << category) - 1;
			}
		}else if(category < 0){
			throw std::runtime_error(__func__);
		}
		pred_dct[sc] += dc_diff;
		dct[0] = pred_dct[sc];

		// AC
		for(std::size_t k = 1; k < blocks_size;){
			category = decode_huffman_impl<AC>(sc);
			if(!category){
				for(; k < blocks_size; ++k)dct[ZZ[k]] = 0;
				break;
			}else if(category < 0){
				throw std::runtime_error(__func__);
			}

			int run = category >> 4;
			category &= 0x0f;

			int acv = 0;
			if(category){
				(bifs | srook::bifstream::Bits(category)) >> acv;
				if(!(acv & (1 << (category - 1)))){
					acv -= (1 << category) - 1;
				}
			}else if(run != 15){
				std::runtime_error(__func__);
			}

			if((run + k) > int(blocks_size - 1))throw std::runtime_error(__func__);
			
			for(; run-- > 0; ++k)dct[ZZ[k]] = 0;
			dct[ZZ[k++]] = acv;
		}
	}

	template<class TC>
	int decode_huffman_impl(std::size_t sc)
	{
		static_assert(std::is_same<TC,DC>() or std::is_same<TC,AC>());
		huffman_table& ht_ = ht[std::is_same<TC,AC>()][scomp[sc].Td];
		
		for(auto [code,length,next,k] = std::make_tuple(0,0u,0,0u); k < ht_.size() and length < (block_size * 2);){
			++length;
			code <<= 1;
			(bifs | srook::bifstream::Bits(1)) >> next;
			if(next < 0)return next;
			code |= next;
			for(;ht_.sizeTP[k] == length; ++k){
				if(ht_.codeTP[k] == code)return ht_.valueTP[k];
			}
		}
		throw std::runtime_error(__func__);
	}

	void inverse_quantization(std::size_t sc)noexcept
	{
		typename std::remove_reference_t<decltype(dct)>::iterator p = std::begin(dct);
		typename std::remove_reference_t<decltype(qt)>::value_type::const_iterator iter = std::begin(qt[fcomp[sc].Tq]);
		
		for(std::size_t i = 0; i < blocks_size; ++i)*p++ *= *iter++;
	}

	void inverse_dct()noexcept
	{
		const int sl = pr.get<property::At::SamplePrecision>() == block_size ? 128 : 2048;
		const double disqrt2 = (1.0 / std::sqrt(2));
		
		for(std::size_t y = 0; y < block_size; ++y){
			for(std::size_t x = 0; x < block_size; ++x){
				double sum = 0;

				for(std::size_t v = 0; v < block_size; ++v){
					const double cv = (!v) ? disqrt2 : 1.0;
					for(std::size_t u = 0; u < block_size; ++u){
						const double cu = (!u) ? disqrt2 : 1.0;
						sum += cu * cv * dct[v * block_size + u] * cos_table[u][x] * cos_table[v][y];
					}
				}
				block[y * block_size + x] = int(sum / 4 + sl);
			}
		}
	}

	constexpr srook::byte revise_value(double v)const noexcept
	{
		using namespace srook::literals::byte_literals;
		return (v < 0.0) ? 0_byte : (v > 255.0) ? 255_byte : srook::byte(v);
	}
private:
	static constexpr std::size_t rgb_size = 3;
	static constexpr std::size_t block_size = 8;
	static constexpr std::size_t blocks_size = block_size * block_size;
	static constexpr std::size_t mcu_size = 4;
	
	Scan_header s_header;
	std::array<Frame_component,3> fcomp;
	std::array<Scan_component,3> scomp;

	std::size_t restart_interval;
	std::array<std::vector<srook::byte>,rgb_size> rgb;
	std::array<std::vector<int>,rgb_size> comp;
	std::array<int,rgb_size> pred_dct;
	std::array<int,blocks_size> dct;
	std::array<int,blocks_size> block;

	std::array<std::array<huffman_table,4>,2> ht;
	std::array<std::array<int,blocks_size>,mcu_size> qt;
	std::array<std::array<double,block_size>,block_size> cos_table;

	srook::bifstream bifs;
	std::make_signed_t<std::underlying_type_t<srook::byte>> hmax,vmax;
	bool enable;
};

} // namespace jpezy
#endif
