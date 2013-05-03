#include "datatypes.h"
#include "histogram.h"
#include "rdrand.h"
#include "variations.h"
#include "vmath.h"

#include <stdio.h>
#include <time.h>
#include <immintrin.h>
#include <Windows.h>

#include <omp.h>

#define n_affine_matrix (6)
#define jump_table_size (1024)
#define MAX_VARIATIONS (50)

#define FLAME_ITTS (2)

#define abs(x) (x >= 0 ? x : - x)


typedef struct {
    f32 a, b, c, d, e, f;
    f32 red, green, blue;
} affinematrix;

void affineinit();
void variationinit();
void compressimage();

static affinematrix am[n_affine_matrix];
static affinematrix * affine_jump_table[jump_table_size];
static f128 variation_weights[MAX_VARIATIONS];

int main(i32 argc, i8 **argv){
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);

    while(1){
        printf("allocating memory... ");
        histoinit();
        affineinit();
        variationinit();
        printf("done\n");

        printf("plotting points\n");
        clock_t start;
        start = clock();

        i32 th_id;

#pragma omp parallel private(th_id)
        {
            th_id = omp_get_thread_num();

            f128 pointvecx = { 0, 0, 0, 0 };
            f128 pointvecy = { 0, 0, 0, 0 };
            f128 pointvecr = { 0, 0, 0, 0 };
            f128 pointvecg = { 0, 0, 0, 0 };
            f128 pointvecb = { 0, 0, 0, 0 };

            f128tuple xyvec;

            u32 tmp;

            printf("thread id: %d\n", th_id);
            Sleep(1000);
            for(u32 j = 0; j < FLAME_ITTS * 1000000; j++){

                if(j % 1000000 == 0){
                    printf("...%u", j/1000000);
                }

                for(u64 i = 0; i < 10; i++){
                    affinematrix *am_itt[4];
                    u32 jumpTable[4];

                    jumpTable[0] = rdrand_u32(&jumpTable[0]);
                    jumpTable[1] = rdrand_u32(&jumpTable[1]);
                    jumpTable[2] = rdrand_u32(&jumpTable[2]);
                    jumpTable[3] = rdrand_u32(&jumpTable[3]);

                    for(u32 j = 0; j < 4; j++){
                        i32 n = jumpTable[j] % jump_table_size;
                        am_itt[j] = affine_jump_table[n];
                    }

                    // this is slow I think
                    // will be fast with gather instruction on Haswell
                    const __m128 affinea = { am_itt[0]->a, am_itt[1]->a, am_itt[2]->a, am_itt[3]->a };
                    const __m128 affineb = { am_itt[0]->b, am_itt[1]->b, am_itt[2]->b, am_itt[3]->b };
                    const __m128 affinec = { am_itt[0]->c, am_itt[1]->c, am_itt[2]->c, am_itt[3]->c };
                    const __m128 affined = { am_itt[0]->d, am_itt[1]->d, am_itt[2]->d, am_itt[3]->d };
                    const __m128 affinee = { am_itt[0]->e, am_itt[1]->e, am_itt[2]->e, am_itt[3]->e };
                    const __m128 affinef = { am_itt[0]->f, am_itt[1]->f, am_itt[2]->f, am_itt[3]->f };

                    const __m128 colorsetr = { am_itt[0]->red,   am_itt[1]->red,   am_itt[2]->red,   am_itt[3]->red   };
                    const __m128 colorsetg = { am_itt[0]->green, am_itt[1]->green, am_itt[2]->green, am_itt[3]->green };
                    const __m128 colorsetb = { am_itt[0]->blue,  am_itt[1]->blue,  am_itt[2]->blue,  am_itt[3]->blue  };
                    


                    const __m128 zerovec   = { 0, 0, 0, 0 };
                    const __m128 onevec    = { 1, 1, 1, 1 };
                    const __m128 twovec    = { 2, 2, 2, 2 };
                    const __m128 threevec  = { 3, 3, 3, 3 };
                    const __m128 pivec     = { PI, PI, PI, PI };
                    const __m128 negonevec = { -1, -1, -1, -1 };

                    // update colors: colorfinal = (colorold + colornew) / 2.0
                    pointvecr.v = vdiv(vadd(pointvecr.v, colorsetr), twovec);
                    pointvecg.v = vdiv(vadd(pointvecg.v, colorsetg), twovec);
                    pointvecb.v = vdiv(vadd(pointvecb.v, colorsetb), twovec);
                    
                    const __m128 affinedx = vadd(
                                                vadd(
                                                    vmul(affinea, pointvecx.v),
                                                    vmul(affineb, pointvecy.v)),
                                                affinec);

                    const __m128 affinedy = vadd(
                                                vadd(
                                                    vmul(affined, pointvecx.v),
                                                    vmul(affinee, pointvecy.v)),
                                                affinef);

                    const __m128 rsq = vadd(
                                        vmul(affinedx, affinedx),
                                        vmul(affinedy, affinedy));

                    const __m128 r = vsqrt(rsq);

                    const __m128 theta = vatan2(affinedx, affinedy);

                    const __m128 thetaaddr = vadd(theta, r);
                    const __m128 thetasubr = vsub(theta, r);
                    const __m128 sinrsq = vsin(rsq);
                    const __m128 cosrsq = vcos(rsq);
                    const __m128 sintheta = vsin(theta);
                    const __m128 costheta = vcos(theta);
                    const __m128 sinr = vsin(r);
                    const __m128 cosr = vcos(r);
                    const __m128 thetamulpi = vmul(theta, r);
                    const __m128 thetadivpi = vdiv(theta, pivec);
                    const __m128 pimulr = vmul(pivec, r);
                    const __m128 invr = vdiv(onevec, r);
                    const __m128 sqrtr = vsqrt(r);
                    const __m128 halftheta = vdiv(theta, twovec);
                    
                    __m128 sumvecx = { 0, 0, 0, 0 };
                    __m128 sumvecy = { 0, 0, 0, 0 };


                    //v1;
                    //v2;
                    //v3;
                    //v4;
                    //v5;
                    //v6;
                    //v7;
                    //v8;
                    //v9;
                    //v10;
                    //v11;
                    //v12;
                    //v13;
                    v14;
                    //v15;
                    //v16;
                    v17;
                    //v18;
                    //v19;
                    //v20;
                    v21;
                    v22;
                    
                    xyvec.x.v = sumvecx;
                    xyvec.y.v = sumvecy;

                    xyvec = histohit(xyvec, pointvecr, pointvecg, pointvecb, th_id);
                    pointvecx = xyvec.x;
                    pointvecy = xyvec.y;
                }
            }
        }

        clock_t end = clock();

        printf(" done took %f seconds\n", (f64)(end - start)/(f64)CLOCKS_PER_SEC);
        saveimage();

        // create random filename for each image
        compressimage();

    }
    //getchar();
}

