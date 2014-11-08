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


void _indent_with_tab(size_t level)
{
	while (level) {
		printf("\t");
		level--;
	}
}

void _print_escaped_string(size_t lvl, const char* name, const char* str, int w)
{
	size_t i;
	size_t l = strlen(str);
	if (w) {
		_indent_with_tab(lvl);
		printf(".%s = \"", name);
	} else
		printf(".%s=\"", name);

	for (i = 0; i < l; ++i) {
		if (str[i] == '"' || str[i] == '\\') {
			printf("%c", '\\');
		}
		printf("%c", str[i]);
	}

	if (w)
		printf("\";\n");
	else
		printf("\";");
}

void _print_escaped_array_string(const char* str)
{
	size_t i;
	size_t l = strlen(str);

	printf("\"");
	for (i = 0; i < l; ++i) {
		if (str[i] == '"' || str[i] == '\\') {
			printf("%c", '\\');
		}
		printf("%c", str[i]);
	}
	printf("\"");
}

void _print_num_array(conf_node_t* n)
{
	uint64_t row, column;
	int64_t* v;
	int64_t** values = (int64_t**)n->value;

	for (row = 0; row < n->length; ++row) {
		printf("[");
		for (column = 0; column < (n->items_per_row-1); ++column) {
			v = values[row*n->items_per_row + column];

			if (v)
				printf("%" PRIi64 ",", *v);
			else
				printf(",");
		}
		v = values[row*n->items_per_row + n->items_per_row-1];
		if (v)
			printf("%" PRIi64 "", *v);

		printf("]");
	}
}

void _print_string_array(conf_node_t* n)
{
	uint64_t row, column;
	char* v;
	char** values = (char**)n->value;

	for (row = 0; row < n->length; ++row) {
		printf("[");
		for (column = 0; column < (n->items_per_row-1); ++column) {
			v = values[row*n->items_per_row + column];

			if (v)
				_print_escaped_array_string(v), printf(",");
			else
				printf(",");
		}
		v = values[row*n->items_per_row + n->items_per_row-1];
		if (v)
			_print_escaped_array_string(v);

		printf("]");
	}
}

void _print_nodes_recursive(size_t level, conf_node_t* first, int w)
{
	conf_node_t* e = first;
	int whitespace = w;
	int64_t i;

	while (e) {
		if (e->type == node_is_struct) {
			/* NOTE: may cause stackoverflow if nesting level
			 * is too high, use an own stack in order to
			 * be able to apply a configurable limit */
			if (whitespace)
				_indent_with_tab(level);
			if (!e->value) {
				/* struct is empty */
				if (whitespace)
					printf(".%s (%" PRIu64
					" members) = {};\n", e->name,
					e->length);
				else
					printf(".%s={};", e->name);
			} else {
				if (whitespace)
					printf(".%s (%" PRIu64
					" members) = {\n", e->name, e->length);
				else
					printf(".%s={", e->name);

				_print_nodes_recursive(level+1,
					(conf_node_t*)e->value, whitespace);
				if (whitespace) {
					_indent_with_tab(level);
					printf("};\n");
				} else
					printf("};");
			}
		}
		else if (e->type == node_is_null) {
			if (whitespace) {
				_indent_with_tab(level);
				printf(".%s = ;\n", e->name);
			} else
				printf(".%s=;", e->name);
		} else if (e->type == node_is_string) {
			_print_escaped_string(level, e->name,
				(const char*)e->value, whitespace);
		} else if (e->type == node_is_integer) {
			i = *(int64_t*)e->value;
			if (whitespace) {
				_indent_with_tab(level);
				printf(".%s = %" PRIi64 ";\n", e->name, i);
			} else
				printf(".%s=%" PRIi64 ";", e->name, i);
		} else if (e->type == node_is_array) {
			if (whitespace) {
				_indent_with_tab(level);
				printf(".%s (%" PRIu64 "x%" PRIu64 ") = ",
					e->name, e->items_per_row, e->length);
				if (e->array_type ==
				array_contains_long_long_int)
					_print_num_array(e);
				else
					_print_string_array(e);
				printf("; (type: %d) \n", e->array_type);
			} else
				printf(".%s=[];", e->name);
		}
		e = e->next_in_scope;
	}
}

void _print_nodes(sdtlconf_ctx_t* c, int use_whitespace)
{
	printf("root has %" PRIu64 " members\n", c->root.length);
	_print_nodes_recursive(0, (conf_node_t*)c->root.value, use_whitespace);
}

