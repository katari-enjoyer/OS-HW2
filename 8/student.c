#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>


#define MAX_ROWS 16
#define MAX_SHLVS 8
#define MAX_BOOKS 8
#define SUB_SIZE 64

static volatile int keepRunning = 1;

// Структура для общей памяти
typedef struct shared_memory {
  sem_t mutex;
  int shelves_count;
  int books_count;
  int sub_catalog[MAX_ROWS][SUB_SIZE];
}shared_memory;

// Функция обработки прерывания
void intHandler(int dummy) {
    printf("SIGINT!\n");
    keepRunning = 0;
}

int main(int argc, char ** argv) {
   if (argc < 2) {
    printf("Usage: ./student <his row number>\n");
    return -1;
  }
  int row = atoi(argv[1]);
  signal(SIGINT, intHandler);
  char memn[] = "shared-memory"; //  имя объекта
  int mem_size = sizeof(shared_memory);
  int shm;
  sem_t* mutex;

  if ((shm = shm_open(memn, O_CREAT | O_RDWR, 0666)) == -1) {
      printf("Object is already open\n");
      perror("shm_open");
      return 1;
  } else {
      printf("Object is open: name = %s, id = 0x%x\n", memn, shm);
  }
  if (ftruncate(shm, mem_size) == -1) {
      printf("Memory sizing error\n");
      perror("ftruncate");
      return 1;
  } else {
      printf("Memory size set and = %d\n", mem_size);
  }

  //получить доступ к памяти
  void* addr = mmap(0, mem_size, PROT_WRITE, MAP_SHARED, shm, 0);
  if (addr == (int * ) - 1) {
      printf("Error getting pointer to shared memory\n");
      return 1;
  }

  shared_memory* shmptr = addr;
  printf("Student #%d checking row: %d\n", getpid(), row);
  int sub[SUB_SIZE];
  sleep(3 + rand() % 10);
  int ptr = 0;
  for (int i = 0; i < shmptr->shelves_count; i++) {
    for (int j = 0; j < shmptr->books_count; j++) {
      sub[ptr++] = 100*row + 10 * i + j;
    }
  }
  for (int i = 0; i < ptr; i++) {
    shmptr->sub_catalog[row][i] = sub[i];
  }
  printf("Student #%d finished\n", getpid());
  sem_post(&shmptr->mutex);
  close(shm);
  return 0;
}