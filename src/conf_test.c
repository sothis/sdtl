#include "conf.h"
#include <stdio.h>

int main(int argc, char* argv[])
{
	conf_t		conf;
	int		r = -1;
	uint64_t	rows, cols;
	const char**	str_array = 0;
	const int64_t**	int_array = 0;

	r = conf_read(&conf, "examples/test.conf");
	if (r) {
		fprintf(stderr, "error parsing config stream\n");
		goto out;
	}


	str_array = conf_get_utf8string_array_by_key(&conf,
			".general.endpoints", &rows, &cols);

	for (uint64_t i = 0; i < rows; ++i) {
		for (uint64_t j = 0; j < cols; ++j) {
			const char* val = str_array[i*cols + j];
			if (val) printf("%s\n", val);
		}
	}

	printf("\n");

	int_array = conf_get_int64_array_by_key(&conf,
			".general.arr", &rows, &cols);

	for (uint64_t i = 0; i < rows; ++i) {
		for (uint64_t j = 0; j < cols; ++j) {
			const int64_t* val = int_array[i*cols + j];
			if (val) printf("%" PRIu64 "\n", *val);
		}
	}

out:
	conf_cleanup(&conf);
	return r;
}
