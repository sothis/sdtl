#include "conf.h"
#include <stdio.h>

int main(int argc, char* argv[])
{
	conf_t		config;
	int		r = -1;
	uint64_t	rows, cols;
	const char*	username = 0;
	const char**	str_array = 0;
	const int64_t**	int_array = 0;

	r = conf_read(&config, "examples/test.conf");
	if (r) {
		fprintf(stderr, "error parsing config stream\n");
		goto out;
	}


	username = conf_get_utf8string_by_key(&config, ".general.username");
	if (username)
		printf("user: '%s'\n", username);

	str_array = conf_get_utf8string_array_by_key(&config,
			".general.endpoints", &rows, &cols);

	for (uint64_t i = 0; i < rows; ++i) {
		for (uint64_t j = 0; j < cols; ++j) {
			const char* val = str_array[i*cols + j];
			if (val) printf("%s\n", val);
		}
	}

	printf("\n");

	int_array = conf_get_int64_array_by_key(&config,
			".general.arr", &rows, &cols);

	for (uint64_t i = 0; i < rows; ++i) {
		for (uint64_t j = 0; j < cols; ++j) {
			const int64_t* val = int_array[i*cols + j];
			if (val) printf("%" PRIu64 "\n", *val);
		}
	}

out:
	conf_cleanup(&config);
	return r;
}
