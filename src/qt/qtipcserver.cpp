// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/version.hpp>
#if defined(WIN32) && BOOST_VERSION == 104900
#define BOOST_INTERPROCESS_HAS_WINDOWS_KERNEL_BOOTTIME
#defi