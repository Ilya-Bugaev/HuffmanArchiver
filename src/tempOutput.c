#include "tempOutput.h"

#include <stdio.h>

#define TEMP_NAME_ATTEMPTS 1000

bool createTempOutput(const char* outputPath, char* tempPath, size_t tempPathSize, FILE** outFile)
{
    if (!outputPath || !tempPath || !outFile || tempPathSize == 0) {
        return false;
    }

    for (unsigned attempt = 0; attempt < TEMP_NAME_ATTEMPTS; attempt++) {
        int written = snprintf(tempPath, tempPathSize, "%s.tmp%u", outputPath, attempt);
        if (written < 0 || (size_t)written >= tempPathSize) {
            return false;
        }

        // Режим "x" (C11) не открывает уже существующий файл, поэтому чужой файл не затирается
        FILE* file = fopen(tempPath, "wbx");
        if (file) {
            *outFile = file;
            return true;
        }
    }
    return false;
}

bool commitTempOutput(const char* tempPath, const char* outputPath)
{
    if (!tempPath || !outputPath) {
        return false;
    }
    if (rename(tempPath, outputPath) == 0) {
        return true;
    }
#ifdef _WIN32
    // rename в Windows не заменяет существующий файл, поэтому подмена здесь не атомарна
    if (remove(outputPath) == 0 && rename(tempPath, outputPath) == 0) {
        return true;
    }
#endif
    remove(tempPath);
    return false;
}

void discardTempOutput(const char* tempPath)
{
    if (tempPath) {
        remove(tempPath);
    }
}
