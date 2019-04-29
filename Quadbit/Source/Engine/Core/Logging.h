#pragma once

#ifdef QBDEBUG
#define QB_LOG_INFO(...) ( printf("Quadbit Logger [INFO]: ") , printf(__VA_ARGS__) )
#define QB_LOG_WARN(...) ( printf("Quadbit Logger [WARN]: ") , printf(__VA_ARGS__) )
#define QB_LOG_ERROR(...) ( printf("Quadbit Logger [ERROR]: ") , printf(__VA_ARGS__) , assert(false) )
#else
#define QB_LOG_INFO
#define QB_LOG_WARN
#define QB_LOG_ERROR
#endif