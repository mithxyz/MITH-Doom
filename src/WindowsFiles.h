#pragma once
// The FFGL file picker gives UTF-8 paths. Doom's C stdio needs wide filenames
// on Windows so WADs also work in folders with non-ASCII names.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef LoadMenu
#include <stdio.h>
#include <stdlib.h>
static inline FILE *md_fopen(const char *path,const char *mode) {
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);
    if (!count) return NULL;
    wchar_t *wide=(wchar_t*)malloc(count*sizeof(wchar_t));
    if (!wide) return NULL;
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,count);
    wchar_t wmode[16]; int i=0;
    while(mode[i] && i<15) { wmode[i]=(wchar_t)mode[i]; ++i; } wmode[i]=0;
    FILE *file=_wfopen(wide,wmode); free(wide); return file;
}
#define fopen md_fopen
#endif
