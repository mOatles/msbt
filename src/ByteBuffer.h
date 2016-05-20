#pragma once

#include "Types.h"

#include <vector>
#include <string>

struct ByteBuffer
{
    void writeU32BE (u32 num)
    {
        u8 *fml = (u8*)&num;
        m_storage.push_back(fml[3]);
        m_storage.push_back(fml[2]);
        m_storage.push_back(fml[1]);
        m_storage.push_back(fml[0]);
    }

    void writeU16BE (u16 num)
    {
        u8 *fml = (u8*)&num;
        m_storage.push_back(fml[1]);
        m_storage.push_back(fml[0]);
    }

    void writeU16LE (u16 num)
    {
        u8 *fml = (u8*)&num;
        m_storage.push_back(fml[0]);
        m_storage.push_back(fml[1]);
    }

    void writeSizedString (const std::string& str)
    {
        m_storage.push_back(str.size());

        for (char c : str) {
            m_storage.push_back(c);
        }
    }

    void writeWideString (const std::vector<u16>& str)
    {
        for (u16 c : str) {
            writeU16BE(c);
        }

        m_storage.push_back(0);
        m_storage.push_back(0);
    }

    size_t size() const
    {
        return m_storage.size();
    }

    u8* data()
    {
        return m_storage.data();
    }

private:
    std::vector<u8> m_storage;
};

