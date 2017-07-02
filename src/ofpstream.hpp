/*
 *
 * JPEZY
 * Copyright (c) 2017 Roki
 *
 */

#ifndef INCLUDED_JPEZY_OFSTREAM_BIT_HPP
#define INCLUDED_JPEZY_OFSTREAM_BIT_HPP

#include"jpezy.hpp"
#include"noexcept_detection.hpp"
#include<fstream>
#include<string_view>
#include<type_traits>
#include<array>
#include<memory>
#include<srook/type_traits/has_iterator.hpp>

namespace jpezy{

template<class T>
struct is_not_libarray:std::true_type{};
template<class T,std::size_t v>
struct is_not_libarray<std::array<T,v>>:std::false_type{};

struct ofpstream final : std::ofstream{
	using std::ofstream::ofstream;

	explicit ofpstream(std::size_t buffer_size,const char* filename,const std::ios_base::openmode& open_mode)
		:std::ofstream(filename,open_mode),
		buffer(std::make_unique<byte[]>(buffer_size)),
		first(buffer.get()),last(buffer.get()+buffer_size),
		forward_iter(buffer.get())
	{}

	std::size_t capacity()const noexcept{return last - first;}
	std::size_t wrote_size()const noexcept{return forward_iter - first;}
	
	void output_file()
	JPEZY_NOEXCEPT(write(reinterpret_cast<const char*>(std::declval<byte>()),wrote_size()))
	{
		write(reinterpret_cast<const char*>(first),wrote_size());
	}
private:
	struct tag_argument{
		explicit constexpr tag_argument(int n):n(std::move(n)){}
		int n;
	};
public:	
	static constexpr auto Byte = []{};
	static constexpr auto Word = []{};
	static constexpr auto Bytes = []{};

	struct Byte_n : tag_argument{
		using tag_argument::tag_argument;
	};
	struct Bits : tag_argument{
		using tag_argument::tag_argument;
	};
private:
	std::unique_ptr<byte[]> buffer;
	const byte* first,*last;
	byte* forward_iter;

	bool writable = true;
	int bit_position = 7;
	const std::array<byte,8> bit_fullmask{0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

	void increment_buffer()noexcept
	{
		if(++forward_iter >= last){
			writable = false;
		}
	}	

	void set_fullbit()
	JPEZY_NOEXCEPT(write_fewbits(std::declval<byte>(),int()))
	{
		if(bit_position != 7){
			write_fewbits(bit_fullmask[bit_position],bit_position + 1);
		}
	}
		
	void write8bits(byte value,int bit_size)
	JPEZY_NOEXCEPT(
			noexcept(write_fewbits(std::declval<byte>(),int()))
			and
			noexcept(write_bridge(std::declval<byte>(),int()))
	)
	{
		if(bit_position + 1 >= bit_size){
			write_fewbits(byte(value),bit_size);
		}else{
			write_bridge(byte(value),bit_size);
		}
	}
		
	void write_fewbits(byte value,int bit_size)
	JPEZY_NOEXCEPT(increment_buffer())
	{
		value &= bit_fullmask[bit_size-1];
		*forward_iter |= (value << (bit_position + 1 - bit_size));
		if((bit_position -= bit_size) < 0){
			if(*forward_iter == 0xff){
				increment_buffer();
				*forward_iter = 0;
			}
			increment_buffer();
			bit_position = 7;
		}
	}

	void write_bridge(byte value,int bit_size)
	JPEZY_NOEXCEPT(increment_buffer())
	{
		value &= bit_fullmask[bit_size-1];
		int next_bits = bit_size - (bit_position + 1);
		*forward_iter |= ((value >> next_bits) & bit_fullmask[bit_position]);
		if(*forward_iter == 0xff){
			increment_buffer();
			*forward_iter = 0;
		}
		increment_buffer();
		*forward_iter = (value << (8 - next_bits));
		bit_position = 7 - next_bits;
	}

	template<class T,std::size_t v>
	friend std::pair<const decltype(Bytes)&,ofpstream&>
	operator<<(std::pair<const decltype(Bytes)&,ofpstream&> p,const std::array<T,v>& ar)
	noexcept(false)
	{
		if(p.second.forward_iter+ar.size() < p.second.last){
			p.second.set_fullbit();
			std::memcpy(p.second.forward_iter,ar.data(),ar.size());
			p.second.forward_iter += ar.size();
		}else{
			throw std::runtime_error(__func__);
		}
		return p;
	}

	template<
		class T,
		std::enable_if_t<
			std::is_convertible_v<byte,std::decay_t<T>> or 
			std::is_same_v<MARKER,std::decay_t<T>> or
			std::is_same_v<property::Units,std::decay_t<T>>
			,std::nullptr_t> = nullptr
	>
	friend std::pair<const decltype(Byte)&,ofpstream&>
   	operator<<(std::pair<const decltype(Byte)&,ofpstream&> p,T&& src)
	noexcept(false)
	{
		if(p.second.writable){
			p.second.set_fullbit();
			if constexpr(
					std::is_same_v<MARKER,std::decay_t<T>> or 
					std::is_same_v<property::Units,std::decay_t<T>>
			){
				*p.second.forward_iter = static_cast<byte>(src);
			}else{
				*p.second.forward_iter = src;
			}
			p.second.increment_buffer();
		}else{
			throw std::runtime_error(__func__);
		}
		return p;
	}

	friend std::pair<const decltype(Word)&,ofpstream&> 
	operator<<(std::pair<const decltype(Word)&,ofpstream&> p,unsigned int value)
	noexcept(false)
	{
		if(p.second.writable){
			p.second.set_fullbit();
			*p.second.forward_iter = (value >> 8) & 0xff;
			p.second.increment_buffer();
			*p.second.forward_iter = (value & 0xff);
			p.second.increment_buffer();
		}else{
			throw std::runtime_error(__func__);
		}
		return p;
	}

	friend std::pair<Bits,ofpstream&>
	operator<<(std::pair<Bits,ofpstream&> p,int value)
	noexcept(false)
	{
		if(p.first.n==0)return p;
		
		if(p.first.n > 16){
			std::string message = "Over max bits size ";
			message += __func__;
			throw std::invalid_argument(message.c_str());
		}


		if(p.first.n > 8){
			p.second.write8bits(byte(value >> 8),p.first.n - 8);
			p.first.n = 8;
		}
		p.second.write8bits(byte(value),p.first.n);
		return p;
	}

	template<class Range>
	friend std::pair<const Byte_n&,ofpstream&>
	operator<<(std::pair<const Byte_n&,ofpstream&> p,const Range& r)
	noexcept(false)
	{
		if(p.second.forward_iter + p.first.n < p.second.last){
			p.second.set_fullbit();

			if constexpr(srook::has_iterator_v<std::decay_t<Range>>){
				std::memcpy(p.second.forward_iter,r.data(),p.first.n);
			}else{
				std::memcpy(p.second.forward_iter,r,p.first.n);
			}
			p.second.forward_iter += p.first.n;
		}else{
			throw std::runtime_error(__func__);
		}
		return p;
	}
	
	template<class Tag>
	constexpr friend std::pair<Tag,ofpstream&> operator|(ofpstream& ofps,const Tag& t)
	{	
		return std::make_pair(t,std::ref(ofps));
	}
	
	friend std::pair<Bits,ofpstream&> operator|(ofpstream& ofps,const Bits& bits)
	{
		return std::make_pair(bits,std::ref(ofps));
	}
};


} // namespace jpezy
#endif
