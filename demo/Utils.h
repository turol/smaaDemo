/*
Copyright (c) 2015-2017 Alternative Games Ltd / Turo Lamminen

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


#include <memory>
#include <string>
#include <vector>

#include <cstdio>


#ifdef _MSC_VER

#define fileno _fileno
#define UNREACHABLE() abort()
#define PRINTF(x, y)

#else   // _MSC_VER

#define UNREACHABLE() __builtin_unreachable()
#define PRINTF(x, y) __attribute__((format(printf, x, y)))

#endif  // _MSC_VER


// should be ifdeffed out on compilers which already have it (eg. VS2013)
// http://isocpp.org/files/papers/N3656.txt
#ifndef _MSC_VER
namespace std {

    template<class T> struct _Unique_if {
        typedef unique_ptr<T> _Single_object;
    };

    template<class T> struct _Unique_if<T[]> {
        typedef unique_ptr<T[]> _Unknown_bound;
    };

    template<class T, size_t N> struct _Unique_if<T[N]> {
        typedef void _Known_bound;
    };

    template<class T, class... Args>
        typename _Unique_if<T>::_Single_object
        make_unique(Args&&... args) {
            return unique_ptr<T>(new T(std::forward<Args>(args)...));
        }

    template<class T>
        typename _Unique_if<T>::_Unknown_bound
        make_unique(size_t n) {
            typedef typename remove_extent<T>::type U;
            return unique_ptr<T>(new U[n]());
        }

    template<class T, class... Args>
        typename _Unique_if<T>::_Known_bound
        make_unique(Args&&...) = delete;


}  // namespace std
#endif


struct FILEDeleter {
	void operator()(FILE *f) { fclose(f); }
};

#define LOG(msg, ...) logWrite(msg, ##__VA_ARGS__)

void logInit();
void logWrite(const char* message, ...) PRINTF(1, 2);
void logShutdown();

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


#endif  // UTILS_H
