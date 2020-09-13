// Copyright (c) 2012 Pieter Wuille
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef _BITCOIN_ADDRMAN
#define _BITCOIN_ADDRMAN 1

#include "netbase.h"
#include "protocol.h"
#include "util.h"
#include "sync.h"


#include <map>
#include <vector>

#include <openssl/rand.h>


/** Extended statistics about a CAddress */
class CAddrInfo : public CAddress
{
private:
    // where knowledge about this address first came from
    CNetAddr source;

    // last successful connection by us
    int64_t nLastSuccess;

    // last try whatsoever by us:
    // int64_t CAddress::nLastTry

    // connection attempts since last successful attempt
    int nAttempts;

    // reference count in new sets (memory only)
    int nRefCount;

    // in tried set? (memory only)
    bool fInTried;

    // position in vRandom
    int nRandomPos;

    friend class CAddrMan;

public:

    IMPLEMENT_SERIALIZE(
        CAddress* pthis = (CAddress*)(this);
        READWRITE(*pthis);
        READWRITE(source);
        READWRITE(nLastSuccess);
        READWRITE(nAttempts);
    )

    void Init()
    {
        nLastSuccess = 0;
        nLastTry = 0;
        nAttempts = 0;
        nRefCount = 0;
        fInTried = false;
        nRandomPos = -1;
    }

    CAddrInfo(const CAddress &addrIn, const CNetAddr &addrSource) : CAddress(addrIn), source(addrSource)
    {
        Init();
    }

    CAddrInfo() : CAddress(), source()
    {
        Init();
    }

    // Calculate in which "tried" bucket this entry belongs
    int GetTriedBucket(const std::vector<unsigned char> &nKey) const;

    // Calculate in which "new" bucket this entry belongs, given a certain source
    int GetNewBucket(const std::vector<unsigned char> &nKey, const CNetAddr& src) const;

    // Calculate in which "new" bucket this entry belongs, using its default source
    int GetNewBucket(const std::vector<unsigned char> &nKey) const
    {
        return GetNewBucket(nKey, source);
    }

    // Determine whether the statistics about this entry are bad enough so that it can just be deleted
    bool IsTerrible(int64_t nNow = GetAdjustedTime()) const;

    // Calculate the relative chance this entry should be given when selecting nodes to connect to
    double GetChance(int64_t nNow = GetAdjustedTime()) const;

};

// Stochastic address manager
//
// Design goals:
//  * Only keep a limited number of addresses around, so that addr.dat and memory requirements do not grow without bound.
//  * Keep the address tables in-memory, and asynchronously dump the entire to able in addr.dat.
//  * Make sure no (localized) attacker can fill the entire table with his nodes/addresses.
//
// To that end:
//  * Addresses are organized into buckets.
//    * Address that have not yet been tried go into 256 "new" buckets.
//      * Based on the address range (/16 for IPv4) of source of the information, 32 buckets are selected at random
//      * The actual bucket is chosen from one of these, based on the range the address itself is located.
//      * One single address can occur in up to 4 different buckets, to increase selection chances for addresses that
//        are seen frequently. The chance for increasing this multiplicity decreases exponentially.
//      * When adding a new address to a full bucket, a randomly chosen entry (with a bias favoring less recently seen
//        ones) is removed from it first.
//    * Addresses of nodes that are known to be accessible go into 64 "tried" buckets.
//      * Each address range selects at random 4 of these buckets.
//      * The actual bucket is chosen from one of these, based on the full address.
//      * When adding a new good address to a full bucket, a randomly chosen entry (with a bias favoring less recently
//        tried ones) is evicted from it, back to the "new" buckets.
//    * Bucket selection is based on cryptographic hashing, using a randomly-generated 256-bit key, which should not
//      be observable by adversaries.
//    * Several indexes are kept for high performance. Defining DEBUG_ADDRMAN will introduce frequent (and expensive)
//      consistency checks for the entire data structure.

// total number of buckets for tried addresses
#define ADDRMAN_TRIED_BUCKET_COUNT 64

// maximum allowed number of entries in buckets for tried addresses
#define ADDRMAN_TRIED_BUCKET_SIZE 64

// total number of buckets for new addresses
#define ADDRMAN_NEW_BUCKET_COUNT 256

// maximum allowed number of entries in buckets for new addresses
#define ADDRMAN_NEW_BUCKET_SIZE 64

// over how many buckets entries with tried addresses from a single group (/16 for IPv4) are spread
#define ADDRMAN_TRIED_BUCKETS_PER_GROUP 4

// over how many buckets entries with new addresses originating from a single group are spread
#define ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP 32

// in how many buckets for entries with new addresses a single address may occur
#def