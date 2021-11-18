/*
	Glidix init
	
	Copyright (c) 2021, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main()
{
	// open the initrd console, and make it stdin, stdout and stderr
	int fd = open("/initrd-console", O_RDWR);
	if (fd != 0)
	{
		return 1;
	};

	if (dup(0) != 1)
	{
		return 1;
	};

	if (dup(1) != 2)
	{
		return 1;
	};

	printf("Hello, world! This is init!!!\n");

	fd = open("/test.txt", O_WRONLY | O_CREAT, 0644);
	if (write(fd, "value1", 6) != 6)
	{
		printf("ERROR 1\n");
		return 1;
	};

	close(fd);

	fd = open("/test.txt", O_RDONLY);
	char test[16];
	memset(test, 0, 16);

	if (read(fd, test, 6) != 6)
	{
		printf("ERROR 2\n");
		return 1;
	};

	close(fd);
	printf("We got: [%s]\n", test);

	fd = open("/test.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (read(fd, test, 6) != 0)
	{
		printf("ERROR 3\n");
		return 1;
	};

	close(fd);

	printf("Tests ended.\n");
	return 0x45;
};