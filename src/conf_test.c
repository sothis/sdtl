#include "conf.h"
#include <stdio.h>

int main(int argc, char* argv[])
{
	conf_t conf;
	int r = -1;
	const char* key = 0;
#if 0
	if (argc == 1)
		r = conf_read_fd(&conf, fileno(stdin));

	if (argc == 2)
		r = conf_read(&conf, argv[1]);
#endif

	if (argc == 1)
		r = conf_read(&conf, "examples/test.conf");
	if (argc == 2) {
		r = conf_read(&conf, "examples/test.conf");
		key = argv[1];
	}

	if (r) {
		fprintf(stderr, "error parsing config stream\n");
		goto out;
	}

	if (key) {
		uint64_t r, c;
		const char** tmp = get_utf8string_array_by_key(&conf, key, &r, &c);
		for (uint64_t i = 0; i < c; ++i) {
			printf("%s\n", tmp[i]);
		}
#if 0
		const int64_t* intval = get_value_by_key_int64(&conf, key);
		const char* strval = get_value_by_key_utf8string(&conf, key);

		if (strval)
			printf("val: '%s'\n", strval);
		if (intval)
			printf("val: %" PRIi64 "\n", *intval);
#endif

	}
out:
	conf_cleanup(&conf);
	return r;
}
