#include "errors.h"

const char *
ret_code_to_str(ret_code_t code)
{
    switch (code) {
    case RET_SUCCESS:
        return "OK";
    case RET_ERROR_INTERNAL:
        return "INTERNAL";
    case RET_ERROR_NO_MEM:
        return "NO_MEM";
    case RET_ERROR_NOT_FOUND:
        return "NOT_FOUND";
    case RET_ERROR_INVALID_PARAM:
        return "INVALID_PARAM";
    case RET_ERROR_INVALID_STATE:
        return "INVALID_STATE";
    case RET_ERROR_INVALID_ADDR:
        return "INVALID_ADDR";
    case RET_ERROR_BUSY:
        return "BUSY";
    case RET_ERROR_OFFLINE:
        return "OFFLINE";
    case RET_ERROR_FORBIDDEN:
        return "FORBIDDEN";
    case RET_ERROR_TIMEOUT:
        return "TIMEOUT";
    case RET_ERROR_NOT_INITIALIZED:
        return "NOT_INITIALIZED";
    case RET_ERROR_ASSERT_FAILS:
        return "ASSERT_FAILS";
    case RET_ERROR_ALREADY_INITIALIZED:
        return "ALREADY_INITIALIZED";
    case RET_ERROR_NOT_SUPPORTED:
        return "NOT_SUPPORTED";
    case RET_ERROR_UNSAFE:
        return "UNSAFE";
        // no default to ensure all cases are handled and to avoid silent errors
    }
    return "UNKNOWN_ERROR";
}
