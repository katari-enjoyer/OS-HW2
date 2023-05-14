#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#define MAX_ROWS 16
#define MAX_SHLVS 8
#define MAX_BOOKS 8
#define SUB_SIZE 64

static volatile int keepRunning = 1;

// Структура для общей памяти
typedef struct shared_memory {
  int semid;
  int shelves_count;
  int books_count;
  int sub_catalog[MAX_ROWS][SUB_SIZE];
}shared_memory;

// Функция обработки прерывания
void intHandler(int dummy) {
    printf("SIGINT!\n");
    keepRunning = 0;
}

// Функция для уменьшения значения семафора на 1
void sem_close(int semid) {
 struct sembuf parent_buf = {
    .sem_num = 0,
    .sem_op = -1,
    .sem_flg = 0
  };
  if (semop(semid, & parent_buf, 1) < 0) {
    printf("Can\'t sub 1 from semaphor\n");
    exit(-1);
  }
}

// Функция для увеличения семафора на 1
void sem_open(int semid) {
 struct sembuf parent_buf = {
    .sem_num = 0,
    .sem_op = 1,
    .sem_flg = 0
  };
  if (semop(semid, & parent_buf, 1) < 0) {
    printf("Can\'t add 1 to semaphor\n");
    exit(-1);
  }  
}


int main(int argc, char ** argv) {
  if (argc < 2) {
    printf("Usage: ./student <his row number>\n");
    return -1;
  }
  int row = atoi(argv[1]);
  signal(SIGINT, intHandler);
  key_t key = ftok("library.c", 0);
  shared_memory *shmptr;
  int shmid;

  if ((shmid = shmget(key, sizeof(shared_memory), 0666 | IPC_CREAT)) < 0) {
    printf("Can\'t create shmem\n");
    return -1;
  }
  if ((shmptr = (shared_memory*) shmat(shmid, NULL, 0)) == (shared_memory *) -1) {
    printf("Cant shmat!\n");
    return -1;
  }
  printf("Student #%d checking row: %d\n", getpid(), row);
  int sub[SUB_SIZE];
  sleep(2 + rand() % 10);
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
  sem_open(shmptr->semid);
  return 0;
}