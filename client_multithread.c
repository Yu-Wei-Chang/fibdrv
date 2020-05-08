#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

void *thread_func(void *arg)
{
    long long sz;
    char buf[1];
    char write_buf[] = "testing writing";
    int offset = 100;
    char *thread_name = arg;
    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device\n");
        goto pthread_exit;
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("[%s]Writing to " FIB_DEV ", returned the sequence %lld\n",
               thread_name, sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("[%s]Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               thread_name, i, sz);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("[%s]Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               thread_name, i, sz);
    }

pthread_exit:
    close(fd);
    pthread_exit(NULL);
}

int main()
{
    long long sz;

    char buf[1];
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */
    pthread_t thread1;
    pthread_t thread2;
    pthread_t thread3;

    pthread_create(&thread1, NULL, thread_func, "clone 1");
    pthread_create(&thread2, NULL, thread_func, "clone 2");
    pthread_create(&thread3, NULL, thread_func, "clone 3");

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }

    close(fd);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    return 0;
}
