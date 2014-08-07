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
		return -1;
	}

	_print_nodes(&c, 1);
	conf_cleanup(&c);

	return 0;
}
