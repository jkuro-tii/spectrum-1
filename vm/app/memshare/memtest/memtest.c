#include <stdio.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>

#define PMEM_DEVICE "/dev/pmem0"

void memtest(void *pmem, long int pmem_size)
{

  printf("Shared memory size: %ld bytes. Mapped at: %p\n", pmem_size, pmem);

  printf("Will perform shared memory tests here.\n");

  return;
}

int main()
{

  void *pmem;
  int pmem_fd = -1; 
  long int pmem_size = 0;

  /* Open shared memory */
  pmem_fd = open(PMEM_DEVICE, O_RDWR);
  if (pmem_fd < 0)
  {
    perror(PMEM_DEVICE);
    goto err;
  }  

  /* Get memory size */
  int res = ioctl(pmem_fd, BLKGETSIZE64, &pmem_size);
  if (res < 0)
  {
    perror(PMEM_DEVICE);
    goto err;
  }
  if (pmem_size == 0)
  {
    printf("No shared memory detected.\n");
    goto err_close; 
  }

  pmem = mmap(pmem, pmem_size, PROT_READ|PROT_WRITE, MAP_SHARED, pmem_fd, 0);
  if (pmem != NULL)
  {

    memtest(pmem, pmem_size);

    res = munmap(pmem, pmem_size);
    if (res < 0)
    {
      perror(PMEM_DEVICE);
      goto err;
    }
  };

  close(pmem_fd);
  return 0;

err:
  return 1;
err_close:
  close(pmem_fd);
  return 1;
}
