#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdint.h>
#include <sys/statfs.h>

size_t io_blocksize(const char *filename) {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        perror("Failed to get page size, using default 4096");
        page_size = 4096;
    }

    struct statfs fs_stat;
    if (statfs(filename, &fs_stat) < 0) {
        perror("Failed to get filesystem block size, using page size");
        return (size_t)page_size;
    }

    long fs_block_size = fs_stat.f_bsize;
    if (fs_block_size <= 0 || (fs_block_size & (fs_block_size - 1)) != 0) {
        fprintf(stderr, "Invalid or non-power-of-2 block size %ld, using page size %ld\n", fs_block_size, page_size);
        return (size_t)page_size;
    }

    // Choose the larger of page_size and fs_block_size, rounded up to the next power of 2 if needed
    size_t block_size = (size_t)(page_size > fs_block_size ? page_size : fs_block_size);
    return block_size;
}

char* align_alloc(size_t size) {
    void *ptr = NULL;
    size_t page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = 4096; // Fallback
    }
    char *raw_ptr = malloc(size + page_size + sizeof(void*));
    if (raw_ptr == NULL) {
        perror("Failed to allocate memory");
        return NULL;
    }
    
    ptr = (void*)(((uintptr_t)(raw_ptr + sizeof(void*) + page_size - 1)) & ~(page_size - 1));
    *((void**)((char*)ptr - sizeof(void*))) = raw_ptr;
    
    return (char*)ptr;
}

void align_free(void* ptr) {
    if (ptr == NULL) return;
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

    size_t block_size = io_blocksize(filename);
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