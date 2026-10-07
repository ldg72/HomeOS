#ifndef EXEC64_STDIO_H
#define EXEC64_STDIO_H

#include <stddef.h>
#include <stdarg.h>

// FILE structure
typedef struct FILE {
    int fd;              // File descriptor
    char *buffer;        // Internal buffer
    size_t buf_size;     // Buffer size (usually 4KB)
    size_t buf_pos;      // Current position in buffer
    size_t buf_end;      // End of valid data in buffer
    int flags;           // Stream flags
    int error;           // Error indicator
    int eof;             // EOF indicator
    int mode;            // Open mode
} FILE;

int remove(const char *pathname);
int rename(const char *oldpath, const char *newpath);

// Standard streams
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

// Buffer modes
#define _IONBF 0  // No buffering
#define _IOLBF 1  // Line buffering
#define _IOFBF 2  // Full buffering

// Seek modes
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// EOF
#define EOF (-1)

// File operations
FILE* fopen(const char *path, const char *mode);
FILE* fdopen(int fd, const char *mode);
FILE* freopen(const char *path, const char *mode, FILE *stream);
int fclose(FILE *stream);
int fflush(FILE *stream);
void setbuf(FILE *stream, char *buf);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);

// Character I/O
int fgetc(FILE *stream);
int getc(FILE *stream);
int getchar(void);
int fputc(int c, FILE *stream);
int putc(int c, FILE *stream);
int putchar(int c);
int ungetc(int c, FILE *stream);

// Line I/O
char* fgets(char *s, int size, FILE *stream);
int fputs(const char *s, FILE *stream);
int puts(const char *s);

// Block I/O
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

// Formatted I/O
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vprintf(const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

// File positioning
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);

// Error handling
void clearerr(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void perror(const char *s);

#endif
