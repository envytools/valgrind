#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

typedef float v4sf __attribute__((vector_size(16)));

#define MMAP_LEN 0x1000
#define STARTING_ADDRESS ((void *)0x77770000)
#define MMAP_OFFSET 0x2000

#define NVIDIA_IOCTL_REQUEST 0xc030464e

unsigned long long data128[] = {
	0x1234567890abcdefULL, 0xdeadbeeffeeddeadULL,
	0x1234567890abcdefULL, 0xdeadbeeffeeddeadULL};

int main()
{
	char *ptr;
	int fd = open("/dev/zero", O_RDWR);
	if (fd < 0)
	{
		perror("open");
		exit(-1);
	}

	unsigned int ioctl_data[16] = {
			0, 0, 0, 0,
			0, 0, 0, 0,
			MMAP_OFFSET};

	ioctl(fd, NVIDIA_IOCTL_REQUEST, ioctl_data);
	
	ptr = mmap(STARTING_ADDRESS, MMAP_LEN, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, fd, MMAP_OFFSET);
	if (ptr == MAP_FAILED)
	{
		perror("mmap");
		exit(-2);
	}

	if (ptr != STARTING_ADDRESS)
	{
		fprintf(stderr, "failed to mmap at specified address: %p != %p\n", ptr, STARTING_ADDRESS);
		exit(-3);
	}
	
	ptr[0x11] = 0x0f;
	ptr[0x14] = 0x77;
	
	((short *)ptr) [0xC8 / 2] = 0x1234;
	((short *)ptr) [0xCA / 2] = 0x5678;

	((int *)ptr) [0x190 / 4] = 0x98765432;
	((int *)ptr) [0x194 / 4] = 0xdeadbeef;
	
	((long long *)ptr) [0x320 / 8] = 0x1234567890abcdefULL;
	((long long *)ptr) [0x328 / 8] = 0xfedcba9876543210ULL;
	
	__builtin_ia32_movntq((void *)(ptr + 0x20), data128[0]);

	__builtin_ia32_movntps((void *)(ptr + 0x40), *((v4sf *)data128));
	
	printf("%x\n", ptr[0x11]);
	printf("%x\n", ptr[0x14]);
	printf("%x\n", ((short *)ptr)[0xC8 / 2]);
	printf("%x\n", ((short *)ptr)[0xCA / 2]);
	printf("%x\n", ((int *)ptr)[0x190 / 4]);
	printf("%x\n", ((int *)ptr)[0x194 / 4]);
	printf("%llx\n", ((long long *)ptr)[0x320 / 8]);
	printf("%llx\n", ((long long *)ptr)[0x328 / 8]);

	__builtin_ia32_movntq(data128, *(unsigned long long *)(ptr + 0x20));

	__builtin_ia32_movntps((float *)data128, *(v4sf *)(ptr + 0x40));
	
	ptr = mremap(ptr, MMAP_LEN, MMAP_LEN + 4096, MREMAP_FIXED | MREMAP_MAYMOVE,
			STARTING_ADDRESS + 213411 * 4096);
	if (ptr == MAP_FAILED)
	{
		perror("mremap");
		exit(-97);
	}
	
	if (munmap(ptr, MMAP_LEN) < 0)
	{
		perror("munmap");
		exit(-98);
	}
	
	if (close(fd) < 0)
	{
		perror("close");
		exit(-99);
	}
	
	return 0;
}
