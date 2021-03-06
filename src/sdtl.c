#include "sdtl.h"

/*
 * TODO:
 *
 * The octet stream feature of SDTL is currently implemented as such:
 * <var> = {0x00} {l}{h} <chunk data> {l}{h} <chunk data> {0x00} {0x00} {0x00}
 * with l and h being the little endian representation of the chunksize
 * of that chunk, which follows directly afterwards.
 * The problem with this solution is, that the chunksize can't be 65536.
 *
 * ideas:
 *	- represent the chunksize of 65536 as {0x00}{0x00}. the problem with
 *	  that is, that it would make the end marker ({0x00}{0x00}{0x00})
 *	  ambiguous
 *	- make chunksize 4 byte wide. it would add 2 additional bytes per chunk
 *	  to the stream and we would have to define a wider end marker
 *	- add one byte preceding the chunksize, which is used as a tag
 *	  (comparable to TLV encoding in ASN.1/DER) to differ between chunksize
 *	  ({0x00}{0x00} would be interpreted then as 65536) and octet stream
 *	  end marker. this would add 1 additional byte per chunk
 *
*/

#define SDTL_READ_BUFFER_SIZE	65536

typedef int (*action_t)(struct sdtl_read_fd_t* p);

const char* const _value_types[dimension_value_type] = {
	"plain",
	"utf8",
	"base02", "base03", "base04", "base05", "base06", "base07",
	"base08", "base09", "base10", "base11", "base12", "base13",
	"base14", "base15", "base16", "base17", "base18", "base19",
	"base20", "base21", "base22", "base23", "base24", "base25",
	"base26", "base27", "base28", "base29", "base30", "base31",
	"base32", "base33", "base34", "base35", "base36", "base37",
	"base38", "base39", "base40", "base41", "base42", "base43",
	"base44", "base45", "base46", "base47", "base48", "base49",
	"base50", "base51", "base52", "base53", "base54", "base55",
	"base56", "base57", "base58", "base59", "base60", "base61",
	"base62",
	"base63", "base64", "base85",
	"ip-socket",
	"iso8601",

	"int8",
	"uint8",
	"int16",
	"uint16",
	"int32",
	"uint32",
	"int64",
	"uint64",
	"int128",
	"uint128",
	"int256",
	"uint256",
	"int512",
	"uint512",
	"int1024",
	"uint1024"
};

const char* const _value_units[dimension_value_unit] = {
	"none",
	"b", "s", "ys", "g", "yg"
};

value_type_t _get_value_type(const unsigned char* type)
{
	int i;
	for (i = 0; i < dimension_value_type; ++i) {
		if (!strcmp((const char*)type, _value_types[i]))
			return (value_type_t)i;
	}
	return st_illegal;
}

action_t _after_state[dimension_state][uint8_max+1];


