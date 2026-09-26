#pragma once

#if defined(_WIN32) || defined(WIN32)
  #define IEEE754_64FLOAT 1
  #define NATIVE_INT64 0
  #define ASIO_LITTLE_ENDIAN 1
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif
