#include "bitWriter.h"

#include <stdint.h>
#include <stdlib.h>

struct BitWriter {
    FILE* file;
    uint8_t buffer;
    int bitCount;
};

BitWriter* bitWriterCreate(FILE* file)
{
    BitWriter* writer = malloc(sizeof(BitWriter));
    if (!writer) {
        return NULL;
    }
    writer->file = file;
    writer->buffer = 0;
    writer->bitCount = 0;
    return writer;
}

void bitWriterFree(BitWriter* writer)
{
    free(writer);
}

int bitWriterWriteBit(BitWriter* writer, int bit)
{
    writer->buffer = (uint8_t)((writer->buffer << 1) | (bit && 1));
    writer->bitCount++;

    if (writer->bitCount == 8) {
        if (fputc(writer->buffer, writer->file) == EOF) {
            return -1;
        }
        writer->buffer = 0;
        writer->bitCount = 0;
    }
    return 0;
}

int bitWriterFlush(BitWriter* writer)
{
    if (writer->bitCount > 0) {
        writer->buffer = (uint8_t)(writer->buffer << (8 - writer->bitCount));
        if (fputc(writer->buffer, writer->file) == EOF) {
            return -1;
        }
        writer->buffer = 0;
        writer->bitCount = 0;
    }
    return 0;
}
