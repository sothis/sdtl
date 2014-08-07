#ifndef _SDTL_H
#define _SDTL_H

/*
public functions:

	int sdtl_get_error
		(sdtl_read_fd_t* r)

	int sdtl_open_read
		(sdtl_read_fd_t* r, int fd, sdtl_read_flags_t* options)
	int sdtl_read
		(sdtl_read_fd_t* r)

	void sdtl_open_write
		(sdtl_write_fd_t* w, int fd, int with_whitespace)
	int sdtl_write_utf8string
		(sdtl_write_fd_t* w, const char* key, const char* value)
	int sdtl_write_number
		(sdtl_write_fd_t* w, const char* key, const char* value)
	int sdtl_write_enum
		(sdtl_write_fd_t* w, const char* key, const char* value)
	int sdtl_write_symlink
		(sdtl_write_fd_t* w, const char* key, const char* value)
	int sdtl_write_start_struct
		(sdtl_write_fd_t* w, const char* key)
	int sdtl_write_end_struct
		(sdtl_write_fd_t* w)
	int sdtl_write_start_octet_stream
		(sdtl_write_fd_t* w, const char* key)
	int sdtl_write_chunk
		(sdtl_write_fd_t* w, unsigned char* data, uint16_t len)
	int sdtl_write_end_octet_stream
		(sdtl_write_fd_t* w)
	int sdtl_flush
		(sdtl_write_fd_t* w)
*/

/* When using sdtl_read and sdtl_write_* on the same filedescriptor, then, if
 * the filedescriptor is a TCP socket, the code will only benefit on a at least
 * dual SMP system from a full duplex connection.
 * Opening two separate sockets, one for read, one for write is also possible
 * (making it peer-to-peer).
 */

#if !defined(_WIN32)
	#define _POSIX_C_SOURCE	200112L
	#define _GNU_SOURCE 1
	#define _BSD_SOURCE 1
	#include <stdint.h>
	#include <string.h>
	#include <stdlib.h>
	#include <fcntl.h>
	#include <unistd.h>
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
		#include "uintx.h"
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

#include "sdtl.c"

#if defined(__cplusplus)
}
#endif

#endif /* _SDTL_H */
