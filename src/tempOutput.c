#define _POSIX_C_SOURCE 200809L

#include "tempOutput.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool createTempOutput(const char* outputPath, char* tempPath, size_t tempPathSize, FILE** outFile)
{
    if (!outputPath || !tempPath || !outFile || tempPathSize == 0) {
        return false;
    }

    int written = snprintf(tempPath, tempPathSize, "%s.XXXXXX", outputPath);
    if (written < 0 || (size_t)written >= tempPathSize) {
        return false;
    }

    int fd = mkstemp(tempPath);
    if (fd < 0) {
        return false;
    }
    fchmod(fd, 0644);

    FILE* file = fdopen(fd, "wb");
    if (!file) {
        close(fd);
        remove(tempPath);
        return false;
    }

    *outFile = file;
    return true;
}

bool commitTempOutput(const char* tempPath, const char* outputPath)
{
    if (!tempPath || !outputPath) {
        return false;
    }
    if (rename(tempPath, outputPath) == 0) {
        return true;
    }
    remove(tempPath);
    return false;
}

void discardTempOutput(const char* tempPath)
{
    if (tempPath) {
        remove(tempPath);
    }
}
