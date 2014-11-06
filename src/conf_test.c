#include "configfile.h"

int main(int argc, char* argv[])
{
	conf_t		config;
	int		r = -1;
	uint64_t	rows, cols;
	const int64_t*	octal = 0;
	const char*	username = 0;
	const char*	enumeration = 0;
	const char**	str_array = 0;
	const int64_t**	int_array = 0;

	r = conf_read(&config, "examples/test.conf");
	if (r) {
		fprintf(stderr, "error parsing config stream\n");
		goto out;
	}

	octal = conf_get_int64_by_key(&config, ".general.octal");
	if (octal)
		printf("octal: '%" PRIi64 "'\n", *octal);

	enumeration = conf_get_enum_by_key(&config, ".general.enumeration");
	if (enumeration)
		printf("enumeration: '%s'\n", enumeration);


	username = conf_get_utf8string_by_key(&config, ".general.usernäme");
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
			if (val) printf("%" PRIi64 "\n", *val);
		}
	}

out:
	conf_cleanup(&config);
	return r;
}
