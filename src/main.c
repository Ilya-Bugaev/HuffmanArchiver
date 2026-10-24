#include "compress.h"
#include "decompress.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void printUsage(const char* programName)
{
    fprintf(stderr, "Использование:\n");
    fprintf(stderr, "  %s c <входной_файл> <выходной_архив>     — сжать файл\n", programName);
    fprintf(stderr, "  %s d <входной_архив> <выходной_файл>     — разжать файл\n", programName);
}

int main(int argc, char** argv)
{
    if (argc != 4) {
        printUsage(argv[0]);
        return 1;
    }

    const char* mode = argv[1];
    const char* inputPath = argv[2];
    const char* outputPath = argv[3];

    const char* action = NULL;
    bool success = false;

    if (strcmp(mode, "c") == 0) {
        action = "сжать";
        success = compressFile(inputPath, outputPath);
    } else if (strcmp(mode, "d") == 0) {
        action = "разжать";
        success = decompressFile(inputPath, outputPath);
    } else {
        fprintf(stderr, "Неизвестный режим: %s\n", mode);
        printUsage(argv[0]);
        return 1;
    }

    if (!success) {
        fprintf(stderr, "Ошибка: не удалось %s файл \"%s\"\n", action, inputPath);
        return 1;
    }

    return 0;
}
