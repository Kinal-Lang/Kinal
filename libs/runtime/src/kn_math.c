/* kn_math.c — Kinal standard math library (C backend) */
#include <math.h>

/* Trigonometric */
double __kn_math_sin(double x)  { return sin(x); }
double __kn_math_cos(double x)  { return cos(x); }
double __kn_math_tan(double x)  { return tan(x); }
double __kn_math_asin(double x) { return asin(x); }
double __kn_math_acos(double x) { return acos(x); }
double __kn_math_atan(double x) { return atan(x); }
double __kn_math_atan2(double y, double x) { return atan2(y, x); }

/* Hyperbolic */
double __kn_math_sinh(double x) { return sinh(x); }
double __kn_math_cosh(double x) { return cosh(x); }
double __kn_math_tanh(double x) { return tanh(x); }

/* Power / exponential */
double __kn_math_sqrt(double x)            { return sqrt(x); }
double __kn_math_cbrt(double x)            { return cbrt(x); }
double __kn_math_pow(double b, double e)   { return pow(b, e); }
double __kn_math_exp(double x)             { return exp(x); }
double __kn_math_log(double x)             { return log(x); }
double __kn_math_log2(double x)            { return log2(x); }
double __kn_math_log10(double x)           { return log10(x); }

/* Rounding */
double __kn_math_floor(double x) { return floor(x); }
double __kn_math_ceil(double x)  { return ceil(x); }
double __kn_math_round(double x) { return round(x); }
double __kn_math_trunc(double x) { return trunc(x); }

/* Misc */
double __kn_math_fabs(double x)            { return fabs(x); }
double __kn_math_fmod(double x, double y)  { return fmod(x, y); }
double __kn_math_fmin(double x, double y)  { return fmin(x, y); }
double __kn_math_fmax(double x, double y)  { return fmax(x, y); }
double __kn_math_copysign(double x, double y) { return copysign(x, y); }
int    __kn_math_isinf(double x)  { return isinf(x) ? 1 : 0; }
int    __kn_math_isnan(double x)  { return isnan(x) ? 1 : 0; }
