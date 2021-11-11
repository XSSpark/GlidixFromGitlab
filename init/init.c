/*
	TEMPORARY INIT
*/

#include "init.h"

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
	write(1, line, strlen(line));
	write(1, "\n", 1);
};

int main()
{
	int fd = openat(AT_FDCWD, "/initrd-console", O_RDWR);
	if (fd != 0)
	{
		return 1;
	};

	if (dup3(0, 1, 0) != 1)
	{
		return 2;
	};

	if (dup3(1, 2, 0) != 2)
	{
		return 3;
	};

	println("I'm printing this via the duplicated descriptor!");

	return 0;
};