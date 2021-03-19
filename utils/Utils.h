/*
Copyright (c) 2015-2021 Alternative Games Ltd / Turo Lamminen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


#ifndef UTILS_H
#define UTILS_H


#include <cinttypes>

#include <memory>
#include <string>
#include <vector>

#ifndef nssv_CONFIG_SELECT_STRING_VIEW
#define nssv_CONFIG_SELECT_STRING_VIEW nssv_STRING_VIEW_NONSTD
#endif  // nssv_CONFIG_SELECT_STRING_VIEW
#include <nonstd/string_view.hpp>

#include <fmt/format.h>

#include <hedley/hedley.h>


#ifdef _MSC_VER

#define fileno _fileno
#define UNUSED
#define DEBUG_ASSERTED

#else   // _MSC_VER

#define UNUSED        __attribute__((unused))


#ifdef NDEBUG

#define DEBUG_ASSERTED __attribute__((unused))

#else  // NDEBUG

#define DEBUG_ASSERTED

#endif  // NDEBUG


#endif  // _MSC_VER


#ifdef _MSC_VER
#define __PRETTY_FUNCTION__  __FUNCTION__
#endif

#define LOG_TODO(str) \
	{ \
		static bool seen = false; \
		if (!seen) { \
			printf("TODO: %s in %s at %s:%d\n", str, __PRETTY_FUNCTION__, __FILE__,  __LINE__); \
			seen = true; \
		} \
	}


#define LOG(msg, ...)        logWriteFmt(FMT_STRING(msg), ##__VA_ARGS__)
#define LOG_ERROR(msg, ...)  logWriteErrorFmt(FMT_STRING(msg), ##__VA_ARGS__)


void logInit();
void logWrite(const nonstd::string_view &message);
void logWriteError(const nonstd::string_view &message);

template <typename S, typename... Args>
static void logWriteFmt(const S &format, const Args & ... args) {
	logWrite(fmt::format(format, args...));
}

template <typename S, typename... Args>
static void logWriteErrorFmt(const S &format, const Args & ... args) {
	logWriteError(fmt::format(format, args...));
}

void logShutdown();
void logFlush();


#define THROW_ERROR(msg, ...)                                                    \
	{                                                                            \
		std::string errorMessage = fmt::format(FMT_STRING(msg), ##__VA_ARGS__);  \
		logWriteError(errorMessage);                                             \
		throw std::runtime_error(errorMessage);                                  \
	}


std::vector<char> readTextFile(std::string filename);
std::vector<char> readFile(std::string filename);
void writeFile(const std::string &filename, const void *contents, size_t size);
bool fileExists(const std::string &filename);
int64_t getFileTimestamp(const std::string &filename);

// From https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
static inline bool isPow2(unsigned int value) {
	return (value & (value - 1)) == 0;
}


// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static inline uint32_t nextPow2(unsigned int v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;

	return v;
}


static inline uint64_t gcd(uint64_t a, uint64_t b) {
	uint64_t c;
	while (a != 0) {
		c = a;
		a = b % a;
		b = c;
	}
	return b;
}


#define OPTIMIZED_FOREACHBIT 1


#if OPTIMIZED_FOREACHBIT


#ifdef __GNUC__


template <typename F>
void forEachSetBit(uint32_t v_, F &&f) {
	uint32_t value = v_;

	while (value != 0) {
		int bit = __builtin_ctz(value);
		uint32_t mask = 1U << bit;

		assert((value & mask) != 0);
		f(bit, mask);

		uint32_t UNUSED oldValue = value;
		value ^= mask;
		assert(value < oldValue);
	}
}


#else // __GNUC__


template <typename F>
void forEachSetBit(uint32_t v_, F &&f) {
	unsigned long value = v_;

	unsigned long bit = 0;
	while (_BitScanForward(&bit, value)) {
		uint32_t mask = 1U << bit;

		assert((value & mask) != 0);
		f(bit, mask);

		uint32_t oldValue = value;
		value ^= mask;
		assert(value < oldValue);
	}
}


#endif // __GNUC__


#else  // OPTIMIZED_FOREACHBIT


template <typename F>
void forEachSetBit(uint32_t v_, F &&f) {
	uint32_t value = v_;

	uint32_t bit = 0;
	while (value != 0) {
		uint32_t mask = 1 << bit;
		if ((value & mask) != 0) {
			f(bit, mask);

			uint32_t oldValue = value;
			value ^= mask;
			assert(value < oldValue);
		}

		bit++;
	}
}


#endif  // OPTIMIZED_FOREACHBIT


#endif  // UTILS_H
