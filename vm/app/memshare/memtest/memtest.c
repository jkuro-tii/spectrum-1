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
#include <stdlib.h>

#define PMEM_DEVICE "/dev/pmem0"
#define TEST_TYPE long int
#define TEST_TYPE_LENGTH sizeof(TEST_TYPE)
#define MB (1048576)


//#define IS_BLK 1

int pmem_fd = -1; 
void *pmem_ptr;
long int pmem_size;

struct
{
  volatile int filler[137];
  volatile int ready;
  volatile int start;
  volatile int done;
  volatile int shutdown;
} volatile *vm_control;

int is_netvm()
{
  FILE* test_file = fopen("/etc/nftables.conf", "r"); // This file exists only on the netvm virtual machine

  int first = test_file != NULL;
  if (first)
    fclose(test_file);

  return first;
}

void hexdump(volatile void *mem, int size)
{
  mem += sizeof(vm_control->filler);
  size -= sizeof(vm_control->filler);
  for(int i = 0; i < size; i++)
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

void flush()
{
  __builtin___clear_cache((void*)pmem_ptr, (void*)pmem_ptr+pmem_size);
  // printf("flush\n");
  int res;
  #ifdef IS_BLK
  res = ioctl(pmem_fd, BLKFLSBUF);
  if (res < 0)
  {
    perror("ioctl");
  }
  #endif
  // system("echo 1 > /proc/sys/vm/drop_caches");
  // res = fflush(pmem_fd);
  // if (res < 0)
  // {
  //   perror("fflush");
  // }  
  // res = fsync(pmem_fd);
  // if (res < 0)
  // {
  //   perror("fsync");
  // }

  // res = msync((void*)pmem_ptr, pmem_size, MS_SYNC);
  // if (res < 0)
  // {
  //   perror("msync");
  // }  

  __builtin___clear_cache((void*)pmem_ptr, (void*)pmem_ptr+pmem_size);
  sync();

  return;
  res = munmap((void*)pmem_ptr, pmem_size);
  if (res < 0)
  {
    perror("munmap "PMEM_DEVICE);
  }
  printf("Close\n");
  close(pmem_fd);
  pmem_fd = open(PMEM_DEVICE, O_RDWR|O_DIRECT);
  if (pmem_fd < 0)
  {
    perror("open "PMEM_DEVICE);
    exit(1);
  }
  #ifdef IS_BLK
  res = ioctl(pmem_fd, BLKGETSIZE64, &pmem_size);
  if (res < 0)
  {
    perror(PMEM_DEVICE);
  }
  res = ioctl(pmem_fd, BLKFLSBUF);
  if (res < 0)
  {
    perror("ioctl");
  }  
  #endif
  pmem_ptr = mmap(NULL, pmem_size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_NORESERVE|MAP_NONBLOCK, pmem_fd, 0);
  if (pmem_ptr == MAP_FAILED)
  {
    perror("mmap");
    exit(1);
  }
  __builtin___clear_cache((void*)pmem_ptr, (void*)pmem_ptr+pmem_size);
}

#define START (0x11111111) 
#define READY (0x55555555)
#define DONE  (0x99999999)

//#define usleep(x) {};


void proc_netvm()
{
  printf("Server: "); hexdump(vm_control, sizeof(*vm_control));
  //memset(vm_control, 0, sizeof(*vm_control));
  do
  {
    printf("Server: &vm_control->ready=%p\n", &(vm_control->ready));
    // Wait for start
    printf("Server: Ready (%x). Waiting to be started.\n", READY);
    vm_control->ready = READY;
    printf("Server: "); hexdump(vm_control, sizeof(*vm_control));
    do
    {
      vm_control->ready = READY;
      flush();
      usleep(10000);
    } while(!vm_control->start);

    // test todo
    vm_control->start = 0;
    printf("Server: Setting done to 0\n");
    vm_control->done = 0;
    flush();

    printf("Server: Start received.\n");
    printf("Server: "); hexdump(vm_control, sizeof(*vm_control));    

    printf("Server: Executing a task.\n");
    usleep(3000000); // 3 secs

    // Fill memory with a pattern

    printf("Server: Task finished. Ready (0) Done(%x) \n", DONE);
    vm_control->ready = 0;
    vm_control->done = DONE;
    flush();
  } while(!vm_control->shutdown);
}

void proc_test()
{
  do
  {
    printf("Client: "); hexdump(vm_control, sizeof(*vm_control));
    memset((void*)vm_control, 0, sizeof(*vm_control));
    flush();
    printf("Client: &vm_control->ready=%p vm_control->ready=%x\n", &(vm_control->ready), (*vm_control).ready);

    // Wait for the peer VM be ready
    printf("Client: Waiting for the server to be ready.\n");
    do
    {
      usleep(1000);
      flush();
      // printf("Client: "); hexdump(vm_control, sizeof(*vm_control));
      // printf("Client: &vm_control->ready?=%p vm_control->ready?=%x\n", &(vm_control->ready), (*vm_control).ready);
    } while(!vm_control->ready);
    printf("Client: Ready (0).\n");
    vm_control->ready = 0;

    // Start the partner VM
    printf("Client: Starting the server. Done (0) Start (%x)\n", START);
    vm_control->done = 0;
    vm_control->start = START;
    flush();

    printf("Client: Waiting for server completion.\n");
    printf("Client: "); hexdump(vm_control, sizeof(*vm_control));
    // Wait for completion
    do 
    {
      usleep(1000); // 100ms
      flush();
    } while(!vm_control->done);

    printf("Client: task done. Setting Done (0) Start (0)\n");
    vm_control->done = 0;
    vm_control->start = 0;
    flush();
    printf("Client: "); hexdump(vm_control, sizeof(*vm_control));
 
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

int main(int argc, char**argv )
{
  volatile void *test_pmem;
  long test_mem_size = 0;

  /* Open shared memory */
  pmem_fd = open(PMEM_DEVICE, O_RDWR);
  if (pmem_fd < 0)
  {
    perror(PMEM_DEVICE);
    goto exit_error;
  }  

  /* Get memory size */
  int res;
  #if 0
  res = ioctl(pmem_fd, BLKGETSIZE64, &pmem_size);
  #else
  pmem_size = 1*MB;
  #endif

  printf("pmem_size=%ld\n", pmem_size);
  if (res < 0)
  {
    perror(PMEM_DEVICE);
    goto exit_error;
  }
  
  if (pmem_size == 0)
  {
    printf("No shared memory detected.\n");
    goto exit_close; 
  }

  pmem_ptr = mmap(NULL, pmem_size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_NORESERVE, pmem_fd, 0);
  printf("pmem_size=%ld pmem=%p\n", pmem_size, pmem_ptr);

  if (pmem_ptr != NULL)
  {
    vm_control = pmem_ptr;
    test_pmem = pmem_ptr + sizeof(*vm_control);
    test_mem_size = pmem_size - sizeof(*vm_control);

    if (is_netvm() || argc > 1)
    {
      proc_netvm();
    }
    else
    {
      proc_test();
    }

    // memtest(test_pmem, test_mem_size, 0);

    res = munmap((void*)pmem_ptr, pmem_size);
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
