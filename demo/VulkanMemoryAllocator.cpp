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
