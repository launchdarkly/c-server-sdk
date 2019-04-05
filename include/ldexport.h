/*!
 * @file ldexport.h
 * @brief Public. Configuration of exported symbols.
 */

#pragma once

#ifdef _WIN32
    #define LD_EXPORT(x) __declspec(dllexport) x __stdcall
#else
    #define LD_EXPORT(x) __attribute__((visibility("default"))) x
#endif
