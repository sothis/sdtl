#include "conf.h"
#include <stdio.h>

int main(int argc, char* argv[])
{
	conf_t c;
	int r = -1;
	const char* key = 0;
#if 0
	if (argc == 1)
		r = conf_read_fd(&c, fileno(stdin));

	if (argc == 2)
		r = conf_read(&c, argv[1]);
#endif

	if (argc == 1)
		r = conf_read(&c, "examples/test.conf");
	if (argc == 2) {
		r = conf_read(&c, "examples/test.conf");
		key = argv[1];
	}

	if (r) {
		fprintf(stderr, "error parsing config stream\n");
		goto out;
	}

	if (key) {
		const int64_t* intval = get_value_by_key_int64(&c, key);
		const char* strval = get_value_by_key_utf8string(&c, key);

		if (strval)
			printf("val: '%s'\n", strval);
		if (intval)
			printf("val: %" PRIi64 "\n", *intval);

	}
out:
	conf_cleanup(&c);
	return r;
}
