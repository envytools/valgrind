#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

static void *mmap_and_check(void *addr, size_t len, int fd, off_t offset)
{
	void *ptr = mmap(addr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, fd, offset);
	if (ptr == MAP_FAILED)
	{
		perror("mmap");
		exit(-2);
	}

	if (ptr != addr)
	{
		fprintf(stderr, "failed to mmap at specified address: %p != %p\n", ptr, addr);
		exit(-3);
	}

	return ptr;
}

int main()
{
	int fd, fdfake;

	fd = open("/dev/zero", O_RDWR);
	if (fd < 0)
	{
		perror("open");
		exit(-1);
	}

	fdfake = open("/dev/null", O_RDWR);
	if (fdfake < 0)
	{
		perror("open");
		exit(-1);
	}
	void *ptr1, *ptr2, *ptr3, *ptr1_fake, *ptr2_fake, *ptr3_fake, *ptr4_fake;

	ptr1_fake = mmap_and_check(((void *)0x00011000),     0x1000,  fdfake, 0x0);

	ptr1 = mmap_and_check(((void *)0x00117000),          0xC4000, fd,     0x0);
	((int *)ptr1_fake)[1] = 7;
	((int *)ptr1)[1] = 1;
	ptr2 = mmap_and_check(((void *)0x00025000),          0x1000,  fd,     0x101000);
	ptr2_fake = mmap_and_check(((void *)0x00040000),     0x1000,  fdfake, 0x10000);
	((int *)ptr2_fake)[1] = 7;
	((int *)ptr1)[1] = 2;
	((int *)ptr2)[1] = 3;
	ptr3_fake = mmap_and_check(((void *)0x00010000),     0x1000,  fdfake, 0x20000);
	((int *)ptr3_fake)[1] = 7;

	munmap(ptr2, 0x1000);
	ptr2 = mmap_and_check(((void *)0x00025000),          0x1000,  fd,     0x100000);
	((int *)ptr1)[1] = 4;
	((int *)ptr2)[1] = 5;
	((int *)ptr2_fake)[1] = 7;
	ptr3 = mmap_and_check(((void *)0x00026000),          0x1000,  fd,     0x200000);
	ptr4_fake = mmap_and_check(((void *)0x00027000),     0x1000,  fdfake, 0x40000);
	((int *)ptr4_fake)[1] = 7;
	((int *)ptr1)[1] = 6;
	((int *)ptr2)[1] = 7;
	((int *)ptr3)[1] = 8;
	munmap(ptr2, 0x1000);
	munmap(ptr3, 0x1000);

	return 0;
}
