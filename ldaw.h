#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define SONG EMSCRIPTEN_KEEPALIVE
#else
#define SONG _declspec(dllexport)
#endif

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define CLAMP(min, x, max) MAX(min, MIN(max, x))
#define CLAMP01(x) CLAMP(0.0, x, 1.0)
#define CLAMP11(x) CLAMP(-1.0, x, 1.0)
#define OUTPUT(x) ((int16_t)(CLAMP11(x) * INT16_MAX))

#define COUNTOF(arr) (sizeof(arr) / sizeof(arr[0]))

#define PI 3.14159265358979323846

double sineWave(double phase, double hz) {
    return sin(phase * PI * hz);
}

double sawtoothWave(double phase, double hz) {
    return 1.0 - fmod(phase * hz, 1.0);
}

double squareWave(double phase, double hz) {
    return (fmod(phase * hz, 2.0) > 1.0) ? 1.0 : -1.0;
}

double sqr(double x) {
    return x * x;
}