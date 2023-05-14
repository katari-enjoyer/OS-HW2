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

// Функция для сортировки книг
int compare_function(const void *a, const void *b) {
  int *x = (int *) a;
  int *y = (int *) b;
  return *x - *y;
}


// Функция для студента
void child(int semid, int row, shared_memory* shmptr)  {
  srand(time(NULL) ^ (getpid()<<16));
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
  sem_open(semid);
  exit(0);
}



int main(int argc, char ** argv) {
  if (argc < 4) {
    printf("usage: ./main <rows count> <shelves count> <books count>");
    return -1;
  }
  signal(SIGINT, intHandler);
  int rows_count = atoi(argv[1]);
  int shelves_count = atoi(argv[2]);
  int books_count = atoi(argv[3]);
  if (rows_count > MAX_ROWS) {
    printf("Max rows: %d\n", MAX_ROWS);
    return -1;
  }
  if (shelves_count > MAX_SHLVS) {
    printf("Max shelves: %d\n", MAX_SHLVS);
    return -1;
  }
  if (books_count > MAX_BOOKS) {
    printf("Max books: %d\n", MAX_BOOKS);
    return -1;
  }
  key_t key = ftok(argv[0], 0);
  shared_memory *shmptr;
  int semid;
  int shmid;

  int* children = (int*)malloc(sizeof(int) * rows_count);
  int* catalog = (int*)malloc(sizeof(int) * rows_count * shelves_count * books_count);

  // Создаем память и семафор
  if ((semid = semget(key, 1, 0666 | IPC_CREAT)) < 0) {
    printf("Can\'t create semaphore\n");
    return -1;
  }

  if ((shmid = shmget(key, sizeof(shared_memory), 0666 | IPC_CREAT)) < 0) {
    printf("Can\'t create shmem\n");
    return -1;
  }
  if ((shmptr = (shared_memory*) shmat(shmid, NULL, 0)) == (shared_memory *) -1) {
    printf("Cant shmat!\n");
    return -1;
  }

  semctl(semid, 0, SETVAL, 0);
  
  
  shmptr->shelves_count = shelves_count;
  shmptr->books_count = books_count;

  // Создаем процессы студентов
  for (int i = 0; i < rows_count; i++) {
    children[i] = fork();
    if (children[i] == 0) {
      child(semid, i, shmptr);
    }
  }

  // Процесс библиотекаря 
  for (int i = 0; i < rows_count; i++) {
    sem_close(semid);
    printf("Got sub calalog from student\n");
  }
  int ptr = 0;
  printf("Final calalog:\n");
  for (int i = 0; i < rows_count; i++) {
    for (int j = 0; j < shelves_count * books_count; j++) {
      catalog[ptr++] = shmptr->sub_catalog[i][j];
    }
  }

  qsort(catalog, rows_count * shelves_count * books_count, sizeof(int), compare_function);
  for (int i = 0; i < rows_count * shelves_count * books_count; i++) {
    printf("%d\n", catalog[i]);
  }

  // Завершаем процессы детей если они еще активны
  for (int i = 0; i < rows_count; i++) {
    kill(children[i], SIGTERM);
    printf("Killing child #%d\n", children[i]);
  }

  // Очищаем память
  free(children);
  free(catalog);

  // Удаляем семафоры
  if (semctl(semid, 0, IPC_RMID, 0) < 0) {
    printf("Can\'t delete semaphore\n");
    return -1;
  }
  // Удаляем память
  shmdt(shmptr);
  shmctl(shmid, IPC_RMID, NULL);
  return 0;
}