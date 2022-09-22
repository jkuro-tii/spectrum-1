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

int is_first()
{
  FILE* test_file = fopen("/etc/nftables.conf", "r");

  int first = test_file != NULL;
  if (first)
    fclose(test_file);
  
  if (first)
    printf("First\n");
  else
    printf("Second\n");

  return first;
}

void print_report(double cpu_time_s, double real_time_s, long int data_written, long int data_read)
{
  cpu_time_s /= 1000.0;
  real_time_s /= 1000.0;

  printf("CPU time [s]: %.0f Real time [s]: %.0f Read: %ld MB Written %ld MB\n", 
    cpu_time_s, real_time_s, data_read/MB, data_written/MB);
  printf("I/O rate: read: %.2f MB/s write: %.2f MB/s R&W: %.2f MB/s        Total I/O in realtime: %.2f MB/s\n",
    (double)data_read/MB/cpu_time_s, (double)data_written/MB/cpu_time_s, (double)(data_read+data_written)/MB/cpu_time_s, 
    (double)(data_read+data_written)/MB/real_time_s);
}

void memtest(void *pmem, long int size)
{
  long int read_counter = 0, write_counter = 0;
  
  double cpu_time_ms = 0.0;
  clock_t cpu_time_start = clock();

  struct timeval time_start;
  gettimeofday(&time_start, NULL);
  double time_start_msec = 1000.0*time_start.tv_sec + (double)time_start.tv_usec/1000.0;
  struct timeval current_time;
  double current_time_ms;

  unsigned TEST_TYPE* pmem_tab = (unsigned TEST_TYPE*) pmem;
  long int pmem_size = size / sizeof(TEST_TYPE);
  unsigned int counter;
  int base = is_first();

  printf("Shared memory size: %ld bytes. Mapped at: %p\n", size, pmem);

    for(counter = 0; counter < 100; counter++)
      {
        for(unsigned int n = base; n < pmem_size; n++) // TODO +2 instead of +1 
        {
          write_counter++;
          pmem_tab[n] = counter;
        }
      }

      gettimeofday(&current_time, NULL);
      cpu_time_ms = (double)(clock() - cpu_time_start) / CLOCKS_PER_SEC * 1000;
      current_time_ms = 1000.0*current_time.tv_sec + (double)current_time.tv_usec/1000.0 - time_start_msec;
      print_report(cpu_time_ms, current_time_ms, write_counter*sizeof(TEST_TYPE), read_counter*sizeof(TEST_TYPE));

  /*
   * Even/odd ???
   * 
   * Time start
   * Time end
   * Elapsed time
   * CPU time
   * Read operations
   * Write operations 
   * Speed
   */

  return;
}

int main()
{

  void *pmem;
  int pmem_fd = -1; 
  long int pmem_size = 0;

  printf("At %d sizeof(long int)=%ld sizeof(long long int)=%ld \n", __LINE__, sizeof(long int), sizeof(long long int));
  /* Open shared memory */
  pmem_fd = open(PMEM_DEVICE, O_RDWR);
  if (pmem_fd < 0)
  {
    perror(PMEM_DEVICE);
    goto err;
  }  

  printf("At %d\n", __LINE__);
  /* Get memory size */
  int res = ioctl(pmem_fd, BLKGETSIZE64, &pmem_size);
  printf("pmem_size=%ld\n", pmem_size);
  printf("At %d\n", __LINE__);
  if (res < 0)
  {
    perror(PMEM_DEVICE);
    goto err;
  }
  printf("At %d\n", __LINE__);
  if (pmem_size == 0)
  {
    printf("No shared memory detected.\n");
    goto err_close; 
  }
  printf("At %d\n", __LINE__);
  pmem = mmap(NULL, pmem_size, PROT_READ|PROT_WRITE, MAP_SHARED, pmem_fd, 0);
  printf("At %d\n", __LINE__);
  printf("pmem_size=%ld pmem=%p\n", pmem_size, pmem);
  if (pmem != NULL)
  {
    // TODO
    printf("pmem_size=%ld\n", pmem_size);
    // pmem_size = 1024*1024*1024; // 1 GB  
    printf("pmem_size=%ld\n", pmem_size);
    memtest(pmem, pmem_size);

    res = munmap(pmem, pmem_size);
    if (res < 0)
    {
      perror(PMEM_DEVICE);
      goto err;
    }
  }
  else
  {
    printf("At %d\n", __LINE__);
    printf("pmem=%p\n", pmem);
  }
  printf("At %d\n", __LINE__);
  close(pmem_fd);
  return 0;

err:
  return 1;
err_close:
  close(pmem_fd);
  return 1;
}
