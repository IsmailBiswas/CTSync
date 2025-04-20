#pragma once

// #define DEBUG
// #define INFO
#define WARNING
#define ERROR
#define FATAL

#ifdef DEBUG
    #define LOG_LEVEL_DEBUG
#endif
#ifdef INFO
    #define LOG_LEVEL_INFO
#endif
#ifdef WARNING
    #define LOG_LEVEL_WARNING
#endif
#ifdef ERROR
    #define LOG_LEVEL_ERROR
#endif
#ifdef FATAL
    #define LOG_LEVEL_FATAL
#endif

#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define ORANGE  "\x1b[38;5;214m"
#define BLUE    "\x1b[34m"
#define PURPLE  "\x1b[35m"
#define RESET   "\x1b[0m"

#define CTLOG(level, fmt, ...) \
    do { \
        if (CTLOG_ENABLED_##level) { \
            FILE *ctlog_out = CTLOG_STREAM_##level; \
            fprintf(ctlog_out, CTLOG_COLOR_##level "[%s] " RESET fmt "\n", #level, ##__VA_ARGS__); \
        } \
    } while (0)

#ifdef LOG_LEVEL_DEBUG
    #define CTLOG_ENABLED_debug 1
#else
    #define CTLOG_ENABLED_debug 0
#endif

#ifdef LOG_LEVEL_INFO
    #define CTLOG_ENABLED_info 1
#else
    #define CTLOG_ENABLED_info 0
#endif

#ifdef LOG_LEVEL_WARNING
    #define CTLOG_ENABLED_warning 1
#else
    #define CTLOG_ENABLED_warning 0
#endif

#ifdef LOG_LEVEL_ERROR
    #define CTLOG_ENABLED_error 1
#else
    #define CTLOG_ENABLED_error 0
#endif

#ifdef LOG_LEVEL_FATAL
    #define CTLOG_ENABLED_fatal 1
#else
    #define CTLOG_ENABLED_fatal 0
#endif

#define CTLOG_COLOR_debug   GREEN
#define CTLOG_COLOR_info    BLUE
#define CTLOG_COLOR_warning ORANGE
#define CTLOG_COLOR_error   RED
#define CTLOG_COLOR_fatal   PURPLE

#define CTLOG_STREAM_debug   stdout
#define CTLOG_STREAM_info    stdout
#define CTLOG_STREAM_warning stdout
#define CTLOG_STREAM_error   stderr
#define CTLOG_STREAM_fatal   stderr
