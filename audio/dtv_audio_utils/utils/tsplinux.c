#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "tsplinux.h"

void linux_log_assert(const char* cond, const char* tag,
                                              const char* fmt, ...) {
    char buf[LOG_BUF_SIZE];

    if (fmt) {
      va_list ap;
      va_start(ap, fmt);
      vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
      va_end(ap);
    } else {
      /* Msg not provided, log condition.  N.B. Do not use cond directly as
       * format string as it could contain spurious '%' syntax (e.g.
       * "%d" in "blocks%devs == 0").
       */
      if (cond)
        snprintf(buf, LOG_BUF_SIZE, "%s Assertion failed: %s",tag,cond);
      else
        strcpy(buf, "Unspecified assertion failed");
    }

    printf("%s\n",buf);

}
//get property
static unsigned int replacechar(char *dst,const char *src)
{
    unsigned int len = 0;

    if (strlen(src) > PROPERTY_VALUE_MAX-1)
        return len;

    while (len < strlen(src)) {
        if (src[len] == '.')
            dst[len] = '_';
        else
            dst[len] = src[len];
        len++;
    }
    dst[len] = '\0';
    return len;
}

int property_get(const char *key, char *value, const char *default_value) {

    int len = 0;
    char str[PROPERTY_VALUE_MAX] = {0};
    replacechar(str,key);
    char *result = getenv(str);
    if (result == NULL) {
        if (default_value) {
           len = strnlen(default_value, PROPERTY_VALUE_MAX - 1);
           memcpy(value, default_value, len);
           value[len] = '\0';
        }
    }else{
           len = strnlen(result, PROPERTY_VALUE_MAX - 1);
           memcpy(value, result, len);
           value[len] = '\0';
    }
    return len;
}

int android_errorWriteLog(int tag, const char* info)
{
    printf("!!!error info %d:%s\n",tag, info);
    return 0;
}