int _start_new_node(sdtlconf_ctx_t* c, const char* name, uint16_t length)
{
	conf_node_t* previous_node = c->workspace;

	c->workspace = (conf_node_t*) calloc(1, sizeof(conf_node_t));
	if (!c->workspace)
		return -1;

	c->workspace->name = (char*) malloc(length+1);
	if (!c->workspace->name)
		return -1;
	memmove(c->workspace->name, name, length+1);
	c->workspace->prev_in_scope = previous_node;

	if (previous_node->struct_is_not_finalized && !previous_node->value) {
		previous_node->value = c->workspace;
	} else {
		previous_node->next_in_scope = c->workspace;
	}

	return 0;
}

void _increase_current_struct_member_count(sdtlconf_ctx_t* c)
{
	conf_node_t* current_scope = c->open_scopes[c->nopen_scopes-1];
	current_scope->length++;
}

int _enter_scope(sdtlconf_ctx_t* c)
{
	/* full */
	if (c->nopen_scopes == sizeof(c->open_scopes)/sizeof(conf_node_t*)) {
		return -1;
	}
	c->workspace->type = node_is_struct;
	c->workspace->length = 0;
	c->workspace->struct_is_not_finalized = 1;
	c->open_scopes[c->nopen_scopes] = c->workspace;
	c->nopen_scopes++;
	return 0;
}

int _leave_scope(sdtlconf_ctx_t* c)
{
	/* empty */
	if (!c->nopen_scopes)
		return -1;
	c->nopen_scopes--;
	c->workspace = c->open_scopes[c->nopen_scopes];
	c->workspace->struct_is_not_finalized = 0;
	return 0;
}

int _add_data(sdtlconf_ctx_t* c, sdtl_data_t* data)
{
	int64_t i;
	char* end = 0;
	void* tmp;
	int64_t** ip_array;
	char** cp_array;
	void** vp_array;
	uint64_t item;

	if (c->workspace->type == node_is_array) {
		if (c->workspace->array_type == array_contains_null) {
			if (data->type == datatype_utf8string)
				c->workspace->array_type = array_contains_char;
			if (data->type == datatype_number)
				c->workspace->array_type =
					array_contains_long_long_int;
		}

		tmp = realloc(c->workspace->value,
			c->current_array_items*sizeof(void*));
		if (!tmp)
			return -1;
		c->workspace->value = tmp;
		item = c->current_array_items - 1;

		if (data->type == datatype_null_value) {
			vp_array = (void**)(c->workspace->value);
			vp_array[item] = 0;
		}

		if (data->type == datatype_utf8string) {
			cp_array = (char**)(c->workspace->value);
			cp_array[item] = (char*) malloc(data->length + 1);
			if (!cp_array[item])
				return -1;
			memmove(cp_array[item], data->data, data->length + 1);
		}

		if (data->type == datatype_number) {
			ip_array = (int64_t**)(c->workspace->value);
			ip_array[item] = (int64_t*) malloc(sizeof(int64_t));
			if (!ip_array[item])
				return -1;
			end = 0;
			i = (int64_t)strtoll((const char*)data->data, &end, 0);
			if ((!end) || (*end != 0) ||
			(i == LLONG_MIN) || (i == LLONG_MAX)) {
				return -1;
			}
			memmove(ip_array[item], &i, sizeof(int64_t));
		}

		return 0;
	}

	switch (data->type) {
		/* note: we ignore data->value_type (the type tag) here
		 * and assume following defaults:
		 * - strings are interpreted as SDTL default, which is UTF-8
		 * - integers are stored as int64_t with correspondent range
		 * limitations
		 * */
		case datatype_utf8string:
			c->workspace->type = node_is_string;
			c->workspace->length = data->length + 1;
			c->workspace->value = malloc(c->workspace->length);
			if (!c->workspace->value)
				return -1;
			memmove(c->workspace->value, data->data,
				c->workspace->length);
			break;
		case datatype_number:
			c->workspace->type = node_is_integer;
			c->workspace->length = sizeof(int64_t);
			c->workspace->value = malloc(c->workspace->length);
			if (!c->workspace->value)
				return -1;

			end = 0;
			i = (int64_t)strtoll((const char*)data->data, &end, 0);
			if ((!end) || (*end != 0) ||
			(i == LLONG_MIN) || (i == LLONG_MAX)) {
				return -1;
			}
			memmove(c->workspace->value, &i, c->workspace->length);
			break;
		case datatype_unit:
			/* note: we don't care about units for now */
			break;
		case datatype_null_value:
			c->workspace->type = node_is_null;
			c->workspace->length = 0;
			c->workspace->value = 0;
			break;
		case datatype_enum:
			c->workspace->type = node_is_enum;
			c->workspace->length = data->length + 1;
			c->workspace->value = malloc(c->workspace->length);
			if (!c->workspace->value)
				return -1;
			memmove(c->workspace->value, data->data,
				c->workspace->length);
			break;
		case datatype_symlink:
		case datatype_octet_stream:
		default:
			return -1;
	}
	return 0;
}

