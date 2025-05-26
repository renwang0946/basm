
/* 2025.05.22 by renwang */

#ifndef __BASM_GLOBAL_H__
#define __BASM_GLOBAL_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifdef _MSC_VER
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <crtdbg.h>
#else
  #include <assert.h>
#endif/*_MSC_VER*/

/* for visual leak detector */
#ifdef _WIN32
  #if defined(_DEBUG) && !defined(_WIN64)  /* Could not debug IA64 CPU */
    #include <vld.h>  /* NOTE: if you debug your ocx control, please disable the code line */
  #endif
#endif//_WIN32

#ifndef _countof
  #define _countof(array)  (sizeof(array)/sizeof(array[0]))
#endif/*_countof*/

#ifdef _DEBUG
  #ifdef _MSC_VER
    #define ASSERT(a)  _ASSERT(a)
  #else
    #define ASSERT assert
  #endif/*_MSC_VER*/
#else
  #define ASSERT(a)
#endif/*_DEBUG*/

#ifndef VERIFY
  #ifdef _DEBUG
    #define VERIFY(f)  ASSERT(f)
  #else/*_DEBUG*/
    #define VERIFY(f)  ((void)(f))
  #endif/*_DEBUG*/
#endif/*VERIFY*/

#ifndef FALSE
  #define FALSE  0
  #define TRUE   1
#endif/*FALSE*/

#ifndef FREE_PTR
  #define FREE_PTR(p)  do { free(p); (p) = NULL; } while (0);
#endif/*FREE_PTR*/

#ifdef _WIN64
  #define __BX_X64__
#endif/*_WIN64*/

inline uint32_t bsm_lo_dword(uint64_t qw) { return (uint32_t)qw; }
inline uint32_t bsm_hi_dword(uint64_t qw) { return (uint32_t)((qw >> 32) & 0xFFFFFFFF); }

inline size_t bsm_align(size_t dw, size_t align) { return ((dw + (align - 1)) & (~(align -1))); }

#endif/*__BASM_GLOBAL_H__*/
