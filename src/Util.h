#pragma once

#include "Types.h"

#include <vector>

u32 endianReverse32 (u32 num)
{
    u8 *bytes = (u8*)&num;

    u32 ret;
    u8 *r = (u8*)&ret;
    *(r+0) = *(bytes+3);
    *(r+1) = *(bytes+2);
    *(r+2) = *(bytes+1);
    *(r+3) = *(bytes+0);

    return ret;
}

void endianReverse32 (u32 *num)
{
    u8 *bytes = (u8*)num;

    u32 tmp = *num;
    u8 *t = (u8*)&tmp;
    *(bytes+0) = *(t+3);
    *(bytes+1) = *(t+2);
    *(bytes+2) = *(t+1);
    *(bytes+3) = *(t+0);
}

u16 endianReverse16 (u16 num)
{
    u8 *bytes = (u8*)&num;

    u16 ret;
    u8 *r = (u8*)&ret;
    *(r+0) = *(bytes+1);
    *(r+1) = *(bytes+0);

    return ret;
}

// Pad current file length to multiple of 16 bytes.
void pad16 (FILE *file)
{
    auto offset = ftell(file);
    u32 neededPadding = 16 - (offset % 16);

    if (neededPadding == 16) {
        return;
    }

    std::vector<u8> padData (neededPadding, 0xAB);

    fwrite(padData.data(), padData.size(), 1, file);
}