int _on_sdtl_event(void* userdata, sdtl_event_t e, sdtl_data_t* data)
{
	sdtlconf_ctx_t* c = (sdtlconf_ctx_t*)userdata;

	if (!c->sdtl_stream_has_started) {
		if (e != ev_assignment_start)
			return -1;
		c->root.type = node_is_struct;
		c->root.name = (char*)(const char*)"";
		c->root.value = 0;
		c->root.length = 0;
		c->root.struct_is_not_finalized = 1;
		c->root.next_in_scope = 0;
		c->root.prev_in_scope = 0;
		c->workspace = &c->root;
		c->nopen_scopes = 0;
		c->open_scopes[c->nopen_scopes] = &c->root;
		c->nopen_scopes++;
		c->sdtl_stream_has_started = 1;
	}

	switch (e) {
		case ev_assignment_start:
			c->current_array_items = 0;
			_increase_current_struct_member_count(c);
			return _start_new_node(c, (char*)data->data,
				data->length);
		case ev_data:
			if (c->workspace->type == node_is_array)
				c->current_array_items++;
			return _add_data(c, data);
		case ev_struct_start:
			return _enter_scope(c);
		case ev_struct_end:
			return _leave_scope(c);
		case ev_array_new_row:
			c->workspace->length++;
			c->workspace->type = node_is_array;
			break;
		case ev_array_end_row:
			if (!c->workspace->items_per_row) {
				c->workspace->items_per_row =
					c->current_array_items;
			}
			break;
		/* disallow octet streams in config files */
		case ev_octet_stream_start:
		case ev_octet_stream_end:
		default:
			return -1;
	}

	return 0;
}

void _free_array_values(conf_node_t* n)
{
	uint64_t row, column;
	void* v;
	void** values = (void**)n->value;

	for (row = 0; row < n->length; ++row) {
		for (column = 0; column < n->items_per_row; ++column) {
			v = values[row*n->items_per_row + column];
			if (v)
				free(v);
		}
	}
}

void _free_nodes_recursive(conf_node_t* first)
{
	conf_node_t* e = first;
	conf_node_t* n = 0;

	while (e) {
		if (e->type == node_is_struct) {
			if (e->value) {
				_free_nodes_recursive((conf_node_t*)e->value);
				free(e->value);
			}
			free(e->name);
		}
		else if (e->type == node_is_null) {
			free(e->name);
		} else if (e->type == node_is_string) {
			free(e->name);
			free(e->value);
		} else if (e->type == node_is_enum) {
			free(e->name);
			free(e->value);
		} else if (e->type == node_is_integer) {
			free(e->name);
			free(e->value);
		} else if (e->type == node_is_array) {
			_free_array_values(e);
			free(e->name);
			free(e->value);
		}
		n = e->next_in_scope;
		if (e != first) {
			free(e);
		}
		e = n;
	}
}

void conf_cleanup(sdtlconf_ctx_t* c)
{
	_free_nodes_recursive((conf_node_t*)c->root.value);
	free(c->root.value);
	memset(c, 0, sizeof(sdtlconf_ctx_t));
}

int conf_read_fd(sdtlconf_ctx_t* c, int fd)
{
	sdtl_read_fd_t rfd;
	sdtl_read_flags_t opts;

	memset(c, 0, sizeof(sdtlconf_ctx_t));
	memset(&opts, 0, sizeof(sdtl_read_flags_t));
	opts.on_event = &_on_sdtl_event;
	opts.userdata = c;
	opts.max_struct_nesting = _CONF_MAX_STRUCT_NESTING;
	opts.max_file_size = opts.max_text_bytes = _CONF_MAX_FILESIZE_BYTES;

	if (sdtl_open_read(&rfd, fd, &opts)) {
		return -1;
	}
	if (sdtl_read(&rfd)) {
		fprintf(stderr, "the parser has interrupted its work "
			"(error %d) @ '%c'\n", sdtl_get_error(&rfd), rfd.byte);
		return -1;
	}

	return 0;
}

