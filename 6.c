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

// Функция для сортировки книг
int compare_function(const void *a, const void *b) {
  int *x = (int *) a;
  int *y = (int *) b;
  return *x - *y;
}

// Функция для студента
void child(int row, shared_memory* shmptr)  {
  srand(time(NULL) ^ (getpid()<<16));
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

  int* children = (int*)malloc(sizeof(int) * rows_count);
  int* catalog = (int*)malloc(sizeof(int) * rows_count * shelves_count * books_count);
  char memn[] = "shared-memory"; //  имя объекта
  char sem_name[] = "sem-mutex";
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
  if ((mutex = sem_open(sem_name, O_CREAT, 0666, 0)) < 0) {
    printf("Error creating semaphore!\n");
      return 1;
  }
  shmptr->mutex = *mutex;
  shmptr->shelves_count = shelves_count;
  shmptr->books_count = books_count;

    // Создаем процессы студентов
  for (int i = 0; i < rows_count; i++) {
    children[i] = fork();
    if (children[i] == 0) {
      child(i, shmptr);
    }
  }

 // Процесс библиотекаря 
  for (int i = 0; i < rows_count; i++) {
    sem_wait(&shmptr->mutex);
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

  if (sem_unlink(sem_name) == -1) {
    perror ("sem_unlink"); exit (1);
  }
  close(shm);
  // удалить выделенную память
  if(shm_unlink(memn) == -1) {
    printf("Shared memory is absent\n");
    perror("shm_unlink");
    return 1;
  }
  return 0;
}