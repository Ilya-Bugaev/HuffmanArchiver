#include "compress.h"
#include "bitStream.h"
#include "huffmanTree.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Читает содержимое файла целиком в буфер
static bool readEntireFile(const char* path, uint8_t** outData, size_t* outSize)
{
    FILE* file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        return false;
    }

    size_t size = (size_t)length;
    rewind(file);

    if (size == 0) {
        fclose(file);
        *outData = NULL;
        *outSize = 0;
        return true;
    }

    uint8_t* data = malloc(size);
    if (!data) {
        fclose(file);
        return false;
    }

    if (fread(data, 1, size, file) != size) {
        free(data);
        fclose(file);
        return false;
    }

    fclose(file);
    *outData = data;
    *outSize = size;
    return true;
}

// Записывает 8 байт uint64_t в little-endian порядке
static bool writeUint64(FILE* file, uint64_t value)
{
    for (int i = 0; i < 8; i++) {
        if (fputc((int)(value & 0xFF), file) == EOF) {
            return false;
        }
        value >>= 8;
    }
    return true;
}

// Подсчитывает частоты всех 256 возможных значений байтов
static void countFrequencies(const uint8_t* data, size_t size, FrequencyTable freq)
{
    for (int i = 0; i < 256; i++) {
        freq[i] = 0;
    }
    for (size_t i = 0; i < size; i++) {
        freq[data[i]]++;
    }
}

// Кодирует каждый байт данных соответствующим кодом из таблицы и записывает биты через BitWriter
static bool encodeData(const uint8_t* data, size_t size,
    const CodeTable* codes, BitWriter* writer)
{
    for (size_t i = 0; i < size; i++) {
        uint8_t byte = data[i];
        uint16_t length = getCodeLength(codes, byte);
        for (uint16_t bit = 0; bit < length; bit++) {
            if (bitWriterWriteBit(writer, getCodeBit(codes, byte, bit)) != 0) {
                return false;
            }
        }
    }
    return true;
}

bool compressFile(const char* inputPath, const char* outputPath)
{
    if (!inputPath || !outputPath) {
        return false;
    }

    uint8_t* data = NULL;
    size_t dataSize = 0;
    if (!readEntireFile(inputPath, &data, &dataSize)) {
        return false;
    }

    FILE* outFile = fopen(outputPath, "wb");
    if (!outFile) {
        free(data);
        return false;
    }

    if (!writeUint64(outFile, (uint64_t)dataSize)) {
        fclose(outFile);
        remove(outputPath);
        free(data);
        return false;
    }

    if (dataSize == 0) {
        fclose(outFile);
        free(data);
        return true;
    }

    FrequencyTable freq;
    countFrequencies(data, dataSize, freq);

    Node* root = huffmanBuildTree(freq);
    if (!root) {
        fclose(outFile);
        remove(outputPath);
        free(data);
        return false;
    }

    CodeTable* codes = huffmanBuildCodeTable(root);
    if (!codes) {
        huffmanFreeTree(root);
        fclose(outFile);
        remove(outputPath);
        free(data);
        return false;
    }

    BitWriter writer;
    bitWriterInit(&writer, outFile);

    if (!huffmanWriteTree(root, &writer)) {
        huffmanFreeCodeTable(codes);
        huffmanFreeTree(root);
        fclose(outFile);
        remove(outputPath);
        free(data);
        return false;
    }

    if (!encodeData(data, dataSize, codes, &writer)) {
        huffmanFreeCodeTable(codes);
        huffmanFreeTree(root);
        fclose(outFile);
        remove(outputPath);
        free(data);
        return false;
    }

    if (bitWriterFlush(&writer) != 0) {
        huffmanFreeCodeTable(codes);
        huffmanFreeTree(root);
        fclose(outFile);
        remove(outputPath);
        free(data);
        return false;
    }

    huffmanFreeCodeTable(codes);
    huffmanFreeTree(root);
    fclose(outFile);
    free(data);
    return true;
}
