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

// int usleep(useconds_t usec);

// clock_t times(struct tms *buf); // get process time
// time_t time(time_t *tloc);

// clock_t clock(void); // CPU time
// time_t time(time_t *tloc); // time - get time in seconds

// void handle() {
//     times(&time_end);
// }

// int main(int argc, char *argv[]) {
//     signal(SIGINT, handle);

#define PMEM_DEVICE "/dev/pmem0"
#define TEST_TYPE long int
#define TEST_TYPE_LENGTH sizeof(TEST_TYPE)
#define MB (1048576)

/* As the same application is working on a VM and the netvm machine,
 * lets start the application on netvm as the second one.
 */

struct {
  long int netvm_ready;
  long int start;
  long int netvm_done;
} *comm;


int is_netvm()
{
  FILE* test_file = fopen("/etc/nftables.conf", "r"); // This is exists only on the netvm virtual machine

  int first = test_file != NULL;
  if (first)
    fclose(test_file);
  
  printf("Running on ");
  if (first)
    printf("netvm\n");
  else
    printf("other VM\n");

  return first;
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
  // READY = 0 
  // START = 0
  // DONE = 0
  
  // Wait for start
  // READY = 1
  // ?START

  // START = 0
  // READY = 0

  // execute

  // DONE = 1

  // Loop Wait for Start
}

void proc_test()
{
  // START = 0 DONE = 0

  // Start
  // ?READY
  // DONE = 0
  // START = 1

  // Wait for completion
  // ?DONE
  // DONE = 0

  // Verify

  // Loop Start

}


void memtest(void *pmem_ptr, long int size)
{
  long int read_counter = 0, write_counter = 0;
  
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
  int base = is_netvm();

  printf("Shared array size: %ld bytes  size=%ld. Mapped at: %p base=%d\n", pmem_size, size, pmem_ptr, base);

  do 
  {
    for(counter = 0; counter < 5000; counter++)
    {
      for(unsigned int n = base; n < pmem_size; n++) // TODO +2 instead of +1 
      {
        write_counter++;
        pmem_array[n] = counter;
        (volatile void) pmem_array[n];
      }
    }

    gettimeofday(&current_time, NULL);
    cpu_time_ms = (double)(clock() - cpu_time_start) / CLOCKS_PER_SEC * 1000;
    current_time_ms = 1000.0*current_time.tv_sec + (double)current_time.tv_usec/1000.0 - time_start_msec;
    print_report(cpu_time_ms, current_time_ms, write_counter*sizeof(TEST_TYPE), read_counter*sizeof(TEST_TYPE));
  
  } while(1);

  return;
}

int main()
{
  void *pmem_ptr;
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
  printf("pmem_size=%ld\n", pmem_size);
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

  pmem_ptr = mmap(NULL, pmem_size, PROT_READ|PROT_WRITE, MAP_SHARED, pmem_fd, 0);
  printf("pmem_size=%ld pmem=%p\n", pmem_size, pmem_ptr);

  if (pmem_ptr != NULL)
  {
    memtest(pmem_ptr, pmem_size);

    res = munmap(pmem_ptr, pmem_size);
    if (res < 0)
    {
      perror(PMEM_DEVICE);
      goto err;
    }
  }
  else
  {
    printf("Got NULL pointer from mmap.\n");
  }

  close(pmem_fd);
  return 0;

err:

err_close:
  close(pmem_fd);
err:
  return 1;
}
