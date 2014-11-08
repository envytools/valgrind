#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
	int fd = open("/dev/zero", O_RDWR);
	volatile uint32_t *v1 = mmap(NULL, 0x200, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	v1[0x100/4] = 0x1234567;
	v1[0x400/4] = 0xabcdef; // gets lost
	close(fd);
	return 0;
}

