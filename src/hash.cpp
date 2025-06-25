// Copyright 1999-2005, 2011, 2012, 2014-2017, 2020-2021, 2023-2025 Jon Dart. All Rights Reserved.

#include "hash.h"
#include "constant.h"
#include "globals.h"
#include "learn.h"
#include "legal.h"
#include "options.h"
#include "scoring.h"

#include <cstdlib>

Hash::Hash() {
    hashTable = nullptr;
    hashSize = 0;
    hashMask = 0x0ULL;
    hashFree = 0;
    hash_init_done = 0;
}

void Hash::initHash(size_t bytes) {
    if (!hash_init_done) {
        hashSize = bytes / sizeof(HashEntry);
        if (!hashSize) {
            hashMask = 0;
            hash_init_done++;
            return;
        }
        unsigned hashPower;
        for (hashPower = 1; hashPower < 64; hashPower++) {
            if ((1ULL << hashPower) > hashSize) {
                hashPower--;
                break;
            }
        }
        hashSize = 1ULL << hashPower;
        hashMask = (uint64_t)(hashSize - 1);
        size_t hashSizePlus = hashSize + MaxRehash;
        // round up to ensure allocated size is multiple of alignment
        hashSizePlus += MEMORY_ALIGNMENT - (hashSizePlus % MEMORY_ALIGNMENT);
        hashTable = reinterpret_cast<HashEntry *>(
            ALIGNED_MALLOC(MEMORY_ALIGNMENT, sizeof(HashEntry) * hashSizePlus));
        if (hashTable == nullptr) {
            std::cerr << "hash table allocation failed!" << std::endl;
            hashSize = 0;
        }
        clearHash();
        hash_init_done++;
    }
}

void Hash::resizeHash(size_t bytes) {
    freeHash();
    initHash(bytes);
}

void Hash::freeHash() {
    ALIGNED_FREE(hashTable);
    hash_init_done = 0;
}

void Hash::clearHash() {
    if (hashSize == 0)
        return;
    hashFree = hashSize;
    HashEntry empty;
    for (size_t i = 0; i < hashSize; i++) {
        hashTable[i] = empty;
    }
    if (globals::options.learning.position_learning) {
        loadLearnInfo();
    }
}

void Hash::loadLearnInfo() {
    if (hashSize && globals::options.learning.position_learning) {
        std::ifstream plog;
        plog.open(globals::options.learning.file_name.c_str(), std::ios_base::in);
        while (plog.good() && !plog.eof()) {
            LearnRecord rec;
            if (getLearnRecord(plog, rec)) {
                Move best = NullMove;
                if (rec.start != InvalidSquare)
                    best = CreateMove(rec.start, rec.dest, rec.promotion);
                storeHash(rec.hashcode, rec.depth * DEPTH_INCREMENT, 0, /* age */
                          HashEntry::Valid, rec.score,
                          Constants::INVALID_SCORE, // TBD
                          HashEntry::LEARNED_MASK, best);
            }
        }
    }
}
