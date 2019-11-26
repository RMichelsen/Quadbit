#pragma once

#ifndef NDEBUG
#include <assert.h>
#define QB_ASSERT(x) assert(x)
#define QB_LOG_INFO(...) ( printf("Quadbit Logger [INFO]: ") , printf(__VA_ARGS__) )
#define QB_LOG_WARN(...) ( printf("Quadbit Logger [WARN]: ") , printf(__VA_ARGS__) )
#define QB_LOG_ERROR(...) ( printf("Quadbit Logger [ERROR]: ") , printf(__VA_ARGS__) , QB_ASSERT(false) )
#else
#define QB_ASSERT(x) do { (void)sizeof(x);} while (0)
#define QB_LOG_INFO(...) do { (void)sizeof(__VA_ARGS__);} while (0)
#define QB_LOG_WARN(...) do { (void)sizeof(__VA_ARGS__);} while (0)
#define QB_LOG_ERROR(...) do { (void)sizeof(__VA_ARGS__);} while (0)
#endif