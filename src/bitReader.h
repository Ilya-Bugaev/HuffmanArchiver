#pragma once

#include <stdio.h>

// Побитовое чтение из файла; устройство скрыто в .c
typedef struct BitReader BitReader;

// Создаёт читателя битов для file. NULL при ошибке malloc.
BitReader* bitReaderCreate(FILE* file);

// Освобождает читателя, не закрывая файл. NULL-указатель безопасен.
void bitReaderFree(BitReader* reader);

// Чтение одного бита
int bitReaderReadBit(BitReader* reader);
