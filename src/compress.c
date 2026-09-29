#include "compress.h"
#include "bitStream.h"
#include "huffmanTree.h"
#include "tempOutput.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define COMPRESS_BUFFER_SIZE 65536

// Записывает 8 байт uint64_t в little-endian порядке.
static bool writeUint64(FILE* file, uint64_t value)
{
    for (int i = 0; i < 8; i++) {
        if (fputc((int)(value & 0xFFu), file) == EOF) {
            return false;
        }
        value >>= 8;
    }
    return true;
}

static bool countFrequencies(FILE* file, FrequencyTable freq, uint64_t* outSize)
{
    if (!file || !outSize) {
        return false;
    }

    for (int i = 0; i < 256; i++) {
        freq[i] = 0;
    }

    uint8_t buffer[COMPRESS_BUFFER_SIZE];
    uint64_t totalSize = 0;

    while (true) {
        size_t bytesRead = fread(buffer, 1, sizeof(buffer), file);

        if (bytesRead > 0) {
            if (UINT64_MAX - totalSize < (uint64_t)bytesRead) {
                return false;
            }
            totalSize += (uint64_t)bytesRead;

            for (size_t i = 0; i < bytesRead; i++) {
                if (freq[buffer[i]] == SIZE_MAX) {
                    return false;
                }
                freq[buffer[i]]++;
            }
        }

        if (bytesRead < sizeof(buffer)) {
            if (ferror(file)) {
                return false;
            }
            break;
        }
    }

    *outSize = totalSize;
    return true;
}

static bool encodeFile(FILE* file, const CodeTable* codes, BitWriter* writer)
{
    if (!file || !codes || !writer) {
        return false;
    }

    uint8_t buffer[COMPRESS_BUFFER_SIZE];

    while (true) {
        size_t bytesRead = fread(buffer, 1, sizeof(buffer), file);

        for (size_t i = 0; i < bytesRead; i++) {
            uint8_t byte = buffer[i];
            uint16_t length = getCodeLength(codes, byte);

            for (uint16_t bit = 0; bit < length; bit++) {
                if (bitWriterWriteBit(
                        writer,
                        getCodeBit(codes, byte, bit))
                    != 0) {
                    return false;
                }
            }
        }

        if (bytesRead < sizeof(buffer)) {
            if (ferror(file)) {
                return false;
            }
            break;
        }
    }

    return true;
}

bool compressFile(const char* inputPath, const char* outputPath)
{
    if (!inputPath || !outputPath) {
        return false;
    }

    FILE* inFile = fopen(inputPath, "rb");
    if (!inFile) {
        return false;
    }

    FrequencyTable freq;
    uint64_t dataSize = 0;

    if (!countFrequencies(inFile, freq, &dataSize)) {
        fclose(inFile);
        return false;
    }

    if (fseek(inFile, 0, SEEK_SET) != 0) {
        fclose(inFile);
        return false;
    }

    char tempPath[4096];
    FILE* outFile = NULL;
    if (!createTempOutput(outputPath, tempPath, sizeof(tempPath), &outFile)) {
        fclose(inFile);
        return false;
    }

    if (dataSize == 0) {
        bool success = writeUint64(outFile, 0);

        if (fclose(outFile) != 0) {
            success = false;
        }
        fclose(inFile);

        if (!success) {
            discardTempOutput(tempPath);
            return false;
        }
        return commitTempOutput(tempPath, outputPath);
    }

    Node* root = huffmanBuildTree(freq);
    if (!root) {
        fclose(outFile);
        discardTempOutput(tempPath);
        fclose(inFile);
        return false;
    }

    CodeTable* codes = huffmanBuildCodeTable(root);
    if (!codes) {
        huffmanFreeTree(root);
        fclose(outFile);
        discardTempOutput(tempPath);
        fclose(inFile);
        return false;
    }

    bool success = true;

    if (!writeUint64(outFile, dataSize)) {
        success = false;
    }

    BitWriter writer;
    if (success) {
        bitWriterInit(&writer, outFile);

        if (!huffmanWriteTree(root, &writer)) {
            success = false;
        }
    }

    if (success && !encodeFile(inFile, codes, &writer)) {
        success = false;
    }

    if (success && bitWriterFlush(&writer) != 0) {
        success = false;
    }

    if (fclose(outFile) != 0) {
        success = false;
    }
    fclose(inFile);

    huffmanFreeCodeTable(codes);
    huffmanFreeTree(root);

    if (!success) {
        discardTempOutput(tempPath);
        return false;
    }

    return commitTempOutput(tempPath, outputPath);
}
