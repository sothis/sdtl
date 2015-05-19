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

typedef enum conf_array_type {
	array_contains_null,
	array_contains_long_long_int,
	array_contains_char,

	dimension_conf_array_type
} conf_array_type_t;

typedef enum conf_node_type {
	node_is_null,
	node_is_string,
	node_is_enum,
	node_is_integer,
	node_is_struct,
	node_is_array,

	dimension_conf_node_type
} conf_node_type_t;

typedef struct conf_node {
	conf_node_type_t	type;
	conf_array_type_t	array_type;
	char*			name;
	void*			value;
	uint64_t		length;
	uint64_t		items_per_row;
	int			struct_is_not_finalized;
	struct conf_node*	prev_in_scope;
	struct conf_node*	next_in_scope;
} conf_node_t;

typedef struct sdtlconf_ctx {
	int		sdtl_stream_has_started;
	conf_node_t	root;
	conf_node_t*	workspace;
	uint64_t	current_array_items;
	uint64_t	nopen_scopes;
	conf_node_t*	open_scopes[_CONF_MAX_STRUCT_NESTING+1];
} sdtlconf_ctx_t;

extern int conf_read
	(sdtlconf_ctx_t* c, const char* filename);

extern int conf_read_fd
	(sdtlconf_ctx_t* c, int fd);

extern void conf_cleanup
	(sdtlconf_ctx_t* c);

/* the memory which is returned by the following functions has to be freed
 * with conf_cleanup() */

extern const char* conf_get_enum_by_key
	(const sdtlconf_ctx_t* c, const char* key);

extern const int64_t* conf_get_int64_by_key
	(const sdtlconf_ctx_t* c, const char* key);

extern const char* conf_get_utf8string_by_key
		(const sdtlconf_ctx_t* c, const char* key);

extern const int64_t** conf_get_int64_array_by_key
(const sdtlconf_ctx_t* c, const char* key, uint64_t* rows, uint64_t* columns);

extern const char** conf_get_utf8string_array_by_key
(const sdtlconf_ctx_t* c, const char* key, uint64_t* rows, uint64_t* columns);


#if defined(__cplusplus)
}
#endif

#endif /* _CONF_H */
