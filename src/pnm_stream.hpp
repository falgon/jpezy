#ifndef INCLUDED_JPEZY_PNM_STREAM_HPP
#define INCLUDED_JPEZY_PNM_STREAM_HPP

#include<stdexcept>
#include<vector>

#include"jpezy.hpp"

namespace jpezy{

struct pnm_stream{
	pnm_stream():initializing_succeed(true),width(0),height(0),max_color(0){}
	pnm_stream(bool b,std::size_t w,std::size_t h,std::size_t max):initializing_succeed(b),width(w),height(h),max_color(max){}

	explicit operator bool()const noexcept
	{
		return initializing_succeed;
	}
protected:
	using value_type = srook::byte;
	using rgb_type = srook::byte;

	bool initializing_succeed;
	std::size_t width,height,max_color;
	std::vector<std::array<rgb_type,3>> rgb_img;

	inline void report_error(const char* funcName)const noexcept(false)
	{
		if(initializing_succeed)return;

		std::string str = "Initializing was failed: ";
		str += funcName;
		throw std::runtime_error(str.c_str());
	}
};

} // namespace jpezy

#endif
