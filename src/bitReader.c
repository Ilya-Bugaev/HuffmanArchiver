#include "bitReader.h"

#include <stdint.h>
#include <stdlib.h>

struct BitReader {
    FILE* file;
    uint8_t buffer;
    int bitCount;
};

BitReader* bitReaderCreate(FILE* file)
{
    BitReader* reader = malloc(sizeof(BitReader));
    if (!reader) {
        return NULL;
    }
    reader->file = file;
    reader->buffer = 0;
    reader->bitCount = 0;
    return reader;
}

void bitReaderFree(BitReader* reader)
{
    free(reader);
}

int bitReaderReadBit(BitReader* reader)
{
    if (reader->bitCount == 0) {
        int byte = fgetc(reader->file);
        if (byte == EOF) {
            return -1;
        }
        reader->buffer = (uint8_t)byte;
        reader->bitCount = 8;
    }

    int bit = (reader->buffer >> 7) & 1;
    reader->buffer = (uint8_t)(reader->buffer << 1);
    reader->bitCount--;
    return bit;
}