int conf_read(sdtlconf_ctx_t* c, const char* filename)
{
	int fd;

	fd = open(filename, O_RDONLY | O_NOATIME);
	if (fd < 0)
		return -1;
	return conf_read_fd(c, fd);
}

const conf_node_t*
_get_conf_node_flat(const conf_node_t* parent_struct, const char* name)
{
	const conf_node_t* e = parent_struct;

	if (!name || !*name || (*name == '.'))
		return 0;

	while (e) {
		if (!strcmp(e->name, name))
			break;
		e = e->next_in_scope;
	}
	return e;
}

const conf_node_t* conf_get_conf_node(const sdtlconf_ctx_t* c, const char* path)
{
	char* abspath, *component, *temp;
	const conf_node_t* first = (const conf_node_t*)c->root.value;
	const conf_node_t* curr_node = first;
	const conf_node_t* e = 0;
	const conf_node_t* r = 0;

	/* only allow absolute paths (starting with dot) at the moment */
	if (!path || !*path || (*path != '.'))
		return 0;

	/* special root member case */
	if (!strcmp(path, ".")) {
		return &c->root;
	}

	/* path mustn't end with '.', exception is a single dot for the root
	 * structure */
	if (*(strrchr(path, 0)-1) == '.')
		return 0;

	abspath = strdup(path);
	if (!abspath)
		return 0;

	temp = abspath;
	while ((component = strtok(temp, "."))) {
		temp = 0;

		if (r) {
			if (r->type == node_is_struct) {
				/* dive into structure */
				curr_node = (conf_node_t*)r->value;
			} else {
				/* The current node is not a structure, but
				 * we still have components in the path.
				 * This is exactly ENOTDIR in the context
				 * of POSIX paths. */
				r = 0;
				break;
			}
		}
		e = _get_conf_node_flat(curr_node, component);
		if (!e) {
			/* no such structure member in curr_node */
			r = 0;
			break;
		} else {
			r = e;
			continue;
		}
	}

	free(abspath);
	return r;
}

const char* conf_get_utf8string_by_key(const sdtlconf_ctx_t* c, const char* key)
{
	const conf_node_t* key_node = 0;

	if (!c || !c->root.value)
		return 0;

	key_node = conf_get_conf_node(c, key);
	if (!key_node)
		return 0;

	if (key_node->type == node_is_string)
		return (const char*) key_node->value;
	else
		return 0;
}

const char* conf_get_enum_by_key(const sdtlconf_ctx_t* c, const char* key)
{
	const conf_node_t* key_node = 0;

	if (!c || !c->root.value)
		return 0;

	key_node = conf_get_conf_node(c, key);
	if (!key_node)
		return 0;

	if (key_node->type == node_is_enum)
		return (const char*) key_node->value;
	else
		return 0;
}

const char** conf_get_utf8string_array_by_key
(const sdtlconf_ctx_t* c, const char* key, uint64_t* rows, uint64_t* columns)
{
	const conf_node_t* key_node = 0;

	if (!c || !c->root.value)
		return 0;

	key_node = conf_get_conf_node(c, key);
	if (!key_node)
		return 0;

	if (key_node->type == node_is_array &&
	key_node->array_type == array_contains_char) {
		if (rows)
			*rows = key_node->length;
		if (columns)
			*columns = key_node->items_per_row;
		return (const char**) key_node->value;
	} else
		return 0;
}

const int64_t* conf_get_int64_by_key(const sdtlconf_ctx_t* c, const char* key)
{
	const conf_node_t* key_node = 0;

	if (!c || !c->root.value)
		return 0;

	key_node = conf_get_conf_node(c, key);
	if (!key_node)
		return 0;

	if (key_node->type == node_is_integer)
		return (int64_t*)key_node->value;
	else
		return 0;
}

const int64_t** conf_get_int64_array_by_key
(const sdtlconf_ctx_t* c, const char* key, uint64_t* rows, uint64_t* columns)
{
	const conf_node_t* key_node = 0;

	if (!c || !c->root.value)
		return 0;

	key_node = conf_get_conf_node(c, key);
	if (!key_node)
		return 0;

	if (key_node->type == node_is_array &&
	key_node->array_type == array_contains_long_long_int) {
		if (rows)
			*rows = key_node->length;
		if (columns)
			*columns = key_node->items_per_row;
		return (const int64_t**) key_node->value;
	} else
		return 0;
}
