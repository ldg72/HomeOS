#ifndef EXEC64_STRING_H
#define EXEC64_STRING_H

#include <stddef.h>

// Memory operations
void* memcpy(void *dest, const void *src, size_t n);
void* memmove(void *dest, const void *src, size_t n);
void* memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void* memchr(const void *s, int c, size_t n);

// String operations
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
char* strcpy(char *dest, const char *src);
char* strncpy(char *dest, const char *src, size_t n);
char* strcat(char *dest, const char *src);
char* strncat(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
char* strchr(const char *s, int c);
char* strrchr(const char *s, int c);
char* strpbrk(const char *s, const char *accept);
char* strstr(const char *haystack, const char *needle);
char* strdup(const char *s);
char* strndup(const char *s, size_t n);
char* strtok(char *str, const char *delim);
char* strtok_r(char *str, const char *delim, char **saveptr);

// BSD/GNU extensions
size_t strlcpy(char *dst, const char *src, size_t size);
size_t strlcat(char *dst, const char *src, size_t size);

#endif
