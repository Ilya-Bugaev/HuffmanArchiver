#define _POSIX_C_SOURCE 200809L

#include "compress.h"
#include "decompress.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static double secondsBetween(const struct timespec* start, const struct timespec* end)
{
    return (double)(end->tv_sec - start->tv_sec) + (double)(end->tv_nsec - start->tv_nsec) / 1e9;
}

static long fileSize(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (!file) {
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    long size = ftell(file);
    fclose(file);
    return size;
}

static bool filesEqual(const char* leftPath, const char* rightPath)
{
    FILE* left = fopen(leftPath, "rb");
    FILE* right = fopen(rightPath, "rb");
    if (!left || !right) {
        if (left) {
            fclose(left);
        }
        if (right) {
            fclose(right);
        }
        return false;
    }

    bool equal = true;
    uint8_t leftBuffer[65536];
    uint8_t rightBuffer[65536];
    while (equal) {
        size_t leftCount = fread(leftBuffer, 1, sizeof(leftBuffer), left);
        size_t rightCount = fread(rightBuffer, 1, sizeof(rightBuffer), right);
        if (leftCount != rightCount || memcmp(leftBuffer, rightBuffer, leftCount) != 0) {
            equal = false;
            break;
        }
        if (leftCount == 0) {
            break;
        }
    }

    if (ferror(left) || ferror(right)) {
        equal = false;
    }

    fclose(left);
    fclose(right);
    return equal;
}

int main(int argc, char** argv)
{
    if (argc != 4) {
        fprintf(stderr, "Использование: %s <имя> <файл> <повторы>\n", argv[0]);
        return 1;
    }

    const char* name = argv[1];
    const char* inputPath = argv[2];
    char* end = NULL;
    long repeats = strtol(argv[3], &end, 10);
    if (!end || *end != '\0' || repeats < 1) {
        fprintf(stderr, "Число повторов должно быть положительным целым\n");
        return 1;
    }

    char archivePath[64];
    char outputPath[64];
    int archiveWritten = snprintf(archivePath, sizeof(archivePath), "/tmp/huff_bench_%d.huff", getpid());
    int outputWritten = snprintf(outputPath, sizeof(outputPath), "/tmp/huff_bench_%d.out", getpid());
    if (archiveWritten < 0 || outputWritten < 0
        || (size_t)archiveWritten >= sizeof(archivePath)
        || (size_t)outputWritten >= sizeof(outputPath)) {
        fprintf(stderr, "Не удалось собрать пути временных файлов\n");
        return 1;
    }

    if (!compressFile(inputPath, archivePath) || !decompressFile(archivePath, outputPath)) {
        fprintf(stderr, "Пробный проход не удался: %s\n", inputPath);
        remove(archivePath);
        remove(outputPath);
        return 1;
    }
    if (!filesEqual(inputPath, outputPath)) {
        fprintf(stderr, "Разжатый файл не совпал с исходным: %s\n", inputPath);
        remove(archivePath);
        remove(outputPath);
        return 1;
    }

    long originalSize = fileSize(inputPath);
    long compressedSize = fileSize(archivePath);
    if (originalSize < 0 || compressedSize < 0) {
        fprintf(stderr, "Не удалось узнать размер файла: %s\n", inputPath);
        remove(archivePath);
        remove(outputPath);
        return 1;
    }

    for (long i = 0; i < repeats; i++) {
        struct timespec compressStart;
        struct timespec compressEnd;
        struct timespec decompressEnd;

        clock_gettime(CLOCK_MONOTONIC, &compressStart);
        bool compressed = compressFile(inputPath, archivePath);
        clock_gettime(CLOCK_MONOTONIC, &compressEnd);
        bool decompressed = compressed && decompressFile(archivePath, outputPath);
        clock_gettime(CLOCK_MONOTONIC, &decompressEnd);

        if (!compressed || !decompressed) {
            fprintf(stderr, "Прогон %ld не удался: %s\n", i + 1, inputPath);
            remove(archivePath);
            remove(outputPath);
            return 1;
        }

        printf("%s,%ld,%ld,%.9f,%.9f\n",
            name,
            originalSize,
            compressedSize,
            secondsBetween(&compressStart, &compressEnd),
            secondsBetween(&compressEnd, &decompressEnd));
    }

    remove(archivePath);
    remove(outputPath);
    return 0;
}
