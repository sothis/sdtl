#include "conf.h"
#include <stdio.h>

int main(int argc, char* argv[])
{
	conf_t c;
	int r = -1;

	if (argc == 1)
		r = conf_read_fd(&c, fileno(stdin));

	if (argc == 2)
		r = conf_read(&c, argv[1]);

	if (r) {
		fprintf(stderr, "error parsing config stream\n");
		goto out;
	}

	//_print_nodes(&c, 1);

	const void* val = get_value_by_key(&c, ".general.username");

	get_value_by_key(&c, ".user.name..");
	printf("val: '%s'\n", (const char*)val);
out:
	conf_cleanup(&c);
	return r;
}
