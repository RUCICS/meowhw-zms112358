#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

size_t io_blocksize() {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        perror("Failed to get page size, using default 4096");
        return 4096; // Fallback to a reasonable default
    }
    return (size_t)page_size;
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
    char *buffer = (char *)malloc(block_size);
    if (buffer == NULL) {
        perror("Failed to allocate buffer");
        close(fd);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, block_size)) > 0) {
        if (write(STDOUT_FILENO, buffer, bytes_read) < 0) {
            perror("Error writing to stdout");
            free(buffer);
            close(fd);
            exit(EXIT_FAILURE);
        }
    }

    if (bytes_read < 0) {
        perror("Error reading file");
        free(buffer);
        close(fd);
        exit(EXIT_FAILURE);
    }

    free(buffer);
    if (close(fd) < 0) {
        perror("Error closing file");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}