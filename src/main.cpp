#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "Util.h"
#include "Types.h"
#include "ByteBuffer.h"
#include "jsmn.h"

#include <map>
#include <string>
#include <vector>

constexpr u32 HashMultiplier { 0x492 };
constexpr u32 NumLabels { 101 };

constexpr u16 BigEndian { 0xFEFF };
constexpr u16 LittleEndian { 0xFFFE };

typedef std::vector<u8> String;

struct StringValue
{
    StringValue() {}

    StringValue (std::vector<String> values) : values(values)
    {
    }

    StringValue (String str)
    {
        String currSubStr;

        auto size = str.size();

        for (int i = 0; i < size; ++i) {
            if (str[i] == 0) {
                values.push_back(currSubStr);
                currSubStr.clear();
                continue;
            }

            u8 c = str[i];

            if (c == '\\' && i+1 < size) {
                switch (str[i+1]) {
                    case 'n':
                        c = '\n';
                        break;
                    case 'r':
                        c = '\r';
                        break;
                    case 't':
                        c = '\t';
                        break;
                    case '\\':
                        c = '\\';
                        break;
                    default:
                        c = ' '; // XXX: is there something better for this?
                        break;
                }

                ++i; // Skip next character.
            }

            currSubStr.push_back(c);
        }

        values.push_back(currSubStr);
    }

    std::vector<String> values;
    u32 id;
};

typedef std::map<std::string, StringValue> StringMap;

#pragma pack(push, 1)
struct MSBTHeader
{
    char magic[8]; // "MsgStdBn"
    u16 endianness;
    u16 unk0;
    u8 charSize;
    u8 unk1;
    u16 numSections;
    u16 unk2;
    u32 fileSize;
    u8 pad[10];
};

struct ChunkHeader
{
    char tag[4];
    u32 size;
    u8 pad[8];
};

struct MSBTLabelGroup
{
    u32 numLabels;
    u32 offset; // from start of LBL1
};
#pragma pack(pop)

u32 hashString (const std::string& str)
{
    u32 hash = 0;

    for (char c : str) {
        hash = hash * HashMultiplier + c;
    }

    hash &= 0xFFFFFFFF;
    hash %= NumLabels;

    return hash;
}

StringMap parseFile (const char *filename)
{
    std::vector<String> strings;
    StringMap stringMap;

    FILE *file = fopen(filename, "rb");
    
    MSBTHeader header;
    ChunkHeader lblHeader;

    fread(&header, sizeof(header), 1, file);
    fread(&lblHeader, sizeof(lblHeader), 1, file);

    String lbl(endianReverse32(lblHeader.size));
    fread(lbl.data(), lbl.size(), 1, file);

    // FIXME: hack 'cause I don't give a shit about ATR1 right now.
    fseek(file, 0x23, SEEK_CUR);

    ChunkHeader txtHeader;
    fread(&txtHeader, sizeof(txtHeader), 1, file);
    String txt(endianReverse32(txtHeader.size));
    fread(txt.data(), txt.size(), 1, file);
    fclose(file);

    //
    // Read string values
    //
    u32 numStrings;
    memcpy(&numStrings, txt.data(), sizeof(numStrings));
    numStrings = endianReverse32(numStrings);

    std::vector<u32> strOffsets(numStrings);
    memcpy(strOffsets.data(), txt.data() + 4, strOffsets.size() * 4);

    for (u32 offIdx = 0; offIdx < numStrings; ++offIdx) {
        u32 offset = strOffsets[offIdx];
        u8 *nextOff = NULL;

        if (offIdx < numStrings-1) {
            nextOff = txt.data() + endianReverse32(strOffsets[offIdx+1]);
        }

        u8 *ptr = txt.data() + endianReverse32(offset);
        String buf;    

        while (*ptr || (nextOff && ptr+2 < nextOff)) {
            buf.push_back(*(ptr+1));
            ptr += 2;
        }

        strings.push_back(buf);
    }

    //
    // Read labels
    //

    // XXX: not used, 'cause... yknow. no need.
    u32 numLabels;
    memcpy(&numLabels, lbl.data(), sizeof(numLabels));

    MSBTLabelGroup labels[NumLabels];
    memcpy(labels, lbl.data()+4, sizeof(labels));

    for (u32 lblNum = 0; lblNum < NumLabels; ++lblNum) {
        u32 ptr = endianReverse32(labels[lblNum].offset);

        for (u32 i = 0; i < endianReverse32(labels[lblNum].numLabels); ++i) {
            std::string str;
            str.resize(lbl[ptr++]);

            memcpy((char*)str.data(), lbl.data()+ptr, str.size());
            ptr += str.size();

            u32 strOffset;
            memcpy(&strOffset, lbl.data()+ptr, sizeof(strOffset));
            strOffset = endianReverse32(strOffset);
            ptr += sizeof(strOffset);

            stringMap.emplace(str, strings[strOffset]);
        }
    }

    return stringMap;
}

