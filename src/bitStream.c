#include "bitStream.h"

void bitWriterInit(BitWriter* writer, FILE* file)
{
    writer->file = file;
    writer->buffer = 0;
    writer->bitCount = 0;
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

void bitReaderInit(BitReader* reader, FILE* file)
{
    reader->file = file;
    reader->buffer = 0;
    reader->bitCount = 0;
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
