#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "vmath.h"

#include <immintrin.h>
#include <omp.h>
#include <cilk\cilk.h>
#include <cilk\cilk_api_windows.h>
#include <cilk\cilk_stub.h>
#include <intrin.h>

#include "datatypes.h"
#include "rdrand.h"
#include "bmpfile.h"
#include "histogram.h"


static histocell h[hwid * hhei];
volatile long *locks;


__m128 xshrinkvec = { xshrink, xshrink, xshrink, xshrink };
__m128 yshrinkvec = { yshrink, yshrink, yshrink, yshrink };

static const __m128 xoffsetvec = { xoffset, xoffset, xoffset, xoffset };
static const __m128 yoffsetvec = { yoffset, yoffset, yoffset, yoffset };

static const __m128 halfhwidvec = { hwid/2.0, hwid/2.0, hwid/2.0, hwid/2.0 };
static const __m128 halfhheivec = { hhei/2.0, hhei/2.0, hhei/2.0, hhei/2.0 };

static const __m128 hwidShrunkvec = { hwid/xshrink, hwid/xshrink, hwid/xshrink, hwid/xshrink };
static const __m128 hheiShrunkvec = { hhei/yshrink, hhei/yshrink, hhei/yshrink, hhei/yshrink };
static const __m128 halfRGB        =  { 0.5, 0.5, 0.5, 1.0};
static const __m128 halfRGBA       =  { 0.5, 0.5, 0.5, 0.5};
static const __m128 incrementAlpha =  { 0, 0, 0, 1};

void histoinit(){
    int numcells = hwid * hhei;
    size_t histogramSize = numcells * sizeof(histocell);

    memset(h, 0, histogramSize);

    if(locks == NULL)
        locks = (volatile long *) calloc(numcells, sizeof(volatile long));
    //else
       // memset(locks, 0, hwid * sizeof(volatile long));

    if(!h){
        printf("Could not allocate %zu bytes\nPress enter to exit.", hwid * hhei * sizeof(histocell));
        getchar();
        exit(1);
    }

}

u64 goodHits = 0;
u64 missHits = 0;
u64 badHits = 0;

u64 threadHits[12];

typedef union {
    f32 f;
    u32 u;
} f32u32;

static f128 zerovec = { 0, 0, 0, 0 };

f128tuple histohit(f128tuple xyvec, const colorset pointcolors[4], const i32 th_id){
    if(threadHits[th_id]++ > 20){
        f128 xarr = xyvec.x;
        f128 yarr = xyvec.y;

        if(vvalid(xarr.v) && vvalid(yarr.v)){

            f128 scaledX;
            scaledX.v = vadd(
                           vmul(
                               vadd(xarr.v, xoffsetvec),
                               hwidShrunkvec),
                           halfhwidvec);
            f128 scaledY;
            scaledY.v = vadd(
                            vmul(
                                vadd(yarr.v, yoffsetvec),
                                hheiShrunkvec),
                            halfhheivec);
            
            for (i32 i = 0; i < 4; i++){
                u32 ix = scaledX.f[i];
                u32 iy = scaledY.f[i];

                if(ix < hwid && iy < hhei){
                    u64 cell = ix + (iy * hwid);
                    // lock the cell
                    //while(_InterlockedExchange(&(locks[ix]), 1));
                    __m128 histocolor = vload((float *)&(h[cell]));
                    

                    // add half the new color with the exising color
                    histocolor = vadd(
                                    vmul(histocolor, halfRGB), 
                                    vmul(pointcolors[i].vec, halfRGBA));

                    // increment alpha channel
                    //histocolor = vadd(histocolor, incrementAlpha);

                    // write back
                    vstore((float *)&(h[cell]), histocolor);

                    ++goodHits;

                    // unlock the cell
                    //_InterlockedExchange(&(locks[ix]), 0);
                } else {
                    ++missHits;
                }
            }
        } else {
            ++badHits;
            threadHits[th_id] = 0;
            xarr = zerovec;
            yarr = zerovec;
            xyvec.x = xarr;
            xyvec.y = yarr;
        }
    }
    return xyvec;
}

void saveimage(){
    printf("Good hits: %llu\t Miss hits: %llu\t Bad hits: %llu\t %f\n", goodHits, missHits, badHits, (f32)goodHits/(badHits > 0 ? badHits : 1));

    bmpfile_t *bmp;

    f32 amax = 1;

    for(int i = 0; i < hwid * hhei; i++){
        amax = amax > h[i].a ? amax : h[i].a;
    }

    if((bmp = bmp_create(hwid, hhei, 24)) == NULL){
        printf("Invalid depth value: %d.\n", 24);
        exit(1);
    }

    printf("generating image");

    cilk_for (i32 i = 0; i < hwid * hhei; i++){
        f32 a = log(h[i].a) / log(amax);

        u8 r = h[i].r * 0xFF * a;
        u8 g = h[i].g * 0xFF * a;
        u8 b = h[i].b * 0xFF * a;

        rgb_pixel_t pixel = {b, g, r, 0xFF};
        u32 x = i % hwid;
        u32 y = i / hwid;
        bmp_set_pixel(bmp, x, y, pixel);
        if((i % ((hwid * hhei) / 20)) == 0)
            printf(".");
    }

    printf(" done\n");

    printf("saving image... ");
    bmp_save(bmp, "fractal.bmp");
    bmp_destroy(bmp);
    printf("done\n");
}