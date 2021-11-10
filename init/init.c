/*
	TEMPORARY INIT
*/

#include "init.h"

// console file descriptor
static int conFD;

size_t strlen(const char *s)
{
	size_t result = 0;
	while (*s)
	{
		s++;
		result++;
	};

	return result;
};

void println(const char *line)
{
	write(conFD, line, strlen(line));
	write(conFD, "\n", 1);
};

int main()
{
	conFD = openat(AT_FDCWD, "/initrd-console", O_RDWR);
	if (conFD < 0)
	{
		return 1;
	};

	println("Hello world from userspace init!");

	return 0;
};