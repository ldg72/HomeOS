#ifndef EXEC64_MATH_H
#define EXEC64_MATH_H

double floor(double x);
double ceil(double x);
double trunc(double x);
double fabs(double x);
double sqrt(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double atan2(double y, double x);
double fmod(double x, double y);
double copysign(double x, double y);
double exp(double x);
double log(double x);
double pow(double x, double y);

float sinf(float x);
float cosf(float x);
float fabsf(float x);
float floorf(float x);
float scalbnf(float x, int n);

#define M_PI   3.14159265358979323846
#define M_PI_F 3.14159265358979323846f

#endif
