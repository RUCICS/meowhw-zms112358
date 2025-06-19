#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdint.h>

size_t io_blocksize() {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        perror("Failed to get page size, using default 4096");
        return 4096;
    }
    return (size_t)page_size;
}

char* align_alloc(size_t size) {
    void *ptr = NULL;
    size_t page_size = io_blocksize();
    // Allocate extra space for alignment and store original pointer
    char *raw_ptr = malloc(size + page_size + sizeof(void*));
    if (raw_ptr == NULL) {
        perror("Failed to allocate memory");
        return NULL;
    }
    
    // Align the pointer to the next page boundary
    ptr = (void*)(((uintptr_t)(raw_ptr + sizeof(void*) + page_size - 1)) & ~(page_size - 1));
    // Store the original pointer just before the aligned pointer
    *((void**)((char*)ptr - sizeof(void*))) = raw_ptr;
    
    return (char*)ptr;
}

void align_free(void* ptr) {
    if (ptr == NULL) return;
    // Retrieve the original pointer stored before the aligned pointer
    void *raw_ptr = *((void**)((char*)ptr - sizeof(void*)));
    free(raw_ptr);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *filename = argv[1];
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    size_t block_size = io_blocksize();
    char *buffer = align_alloc(block_size);
    if (buffer == NULL) {
        close(fd);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, block_size)) > 0) {
        if (write(STDOUT_FILENO, buffer, bytes_read) < 0) {
            perror("Error writing to stdout");
            align_free(buffer);
            close(fd);
            exit(EXIT_FAILURE);
        }
    }

    if (bytes_read < 0) {
        perror("Error reading file");
        align_free(buffer);
        close(fd);
        exit(EXIT_FAILURE);
    }

    align_free(buffer);
    if (close(fd) < 0) {
        perror("Error closing file");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}