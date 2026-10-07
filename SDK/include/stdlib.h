#ifndef EXEC64_STDLIB_H
#define EXEC64_STDLIB_H

#include <stddef.h>

// Memory management
void* malloc(size_t size);
void free(void *ptr);
void* calloc(size_t nmemb, size_t size);
void* realloc(void *ptr, size_t size);

// String conversion
int atoi(const char *nptr);
long atol(const char *nptr);
long long atoll(const char *nptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);
double atof(const char *nptr);
char* itoa(int value, char *str, int base);

// Sorting/Searching
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));
void* bsearch(const void *key, const void *base, size_t nmemb,
              size_t size, int (*compar)(const void *, const void *));

// Math
int abs(int j);
long labs(long j);
long long llabs(long long j);

// Program termination
void exit(int status);
void abort(void);

// Pseudo-random
int rand(void);
void srand(unsigned int seed);
#define RAND_MAX 2147483647

#endif