void dumpFile (StringMap& stringMap, const char *filename)
{
    MSBTHeader header = {};
    memcpy(header.magic, "MsgStdBn", sizeof(header.magic));
    header.endianness = endianReverse16(BigEndian);
    header.charSize = 1;
    header.unk1 = 3;
    header.numSections = endianReverse16(3);

    std::vector<std::string> labelGroups[NumLabels];

    u32 j = 0;
    for (auto& kv : stringMap) {
        u32 h = hashString(kv.first);
        labelGroups[h].push_back(kv.first);
        kv.second.id = j++;
    }

    ByteBuffer lblData;
    ByteBuffer lblData2;
    lblData.writeU32BE(NumLabels);

    u32 ptr = NumLabels * sizeof(MSBTLabelGroup) + 4;
    for (u32 i = 0; i < NumLabels; ++i) {
        lblData.writeU32BE(labelGroups[i].size());
        lblData.writeU32BE(ptr);

        for (auto& label : labelGroups[i]) {
            lblData2.writeSizedString(label);
            lblData2.writeU32BE(stringMap[label].id);
            ptr += label.size() + 1 + 4;
        }
    }

    // TODO: figure out what this is used for.
    ByteBuffer atrData;
    atrData.writeU32BE(stringMap.size());
    atrData.writeU32BE(0);

    ByteBuffer txtData;
    ByteBuffer txtData2;
    txtData.writeU32BE(stringMap.size());
    ptr = stringMap.size() * 4 + 4;
    for (auto& kv : stringMap) {
        txtData.writeU32BE(ptr);

        for (auto& value : kv.second.values) {
            txtData2.writeFatString(value);
            ptr += value.size() * 2 + 2;
        }
    }

    ChunkHeader lblHeader = {};
    memcpy(lblHeader.tag, "LBL1", 4);
    lblHeader.size = endianReverse32(lblData.size() + lblData2.size());

    ChunkHeader atrHeader = {};
    memcpy(atrHeader.tag, "ATR1", 4);
    atrHeader.size = endianReverse32(atrData.size());

    ChunkHeader txtHeader = {};
    memcpy(txtHeader.tag, "TXT2", 4);
    txtHeader.size = endianReverse32(txtData.size() + txtData2.size());

    FILE *file = fopen(filename, "wb");
    fwrite(&header, sizeof(header), 1, file);
    fwrite(&lblHeader, sizeof(lblHeader), 1, file);
    fwrite(lblData.data(), lblData.size(), 1, file);
    fwrite(lblData2.data(), lblData2.size(), 1, file);
    pad16(file);

    fwrite(&atrHeader, sizeof(atrHeader), 1, file);
    fwrite(atrData.data(), atrData.size(), 1, file);
    pad16(file);

    fwrite(&txtHeader, sizeof(txtHeader), 1, file);
    fwrite(txtData.data(), txtData.size(), 1, file);
    fwrite(txtData2.data(), txtData2.size(), 1, file);
    pad16(file);

    u32 fileSize = endianReverse32(ftell(file));

    // Seek to header.fileSize
    u32 fileSizeOffs = (u32)((u8*)&header.fileSize - (u8*)&header);
    fseek(file, fileSizeOffs, SEEK_SET);
    fwrite(&fileSize, 4, 1, file);

    fclose(file);
}

int main (int argc, char **argv)
{
    if (argc == 1) {
        printf("Usage: %s input.msbt\n", argv[0]);
        return 0;
    }

    auto strings = parseFile(argv[1]);

    FILE *jsonFile = fopen("melee.json", "rb");
    fseek(jsonFile, 0, SEEK_END);
    u32 len = ftell(jsonFile);
    fseek(jsonFile, 0, SEEK_SET);

    std::string jsonStr;
    jsonStr.resize(len);
    fread((char*)jsonStr.data(), jsonStr.size(), 1, jsonFile);
    fclose(jsonFile);

    jsmn_parser p;
    jsmntok_t t[1024]; // TODO: support files of any size.

    jsmn_init(&p);
    int r = jsmn_parse(&p, jsonStr.c_str(), jsonStr.size(), t, 1024);

    if (t[0].type != JSMN_OBJECT) {
        fprintf(stderr, "Invalid JSON format: Top level token must be an object.\n");
        return 1;
    }

    for (int i = 1; i < r; i += 2) {
        if (i + 1 >= r) {
            fprintf(stderr, "Invalid JSON format: Odd number of tokens.\n");
            return 1;
        }

        std::string key;

        key.resize(t[i].end - t[i].start);
        memcpy((char*)key.data(), jsonStr.c_str() + t[i].start, key.size());

        if (t[i+1].type == JSMN_STRING) {
            String value;
            value.resize(t[i+1].end - t[i+1].start);
            memcpy(value.data(), jsonStr.c_str() + t[i+1].start, value.size());

            strings.emplace(key, value);
        } else if (t[i+1].type == JSMN_ARRAY) {
            auto size = t[i+1].size;
            std::vector<String> str;

            for (int j = 1; j < size+1; ++j) {
                String value;
                value.resize(t[i+1+j].end - t[i+1+j].start);
                memcpy(value.data(), jsonStr.c_str() + t[i+1+j].start, value.size());

                str.push_back(value);
            }

            strings.emplace(key, str);

            i += size;
        } else {
            fprintf(stderr, "Invalid JSON value.\n");
            return 1;
        }
    }

    dumpFile(strings, "./melee2.msbt");

    return 0;
}
