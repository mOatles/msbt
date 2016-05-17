#pragma once

#include "Types.h"

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

    void writeSizedString (const std::string& str)
    {
        auto s = str.size();
        m_storage.push_back(s);
        for (u32 i = 0; i < s; ++i) {
            m_storage.push_back(str[i]);
        }
    }

    void writeFatString (const std::string& str)
    {
        for (char c : str) {
            m_storage.push_back(0);
            m_storage.push_back(c);
        }

        m_storage.push_back(0);
        m_storage.push_back(0);
    }

    size_t size() const
    {
        return m_storage.size();
    }

    u8 *data()
    {
        return m_storage.data();
    }

private:
    std::vector<u8> m_storage;
};

