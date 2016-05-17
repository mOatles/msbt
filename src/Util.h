#pragma once

#include "Types.h"

u32 endianReverse32 (u32 num)
{
    auto bytes = (u8*)&num;

    u32 ret;
    u8 *r = (u8*)&ret;
    *(r+0) = *(bytes+3);
    *(r+1) = *(bytes+2);
    *(r+2) = *(bytes+1);
    *(r+3) = *(bytes+0);

    return ret;
}

u16 endianReverse16 (u16 num)
{
    auto bytes = (u8*)&num;

    u16 ret;
    u8 *r = (u8*)&ret;
    *(r+0) = *(bytes+1);
    *(r+1) = *(bytes+0);

    return ret;
}

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

