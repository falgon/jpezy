/*
 *
 * JPEZY
 * Copyright (c) 2017 Roki
 *
 */

// for this issue : https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52869
#ifndef INCLUDED_NOEXCEPT_DETECTION_HPP
#define INCLUDED_NOEXCEPT_DETECTION_HPP

#if defined(__GNUC__) or defined(__GNUG__)
#define JPEZY_NOEXCEPT(x) noexcept(false)
#else
#define JPEZY_NOEXCEPT(...) noexcept(noexcept(__VA_ARGS__))
#endif

#endif
