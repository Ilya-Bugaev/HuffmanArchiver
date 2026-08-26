#pragma once

#include <stdint.h>
#include <stdio.h>

typedef struct {
    FILE* file;
    uint8_t buffer;
    int bitCount;
} BitWriter;

typedef struct {
    FILE* file;
    uint8_t buffer;
    int bitCount;
} BitReader;

// Инициализация писателя битов
void bitWriterInit(BitWriter* writer, FILE* file);

// Запись одного бита (0 или 1)
int bitWriterWriteBit(BitWriter* writer, int bit);

// Сброс буфера: оставшиеся биты дополняются нулями до полного байта и записываются в файл
int bitWriterFlush(BitWriter* writer);

// Инициализация читателя битов
void bitReaderInit(BitReader* reader, FILE* file);

// Чтение одного бита
int bitReaderReadBit(BitReader* reader);