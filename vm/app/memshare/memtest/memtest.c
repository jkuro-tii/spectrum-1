#include <stdio.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/times.h>
#include <sys/time.h>
#include <string.h>

#define PMEM_DEVICE "/dev/pmem0"
#define TEST_TYPE long int
#define TEST_TYPE_LENGTH sizeof(TEST_TYPE)
#define MB (1048576)

int pmem_fd = -1; 

volatile struct
{
  volatile int ready;
  volatile int start;
  volatile int done;
  volatile int shutdown;
} *vm_control;

int is_netvm()
{
  FILE* test_file = fopen("/etc/nftables.conf", "r"); // This file exists only on the netvm virtual machine

  int first = test_file != NULL;
  if (first)
    fclose(test_file);

  return first;
}

void hexdump(void *mem, int size)
{
  for(int i; i < size; i++)
  {
    printf("%02x ", *(unsigned char*) (mem + i));
  }

  printf("\n");

}

void print_report(double cpu_time_s, double real_time_s, long int data_written, long int data_read)
{
  cpu_time_s /= 1000.0;
  real_time_s /= 1000.0;

  printf("CPU time: %.0fs Real time: %.0fs Read: %ld MB Written: %ld MB\n", 
    cpu_time_s, real_time_s, data_read/MB, data_written/MB);
  printf("I/O rate: read: %.2f MB/s write: %.2f MB/s R&W: %.2f MB/s        Total I/O in realtime: %.2f MB/s\n",
    (double)data_read/MB/cpu_time_s, (double)data_written/MB/cpu_time_s, (double)(data_read+data_written)/MB/cpu_time_s, 
    (double)(data_read+data_written)/MB/real_time_s);
}

void proc_netvm()
{
  hexdump(vm_control, sizeof(*vm_control));
  //memset(vm_control, 0, sizeof(*vm_control));
  do
  {
    printf("Running on netvm\n");

    // TODO: remove
    printf("&vm_control->ready=%p\n", &(vm_control->ready));
    // Wait for start
    printf("Waiting to be started.\n");
    vm_control->ready = 0xc1a0c1a0;
    int ioctl_result = ioctl(pmem_fd, BLKFLSBUF);
    if (ioctl_result < 0)
    {
      perror("ioctl");
    }
    hexdump(vm_control, sizeof(*vm_control));

    while(!vm_control->start) 
    {
      ioctl_result = ioctl(pmem_fd, BLKFLSBUF);
      if (ioctl_result < 0)
      {
        perror("ioctl");
      }
      vm_control->ready = 0xc1a0c1a0;
      usleep(10000);
    } 
    vm_control->ready = 0xc1a0c1a0;

    printf("Start received.\n");
    hexdump(vm_control, sizeof(*vm_control));    
    // TODO
    // vm_control->ready = 0; 
    // vm_control->start = 0;

    printf("Executing a task.\n");
    usleep(3000000); // 3 secs

    printf("Task finished.\n\n");
    vm_control->ready = 1;
    vm_control->done = 1;
    ioctl_result = ioctl(pmem_fd, BLKFLSBUF);
    if (ioctl_result < 0)
    {
      perror("ioctl");
    }
  } while(!vm_control->shutdown);
}

void proc_test()
{
  do
  {
    printf("Running on the other VM\n");
    hexdump(vm_control, sizeof(*vm_control));
    //memset(vm_control, 0, sizeof(*vm_control));
    printf("&vm_control->ready=%p vm_control->ready=%x\n", &(vm_control->ready), vm_control->ready);
    
    // Wait for the peer VM be ready
    printf("Waiting for the peer to be ready.\n");
    while(!(volatile)vm_control->ready) 
    {
      usleep(1000);
      int ioctl_result = ioctl(pmem_fd, BLKFLSBUF);
      if (ioctl_result < 0)
      {
        perror("ioctl");
      }
    } 

    // Start the partner VM
    printf("Starting the peer.\n");
    vm_control->done = 0;
    vm_control->start = 0x1316191c;
    int ioctl_result = ioctl(pmem_fd, BLKFLSBUF);
    if (ioctl_result < 0)
    {
      perror("ioctl");
    }

    printf("Waiting for completion.\n");
    hexdump(vm_control, sizeof(*vm_control));
    // Wait for completion
    while(!vm_control->done) 
    {
      usleep(10000); // 10ms
    int ioctl_result = ioctl(pmem_fd, BLKFLSBUF);
    if (ioctl_result < 0)
    {
      perror("ioctl");
    }
    } 

    printf("Done.\n\n");
    ioctl_result = ioctl(pmem_fd, BLKFLSBUF);
    if (ioctl_result < 0)
    {
      perror("ioctl");
    }
  } while(!vm_control->shutdown);
}


