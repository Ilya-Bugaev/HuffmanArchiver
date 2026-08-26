#include "decompress.h"
#include "bitStream.h"
#include "huffmanTree.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DECOMPRESS_BUFFER_SIZE 65536

// Читает 8 байт из файла как uint64_t в little-endian порядке
static bool readUint64(FILE* file, uint64_t* outValue)
{
    uint64_t value = 0;
    for (int i = 0; i < 8; i++) {
        int byte = fgetc(file);
        if (byte == EOF) {
            return false;
        }
        value |= ((uint64_t)(uint8_t)byte) << (8 * i);
    }
    *outValue = value;
    return true;
}

// Освобождает все ресурсы, удаляет недописанный выходной файл и возвращает false.
// Вспомогательная функция для единообразной обработки ошибок.
static bool fail(Node* root, uint8_t* buffer, FILE* inFile, FILE* outFile, const char* outputPath)
{
    if (buffer) {
        free(buffer);
    }
    if (root) {
        huffmanFreeTree(root);
    }
    if (inFile) {
        fclose(inFile);
    }
    if (outFile) {
        fclose(outFile);
        remove(outputPath);
    }
    return false;
}

bool decompressFile(const char* inputPath, const char* outputPath)
{
    if (!inputPath || !outputPath) {
        return false;
    }

    FILE* inFile = fopen(inputPath, "rb");
    if (!inFile) {
        return false;
    }

    uint64_t originalSize = 0;
    if (!readUint64(inFile, &originalSize)) {
        fclose(inFile);
        return false;
    }

    FILE* outFile = fopen(outputPath, "wb");
    if (!outFile) {
        fclose(inFile);
        return false;
    }

    if (originalSize == 0) {
        fclose(inFile);
        fclose(outFile);
        return true;
    }

    BitReader reader;
    bitReaderInit(&reader, inFile);

    Node* root = huffmanReadTree(&reader);
    if (!root) {
        return fail(NULL, NULL, inFile, outFile, outputPath);
    }

    size_t bufferSize = DECOMPRESS_BUFFER_SIZE;
    if (originalSize < bufferSize) {
        bufferSize = (size_t)originalSize;
    }
    uint8_t* buffer = malloc(bufferSize);
    if (!buffer) {
        return fail(root, NULL, inFile, outFile, outputPath);
    }

    Node* current = root;
    size_t bufferPosition = 0;

    for (uint64_t written = 0; written < originalSize; written++) {
        while (!isLeaf(current)) {
            int bit = bitReaderReadBit(&reader);
            if (bit < 0) {
                return fail(root, buffer, inFile, outFile, outputPath);
            }
            current = (bit == 0) ? getLeft(current) : getRight(current);
            if (!current) {
                return fail(root, buffer, inFile, outFile, outputPath);
            }
        }

        buffer[bufferPosition++] = getSymbol(current);

        if (bufferPosition == bufferSize) {
            if (fwrite(buffer, 1, bufferPosition, outFile) != bufferPosition) {
                return fail(root, buffer, inFile, outFile, outputPath);
            }
            bufferPosition = 0;
        }

        current = root;
    }

    if (bufferPosition > 0) {
        if (fwrite(buffer, 1, bufferPosition, outFile) != bufferPosition) {
            return fail(root, buffer, inFile, outFile, outputPath);
        }
    }

    free(buffer);
    huffmanFreeTree(root);
    fclose(inFile);
    fclose(outFile);
    return true;
}