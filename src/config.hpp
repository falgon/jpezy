/*
 *
 * JPEZY
 * Copyright (c) 2017 Roki
 *
 */

#ifndef INCLUDED_JPEZY_OPTIONAL_AND_ARRAY_HPP
#define INCLUDED_JPEZY_OPTIONAL_AND_ARRAY_HPP

#if __has_include(<optional>)
#	include<optional>
#	define OPTIONAL std::optional
#	define NULLOPT_T std::nullopt_t
#	define NULLOPT std::nullopt
#elif __has_include(<boost/optional.hpp>)
#	include<boost/optional.hpp>
#	define OPTIONAL boost::optional
#	define NULLOPT_T boost::none_t
#	define NULLOPT boost::none
#elif __has_include(<srook/optional.hpp>)
#	include<srook/optional.hpp>
#	define OPTIONAL srook::optional
#	define NULLOPT_T srook::nullopt_t
#	define NULLOPT srook::nullopt;
#else
#	error
#endif

#if __has_include(<experimental/array>)
#	include<experimental/array>
#	define USABLE_EXPERIMENTAL_MAKE_ARRAY
#elif __has_include(<srook/array/make_array.hpp>)
#	include<srook/array/make_array.hpp>
#endif

#if __has_include(<experimental/iterator>)
#	include<experimental/iterator>
#	define USABLE_EXPERIMENTAL_OSTREAM_JOINER
#elif __has_include(<srook/iterator/ostream_joiner.hpp>)
#	include<srook/iterator/ostream_joiner.hpp>
#endif

#endif
