#ifndef UTILS_H
#define UTILS_H


#include <memory>
#include <vector>

#include <cstdio>


#ifdef _MSC_VER
#define fileno _fileno
// TODO: we should not define macros starting with two underscores since those are reserved for the compiler
#define __builtin_unreachable() assert(false)
#endif


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


std::vector<char> readTextFile(std::string filename);
std::vector<char> readFile(std::string filename);
void writeFile(const std::string &filename, const std::vector<char> &contents);

#endif  // UTILS_H