void compressimage(){
        char *rand_filename = (char *) calloc(1024, sizeof(char));

        char *base_command = "mogrify -format png -path images -resize 1920x1080 -write images/";
        char *extension = ".png ";
        u64 tmp;
        char *rand_int_string = (char *) calloc(1024, sizeof(char));
        sprintf(rand_int_string, "%llu", rdrand_u64(&tmp));
        strcat(rand_filename, base_command);
        strcat(rand_filename, rand_int_string);
        strcat(rand_filename, extension);
        strcat(rand_filename, " fractal.bmp");

        printf("compressing with command: %s\n", rand_filename);
        system(rand_filename);
        free(rand_int_string);
        free(rand_filename);
}

void affineinit(){
    // init matrix values
    for(i32 i = 0; i < n_affine_matrix; i++){
        am[i].a = rdrand_f32(&am[i].a);
        am[i].b = rdrand_f32(&am[i].b);
        am[i].c = rdrand_f32(&am[i].c);
        am[i].d = rdrand_f32(&am[i].d);
        am[i].e = rdrand_f32(&am[i].e);
        am[i].f = rdrand_f32(&am[i].f);

        f32 r = rdrand_f32(&r);
        f32 g = rdrand_f32(&g);
        f32 b = rdrand_f32(&b);
        am[i].red = abs(r);
        am[i].green = abs(g);
        am[i].blue = abs(b);
    }

    // init jump table
    for(i32 i = 0; i < jump_table_size; i++){
        u32 tmp;
        affine_jump_table[i] = &(am[rdrand_u32(&tmp) % n_affine_matrix]);
    }
}

void variationinit(){
    float total = 0;
    for(int i = 0; i < MAX_VARIATIONS; i++){
        float weight = abs(rdrand_f32(&weight));
        variation_weights[i].f[0] = weight;
        variation_weights[i].f[1] = weight;
        variation_weights[i].f[2] = weight;
        variation_weights[i].f[3] = weight;
        total += weight;
    }

    // this part needs work
    /*
    for(int i = 0; i < MAX_VARIATIONS; i++){
        variation_weights[i].f[0] /= total;
        variation_weights[i].f[1] /= total;
        variation_weights[i].f[2] /= total;
        variation_weights[i].f[3] /= total;
    }
    */
}