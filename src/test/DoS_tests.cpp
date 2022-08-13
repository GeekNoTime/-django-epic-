//
// Unit tests for denial-of-service detection/prevention code
//
#include <algorithm>

#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

#include "main.h"
#include "wallet.h"
#include "net.h"
#include "util.h"

#include <stdint.h>

// Tests this internal-to-main.cpp method:
extern bool AddOrphanTx(const CDataStream& vMsg);
extern unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans);
extern std::map<uint256, CDataStream*> mapOrphanTransactions;
extern std::map<uint256, std::map<uint256, CD