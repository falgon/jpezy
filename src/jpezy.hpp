/*
 *
 * JPEZY
 * Copyright (c) 2017 Roki
 *
 */
#ifndef INCLUDED_JPEZY_JPEG_HPP
#define INCLUDED_JPEZY_JPEG_HPP
#include<tuple>
#include<optional>
#include<chrono>
#include<array>

namespace jpezy{

using byte = unsigned char;

const std::array<int,64> ZZ{
	0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

enum class MARKER {
    	SOF0=0xc0,SOF1=0xc1,SOF2=0xc2,SOF3=0xc3,
    	SOF5=0xc5,SOF6=0xc6,SOF7=0xc7,

    	JPG=0xc8,

    	SOF9=0xc9,SOF10=0xca,SOF11=0xcb,SOF13=0xcd,
    	SOF14=0xce,SOF15=0xcf,

    	DHT=0xc4,

    	DAC=0xcc,

    	RST0=0xd0,RST1=0xd1,RST2=0xd2,RST3=0xd3,
    	RST4=0xd4,RST5=0xd5,RST6=0xd6,RST7=0xd7,

    	SOI=0xd8,EOI=0xd9,SOS=0xda,DQT=0xdb,DNL=0xdc,
    	DRI=0xdd,DHP=0xde,EXP=0xdf,COM=0xfe,

    	APP0=0xe0,APP1=0xe1,APP2=0xe2,APP3=0xe3,
    	APP4=0xe4,APP5=0xe5,APP6=0xe6,APP7=0xe7,
    	APP8= 0xe8,APP9=0xe9,APP10=0xea,APP11=0xeb,
    	APP12=0xec,APP13=0xed,APP14=0xee,APP15=0xef,

    	JPG0=0xf0,JPG1=0xf1,JPG2=0xf2,JPG3=0xf3,
    	JPG4=0xf4,JPG5=0xf5,JPG6=0xf6,JPG7=0xf7,
    	JPG8=0xf8,JPG9=0xf9,JPG10=0xfa,JPG11=0xfb,
    	JPG12=0xfc,JPG13=0xfd,

    	TEM=0x01,RESst=0x02,RESnd=0xbf,

    	Error=0xff,Marker=0xff
};

// ISO/IEC 10918-1:1993(E)
// Table K.1 - Luminance quantization table
static const std::array<int,64> YQuantumTb{
	16,11,10,16,24,40,51,61,
	12,12,14,19,26,58,60,55,
	14,13,16,24,40,57,69,56,
	14,17,22,29,51,87,80,62,
	18,22,37,56,68,109,103,77,
	24,35,55,64,81,104,113,92,
	49,64,78,87,103,121,120,101,
	72,92,95,98,112,100,103,99
};

// Table K.2 - Chrominance quantization table
 static const std::array<int,64> CQuantumTb{
	17,18,24,47,99,99,99,99,
	18,21,26,66,99,99,99,99,
	24,26,56,99,99,99,99,99,
	47,66,99,99,99,99,99,99,
	99,99,99,99,99,99,99,99,
	99,99,99,99,99,99,99,99,
	99,99,99,99,99,99,99,99,
	99,99,99,99,99,99,99,99
};

struct property{
	enum class Format{
		undefined,JFIF,JFXX
	};
	enum class Units{
		undefined,dots_inch,dots_cm
	};
	enum class ExtensionCodes{
		undefined = 0,
		JPEG = 0x10,
		oneByte_pixel = 0x11,
		threeByte_pixel = 0x13
	};
	enum class At{
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
		ELEMENT_SIZE
	};

	using property_type =
		std::tuple<
			int, // src image width
			int, // src images height
			int, // dimension
			int, // sample precision
			const char*, // comment
			Format, // format
			byte, // Major Revisions
			byte, // Minor Revisions
			Units, // units
			int, // width density
			int, // height density
			int, // width thumbnail
			int, // height thumbnail
			ExtensionCodes // Extension Code
		>;

	template<
		class... Ts,
		std::enable_if_t<sizeof...(Ts) == std::tuple_size<property_type>::value,std::nullptr_t> =nullptr>
	constexpr property(Ts&&... ts):pr{std::forward<Ts>(ts)...}
	{
		static_assert(static_cast<std::size_t>(At::ELEMENT_SIZE) == std::tuple_size<property_type>::value);
	}

	template<At at>
	constexpr decltype(auto) get()const noexcept
	{
		return std::get<static_cast<std::size_t>(at)>(pr);
	}
private:
	property_type pr;
};


 struct raii_messenger{
	      raii_messenger(const char* message):mes(message),stoped(false)
		  {
			  std::cout << mes << " ";
			  start = std::chrono::system_clock::now();
		  }

		  void restart(const char* str=nullptr)
		  {
			  if(stoped){
				  if(str){
					  std::cout << str << std::endl;
				  }else{
				  	std::cout << mes << " ";
				  }
				  start = std::chrono::system_clock::now();
				  stoped = false;
			  }
		  }

		  std::optional<float> stop()
		  {
			  if(!stoped){
				  end = std::chrono::system_clock::now();
				  const float time = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count()) / 1000 ;
				  std::cout << "Done! Processing time: " << time << "(sec)" << std::endl;
				  stoped = true;
				  return std::make_optional(time);
			  }
			  return std::nullopt;
		  }

		  ~raii_messenger()
		  {
			  stop();
		  }
private:
		  std::chrono::system_clock::time_point start,end;
		  const char* mes;
		  bool stoped;
};


} // namespace jpezy

#endif
