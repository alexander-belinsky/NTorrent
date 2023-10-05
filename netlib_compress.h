#ifndef NTORRENT_NETLIB_COMPRESS_H
#define NTORRENT_NETLIB_COMPRESS_H

#include "netlib_header.h"

namespace netlib {
    std::vector<uint8_t> CompressData(std::vector<uint8_t>& prevData) {
        std::vector<bool> binData(prevData.size() * 8);

        for (size_t i = 0; i < prevData.size(); i++) {
            for (short j = 0; j < 8; j++)
                binData[i * 8 + j] = (prevData[i] >> j) & 1;
        }

        std::vector<bool> newBinData;


    }

    std::vector<uint8_t> DecompressData(std::vector<uint8_t>& prevData) {

    }
}

#endif //NTORRENT_NETLIB_COMPRESS_H
