#ifndef _SDTL_H_
#define _SDTL_H_

/* When using sdtl_read and sdtl_write_* on the same filedescriptor, then, if
 * the filedescriptor is a TCP socket, the code will only benefit on a at least
 * dual SMP system from a full duplex connection.
 * Opening two separate sockets, one for read, one for write is also possible
 * (making it peer-to-peer).
 */

#if !defined(_WIN32)
//	#define _POSIX_C_SOURCE	200112L
	#define _GNU_SOURCE 1
	#define _BSD_SOURCE 1
	#include <stdint.h>
	#include <string.h>
	#include <stdlib.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <errno.h>
#else
	#if defined(_MSC_VER)
		/* unreferenced inline removed */
		#pragma warning (disable: 4514)

		#pragma section(".CRT$XCU", read)
		#define __do_before_main(f)			\
			static void __cdecl f(void);		\
			__declspec(allocate(".CRT$XCU"))	\
			void (__cdecl *f##_)(void) = f; 	\
			static void __cdecl f(void)

		#define	_CRT_SECURE_NO_WARNINGS	1
		#pragma warning (push)
		/* nonstandard extension 'single line comment' was used */
		#pragma warning (disable: 4001)
		/* padding inserted after struct member */
		#pragma warning (disable: 4820)
			#include <stdint.h>
			#include <string.h>
			#include <stdlib.h>
			#include <fcntl.h>
			#include <io.h>
		#pragma warning (pop)

		#define O_RDONLY	_O_RDONLY
		#define O_BINARY	_O_BINARY
		#define read		_read
		#define write		_write
	#else
		#define _POSIX_C_SOURCE	200112L
		#define _GNU_SOURCE 1
		#define _BSD_SOURCE 1
		#include <stdint.h>
		#include <stdlib.h>
		#include <fcntl.h>
		#include <unistd.h>

		#define __do_before_main(f)			\
			__attribute__((constructor))		\
			static void f(void)
	#endif
	#define	O_NOATIME 0

	#define pipe(fds) _pipe(fds, 512, O_BINARY)

	__do_before_main(__disable_msvcrt_text_translation) {
		/* set stdin, stdout and stderr to binary mode, make binary
		 * mode default for all subsequent open() calls */
		_fmode = O_BINARY;
		if (_setmode(0, O_BINARY) == -1)
			return;
		if (_setmode(1, O_BINARY) == -1)
			return;
		if (_setmode(2, O_BINARY) == -1)
			return;
	}
#endif /* _WIN32 */

#if defined(__cplusplus)
namespace sdtl {
#endif

#define uint8_max	((uint8_t)~0)
#define uint16_max	((uint16_t)~0)
#define uint32_max	((uint32_t)~0ul)
#define uint64_max	((uint64_t)~0ull)

typedef enum sdtl_event_e {
	ev_sdtl_stream_begin,
	ev_assignment_start,
	ev_octet_stream_start,
	ev_octet_stream_end,
	ev_struct_start,
	ev_struct_end,
	ev_array_new_row,
	ev_array_end_row,
	ev_data,
	dimension_sdtl_event
} sdtl_event_t;

typedef enum value_type_e {
	/* Don't interpret the string with these two types. If no type is given,
	 * the string defaults to type 'st_plain'. */
	st_plain,
	st_utf8,
	/* simple base transformations, turning the string into a number,
	 * keep these constants at this place, so that the integer
	 * representation of the enumeration constant equals to the base
	 * in which the string value should be interpreted */
	st_base02, st_base03, st_base04, st_base05, st_base06, st_base07,
	st_base08, st_base09, st_base10, st_base11, st_base12, st_base13,
	st_base14, st_base15, st_base16, st_base17, st_base18, st_base19,
	st_base20, st_base21, st_base22, st_base23, st_base24, st_base25,
	st_base26, st_base27, st_base28, st_base29, st_base30, st_base31,
	st_base32, st_base33, st_base34, st_base35, st_base36, st_base37,
	st_base38, st_base39, st_base40, st_base41, st_base42, st_base43,
	st_base44, st_base45, st_base46, st_base47, st_base48, st_base49,
	st_base50, st_base51, st_base52, st_base53, st_base54, st_base55,
	st_base56, st_base57, st_base58, st_base59, st_base60, st_base61,
	st_base62,
	/* those use their own special alphabet/encoding scheme */
	st_base63, st_base64, st_base85,
	/* IP address representations */
	st_ip_socket,
	/* date/time values according to ISO 8601 */
	st_iso8601,

	int_u8,
	int_s8,
	int_u16,
	int_s16,
	int_u32,
	int_s32,
	int_u64,
	int_s64,
	int_u128,
	int_s128,
	int_u256,
	int_s256,
	int_u512,
	int_s512,
	int_u1024,
	int_s1024,
	/* insert new types here */

	dimension_value_type,
	st_illegal
} value_type_t;

typedef enum token_type_e {
	token_is_identifier,
	token_is_utf8string,
	token_is_number,
	token_is_enum,
	token_is_symlink,
	token_is_array_number,
	token_is_array_utf8string,
	token_is_type,
	token_is_unit,
	token_is_octet_stream,
	token_is_null_value,
	token_is_structure,
	token_is_unknown
} token_type_t;

typedef enum data_type_e {
	datatype_identifier		= token_is_identifier,
	datatype_utf8string		= token_is_utf8string,
	datatype_number			= token_is_number,
	datatype_unit			= token_is_unit,
	datatype_enum			= token_is_enum,
	datatype_symlink		= token_is_symlink,
	datatype_octet_stream		= token_is_octet_stream,
	datatype_null_value		= token_is_null_value
} data_type_t;

typedef struct sdtl_data_s {
	data_type_t	type;
	value_type_t	value_type;
	void*		data;
	uint16_t	length;
} sdtl_data_t;

typedef struct sdtl_read_flags_t {
	/* Per entity limits. If set to 0 a default value
	 * will be applied */
	uint16_t	max_identifier_length;	/* default: uint8_max */
	uint16_t	max_utf8string_length;	/* default: uint16_max */
	uint8_t		max_number_length;	/* default: uint8_max */
	uint16_t	max_enum_length;	/* default: uint8_max */
	uint16_t	max_symlink_length;	/* default: uint16_max */

	/* Limits total count of array items (total sum of all dimensions).
	 * If set to 0, this is unlimited (until max_text_bytes or
	 * max_file_size comes into play, if they are non-zero). */
	uint64_t	max_array_items;	/* default: 0 */

	/* Limits total count of text bytes (not counting bytes in
	 * octet streams). If set to 0, the total text data length
	 * is unlimited. */
	uint64_t	max_text_bytes;		/* default: 0 */

	/* Limits total bytes (including octet stream bytes) to be read. If
	 * set to 0, the file size is unlimited (useful for streaming over
	 * network). If this value is lower or equal than max_text_bytes, then
	 * the max_file_size limit will take precedence over max_text_bytes. */
	uint64_t	max_file_size;		/* default: 0 */

	/* Limits maximum struct nesting level. If set to 0 a default
	 * value will be applied. */
	uint64_t	max_struct_nesting;	/* default: uint8_max */

	/* A pointer which will be passed back to the user callback. Can be
	 * set to NULL. */
	void*		userdata;
	/* the user callback, which will receive all parser events */
	int		(*on_event)		/* mandatory, must be set */
			(void* userdata, sdtl_event_t e, sdtl_data_t* data);
} sdtl_read_flags_t;

typedef enum state_e {
	undefined,
	first_bom,
	utf8bom_intro,
	comment_intro,
	ignore_comment_byte,
	comment_outro,
	nullstring_intro,
	ign_nullstring_byte,
	escape_from_ign,
	nullstring_outro,
	identifier_intro,
	type_intro,
	type_outro,
	value_intro,
		array_intro,
		new_array_value,
		array_outro,
		neg_number_intro,
		neg_arr_number_intro,
		copy_type_byte,
		copy_unit_byte,
		copy_number_byte,
		fraction_intro,
		copy_fraction_byte,
		arr_fraction_intro,
		copy_arr_fraction_byte,
		copy_arr_number_byte,
		copy_enum_byte,
		copy_symlink_byte,
		copy_utf8string_byte,
		copy_arr_utf8string_byte,
		escape_from_copy,
		arr_escape_from_copy,
		utf8string_intro,
		utf8string_outro,
		arr_utf8string_intro,
		arr_utf8string_outro,
		octet_stream_intro,
		octet_stream_outro,
		struct_intro,
		struct_outro,
	value_outro,
	dimension_state
} state_t;

typedef enum error_code_e {
	error_none				= 0,
	error_unknown_token_type		= 1,
	error_array_row_size_mismatch		= 2,
	error_identifier_too_long		= 3,
	error_empty_identifier			= 4,
	error_struct_nesting_too_high		= 5,
	error_illegal_struct_close		= 6,
	error_utf8string_too_long		= 7,
	error_illegal_escape_sequence		= 8,
	error_number_too_long			= 9,
	error_enum_too_long			= 10,
	error_symlink_too_long			= 11,
	error_read_complete_chunk_failed	= 12,
	error_octet_stream_out_of_sync		= 13,
	error_invalid_binstart_or_length	= 14,
	error_unexpected_input_byte		= 15,
	error_text_data_too_long		= 16,
	error_reading_from_source_fd		= 17,
	error_got_incomplete_sdtl_stream	= 18,
	error_invalid_utf8_byte			= 19,
	error_invalid_byte_order_mark		= 20,
	error_type_too_long			= 21,
	error_unit_too_long			= 22,
	error_expected_string_in_array		= 23,
	error_expected_number_in_array		= 24,
	error_illegal_value_type		= 25,
	error_scanner_callback_returned_nonzero	= 26,
	error_file_too_long			= 27,
	error_invalid_argument			= 28,
	error_too_much_array_items		= 29,

	dimension_error_code
} error_code_t;

typedef enum value_unit_e {
	unit_none,
	unit_byte,
	unit_second,
	unit_yoctosecond,
	unit_gram,
	unit_yoctogram,
	/* insert new units here */

	dimension_value_unit,
	unit_illegal
} value_unit_t;


typedef struct sdtl_read_fd_t {
	sdtl_read_flags_t	opts;
	int			byte;
	token_type_t		token_type;
	state_t			next_state;
	state_t			last_state;
	value_type_t		value_type;
	int			is_num_array;
	int			is_string_array;

	uint64_t		struct_nesting_level;
	uint64_t		array_items;
	uint64_t		array_row_size;
	uint64_t		curr_row_size;

	uint64_t		processed_bytes;
	uint64_t		text_bytes;

	void*			userdata;

	error_code_t		last_error;
	int			fd;

	int			dbg_fd;
	int			use_dbg_fd;

	uint8_t			bom_len;
	uint8_t			type_len;
	uint8_t			unit_len;
	uint16_t		str_len;
	unsigned char		bom[2];
	unsigned char		type_data[uint8_max+(uint32_t)1];
	unsigned char		unit_data[uint8_max+(uint32_t)1];
	unsigned char		str_data[uint16_max+(uint32_t)1];
} sdtl_read_fd_t;

typedef struct sdtl_write_fd {
	uint64_t	struct_nesting_level;
	int		fd;
	int		dbg_fd;
	int		use_dbg_fd;
	int		white;
	int		last_was_struct;
	int		octet_stream;
	int		padding0;
	uint32_t	next_byte;
	unsigned char	buffer[uint16_max+(uint32_t)1];
} sdtl_write_fd_t;

int sdtl_read
(sdtl_read_fd_t* p);

int sdtl_get_error
(sdtl_read_fd_t* r);

int sdtl_open_read
(sdtl_read_fd_t* r, int fd, int* dbg_fd, sdtl_read_flags_t* options);

void sdtl_open_write
(sdtl_write_fd_t* w, int fd, int* debug_fd);

int sdtl_flush
(sdtl_write_fd_t* w);

int sdtl_write_utf8string
(sdtl_write_fd_t* w, const char* key, const char* value);

int sdtl_write_number
(sdtl_write_fd_t* w, const char* key, const char* value);

int sdtl_write_enum
(sdtl_write_fd_t* w, const char* key, const char* value);

int sdtl_write_symlink
(sdtl_write_fd_t* w, const char* key, const char* value);

int sdtl_write_start_struct
(sdtl_write_fd_t* w, const char* key);

int sdtl_write_end_struct
(sdtl_write_fd_t* w);

int sdtl_write_start_octet_stream
(sdtl_write_fd_t* w, const char* key);

int sdtl_write_end_octet_stream
(sdtl_write_fd_t* w);

int sdtl_write_chunk
(sdtl_write_fd_t* w, unsigned char* data, uint16_t len);

#if defined(__cplusplus)
}
#endif

#endif /* _SDTL_H_ */
