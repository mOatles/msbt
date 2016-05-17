#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "Util.h"
#include "Types.h"
#include "ByteBuffer.h"

constexpr u32 HashMultiplier { 0x492 };
constexpr u32 NumLabels { 101 };

constexpr u16 BigEndian { 0xFEFF };
constexpr u16 LittleEndian { 0xFFFE };

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
    u8 unk[8]; // probably padding, but who cares?
};

struct MSBTLabelGroup
{
    u32 numLabels;
    u32 offset; // from start of LBL1
};
#pragma pack(pop)

u32 hashString (const std::string& str)
{
    int len = str.size();
    u32 hash = 0;

    for (int i = 0; i < len; ++i) {
        hash = hash * HashMultiplier + str[i];
    }

    hash &= 0xFFFFFFFF;
    hash %= NumLabels;

    return hash;
}

std::string readString (u8 *buffer)
{
    u16 *ptr = (u16*)buffer;
    
    std::string out;

    while (ptr && *ptr) {
        out.push_back(*((u8*)ptr+1));
        ptr++;
    }

    return out;
}

StringMap parseFile (const char *filename)
{
    std::vector<std::string> strings;
    StringMap imtired;

    FILE *file = fopen(filename, "rb");
    
    MSBTHeader header;
    ChunkHeader lblHeader;

    fread(&header, sizeof(header), 1, file);
    fread(&lblHeader, sizeof(lblHeader), 1, file);

    u8 *lblData = (u8*)malloc(endianReverse32(lblHeader.size));
    fread(lblData, endianReverse32(lblHeader.size), 1, file);

    // FIXME: hack 'cause I don't give a shit about ATR1 right now.
    fseek(file, 0x23, SEEK_CUR);

    ChunkHeader txtHeader;
    fread(&txtHeader, sizeof(txtHeader), 1, file);
    u8 *txtData = (u8*)malloc(endianReverse32(txtHeader.size));
    fread(txtData, endianReverse32(txtHeader.size), 1, file);
    fclose(file);

    //
    // Read string values
    //

    u32 numStrings;
    memcpy(&numStrings, txtData, sizeof(numStrings));
    numStrings = endianReverse32(numStrings);

    for (u32 i = 0; i < numStrings; ++i) {
        u32 firstOffset;
        memcpy(&firstOffset, txtData + 4 + 4*i, 4);
        strings.push_back(readString(txtData + endianReverse32(firstOffset)));
    }

    //
    // Read labels
    //

    // XXX: not used, 'cause... yknow. no need.
    u32 numLabels;
    memcpy(&numLabels, lblData, sizeof(numLabels));

    MSBTLabelGroup labels[NumLabels];
    memcpy(labels, lblData+4, sizeof(labels));

    for (u32 lblNum = 0; lblNum < NumLabels; ++lblNum) {
        u32 ptr = endianReverse32(labels[lblNum].offset);

        for (u32 i = 0; i < endianReverse32(labels[lblNum].numLabels); ++i) {
            std::string str;
            str.resize(lblData[ptr]);

            memcpy((char*)str.data(), lblData+ptr+1, str.size());

            u32 strOffset;
            memcpy(&strOffset, lblData+ptr+1+str.size(), sizeof(u32));
            strOffset = endianReverse32(strOffset);

            imtired[str] = strings[strOffset];

            ptr += 1 + str.size() + 4;
        }
    }

    free(lblData);
    free(txtData);

    return imtired;
}

void dumpFile (StringMap& stringMap, const char *filename)
{
    MSBTHeader header = {};
    header.magic[0] = 'M';
    header.magic[1] = 's';
    header.magic[2] = 'g';
    header.magic[3] = 'S';
    header.magic[4] = 't';
    header.magic[5] = 'd';
    header.magic[6] = 'B';
    header.magic[7] = 'n';
    header.endianness = endianReverse16(BigEndian);
    header.charSize = 1;
    header.unk1 = 3;
    header.numSections = endianReverse16(3);

    std::array<std::vector<std::string>, NumLabels> labelGroups;
    std::map<std::string, u32> valueIds;

    u32 j = 0;
    for (auto& kv : stringMap) {
        u32 h = hashString(kv.first);
        labelGroups[h].push_back(kv.first);
        valueIds[kv.second] = j++;
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
            auto s = stringMap[label];
            lblData2.writeU32BE(valueIds[s]);
            ptr += label.size() + 1 + 4;
        }
    }

    ByteBuffer atrData;
    atrData.writeU32BE(0x955);
    atrData.writeU32BE(0);

    ByteBuffer txtData;
    ByteBuffer txtData2;
    txtData.writeU32BE(stringMap.size());
    ptr = stringMap.size() * 4 + 4;
    for (auto& kv : stringMap) {
        txtData.writeU32BE(ptr);

        txtData2.writeFatString(kv.second);

        ptr += kv.second.size() * 2 + 2;
    }

    ChunkHeader lblHeader = {};
    lblHeader.tag[0] = 'L';
    lblHeader.tag[1] = 'B';
    lblHeader.tag[2] = 'L';
    lblHeader.tag[3] = '1';
    lblHeader.size = endianReverse32(lblData.size() + lblData2.size());

    ChunkHeader atrHeader = {};
    atrHeader.tag[0] = 'A';
    atrHeader.tag[1] = 'T';
    atrHeader.tag[2] = 'R';
    atrHeader.tag[3] = '1';
    atrHeader.size = endianReverse32(atrData.size());

    ChunkHeader txtHeader = {};
    txtHeader.tag[0] = 'T';
    txtHeader.tag[1] = 'X';
    txtHeader.tag[2] = 'T';
    txtHeader.tag[3] = '2';
    txtHeader.size = endianReverse32(txtData.size() + txtData2.size());

    u32 totalSize = sizeof(ChunkHeader) * 3;
    totalSize += lblData.size() + lblData2.size();
    totalSize += atrData.size();
    totalSize += txtData.size() + txtData2.size();
    header.fileSize = endianReverse32(totalSize);

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

    fclose(file);
}

int main (int argc, char **argv)
{
    if (argc == 1) {
        printf("Usage: %s input.msbt\n", argv[0]);
        return 0;
    }

    auto strings = parseFile(argv[1]);

    strings["MmelCharaA_01_Marth"] = "The Dude";
    strings["MmelCharaC_01_Marth"] = "MARF";
    strings["MmelCharaN_01_Marth"] = "Marf";
    strings["MmelCharaR_01_Marth"] = "MARF";

    printf("%s\n", strings["MmelCharaA_01_Marth"].c_str());

    dumpFile(strings, "./melee22.msbt");

    return 0;
}