static inline int xwrite(int fd, void* data, uint16_t length)
{
	uint16_t total = 0;
	uint16_t written = 0;
	while (total != length) {
		written = write(fd, data + total, length - total);
		if (written <= 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		total += written;
	}
	return 0;
}

int _finalize_binary_chunk
(sdtl_read_fd_t* p, unsigned char* data, uint16_t length)
{
	sdtl_data_t d;

	if (!length) {
		p->token_type = token_is_unknown;
		if (p->opts.on_event(p->userdata, ev_octet_stream_end, 0)) {
			p->last_error = error_scanner_callback_returned_nonzero;
			return -1;
		}
		return 0;
	}
	d.type = datatype_octet_stream;
	d.length = length;
	d.data = data;

	if (p->opts.on_event(p->userdata, ev_data, &d)) {
		p->last_error = error_scanner_callback_returned_nonzero;
		return -1;
	}
	return 0;
}

int _finalize_current_token
(sdtl_read_fd_t* p)
{
	sdtl_data_t data;

	/* if we hit this, the given structure is empty. just ignore and
	 * don't generate an event. */
	if (p->token_type == token_is_structure) {
		p->token_type = token_is_unknown;
		return 0;
	}

	/* handle possible second invocation through _do_value_outro()
	 * after array or (not empty) struct end, just return */
	if (p->token_type == token_is_unknown) {
		return 0;
	}

	p->str_data[p->str_len] = 0;
	p->type_data[p->type_len] = 0;
	p->unit_data[p->unit_len] = 0;

	if (p->token_type == token_is_identifier) {
		if (!p->str_len) {
			p->last_error = error_empty_identifier;
			return -1;
		}
	}

	if (p->token_type == token_is_type) {
		p->value_type = _get_value_type(p->type_data);
		if (p->value_type == st_illegal) {
			p->last_error = error_illegal_value_type;
			return -1;
		}
		return 0;
	}

	if (p->token_type == token_is_unit) {
		data.data =  p->unit_data;
		data.length = p->unit_len;
	} else {
		data.data = (p->token_type != token_is_null_value) ?
			p->str_data : 0;
		data.length = (p->token_type != token_is_null_value) ?
			p->str_len : (uint16_t)0;
	}

	if (p->token_type == token_is_array_number)
		data.type = datatype_number;
	else if (p->token_type == token_is_array_utf8string)
		data.type = datatype_utf8string;
	else
		data.type = (data_type_t)p->token_type;

	data.value_type = p->value_type;


	if (p->token_type == token_is_identifier) {
		if (p->opts.on_event(p->userdata, ev_assignment_start, &data)) {
			p->last_error = error_scanner_callback_returned_nonzero;
			return -1;
		}
	} else if (p->opts.on_event(p->userdata, ev_data, &data)) {
		p->last_error = error_scanner_callback_returned_nonzero;
		return -1;
	}

	p->type_len = 0;
	p->unit_len = 0;
	p->str_len = 0;
	/* make sure that a second call to _finalize_str_data() without
	 * token type change is a noop */
	p->token_type = token_is_unknown;
	return 0;
}

/* state transition actions */
int _do_ignore_whitespace
(sdtl_read_fd_t* p)
{
	/* just keep current state, do nothing */
	return 0;
}

int _do_comment_intro
(sdtl_read_fd_t* p)
{
	p->last_state = p->next_state;
	p->next_state = comment_intro;
	return 0;
}

int _do_ignore_comment_byte
(sdtl_read_fd_t* p)
{
	p->next_state = ignore_comment_byte;
	return 0;
}

int _do_comment_outro
(sdtl_read_fd_t* p)
{
	/* restore state to the state before the comment intro */
	p->next_state = p->last_state;
	return 0;
}

int _do_copy_utf8bom_byte
(sdtl_read_fd_t* p)
{
	if (!p->bom_len) {
		p->last_state = p->next_state;
	}
	if (p->bom_len == 2) {
		if ((p->bom[0] == 0xef) &&
		(p->bom[1] == 0xbb) &&
		(p->byte == 0xbf)) {
			p->bom_len = 0;
			if (p->last_state == utf8string_intro)
				p->next_state = copy_utf8string_byte;
			else if (p->last_state == undefined)
				p->next_state = first_bom;
			else if (p->last_state == arr_utf8string_intro)
				p->next_state = copy_arr_utf8string_byte;
			else
				return -1;
			return 0;
		}
		p->last_error = error_invalid_byte_order_mark;
		return -1;
	}

	p->bom[p->bom_len] = (unsigned char)p->byte;
	p->bom_len++;

	p->next_state = utf8bom_intro;
	return 0;
}

int _do_copy_identifier_byte
(sdtl_read_fd_t* p)
{
	if (p->str_len == p->opts.max_identifier_length) {
		p->last_error = error_identifier_too_long;
		return -1;
	}
	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;

	p->next_state = identifier_intro;
	return 0;
}

int _do_identifier_intro
(sdtl_read_fd_t* p)
{
	p->str_len = 0;
	p->token_type = token_is_identifier;
	p->next_state = identifier_intro;
	return 0;
}

int _do_value_intro
(sdtl_read_fd_t* p)
{
	/* identifier */
	if (_finalize_current_token(p))
		return -1;

	p->str_len = 0;
	p->array_items = 0;
	p->token_type = token_is_null_value;
	p->next_state = value_intro;
	return 0;
}

int _do_value_outro
(sdtl_read_fd_t* p)
{
	/* note: might be called twice after struct and array outro */
	if (_finalize_current_token(p))
		return -1;

	p->is_num_array = 0;
	p->is_string_array = 0;
	p->array_row_size = 0;
	/* the string type remain valid until semicolon */
	p->value_type = st_plain;
	p->next_state = value_outro;
	return 0;
}

int _do_octet_stream_intro
(sdtl_read_fd_t* p)
{
	p->token_type = token_is_octet_stream;
	p->next_state = octet_stream_intro;
	if (p->opts.on_event(p->userdata, ev_octet_stream_start, 0)) {
		p->last_error = error_scanner_callback_returned_nonzero;
		return -1;
	}
	return 0;
}

int _do_struct_intro
(sdtl_read_fd_t* p)
{
	if (p->struct_nesting_level == p->opts.max_struct_nesting) {
		p->last_error = error_struct_nesting_too_high;
		return -1;
	}
	p->struct_nesting_level++;
	if (p->opts.on_event(p->userdata, ev_struct_start, 0)) {
		p->last_error = error_scanner_callback_returned_nonzero;
		return -1;
	}
	p->token_type = token_is_structure;
	p->next_state = struct_intro;
	return 0;
}

int _do_struct_outro
(sdtl_read_fd_t* p)
{
	if (!p->struct_nesting_level) {
		p->last_error = error_illegal_struct_close;
		return -1;
	}
	p->struct_nesting_level--;
	if (p->opts.on_event(p->userdata, ev_struct_end, 0)) {
		p->last_error = error_scanner_callback_returned_nonzero;
		return -1;
	}
	p->next_state = struct_outro;
	return 0;
}

int _do_array_intro
(sdtl_read_fd_t* p)
{
#if 0
	if (p->type_len) {
		p->is_string_array = 1;
	}
#endif

	p->str_len = 0;
	/* an array consists of at least one null value (.a = [];) */
	if (p->opts.max_array_items &&
	(p->array_items == p->opts.max_array_items)) {
		p->last_error = error_too_much_array_items;
		return -1;
	}
	p->array_items++;

	p->curr_row_size++;
	p->token_type = token_is_null_value;
	p->next_state = array_intro;
	if (p->opts.on_event(p->userdata, ev_array_new_row, 0)) {
		p->last_error = error_scanner_callback_returned_nonzero;
		return -1;
	}
	return 0;
}

int _do_new_array_value
(sdtl_read_fd_t* p)
{
	if (_finalize_current_token(p))
		return -1;

	p->str_len = 0;

	if (p->opts.max_array_items &&
	(p->array_items == p->opts.max_array_items)) {
		p->last_error = error_too_much_array_items;
		return -1;
	}
	p->array_items++;

	if (p->array_row_size && (p->array_row_size == p->curr_row_size)) {
		p->last_error = error_array_row_size_mismatch;
		return -1;
	}
	p->curr_row_size++;
	p->token_type = token_is_null_value;
	p->next_state = new_array_value;
	return 0;
}

int _do_array_outro
(sdtl_read_fd_t* p)
{
	if (_finalize_current_token(p))
		return -1;

	if (!p->array_row_size)
		p->array_row_size = p->curr_row_size;
	else if (p->array_row_size != p->curr_row_size) {
		p->last_error = error_array_row_size_mismatch;
		return -1;
	}
	p->curr_row_size = 0;
	p->next_state = array_outro;
	if (p->opts.on_event(p->userdata, ev_array_end_row, 0)) {
		p->last_error = error_scanner_callback_returned_nonzero;
		return -1;
	}
	return 0;
}

int _do_utf8string_intro
(sdtl_read_fd_t* p)
{
	p->token_type = token_is_utf8string;
	p->next_state = utf8string_intro;
	return 0;
}

int _do_copy_utf8string_byte
(sdtl_read_fd_t* p)
{
	if (p->str_len == p->opts.max_utf8string_length) {
		p->last_error = error_utf8string_too_long;
		return -1;
	}
	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;

	p->next_state = copy_utf8string_byte;
	return 0;
}

int _do_replace_escaped_byte
(sdtl_read_fd_t* p)
{
	if (p->str_len == p->opts.max_utf8string_length) {
		p->last_error = error_utf8string_too_long;
		return -1;
	}

	switch (p->byte) {
		case 't':
			p->byte = '\t';
			break;
		case 'n':
			p->byte = '\n';
			break;
		case 'v':
			p->byte = '\v';
			break;
		case 'f':
			p->byte = '\f';
			break;
		case 'r':
			p->byte = '\r';
			break;
		case '"':
			p->byte = '"';
			break;
		case '\\':
			p->byte = '\\';
			break;
		default:
			p->last_error = error_illegal_escape_sequence;
			return -1;
	}

	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;
	p->next_state = copy_utf8string_byte;
	return 0;
}

int _do_escape_from_copy
(sdtl_read_fd_t* p)
{
	p->next_state = escape_from_copy;
	return 0;
}

int _do_utf8string_outro
(sdtl_read_fd_t* p)
{
	p->next_state = utf8string_outro;
	return 0;
}

int _do_nullstring_intro
(sdtl_read_fd_t* p)
{
	p->next_state = nullstring_intro;
	return 0;
}

int _do_ign_nullstring_byte
(sdtl_read_fd_t* p)
{
	p->next_state = ign_nullstring_byte;
	return 0;
}

int _do_ign_escaped_byte
(sdtl_read_fd_t* p)
{
	p->next_state = ign_nullstring_byte;
	return 0;
}

int _do_escape_from_ign
(sdtl_read_fd_t* p)
{
	p->next_state = escape_from_ign;
	return 0;
}

int _do_nullstring_outro
(sdtl_read_fd_t* p)
{
	p->next_state = nullstring_outro;
	return 0;
}

int _do_arr_utf8string_intro
(sdtl_read_fd_t* p)
{
	if (p->is_num_array) {
		p->last_error = error_expected_number_in_array;
		return -1;
	}
	p->is_string_array = 1;
	p->token_type = token_is_array_utf8string;
	p->next_state = arr_utf8string_intro;
	return 0;
}

int _do_copy_arr_utf8string_byte
(sdtl_read_fd_t* p)
{
	if (p->str_len == p->opts.max_utf8string_length) {
		p->last_error = error_utf8string_too_long;
		return -1;
	}
	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;

	p->next_state = copy_arr_utf8string_byte;
	return 0;
}

int _do_arr_replace_escaped_byte
(sdtl_read_fd_t* p)
{
	if (p->str_len == p->opts.max_utf8string_length) {
		p->last_error = error_utf8string_too_long;
		return -1;
	}

	switch (p->byte) {
		case 't':
			p->byte = '\t';
			break;
		case 'n':
			p->byte = '\n';
			break;
		case 'v':
			p->byte = '\v';
			break;
		case 'f':
			p->byte = '\f';
			break;
		case 'r':
			p->byte = '\r';
			break;
		case '"':
			p->byte = '"';
			break;
		case '\\':
			p->byte = '\\';
			break;
		default:
			p->last_error = error_illegal_escape_sequence;
			return -1;
	}

	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;
	p->next_state = copy_arr_utf8string_byte;
	return 0;
}

int _do_arr_escape_from_copy
(sdtl_read_fd_t* p)
{
	p->next_state = arr_escape_from_copy;
	return 0;
}

int _do_arr_utf8string_outro
(sdtl_read_fd_t* p)
{
	p->next_state = arr_utf8string_outro;
	return 0;
}

int _do_type_intro
(sdtl_read_fd_t* p)
{
	p->token_type = token_is_type;
	p->next_state = type_intro;
	return 0;
}

int _do_copy_type_byte
(sdtl_read_fd_t* p)
{
	if (p->type_len == sizeof(p->type_data)-1) {
		p->last_error = error_type_too_long;
		return -1;
	}
	p->type_data[p->type_len] = (unsigned char)p->byte;
	p->type_len++;

	p->next_state = copy_type_byte;
	return 0;
}

int _do_type_outro
(sdtl_read_fd_t* p)
{
	if (_finalize_current_token(p))
		return -1;

	p->next_state = type_outro;
	return 0;
}

int _do_copy_unit_byte
(sdtl_read_fd_t* p)
{
	if (!p->unit_len) {
		if (p->token_type == token_is_number ||
		p->token_type == token_is_utf8string) {
			if (_finalize_current_token(p))
				return -1;
		}
		p->token_type = token_is_unit;
	}
	if (p->unit_len == sizeof(p->unit_data)-1) {
		p->last_error = error_unit_too_long;
		return -1;
	}
	p->unit_data[p->unit_len] = (unsigned char)p->byte;
	p->unit_len++;

	p->next_state = copy_unit_byte;
	return 0;
}

int _do_neg_number_intro
(sdtl_read_fd_t* p)
{
	if (!p->str_len) {
		p->token_type = token_is_number;
	}
	if (p->str_len == p->opts.max_number_length) {
		p->last_error = error_number_too_long;
		return -1;
	}
	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;

	p->next_state = neg_number_intro;
	return 0;
}

int _do_copy_number_byte
(sdtl_read_fd_t* p)
{
	if (!p->str_len) {
		p->token_type = token_is_number;
	}
	if (p->str_len == p->opts.max_number_length) {
		p->last_error = error_number_too_long;
		return -1;
	}
	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;

	p->next_state = copy_number_byte;
	return 0;
}

int _do_fraction_intro
(sdtl_read_fd_t* p)
{
	if (p->str_len == p->opts.max_number_length) {
		p->last_error = error_number_too_long;
		return -1;
	}
	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;

	p->next_state = fraction_intro;
	return 0;
}

int _do_copy_fraction_byte
(sdtl_read_fd_t* p)
{
	if (p->str_len == p->opts.max_number_length) {
		p->last_error = error_number_too_long;
		return -1;
	}
	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;

	p->next_state = copy_fraction_byte;
	return 0;
}

int _do_neg_arr_number_intro
(sdtl_read_fd_t* p)
{
	if (!p->str_len) {
		if (p->is_string_array) {
			p->last_error = error_expected_string_in_array;
			return -1;
		}
		p->token_type = token_is_array_number;
		p->is_num_array = 1;
	}
	if (p->str_len == p->opts.max_number_length) {
		p->last_error = error_number_too_long;
		return -1;
	}
	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;

	p->next_state = neg_arr_number_intro;
	return 0;
}

int _do_copy_arr_number_byte
(sdtl_read_fd_t* p)
{
	if (!p->str_len) {
		if (p->is_string_array) {
			p->last_error = error_expected_string_in_array;
			return -1;
		}
		p->token_type = token_is_array_number;
		p->is_num_array = 1;
	}
	if (p->str_len == p->opts.max_number_length) {
		p->last_error = error_number_too_long;
		return -1;
	}
	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;

	p->next_state = copy_arr_number_byte;
	return 0;
}

int _do_arr_fraction_intro
(sdtl_read_fd_t* p)
{
	if (p->str_len == p->opts.max_number_length) {
		p->last_error = error_number_too_long;
		return -1;
	}
	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;

	p->next_state = arr_fraction_intro;
	return 0;
}

int _do_copy_arr_fraction_byte
(sdtl_read_fd_t* p)
{
	if (p->str_len == p->opts.max_number_length) {
		p->last_error = error_number_too_long;
		return -1;
	}
	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;

	p->next_state = copy_arr_fraction_byte;
	return 0;
}

int _do_copy_enum_byte
(sdtl_read_fd_t* p)
{
	if (!p->str_len) {
		p->token_type = token_is_enum;
	}
	if (p->str_len == p->opts.max_enum_length) {
		p->last_error = error_enum_too_long;
		return -1;
	}
	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;

	p->next_state = copy_enum_byte;
	return 0;
}

int _do_copy_symlink_byte
(sdtl_read_fd_t* p)
{
	if (!p->str_len) {
		p->token_type = token_is_symlink;
	}
	if (p->str_len == p->opts.max_symlink_length) {
		p->last_error = error_symlink_too_long;
		return -1;
	}
	p->str_data[p->str_len] = (unsigned char)p->byte;
	p->str_len++;

	p->next_state = copy_symlink_byte;
	return 0;
}

int _stream_read_complete_length
(sdtl_read_fd_t* p, void* output, int32_t length)
{
	int64_t bytes_read;
	int32_t total = 0;
	int fd = p->fd;
	unsigned char* buf = (unsigned char*)output;

	if (!length) {
		p->last_error = error_invalid_argument;
		return -1;
	}

	if (p->opts.max_file_size) {
		p->processed_bytes += (uint64_t)length;
		if (p->processed_bytes < (uint64_t)length) {
			/* modulo 2^64 overflow */
			p->last_error = error_file_too_long;
			return -1;
		}
		if (p->processed_bytes > p->opts.max_file_size) {
			p->last_error = error_file_too_long;
			return -1;
		}
	}

	do {
		bytes_read = read(fd,
		buf+total,
		(uint32_t)length-total);
		/* returning 0 is an error here */
		if (bytes_read <= 0) {
			p->last_error = error_read_complete_chunk_failed;
			return -1;
		}
		total += (int32_t)bytes_read;
	} while (total != length);
	return 0;
}

int _stream_read_one_chunk
(sdtl_read_fd_t* p, uint16_t length)
{
	if (_stream_read_complete_length(p, p->str_data, length))
		return -1;
	if (_finalize_binary_chunk(p, p->str_data, length))
		return -1;
	return 0;
}

int _stream_read_partial_chunk
(sdtl_read_fd_t* p, uint16_t length, unsigned char* prepend, uint16_t prepsize)
{
	if (prepsize)
		memcpy(p->str_data, prepend, prepsize);
	if (_stream_read_complete_length(p, p->str_data+prepsize, length))
		return -1;
	if (_finalize_binary_chunk(p, p->str_data, (uint16_t)(length+prepsize)))
		return -1;
	return 0;
}

int _stream_read_all_chunks
(sdtl_read_fd_t* p)
{
	unsigned char tmp[3];
	int r = 0;
	uint16_t chunksize = 0;

	for (;;) {
		if (_stream_read_complete_length(p, tmp, 3)) {
			return -1;
		}
		/* 0 is our sync byte right before the 2 byte large chunk
		 * size */
		if (*tmp) {
			p->last_error = error_octet_stream_out_of_sync;
			return -1;
		}
		memcpy(&chunksize, tmp+1, 2);
		if (!chunksize) {
			r = _finalize_binary_chunk(p, 0, 0);
			break;
		}
		if (_stream_read_one_chunk(p, chunksize))
			return -1;
	}
	return r;
}

/* synchronize read strategy */
int32_t _stream_synchronize
(sdtl_read_fd_t* p, unsigned char* data, size_t length, off_t bin_start)
{
	unsigned char tmp[2];
	uint16_t chunksize = 0;
	off_t bytes_after_chunksize;
	off_t chunkstart_off;
	unsigned char* chunkstart;

	next_chunk_in_buffer:
	chunkstart_off = bin_start + 2;
	chunkstart = data + bin_start + 2;
	bytes_after_chunksize = length - bin_start;
	bytes_after_chunksize -= 2;
	if (bytes_after_chunksize < 0) {
		if (bytes_after_chunksize == -1) {
			/* we need 1 byte more for chunksize */
			if(_stream_read_complete_length(p, tmp, 1))
				return -1;
			memcpy(&chunksize, data+bin_start, 1);
			memcpy((char*)&chunksize+1, tmp, 1);
		} else if (bytes_after_chunksize == -2) {
			/* we need 2 bytes more for chunksize */
			if(_stream_read_complete_length(p, tmp, 2))
				return -1;
			memcpy(&chunksize, tmp, 2);
		} else {
			p->last_error = error_invalid_binstart_or_length;
			return -1;
		}
		bytes_after_chunksize = 0;
	} else {
		memcpy(&chunksize, data+bin_start, 2);
	}

	/* most simple case with chunksize being 0 */
	if (!chunksize) {
		if (_finalize_binary_chunk(p, 0, 0))
			return -1;

		/* if the empty chunk ends with the current buffer, just
		 * continue reading text data just like nothing has happened */
		if (!bytes_after_chunksize)
			return 0;
		else {
			/* if there is still text data in the buffer, we need
			 * to adjust offsets and parse the remaining data */
			return chunkstart_off;
		}
	}

	/* chunk data is about to follow */

	/* our buffer is empty, we can proceed with reading the first chunk
	 * and do a synchronized chunk read afterwards */
	if (!bytes_after_chunksize) {
		if (_stream_read_one_chunk(p, chunksize)) {
			return -1;
		}
		if (_stream_read_all_chunks(p)) {
			return -1;
		}
		return 0;
	}
	/* the buffer end and the chunk end are the same */
	if (bytes_after_chunksize == chunksize) {
		if (_finalize_binary_chunk(p, chunkstart, chunksize))
			return -1;
		if (_stream_read_all_chunks(p))
			return -1;
		return 0;
	}
	/* the chunk needs bytes from fd, prepend the bytes we already have */
	if (chunksize > bytes_after_chunksize) {
		if (_stream_read_partial_chunk(p,
		(uint16_t)(chunksize-bytes_after_chunksize), chunkstart,
		(uint16_t)bytes_after_chunksize)) {
			return -1;
		}
		if (_stream_read_all_chunks(p)) {
			return -1;
		}
		return 0;
	}
	/* the chunk ends in the current buffer, we have at least 1 byte of
	 * the next chunk in our buffer */
	if (chunksize < bytes_after_chunksize) {
		if (_finalize_binary_chunk(p, chunkstart, chunksize))
			return -1;
		/* calculate next bin_start in buffer */
		bin_start += chunksize+2;
		/* check sync byte */
		if (data[bin_start]) {
			p->last_error = error_octet_stream_out_of_sync;
			return -1;
		}
		/* bin_start is counted from 1 */
		bin_start++;
		goto next_chunk_in_buffer;
	}
	return 0;
}

/* state machine */
int _call_action_for_new_byte
(sdtl_read_fd_t* p)
{
	action_t action;

	action = _after_state[p->next_state][p->byte];
	if (!action) {
		p->last_error = error_unexpected_input_byte;
		return -1;
	}
	return action(p);
}

int _utf8check(int b)
{
	static int _to_follow = 0;

	if (b == 0xc0 || b == 0xc1)
		goto err_out;
	if (b >= 0xf5)
		goto err_out;
	if (!_to_follow) {
		if ((b & 0xc0) == 0x80)
			goto err_out;
	}
	if (_to_follow > 0) {
		if ((b & 0xc0) != 0x80)
			goto err_out;
		_to_follow--;
		return 0;
	}
	if (b <= 0x7f)
		return 0;
	if ((b & 0xe0) == 0xc0) {
		_to_follow = 1;
		return 0;
	}
	if ((b & 0xf0) == 0xe0) {
		_to_follow = 2;
		return 0;
	}
	if ((b & 0xf8) == 0xf0) {
		_to_follow = 3;
		return 0;
	}

err_out:
	_to_follow = 0;
	return -1;
}

/* feed statemachine, handle binary stream start */
int _interpret_buffer
(sdtl_read_fd_t* p, unsigned char* data, size_t len, off_t* pos)
{
	size_t idx = 0;

	*pos = 0;
	if (p->next_state == octet_stream_intro) {
		p->next_state = octet_stream_outro;
	}

	for (idx = 0; idx < len; ++idx) {
		if (p->opts.max_text_bytes &&
		(p->text_bytes == p->opts.max_text_bytes)) {
			p->last_error = error_text_data_too_long;
			return -1;
		}
		p->text_bytes++;
		p->byte = data[idx];
		if (_utf8check(p->byte)) {
			p->last_error = error_invalid_utf8_byte;
			return -1;
		};
		if (_call_action_for_new_byte(p))
			return -1;
		if (p->next_state == octet_stream_intro) {
			/* the caller shall change read strategy to
			 * 2 byte size prefixed buffers (max 64k) */
			*pos = idx+1;
			break;
		}
	}
	return 0;
}

int sdtl_read
(sdtl_read_fd_t* p)
{
	ssize_t		nb;
	off_t		bin;
	off_t		off;
	unsigned char	data[SDTL_READ_BUFFER_SIZE];
	int 		fd = p->fd;

	if (p->opts.on_event(p->userdata, ev_sdtl_stream_begin, 0)) {
		p->last_error = error_scanner_callback_returned_nonzero;
		return -1;
	}

	for(;;) {
		off = 0;

		nb = read(fd, data, SDTL_READ_BUFFER_SIZE);
		if (nb <= 0) {
			if (nb < 0) {
				p->last_error = error_reading_from_source_fd;
				return -1;
			} else {
				/* The file has ended or the remote side
				 * of the pipe has shutdown its corresponding
				 * write socket, indicating that it wants to
				 * quit communication. In this case we
				 * return only 0, if the current state is
				 * 'value_outro' or 'undefined' and
				 * struct_nesting_level is 0 */
				if ((p->next_state == value_outro ||
				p->next_state == undefined) &&
				(!p->struct_nesting_level)) {
					p->last_error = error_none;
					break;
				}
				/* the remote side has dropped connection
				 * due to an error on the remote side, which
				 * doesn't allow further transmission of
				 * data */
				p->last_error =
					error_got_incomplete_sdtl_stream;
				return -1;
			}
		}
		if (p->opts.max_file_size) {
			p->processed_bytes += (uint64_t)nb;
			if (p->processed_bytes < (uint64_t)nb) {
				/* modulo 2^64 overflow */
				p->last_error = error_file_too_long;
				return -1;
			}
			if (p->processed_bytes > p->opts.max_file_size) {
				p->last_error = error_file_too_long;
				return -1;
			}
		}

		process_remaining:
		if (_interpret_buffer(p, data+off, nb-off, &bin))
			return -1;
		if (bin) {
			off = _stream_synchronize(p, data, nb, bin+off);
			if (off < 0) {
				return -1;
			}
			if (off) {
				goto process_remaining;
			}
		} else if (p->dbg_fd) {
			if (xwrite(p->dbg_fd, data, nb))
				return -1;
		}
	}
	return nb;
}

int sdtl_get_error
(sdtl_read_fd_t* r)
{
	return r->last_error;
}

void _ignore_whitespace_in
(action_t* tofill)
{
	tofill[/*0x09*/(int)'\t'] = &_do_ignore_whitespace;
	tofill[/*0x0a*/(int)'\n'] = &_do_ignore_whitespace;
	tofill[/*0x0b*/(int)'\v'] = &_do_ignore_whitespace;
	tofill[/*0x0c*/(int)'\f'] = &_do_ignore_whitespace;
	tofill[/*0x0d*/(int)'\r'] = &_do_ignore_whitespace;

	tofill[/*0x20*/(int)' '] = &_do_ignore_whitespace;

}

void _disallow_ascii_control_in
(action_t* tofill)
{
	int i;
	for (i = 0x00; i < 0x08; ++i)
		tofill[i] = 0;

	for (i = 0x0e; i < 0x1f; ++i)
		tofill[i] = 0;

	tofill[0x7f] = 0;
}

int sdtl_open_read
(sdtl_read_fd_t* r, int fd, int* dbg_fd, sdtl_read_flags_t* options)
{
	int i;

	memset(_after_state, 0, sizeof(_after_state));

	memset(r, 0, sizeof(sdtl_read_fd_t));
	r->next_state = undefined;
	r->last_error = error_none;
	r->fd = fd;
	if (dbg_fd)
		r->dbg_fd = *dbg_fd;

	if (!options || !options->on_event)
		return -1;

	memcpy(&r->opts, options, sizeof(sdtl_read_flags_t));
	if (!r->opts.max_identifier_length)
		r->opts.max_identifier_length = uint8_max;
	if (!r->opts.max_number_length)
		r->opts.max_number_length = uint8_max;
	if (!r->opts.max_utf8string_length)
		r->opts.max_utf8string_length = uint16_max;
	if (!r->opts.max_enum_length)
		r->opts.max_enum_length = uint8_max;
	if (!r->opts.max_symlink_length)
		r->opts.max_symlink_length = uint16_max;
	if (!r->opts.max_struct_nesting)
		r->opts.max_struct_nesting = uint8_max;

	r->userdata = r->opts.userdata;

	/* init tables */
	/* NOTE: for performance reasons these tables might be
	 * filled statically */

	_ignore_whitespace_in(_after_state[undefined]);
	_after_state[undefined][(int)'.'] = &_do_identifier_intro;
	_after_state[undefined][(int)'#'] = &_do_comment_intro;
	_after_state[undefined][0xef] = &_do_copy_utf8bom_byte;


	_ignore_whitespace_in(_after_state[first_bom]);
	_after_state[first_bom][(int)'.'] = &_do_identifier_intro;
	_after_state[first_bom][(int)'#'] = &_do_comment_intro;


	for (i = 0; i < 256; ++i)
		_after_state[utf8bom_intro][i] = &_do_copy_utf8bom_byte;


	for (i = 0; i < 256; ++i)
		_after_state[comment_intro][i] = &_do_ignore_comment_byte;
	_disallow_ascii_control_in(_after_state[comment_intro]);
	_after_state[comment_intro][(int)'"'] = &_do_nullstring_intro;
	_after_state[comment_intro][(int)'\n'] = &_do_comment_outro;
	_after_state[comment_intro][(int)'\r'] = &_do_comment_outro;


	for (i = 0; i < 256; ++i)
		_after_state[nullstring_intro][i] = &_do_ign_nullstring_byte;
	_disallow_ascii_control_in(_after_state[nullstring_intro]);
	_after_state[nullstring_intro][(int)'"'] = &_do_nullstring_outro;
	_after_state[nullstring_intro][(int)'\\'] = &_do_escape_from_ign;


	for (i = 0; i < 256; ++i)
		_after_state[ign_nullstring_byte][i] = &_do_ign_nullstring_byte;
	_disallow_ascii_control_in(_after_state[ign_nullstring_byte]);
	_after_state[ign_nullstring_byte][(int)'"'] = &_do_nullstring_outro;
	_after_state[ign_nullstring_byte][(int)'\\'] = &_do_escape_from_ign;


	for (i = 0; i < 256; ++i)
		_after_state[escape_from_ign][i] = &_do_ign_escaped_byte;
	_disallow_ascii_control_in(_after_state[escape_from_ign]);


	for (i = 0; i < 256; ++i)
		_after_state[nullstring_outro][i] = &_do_ignore_comment_byte;
	_ignore_whitespace_in(_after_state[nullstring_outro]);
	_disallow_ascii_control_in(_after_state[nullstring_outro]);
	_after_state[nullstring_outro][(int)'"'] = &_do_nullstring_intro;
	_after_state[nullstring_outro][(int)'\n'] = &_do_comment_outro;
	_after_state[nullstring_outro][(int)'\r'] = &_do_comment_outro;


	for (i = 0; i < 256; ++i)
		_after_state[ignore_comment_byte][i] = &_do_ignore_comment_byte;
	_disallow_ascii_control_in(_after_state[ignore_comment_byte]);
	_after_state[ignore_comment_byte][(int)'"'] = &_do_nullstring_intro;
	_after_state[ignore_comment_byte][(int)'\n'] = &_do_comment_outro;
	_after_state[ignore_comment_byte][(int)'\r'] = &_do_comment_outro;


	for (i = 0; i < 256; ++i)
		_after_state[identifier_intro][i] = &_do_copy_identifier_byte;
	_ignore_whitespace_in(_after_state[identifier_intro]);
	_disallow_ascii_control_in(_after_state[identifier_intro]);
	_after_state[identifier_intro][(int)'#'] = &_do_comment_intro;
	_after_state[identifier_intro][(int)'='] = &_do_value_intro;
	_after_state[identifier_intro][(int)'.'] = 0;
	_after_state[identifier_intro][(int)'{'] = 0;
	_after_state[identifier_intro][(int)'}'] = 0;
	_after_state[identifier_intro][(int)'"'] = 0;
	_after_state[identifier_intro][(int)';'] = 0;
	_after_state[identifier_intro][(int)'/'] = 0;
	_after_state[identifier_intro][(int)']'] = 0;
	_after_state[identifier_intro][(int)'['] = 0;
	_after_state[identifier_intro][(int)','] = 0;


	for (i = 0; i < 256; ++i)
		_after_state[value_intro][i] = &_do_copy_enum_byte;
	_ignore_whitespace_in(_after_state[value_intro]);
	_disallow_ascii_control_in(_after_state[value_intro]);
	_after_state[value_intro][(int)'#'] = &_do_comment_intro;
	_after_state[value_intro][(int)';'] = &_do_value_outro;
	_after_state[value_intro][(int)'"'] = &_do_utf8string_intro;
	_after_state[value_intro][(int)'{'] = &_do_struct_intro;
#if SUPPORT_SYMLINKS
	_after_state[value_intro][(int)'.'] = &_do_copy_symlink_byte;
#else
	_after_state[value_intro][(int)'.'] = 0;
#endif
	_after_state[value_intro][(int)'<'] = &_do_type_intro;
	_after_state[value_intro][(int)'['] = &_do_array_intro;
	_after_state[value_intro][0x00] = &_do_octet_stream_intro;
	_after_state[value_intro][(int)'-'] = &_do_neg_number_intro;
	for (i = (int)'0'; i <= (int)'9'; ++i)
		_after_state[value_intro][i] = &_do_copy_number_byte;
	_after_state[value_intro][(int)'='] = 0;
	_after_state[value_intro][(int)'}'] = 0;
	_after_state[value_intro][(int)'/'] = 0;
	_after_state[value_intro][(int)']'] = 0;
	_after_state[value_intro][(int)','] = 0;


	_ignore_whitespace_in(_after_state[array_intro]);
	_after_state[array_intro][(int)'#'] = &_do_comment_intro;
	_after_state[array_intro][(int)','] = &_do_new_array_value;
	_after_state[array_intro][(int)']'] = &_do_array_outro;
	_after_state[array_intro][(int)'"'] = &_do_arr_utf8string_intro;
	_after_state[array_intro][(int)'-'] = &_do_neg_arr_number_intro;
	for (i = (int)'0'; i <= (int)'9'; ++i)
		_after_state[array_intro][i] = &_do_copy_arr_number_byte;


	_ignore_whitespace_in(_after_state[new_array_value]);
	_after_state[new_array_value][(int)'#'] = &_do_comment_intro;
	_after_state[new_array_value][(int)','] = &_do_new_array_value;
	_after_state[new_array_value][(int)']'] = &_do_array_outro;
	_after_state[new_array_value][(int)'"'] = &_do_arr_utf8string_intro;
	_after_state[new_array_value][(int)'-'] = &_do_neg_arr_number_intro;
	for (i = (int)'0'; i <= (int)'9'; ++i)
		_after_state[new_array_value][i] = &_do_copy_arr_number_byte;


	_ignore_whitespace_in(_after_state[array_outro]);
	_after_state[array_outro][(int)'#'] = &_do_comment_intro;
	_after_state[array_outro][(int)'['] = &_do_array_intro;
	_after_state[array_outro][(int)';'] = &_do_value_outro;


	_ignore_whitespace_in(_after_state[neg_number_intro]);
	_after_state[neg_number_intro][(int)'#'] = &_do_comment_intro;
	for (i = (int)'0'; i <= (int)'9'; ++i)
		_after_state[neg_number_intro][i] = &_do_copy_number_byte;


	_ignore_whitespace_in(_after_state[neg_arr_number_intro]);
	_after_state[neg_arr_number_intro][(int)'#'] = &_do_comment_intro;
	for (i = (int)'0'; i <= (int)'9'; ++i)
		_after_state[neg_arr_number_intro][i] =
			&_do_copy_arr_number_byte;


	_ignore_whitespace_in(_after_state[copy_number_byte]);
	_after_state[copy_number_byte][(int)'#'] = &_do_comment_intro;
	_after_state[copy_number_byte][(int)';'] = &_do_value_outro;
#if SUPPORT_FLOATING_POINT
	_after_state[copy_number_byte][(int)'.'] = &_do_fraction_intro;
#else
	_after_state[copy_number_byte][(int)'.'] = 0;
#endif


	for (i = (int)'0'; i <= (int)'9'; ++i)
		_after_state[copy_number_byte][i] = &_do_copy_number_byte;


	_ignore_whitespace_in(_after_state[fraction_intro]);
	_after_state[fraction_intro][(int)'#'] = &_do_comment_intro;
	for (i = (int)'0'; i <= (int)'9'; ++i)
		_after_state[fraction_intro][i] = &_do_copy_fraction_byte;


	_ignore_whitespace_in(_after_state[copy_fraction_byte]);
	_after_state[copy_fraction_byte][(int)'#'] = &_do_comment_intro;
	_after_state[copy_fraction_byte][(int)';'] = &_do_value_outro;
	for (i = (int)'0'; i <= (int)'9'; ++i)
		_after_state[copy_fraction_byte][i] = &_do_copy_fraction_byte;


	_ignore_whitespace_in(_after_state[copy_arr_number_byte]);
	_after_state[copy_arr_number_byte][(int)'#'] = &_do_comment_intro;
	_after_state[copy_arr_number_byte][(int)','] = &_do_new_array_value;
#if SUPPORT_FLOATING_POINT
	_after_state[copy_arr_number_byte][(int)'.'] =
		&_do_arr_fraction_intro;
#else
	_after_state[copy_arr_number_byte][(int)'.'] = 0;
#endif
	_after_state[copy_arr_number_byte][(int)']'] = &_do_array_outro;
	for (i = (int)'0'; i <= (int)'9'; ++i)
		_after_state[copy_arr_number_byte][i] =
			&_do_copy_arr_number_byte;


	_ignore_whitespace_in(_after_state[arr_fraction_intro]);
	_after_state[arr_fraction_intro][(int)'#'] = &_do_comment_intro;
	for (i = (int)'0'; i <= (int)'9'; ++i)
		_after_state[arr_fraction_intro][i] =
			&_do_copy_arr_fraction_byte;


	_ignore_whitespace_in(_after_state[copy_arr_fraction_byte]);
	_disallow_ascii_control_in(_after_state[copy_arr_fraction_byte]);
	_after_state[copy_arr_fraction_byte][(int)'#'] = &_do_comment_intro;
	_after_state[copy_arr_fraction_byte][(int)']'] = &_do_array_outro;
	for (i = (int)'0'; i <= (int)'9'; ++i)
		_after_state[copy_arr_fraction_byte][i] =
			&_do_copy_arr_fraction_byte;


	_ignore_whitespace_in(_after_state[type_intro]);
	_after_state[type_intro][(int)'#'] = &_do_comment_intro;
	for (i = (int)'a'; i <= (int)'z'; ++i)
		_after_state[type_intro][i] = &_do_copy_type_byte;

	_ignore_whitespace_in(_after_state[copy_type_byte]);
	_after_state[copy_type_byte][(int)'#'] = &_do_comment_intro;
	_after_state[copy_type_byte][(int)'>'] = &_do_type_outro;
	_after_state[copy_type_byte][(int)':'] = &_do_copy_type_byte;
	_after_state[copy_type_byte][(int)'_'] = &_do_copy_type_byte;
	_after_state[copy_type_byte][(int)'-'] = &_do_copy_type_byte;
	_after_state[copy_type_byte][(int)','] = &_do_copy_unit_byte;
	for (i = (int)'0'; i <= (int)'9'; ++i)
		_after_state[copy_type_byte][i] = &_do_copy_type_byte;
	for (i = (int)'a'; i <= (int)'z'; ++i)
		_after_state[copy_type_byte][i] = &_do_copy_type_byte;


	_ignore_whitespace_in(_after_state[type_outro]);
	_after_state[type_outro][(int)'#'] = &_do_comment_intro;
	_after_state[type_outro][(int)'"'] = &_do_utf8string_intro;
	_after_state[type_outro][(int)'['] = &_do_array_intro;
	_after_state[type_outro][(int)'-'] = &_do_neg_number_intro;
	for (i = (int)'0'; i <= (int)'9'; ++i)
		_after_state[type_outro][i] = &_do_copy_number_byte;

	for (i = 0; i < 256; ++i)
		_after_state[copy_unit_byte][i] = &_do_copy_unit_byte;
	_ignore_whitespace_in(_after_state[copy_unit_byte]);
	_disallow_ascii_control_in(_after_state[copy_unit_byte]);
	_after_state[copy_unit_byte][(int)'#'] = &_do_comment_intro;
	_after_state[copy_unit_byte][(int)'>'] = &_do_type_outro;
	/* disallow unit containing following */
	_after_state[copy_unit_byte][(int)'.'] = 0;
	_after_state[copy_unit_byte][(int)'{'] = 0;
	_after_state[copy_unit_byte][(int)'}'] = 0;
	_after_state[copy_unit_byte][(int)'"'] = 0;
	_after_state[copy_unit_byte][(int)'/'] = 0;
	_after_state[copy_unit_byte][(int)']'] = 0;
	_after_state[copy_unit_byte][(int)'['] = 0;
	_after_state[copy_unit_byte][(int)','] = 0;
	_after_state[copy_unit_byte][(int)'='] = 0;


	_ignore_whitespace_in(_after_state[value_outro]);
	_after_state[value_outro][(int)'#'] = &_do_comment_intro;
	_after_state[value_outro][(int)'}'] = &_do_struct_outro;
	_after_state[value_outro][(int)'.'] = &_do_identifier_intro;


	for (i = 0; i < 256; ++i)
		_after_state[utf8string_intro][i] = &_do_copy_utf8string_byte;
	_disallow_ascii_control_in(_after_state[utf8string_intro]);
	_after_state[utf8string_intro][(int)'"'] = &_do_utf8string_outro;
	_after_state[utf8string_intro][(int)'\\'] = &_do_escape_from_copy;
	_after_state[utf8string_intro][0xef] = &_do_copy_utf8bom_byte;


	for (i = 0; i < 256; ++i)
		_after_state[copy_utf8string_byte][i] =
			&_do_copy_utf8string_byte;
	_disallow_ascii_control_in(_after_state[copy_utf8string_byte]);
	_after_state[copy_utf8string_byte][(int)'"'] = &_do_utf8string_outro;
	_after_state[copy_utf8string_byte][(int)'\\'] = &_do_escape_from_copy;


	_ignore_whitespace_in(_after_state[utf8string_outro]);
	_after_state[utf8string_outro][(int)'#'] = &_do_comment_intro;
	_after_state[utf8string_outro][(int)'"'] = &_do_utf8string_intro;
	_after_state[utf8string_outro][(int)';'] = &_do_value_outro;


	_after_state[escape_from_copy][(int)'t'] = &_do_replace_escaped_byte;
	_after_state[escape_from_copy][(int)'n'] = &_do_replace_escaped_byte;
	_after_state[escape_from_copy][(int)'v'] = &_do_replace_escaped_byte;
	_after_state[escape_from_copy][(int)'f'] = &_do_replace_escaped_byte;
	_after_state[escape_from_copy][(int)'r'] = &_do_replace_escaped_byte;
	_after_state[escape_from_copy][(int)'"'] = &_do_replace_escaped_byte;
	_after_state[escape_from_copy][(int)'\\'] = &_do_replace_escaped_byte;


	for (i = 0; i < 256; ++i)
		_after_state[arr_utf8string_intro][i] =
			&_do_copy_arr_utf8string_byte;
	_disallow_ascii_control_in(_after_state[arr_utf8string_intro]);
	_after_state[arr_utf8string_intro][(int)'"'] =
		&_do_arr_utf8string_outro;
	_after_state[arr_utf8string_intro][(int)'\\'] =
		&_do_arr_escape_from_copy;
	_after_state[arr_utf8string_intro][0xef] = &_do_copy_utf8bom_byte;


	for (i = 0; i < 256; ++i)
		_after_state[copy_arr_utf8string_byte][i] =
			&_do_copy_arr_utf8string_byte;
	_disallow_ascii_control_in(_after_state[copy_arr_utf8string_byte]);
	_after_state[copy_arr_utf8string_byte][(int)'"'] =
		&_do_arr_utf8string_outro;
	_after_state[copy_arr_utf8string_byte][(int)'\\'] =
		&_do_arr_escape_from_copy;


	_ignore_whitespace_in(_after_state[arr_utf8string_outro]);
	_after_state[arr_utf8string_outro][(int)'#'] = &_do_comment_intro;
	_after_state[arr_utf8string_outro][(int)'"'] =
		&_do_arr_utf8string_intro;
	_after_state[arr_utf8string_outro][(int)','] = &_do_new_array_value;
	_after_state[arr_utf8string_outro][(int)']'] = &_do_array_outro;


	_after_state[arr_escape_from_copy][(int)'t'] =
		&_do_arr_replace_escaped_byte;
	_after_state[arr_escape_from_copy][(int)'n'] =
		&_do_arr_replace_escaped_byte;
	_after_state[arr_escape_from_copy][(int)'v'] =
		&_do_arr_replace_escaped_byte;
	_after_state[arr_escape_from_copy][(int)'f'] =
		&_do_arr_replace_escaped_byte;
	_after_state[arr_escape_from_copy][(int)'r'] =
		&_do_arr_replace_escaped_byte;
	_after_state[arr_escape_from_copy][(int)'"'] =
		&_do_arr_replace_escaped_byte;
	_after_state[arr_escape_from_copy][(int)'\\'] =
		&_do_arr_replace_escaped_byte;


	for (i = 0; i < 256; ++i)
		_after_state[copy_enum_byte][i] = &_do_copy_enum_byte;
	_ignore_whitespace_in(_after_state[copy_enum_byte]);
	_disallow_ascii_control_in(_after_state[copy_enum_byte]);
	_after_state[copy_enum_byte][(int)'#'] = &_do_comment_intro;
	_after_state[copy_enum_byte][(int)';'] = &_do_value_outro;
	_after_state[copy_enum_byte][(int)'='] = 0;
	_after_state[copy_enum_byte][(int)'.'] = 0;
	_after_state[copy_enum_byte][(int)'{'] = 0;
	_after_state[copy_enum_byte][(int)'}'] = 0;
	_after_state[copy_enum_byte][(int)'"'] = 0;
	_after_state[copy_enum_byte][(int)'/'] = 0;
	_after_state[copy_enum_byte][(int)']'] = 0;
	_after_state[copy_enum_byte][(int)'['] = 0;
	_after_state[copy_enum_byte][(int)','] = 0;


	for (i = 0; i < 256; ++i)
		_after_state[copy_symlink_byte][i] = &_do_copy_symlink_byte;
	_ignore_whitespace_in(_after_state[copy_symlink_byte]);
	_disallow_ascii_control_in(_after_state[copy_symlink_byte]);
	_after_state[copy_symlink_byte][(int)'#'] = &_do_comment_intro;
	_after_state[copy_symlink_byte][(int)';'] = &_do_value_outro;
	_after_state[copy_symlink_byte][(int)'='] = 0;
	_after_state[copy_symlink_byte][(int)'{'] = 0;
	_after_state[copy_symlink_byte][(int)'}'] = 0;
	_after_state[copy_symlink_byte][(int)'"'] = 0;
	_after_state[copy_symlink_byte][(int)'/'] = 0;
	_after_state[copy_symlink_byte][(int)']'] = 0;
	_after_state[copy_symlink_byte][(int)'['] = 0;
	_after_state[copy_symlink_byte][(int)','] = 0;


	_ignore_whitespace_in(_after_state[struct_intro]);
	_after_state[struct_intro][(int)'#'] = &_do_comment_intro;
	_after_state[struct_intro][(int)'}'] = &_do_struct_outro;
	_after_state[struct_intro][(int)'.'] = &_do_identifier_intro;


	_ignore_whitespace_in(_after_state[struct_outro]);
	_after_state[struct_outro][(int)'#'] = &_do_comment_intro;
	_after_state[struct_outro][(int)';'] = &_do_value_outro;


	_ignore_whitespace_in(_after_state[octet_stream_outro]);
	_after_state[octet_stream_outro][(int)'#'] = &_do_comment_intro;
	_after_state[octet_stream_outro][(int)';'] = &_do_value_outro;
	return 0;
}

void sdtl_open_write
(sdtl_write_fd_t* w, int fd, int* debug_fd)
{
	memset(w, 0, sizeof(sdtl_write_fd_t));
	w->fd = fd;
	if (debug_fd) {
		w->dbg_fd = *debug_fd;
		w->use_dbg_fd = 1;
		w->white = 1;
	}
}

int sdtl_flush
(sdtl_write_fd_t* w)
{
	int32_t r = 0;
	uint32_t len = w->next_byte;
	if (len) {
		r = xwrite(w->fd, w->buffer, len);
		if (w->use_dbg_fd) {
			r = xwrite(w->dbg_fd, w->buffer, len);
		}
		w->next_byte = 0;
	}

	return r;
}

int _write_byte
(sdtl_write_fd_t* c, int byte)
{
	if (c->next_byte == /*sizeof(f->buffer)*/4096) {
		if (sdtl_flush(c))
			return -1;
	}
	c->buffer[c->next_byte] = (unsigned char)byte;
	c->next_byte++;
	return 0;
}

int _write_identifier
(sdtl_write_fd_t* c, const char* id)
{
	int r = 0;
	size_t i, lid;
	uint64_t indent;

	if (!id || !*id)
		return -1;

	if (c->white) {
		for (indent = 0; indent < c->struct_nesting_level; ++indent) {
			r = _write_byte(c, '\t');
			if (r)
				return -1;
		}
	}

	r = _write_byte(c, '.');
	if (r)
		return -1;

	lid = strlen(id);
	for (i = 0; i < lid; ++i) {
		if (
			id[i] == '.' ||
			id[i] == '/' ||
			id[i] == '='
		)
			return -1;

		r = _write_byte(c, id[i]);
		if (r)
			return -1;
	}
	return r;
}

int _write_string
(sdtl_write_fd_t* c, const char* str)
{
	int r = 0;
	size_t i, lstr;

	r = _write_byte(c, '"');
	if (r)
		return -1;

	lstr = strlen(str);
	for (i = 0; i < lstr; ++i) {
		if (str[i] == '"' || str[i] == '\\') {
			r = _write_byte(c, '\\');
			if (r)
				return -1;
		}
		r = _write_byte(c, str[i]);
		if (r)
			return -1;
	}

	r = _write_byte(c, '"');
	if (r)
		return -1;
	return r;
}

int _write_num
(sdtl_write_fd_t* c, const char* num)
{
	int r = 0;
	size_t i, lnum;

	switch (*num) {
		case '$':
		case '-':
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			break;
		default:
			return -1;
	}

	lnum = strlen(num);
	for (i = 0; i < lnum; ++i) {
		if (
			num[i] == ';' ||
			num[i] == '='
		)
			return -1;

		r = _write_byte(c, num[i]);
		if (r)
			return -1;
	}
	return r;
}

int _write_enum
(sdtl_write_fd_t* c, const char* surr)
{
	int r = 0;
	size_t i, lsurr;

	switch (*surr) {
		case ';':
		case '=':
		case '{':
		case '.':
		case '/':
		case '"':
		case '$':
		case '-':
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			return -1;
		default:
			break;
	}

	lsurr = strlen(surr);
	for (i = 0; i < lsurr; ++i) {
		if (
			surr[i] == ';' ||
			surr[i] == '='
		)
			return -1;

		r = _write_byte(c, surr[i]);
		if (r)
			return -1;
	}
	return r;
}

int _write_symlink
(sdtl_write_fd_t* c, const char* link)
{
	int r = 0;
	size_t i, llink;

	if (*link != '.')
		return -1;

	llink = strlen(link);
	for (i = 0; i < llink; ++i) {
		if (
			link[i] == ';' ||
			link[i] == '='
		)
			return -1;

		r = _write_byte(c, link[i]);
		if (r)
			return -1;
	}
	return r;
}

int _write_end_assignment(sdtl_write_fd_t* c)
{
	int r = 0;

	r = _write_byte(c, ';');
	if (r)
		return -1;
	if (c->white) {
		r = _write_byte(c, '\n');
	}
	return r;
}

int _write_start_assignment(sdtl_write_fd_t* c)
{
	int r = 0;

	if (c->white) {
		r = _write_byte(c, ' ');
		if (r)
			return -1;
	}
	r = _write_byte(c, '=');
	if (r)
		return -1;

	if (c->white) {
		r = _write_byte(c, ' ');
	}
	return r;
}

int sdtl_write_utf8string
(sdtl_write_fd_t* w, const char* key, const char* value)
{
	int r = 0;

	w->last_was_struct = 0;

	r = _write_identifier(w, key);
	if (r)
		return -1;

	r = _write_start_assignment(w);
	if (r)
		return -1;

	if (!value || !*value) {
		return _write_end_assignment(w);
	}

	r = _write_string(w, value);
	if (r)
		return -1;

	r = _write_end_assignment(w);
	if (r)
		return -1;

	return r;
}

int sdtl_write_number
(sdtl_write_fd_t* w, const char* key, const char* value)
{
	int r = 0;

	w->last_was_struct = 0;

	r = _write_identifier(w, key);
	if (r)
		return -1;

	r = _write_start_assignment(w);
	if (r)
		return -1;

	if (!value || !*value)
		return _write_end_assignment(w);

	r = _write_num(w, value);
	if (r)
		return -1;

	r = _write_end_assignment(w);
	if (r)
		return -1;

	return r;
}

int sdtl_write_enum
(sdtl_write_fd_t* w, const char* key, const char* value)
{
	int r = 0;

	w->last_was_struct = 0;

	r = _write_identifier(w, key);
	if (r)
		return -1;

	r = _write_start_assignment(w);
	if (r)
		return -1;

	if (!value || !*value)
		return _write_end_assignment(w);

	r = _write_enum(w, value);
	if (r)
		return -1;

	r = _write_end_assignment(w);
	if (r)
		return -1;

	return r;
}

int sdtl_write_symlink
(sdtl_write_fd_t* w, const char* key, const char* value)
{
	int r = 0;

	w->last_was_struct = 0;

	r = _write_identifier(w, key);
	if (r)
		return -1;

	r = _write_start_assignment(w);
	if (r)
		return -1;

	if (!value || !*value)
		return _write_end_assignment(w);

	r = _write_symlink(w, value);
	if (r)
		return -1;

	r = _write_end_assignment(w);
	if (r)
		return -1;

	return r;
}

int sdtl_write_start_struct
(sdtl_write_fd_t* w, const char* key)
{
	int r = 0;

	if (w->struct_nesting_level == uint8_max)
		return -1;

	if (!w->struct_nesting_level && !w->last_was_struct && w->white) {
		r = _write_byte(w, '\n');
		if (r)
			return -1;
	}

	r = _write_identifier(w, key);
	if (r)
		return -1;
	w->struct_nesting_level++;

	r = _write_start_assignment(w);
	if (r)
		return -1;

	r = _write_byte(w, '{');
	if (r)
		return -1;

	if (w->white) {
		r = _write_byte(w, '\n');
	}

	return r;
}

int sdtl_write_end_struct
(sdtl_write_fd_t* w)
{
	int r = 0;
	uint64_t indent;

	if (!w->struct_nesting_level)
		return -1;

	w->struct_nesting_level--;

	if (w->white) {
		for (indent = 0; indent < w->struct_nesting_level; ++indent) {
			r = _write_byte(w, '\t');
			if (r)
				return -1;
		}
	}

	r = _write_byte(w, '}');
	if (r)
		return -1;

	r = _write_end_assignment(w);
	if (r)
		return -1;

	if (!w->struct_nesting_level && w->white)
		r = _write_byte(w, '\n');

	w->last_was_struct = 1;

	return r;
}

int sdtl_write_start_octet_stream
(sdtl_write_fd_t* w, const char* key)
{
	int r = 0;

	r = _write_identifier(w, key);
	if (r)
		return -1;

	r = _write_start_assignment(w);
	if (r)
		return -1;

	/* flush write buffer, since we use write calls directly when
	 * streaming the chunks onto the destination fd */
	r = sdtl_flush(w);
	if (r)
		return -1;

	w->octet_stream = 1;
	return 0;
}

int sdtl_write_end_octet_stream
(sdtl_write_fd_t* w)
{
	uint32_t zero = 0;

	if (!w->octet_stream)
		return -1;
	w->octet_stream = 0;

	if(xwrite(w->fd, (unsigned char*)&zero, 3))
		return -1;

	if (_write_end_assignment(w))
		return -1;

	if (sdtl_flush(w))
		return -1;

	return 0;
}

int sdtl_write_chunk
(sdtl_write_fd_t* w, unsigned char* data, uint16_t len)
{
	char zero = 0;

	if (!w->octet_stream || !len || !data)
		return -1;

	/* TODO: we might want to use writev() here, but it is less
	 * portable */
	if(xwrite(w->fd, (unsigned char*)&zero, 1))
		return -1;

	/* TODO: make this little endian _always_ */
	if (xwrite(w->fd, (unsigned char*)&len, 2))
		return -1;

	if (xwrite(w->fd, data, len))
		return -1;

	if (w->use_dbg_fd && xwrite(w->dbg_fd, "^", 1))
		return -1;

	return 0;
}
