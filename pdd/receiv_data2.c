#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>



#define MEM_BASE1  0x20000000
#define MEM_BASE11 0x20002000
#define REG_BASE1  0x20001000

#define MEM_BASE2  0x20005000
#define MEM_BASE22 0x20007000
#define REG_BASE2  0x20006000

#define MEM_SIZE	0x1000
#define REG_SIZE	16

#define PLAT_IO_FLAG_REG		(0) /*Offset of flag register*/
#define PLAT_IO_SIZE_REG		(4) /*Offset of flag register*/
#define PLAT_IO_SIZE2_REG		(8) /*Offset of flag register*/
#define PLAT_IO_DATA_READY	(1) /*IO data ready flag */
#define PLAT_IO_DATA_WR (2)

#define MAX_DEVICES	2

extern int errno;

struct my_device {
	uint32_t mem_base;
	uint32_t mem_size;
	uint32_t mem_base2;
	uint32_t mem_size2;
	uint32_t reg_base;
	uint32_t reg_size;
};

static struct my_device my_devices[MAX_DEVICES] = {{
	.mem_base = MEM_BASE1,
	.mem_size = MEM_SIZE,
	.mem_base2 = MEM_BASE11,
	.mem_size2 = MEM_SIZE,
	.reg_base = REG_BASE1,
	.reg_size = REG_SIZE,
	},
	{
	.mem_base = MEM_BASE2,
	.mem_size = MEM_SIZE,
	.mem_base = MEM_BASE22,
	.mem_size = MEM_SIZE,
	.reg_base = REG_BASE2,
	.reg_size = REG_SIZE,
	},
};
int usage(char **argv)
{
	printf("Program receiv data to file from the specific device\n");
	printf("Usage: %s <device> <file>\n", argv[0]);
	return -1;
}

int main(int argc, char **argv)
{
	volatile unsigned int *reg_addr = NULL, *count_addr, *count2_addr, *flag_addr;
	volatile unsigned char *mem_addr = NULL;
	unsigned int i, device, ret, len, count;
	struct stat st;
	uint8_t *buf;
	FILE *f;

	if (argc != 3) {
		return usage(argv);
	}

	device = atoi(argv[1]);
	if (device >= MAX_DEVICES)
		return usage(argv);


	int fd = open("/dev/mem",O_RDWR|O_SYNC);
	if(fd < 0)
	{
		printf("Can't open /dev/mem\n");
		return -1;
	}
	mem_addr = (unsigned char *) mmap(0, my_devices[device].mem_size2,
				PROT_WRITE, MAP_SHARED, fd, my_devices[device].mem_base2);
	if(mem_addr == NULL)
	{
		printf("Can't mmap\n");
		return -1;
	}

	reg_addr = (unsigned int *) mmap(0, my_devices[device].reg_size,
			PROT_WRITE | PROT_READ, MAP_SHARED, fd, my_devices[device].reg_base);

	flag_addr = reg_addr;
	printf("flag_addr =%llx\n", flag_addr);

	count_addr = reg_addr;
//	printf("count_addr =%d\n", count_addr);
	count_addr++;
	printf("count_addr =%llx\n", count_addr);

	count2_addr = count_addr;
//	printf("count2_addr =%x\n", count2_addr);
	count2_addr++;
	printf("count2_addr =%llx\n", count2_addr);

	f = fopen(argv[2], "wb");
            if (!f) {
                printf("fopen error (%s)\n", argv[2]);
                return -1;
            }

	int reads = 3;
    count = 0;
	while (reads) {

        if (*flag_addr & PLAT_IO_DATA_WR) {
            count = *count2_addr;

            buf = (uint8_t *) malloc(count);
            if (!buf) {
                printf("ERROR: Out of memory");
                return -1;
            }
            memcpy(buf, (void *)mem_addr, count);

            len = fwrite(buf, 1U, count, f);

            if (len != count) {
                printf("File write Error (%s)\n", argv[2]);
                return -1;
            }
            *count2_addr = 0x0;
            *flag_addr = 0x0;
			printf("read :%s\n", (char *)buf);
			reads--;
       }
		else sleep(1);

    }

	fclose(f);
	return 0;
}
