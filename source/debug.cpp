#include "debug.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <switch.h>

void OutputDebugString(const char *format, ...) {
    char tmp[0x1000] = {};
    va_list args;
    va_start(args, format);
    vsnprintf(tmp, 0x1000, format, args);
    va_end(args);
    svcOutputDebugString(tmp, strnlen(tmp, 0x1000));
    vfprintf(stdout, format, args);
}

void OutputDebugBytes(const void *buf, size_t len) {
    if (buf == NULL || len == 0) {
        return;
    }

    const u8 *bytes = static_cast<const u8 *>(buf);
    int count = 0;
    OutputDebugString("\n\n00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
    OutputDebugString("-----------------------------------------------\n");

    for (size_t i = 0; i < len; i++) {
        OutputDebugString("%02x ", bytes[i]);
        count++;
        if ((count % 16) == 0) {
            OutputDebugString("\n");
        }
    }

    OutputDebugString("\n");
}