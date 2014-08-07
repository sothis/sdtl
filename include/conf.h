#ifndef _CONF_H
#define _CONF_H

#include "sdtl.h"
#include <limits.h>
#include <stdio.h>
#if !defined(_MSC_VER)
#include <inttypes.h>
#else
#define PRIi64	"I64i"
#define PRIu64	"I64u"
#define strtoll _strtoi64
#endif

#if defined(__cplusplus)
namespace sdtl {
#if !defined (_MSC_VER)
#define PRIi64	"li"
#define PRIu64	"lu"
#endif
#endif

#define _CONF_MAX_STRUCT_NESTING	16
#define _CONF_MAX_FILESIZE_BYTES	16777216

#include "conf.c"

#if defined(__cplusplus)
}
#endif

#endif /* _CONF_H */
