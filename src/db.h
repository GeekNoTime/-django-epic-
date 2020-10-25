// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_DB_H
#define BITCOIN_DB_H

#include "main.h"

#include <map>
#include <string>
#include <vector>

#include <db_cxx.h>

class CAddress;
class CAddrMan;
class CBlockLocator;
class CDiskBlockIndex;
class CDiskTxPos;
class CMasterKey;
class COutPoint;
class CTxIndex;
class CWallet;
class CWalletTx;

extern unsigned int nWalletDBUpdated;

void ThreadFlushWalletDB(void* parg);
bool BackupWallet(const CWallet& wallet, const std::string& strDest);


class CDBEnv
{
private:
    bool fDetachDB;
    bool fDbEnvInit;
    bool fMockDb;
    boost::filesystem::path pathEnv;
    std::string strPath;

    void EnvShutdown();

public:
    mutable CCriticalSection cs_db;
    DbEnv dbenv;
    std::map<std::string, int> mapFileUseCount;
    std::map<std::string, Db*> mapDb;

    CDBEnv();
    ~CDBEnv();
    void MakeMock();
    bool IsMock() { return fMockDb; };

    /*
     * Verify that database file strFile is OK. If it is not,
     * call the callback to try to recover.
     * This must be called BEFORE strFile is opened.
     * Returns true if strFile is OK.
     */
    enum VerifyResult { VERIFY_OK, RECOVER_OK, RECOVER_FAIL };
    VerifyResult Verify(std::string strFile, bool (*recoverFunc)(CDBEnv& dbenv, std::string strFile));
    /*
     * Salvage data from a file that Verify says is bad.
     * fAggressive sets the DB_AGGRESSIVE flag (see berkeley DB->verify() method documentation).
     * Appends binary key/value pairs to vResult, returns true if successful.
     * NOTE: reads the entire database into memory, so cannot be used
     * for huge databases.
     */
    typedef std::pair<std::vector<unsigned char>, std::vector<unsigned char> > KeyValPair;
    bool Salvage(std::string strFile, bool fAggressive, std::vector<KeyValPair>& vResult);

    bool Open(boost::filesystem::path pathEnv_);
    void Close();
    void Flush(bool fShutdown);
    void CheckpointLSN(std::string strFile);
    void SetDetach(bool fDetachDB_) { fDetachDB = fDetachDB_; }
    bool GetDetach() { return fDetachDB; }

    void CloseDb(const std::st