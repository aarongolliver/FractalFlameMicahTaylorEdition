#ifndef __HISTOGRAM_H__
#define __HISTOGRAM_H__ __HISTOGRAM_H__

#include "datatypes.h"
#include <immintrin.h>
void histoinit();

f256tuple histohit(f256tuple xyvec, const colorset pointcolors[4], const i32 th_id);

void saveimage();

histocell histoget(u64 cell);

#define swid 1920
#define shei 1080
#define ss 1

#define hwid (swid * ss)
#define hhei (shei * ss)
#define xshrink 15.0
#define yshrink 15.0

#define xoffset 0.0
#define yoffset 0.0

#endif