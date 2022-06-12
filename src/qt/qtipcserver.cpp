// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/version.hpp>
#if defined(WIN32) && BOOST_VERSION == 104900
#define BOOST_INTERPROCESS_HAS_WINDOWS_KERNEL_BOOTTIME
#define BOOST_INTERPROCESS_HAS_KERNEL_BOOTTIME
#endif

#include "qtipcserver.h"
#include "guiconstants.h"
#include "ui_interface.h"
#include "util.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/version.hpp>

#if defined(WIN32) && (!defined(BOOST_INTERPROCESS_HAS_WINDOWS_KERNEL_BOOTTIME) || !defined(BOOST_INTERPROCESS_HAS_KERNEL_BOOTTIME) || BOOST_VERSION < 104900)
#warning Compiling without BOOST_INTERPROCESS_HAS_WINDOWS_KERNEL_BOOTTIME and BOOST_INTERPROCESS_HAS_KERNEL_BOOTTIME uncommented in boost/interprocess/detail/tmp_dir_helpers.hpp or using a boost version before 1.49 may have unintended results see svn.boost.org/trac/boost/ticket/5392
#endif

using namespace boost;
using namespace boost::interprocess;
using namespace boost::posix_time;

#if defined MAC_OSX || defined __FreeBSD__
// URI handling not implemented on OSX yet

void ipcScanRelay(int argc, char *argv[]) { }
void ipcInit(int argc, char *argv[]) { }

#else

static void ipcThread2(void* pArg);

static bool ipcScanCmd(int argc, char *argv[], bool fRelay)
{
    // Check for URI in argv
    bool fSent = false;
    for (int i = 1; i < argc; i++)
    {
        if (boost::algorithm::istarts_with(argv[i], "HongyunCoin2:"))
        {
            const char *strURI = argv[i];
            try {
                boost::interprocess::message_queue mq(boost::interprocess::open_only, BITCOINURI_QUEUE_NAME);
                if (mq.try_send(strURI, strlen(strURI), 0))
                    fSent = true;
                else if (fRelay)
                    break;
            }
            catch (boost::interprocess::interprocess_exception &ex) {
                // don't log the "file not found" exception, because that's normal for
                // the first start of the first instance
                if (ex.get_error_code() != boost::interprocess::not_found_error || !fRelay)
                {
                    printf("main() - boost interprocess exception #%d: %s\n", ex.get_error_code(), ex.what());
                    break;
                }
            }
        }
    }
    return fSent;
}

void ipcScanRelay(int argc, char *argv[])
{
    if (ipcScanCmd(argc, argv, true))
        exit(0);
}

static void ipcThread(void* pArg)
{
    // Make this thread recognisable as the GUI-IPC thread
    RenameThread("HongyunCoin2-gui-ipc");
	
    try
    {
        ipcThread2(pArg);
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, "ipcThread()");
    } catch (...) {
        PrintExceptionContinue(NULL, "ipcThread()");
    }
    printf("ipcThread exited\n");
}

static void ipcThread2(void* pArg)
{
    printf("ipcThread started\n