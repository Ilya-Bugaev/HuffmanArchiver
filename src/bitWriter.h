#pragma once

#include <stdio.h>

// Побитовая запись в файл; устройство скрыто в .c
typedef struct BitWriter BitWriter;

// Создаёт писателя битов для file. NULL при ошибке malloc.
BitWriter* bitWriterCreate(FILE* file);

// Освобождает писателя, не сбрасывая буфер и не закрывая файл. NULL-указатель безопасен.
void bitWriterFree(BitWriter* writer);

// Запись одного бита (0 или 1)
int bitWriterWriteBit(BitWriter* writer, int bit);

// Сброс буфера: оставшиеся биты дополняются нулями до полного байта и записываются в файл
int bitWriterFlush(BitWriter* writer);