int memtest(void *pmem_ptr, long int size, int verify)
{
  long int read_counter = 0, write_counter = 0;
  int ret_val = 0;

  double cpu_time_ms = 0.0;
  clock_t cpu_time_start = clock();

  struct timeval time_start;
  gettimeofday(&time_start, NULL);
  double time_start_msec = 1000.0*time_start.tv_sec + (double)time_start.tv_usec/1000.0;
  struct timeval current_time;
  double current_time_ms;

  unsigned TEST_TYPE* pmem_array = (unsigned TEST_TYPE*) pmem_ptr;
  long int pmem_size = size / sizeof(TEST_TYPE);
  unsigned int counter;

  printf("Shared array size: %ld bytes  size=%ld. Mapped at: %p is netvm=%d\n", pmem_size, size, pmem_ptr, is_netvm());

  if(!verify) 
  {
    for(counter = 0; counter < 5000; counter++)
    {
      for(unsigned int n = 0; n < pmem_size; n++) // TODO +2 instead of +1 
      {
        write_counter++;
        pmem_array[n] = counter;
      }
    }
    ret_val = 0;
  } 
  else
  {
    ret_val = 0;
    for(counter = 0; counter < 50000; counter++)
    {
      for(unsigned int n = 0; n < pmem_size; n++) // TODO +2 instead of +1 
      {        
        if (pmem_array[n] != counter) 
        {
          printf("memtest error at addr %p\n", &pmem_array[n]);
          ret_val = 1;
          goto exit;
        }
        read_counter++;
      }
    }
  }

  exit:
    gettimeofday(&current_time, NULL);
    cpu_time_ms = (double)(clock() - cpu_time_start) / CLOCKS_PER_SEC * 1000;
    current_time_ms = 1000.0*current_time.tv_sec + (double)current_time.tv_usec/1000.0 - time_start_msec;
    print_report(cpu_time_ms, current_time_ms, write_counter*sizeof(TEST_TYPE), read_counter*sizeof(TEST_TYPE));
  return ret_val;
}

int main()
{
  void *pmem_ptr, *test_pmem;
  long int mem_size = 0, test_mem_size = 0;

  /* Open shared memory */
  pmem_fd = open(PMEM_DEVICE, O_RDWR);
  if (pmem_fd < 0)
  {
    perror(PMEM_DEVICE);
    goto exit_error;
  }  

  /* Get memory size */
  int res = ioctl(pmem_fd, BLKGETSIZE64, &mem_size);
  printf("pmem_size=%ld\n", mem_size);
  if (res < 0)
  {
    perror(PMEM_DEVICE);
    goto exit_error;
  }

  if (mem_size == 0)
  {
    printf("No shared memory detected.\n");
    goto exit_close; 
  }

  pmem_ptr = mmap(NULL, mem_size, PROT_READ|PROT_WRITE, MAP_SHARED, pmem_fd, 0);
  printf("pmem_size=%ld pmem=%p\n", mem_size, pmem_ptr);

  if (pmem_ptr != NULL)
  {
    vm_control = pmem_ptr;
    test_pmem = pmem_ptr + sizeof(*vm_control);
    test_mem_size = mem_size - sizeof(*vm_control);

    if (is_netvm())
    {
      proc_netvm();
    }
    else
    {
      proc_test();
    }

    // memtest(test_pmem, test_mem_size, 0);

    res = munmap(pmem_ptr, mem_size);
    if (res < 0)
    {
      perror(PMEM_DEVICE);
      goto exit_error;
    }
  }
  else
  {
    printf("Got NULL pointer from mmap.\n");
  }

  close(pmem_fd);
  return 0;

exit_close:
  close(pmem_fd);
exit_error:
  return 1;
}
