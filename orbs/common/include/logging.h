//
// Created by Cyril on 17/06/2021.
//

#ifndef FIRMWARE_ORBS_COMMON_INCLUDE_LOGGING_H
#define FIRMWARE_ORBS_COMMON_INCLUDE_LOGGING_H


#ifndef SOURCE_PATH_SIZE
#define SOURCE_PATH_SIZE 0
#endif

#define __FILENAME__ ((const char *)__FILE__ + SOURCE_PATH_SIZE)

#include <stdio.h>

#ifndef CONFIG_LOG_LEVEL
#define LOG_LEVEL 0 // if variable has not been set, print nothing
#endif

#if CONFIG_LOG_LEVEL >= 4

#define LOG_DEBUG(...)                             \
 do {                                                   \
    printf("🟣 [%s:%d] ", __FILENAME__, __LINE__);       \
    printf(__VA_ARGS__);                                \
    printf("\r\n");                                     \
 } while(0)                                             \

#else

#define LOG_DEBUG(...)

#endif

#if CONFIG_LOG_LEVEL >= 3

#define LOG_INFO(...)                                   \
 do {                                                   \
    printf("🟢 [%s:%d] ", __FILENAME__, __LINE__);       \
    printf(__VA_ARGS__);                            \
    printf("\r\n");                                 \
 } while(0)                                             \

#else

#define LOG_INFO(...)

#endif

#if CONFIG_LOG_LEVEL >= 2

#define LOG_WARNING(...)                                   \
 do {                                                   \
    printf("🟠 [%s:%d] ", __FILENAME__, __LINE__);       \
    printf(__VA_ARGS__);                            \
    printf("\r\n");                                 \
 } while(0)                                             \

#else

#define LOG_WARNING(...)

#endif

#if CONFIG_LOG_LEVEL >= 1

#define LOG_ERROR(...)                                   \
 do {                                                   \
    printf("🔴 [%s:%d] ", __FILENAME__, __LINE__);       \
    printf(__VA_ARGS__);                            \
    printf("\r\n");                                 \
 } while(0)                                             \

#else

#define LOG_ERROR(...)

#endif

#endif //FIRMWARE_ORBS_COMMON_INCLUDE_LOGGING_H
