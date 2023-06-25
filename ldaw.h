#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define SONG EMSCRIPTEN_KEEPALIVE
typedef float sample_t;

// :TODO: remove!!
EMSCRIPTEN_KEEPALIVE void* ldaw__alloc_(uint64_t bytes)
{
    return malloc(bytes);
}
#else
#define SONG _declspec(dllexport)
typedef int16_t sample_t;
#endif

typedef struct ldaw_song_info {
    uint32_t sample_rate;
    uint32_t state_size;
    uint32_t entropy_size;
} ldaw_song_info_t;

#define PI 3.14159265358979323846

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define CLAMP(min, x, max) MAX(min, MIN(max, x))
#define CLAMP01(x) CLAMP(0.0, x, 1.0)
#define CLAMP11(x) CLAMP(-1.0, x, 1.0)
#define COUNTOF(arr) (sizeof(arr) / sizeof(arr[0])) // deprecated
#define N(arr) (sizeof(arr) / sizeof(arr[0]))

#ifdef __EMSCRIPTEN__
#define OutputDouble(x) ((float)CLAMP11(x))
#else
#define OutputDouble(x) ((int16_t)(CLAMP11(x) * INT16_MAX))
#endif

// deprecated
#define OUTPUT(x) OutputDouble(x)

// deprecated
static double sineWave(double phase, double hz) {
    return sin(phase * PI * hz);
}

// deprecated
static double sawtoothWave(double phase, double hz) {
    return 1.0 - (fmod(phase * hz, 1.0) * 2.0);
}

// deprecated
static double squareWave(double phase, double hz) {
    return (fmod(phase * hz, 2.0) > 1.0) ? 1.0 : -1.0;
}

//#define sqr(x) _Generic((x), float: sqrf, double (__cdecl *)(double): sqrd, double: sqrd)

static float sqrf(float x) {
    return x * x;
}

static double sqr(double x) {
    return x * x;
}

//#define osc_sin(x) _Generic((x), float: osc_sind, double: sqrd)

static float osc_sinf(float t) {
    return sinf(t * (float)PI * 2.0f);
}

static double osc_sin(double t) {
    return sin(t * PI * 2.0);
}

static double osc_saw(double t) {
    return 1.0 - (fmod(t, 1.0) * 2.0);
}

static double osc_sqr(double t) {
    return (fmod(t, 2.0) > 1.0) ? 1.0 : -1.0;
}