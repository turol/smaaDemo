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


#ifdef RENDERER_VULKAN


#if defined(__GNUC__) && defined(_WIN32)


#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif  // WIN32_LEAN_AND_MEAN

#include <windows.h>


// mingw doesn't have std::mutex, must define this ourselves

   class VmaMutex
   {
   public:
       VmaMutex() { }
       ~VmaMutex() { }
       void Lock() { EnterCriticalSection(&cs); }
       void Unlock() { LeaveCriticalSection(&cs); }
   private:
       CRITICAL_SECTION cs;
   };
   #define VMA_MUTEX VmaMutex

#endif  // defined(__GNUC__) && defined(_WIN32)


// needs to be before RendererInternal.h
// TODO: have implementation in a separate file for compile time?
#define VMA_IMPLEMENTATION

#include "RendererInternal.h"


#endif  // RENDERER_VULKAN
