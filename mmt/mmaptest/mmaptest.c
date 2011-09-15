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

typedef float v4sf __attribute__((vector_size(16)));

#define MMAP_OFFSET1 0x2000
#define MMAP_LEN1 0x1000
#define MMAP_STARTING_ADDRESS1 ((void *)0x77770000)

#define MMAP_OFFSET2 0x5000
#define MMAP_LEN2 0x2000
#define MMAP_STARTING_ADDRESS2 ((void *)0x99990000)

/* all the fake mmaps are to check that MMT _does not_ catch reads/writes
 * to regions user does not have interest in */
#define MMAP_OFFSET1_FAKE 0x2000
#define MMAP_LEN1_FAKE 0x1000
#define MMAP_STARTING_ADDRESS1_FAKE ((void *)0x66660000)

#define MMAP_OFFSET2_FAKE 0x0
#define MMAP_LEN2_FAKE 0x1000
#define MMAP_STARTING_ADDRESS2_FAKE ((void *)0x88880000)

#define MMAP_OFFSET3_FAKE 0x2000
#define MMAP_LEN3_FAKE 0x1000
#define MMAP_STARTING_ADDRESS3_FAKE ((void *)0xAAAA0000)

#define NVIDIA_IOCTL_REQUEST 0xc030464e

unsigned long long data128[] = {
	0x1234567890abcdefULL, 0xdeadbeeffeeddeadULL,
	0x1234567890abcdefULL, 0xdeadbeeffeeddeadULL};

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
	unsigned int ioctl_data[16] = {
			0, 0, 0, 0,
			0, 0, 0, 0,
			MMAP_OFFSET1};
	char *ptr1_fake, *ptr1, *ptr2_fake, *ptr2, *ptr3_fake;
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

	ioctl(fd, NVIDIA_IOCTL_REQUEST, ioctl_data);
	
	ptr1_fake = mmap_and_check(MMAP_STARTING_ADDRESS1_FAKE, MMAP_LEN1_FAKE, fdfake, MMAP_OFFSET1_FAKE);
	ptr2_fake = mmap_and_check(MMAP_STARTING_ADDRESS2_FAKE, MMAP_LEN2_FAKE, fdfake, MMAP_OFFSET2_FAKE);
	ptr3_fake = mmap_and_check(MMAP_STARTING_ADDRESS3_FAKE, MMAP_LEN3_FAKE, fdfake, MMAP_OFFSET3_FAKE);

	ptr1      = mmap_and_check(MMAP_STARTING_ADDRESS1,      MMAP_LEN1,      fd,     MMAP_OFFSET1);
	ptr2      = mmap_and_check(MMAP_STARTING_ADDRESS2,      MMAP_LEN2,      fd,     MMAP_OFFSET2);

	ptr1_fake[0x7] = 0x11;
	ptr3_fake[0x9] = 0x13;

	ptr1[0x11] = 0x0f;
	ptr1[0x14] = 0x77;
	
	((short *)ptr1) [0xC8 / 2] = 0x1234;
	((short *)ptr1) [0xCA / 2] = 0x5678;

	((int *)ptr1) [0x190 / 4] = 0x98765432;
	((int *)ptr1) [0x194 / 4] = 0xdeadbeef;
	
	((long long *)ptr1) [0x320 / 8] = 0x1234567890abcdefULL;
	((long long *)ptr1) [0x328 / 8] = 0xfedcba9876543210ULL;
	
	__builtin_ia32_movntq((void *)(ptr1 + 0x20), data128[0]);

	__builtin_ia32_movntps((void *)(ptr1 + 0x40), *((v4sf *)data128));
	
	printf("%x\n", ptr1[0x11]);
	printf("%x\n", ptr1[0x14]);
	printf("%x\n", ((short *)ptr1)[0xC8 / 2]);
	printf("%x\n", ((short *)ptr1)[0xCA / 2]);
	printf("%x\n", ((int *)ptr1)[0x190 / 4]);
	printf("%x\n", ((int *)ptr1)[0x194 / 4]);
	printf("%llx\n", ((long long *)ptr1)[0x320 / 8]);
	printf("%llx\n", ((long long *)ptr1)[0x328 / 8]);

	ptr2_fake[0x88] = 0x99;
	ptr1_fake[0x123] = 0xBA;
	ptr3_fake[0x321] = 0xBE;

	__builtin_ia32_movntq(data128, *(unsigned long long *)(ptr1 + 0x20));

	__builtin_ia32_movntps((float *)data128, *(v4sf *)(ptr1 + 0x40));

	ptr2[0x35] = 0x11;

	ptr1 = mremap(ptr1, MMAP_LEN1, MMAP_LEN1 + 4096, MREMAP_FIXED | MREMAP_MAYMOVE,
			MMAP_STARTING_ADDRESS1 + 213411 * 4096);
	if (ptr1 == MAP_FAILED)
	{
		perror("mremap");
		exit(-97);
	}
	ptr1[0x101] = 0x34;
	ptr2[0x36] = 0x22;
	if (munmap(ptr1, MMAP_LEN1) < 0)
	{
		perror("munmap");
		exit(-98);
	}
	ptr2[0x36] = 0x22;

	ptr1_fake[0xCAF] = 0xE0;
	ptr3_fake[0xDEA] = 0xD0;
	
	if (munmap(ptr3_fake, MMAP_LEN3_FAKE) < 0)
	{
		perror("munmap");
		exit(-98);
	}

	if (munmap(ptr2, MMAP_LEN2) < 0)
	{
		perror("munmap");
		exit(-98);
	}
	ptr2_fake[0xAA] = 0xCA;

	if (munmap(ptr2_fake, MMAP_LEN2_FAKE) < 0)
	{
		perror("munmap");
		exit(-98);
	}

	if (munmap(ptr1_fake, MMAP_LEN1_FAKE) < 0)
	{
		perror("munmap");
		exit(-98);
	}

	if (close(fdfake) < 0)
	{
		perror("close");
		exit(-99);
	}

	if (close(fd) < 0)
	{
		perror("close");
		exit(-99);
	}

	return 0;
}
