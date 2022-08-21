#include <boost/test/unit_test.hpp>

#include <boost/foreach.hpp>

#include "init.h"
#include "wallet.h"
#include "walletdb.h"

BOOST_AUTO_TEST_SUITE(accounting_tests)

static void
GetResults(CWalletDB& walletdb, std::map<int64, CAccountingEntry>& results)
{
    std::list<CAccountingEntry> aes;

    results.clear();
    BOOST_CHECK(walletdb.ReorderTransactions(pwalletMain) == DB_LOAD_OK);
    walletdb.ListAccountCreditDebit("", aes);
    BOOST_FOREACH(CAccountingEntry& ae, aes)
    {
        results[ae.nOrderPos] = ae;
    }
}

BOOST_AUTO_TEST_CASE(acc_orderupgrade)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);
    std::vector<CWalletTx*> vpwtx;
    CWalletTx wtx;
    CAccountingEntry ae;
    std::map<int64, CAccountingEntry> results;

    ae.strAccount = "";
    ae.nCreditDebit = 1;
    ae.nTime = 1333333333;
    ae.strOtherAccount = "b";
    ae.strComment = "";
    walletdb.WriteAccountingEntry(ae);

    wtx.mapValue["comment"] = "z";
    pwalletMain->AddToWallet(wtx);
    vpwtx.push_back(&pwalletMain->mapWallet[wtx.GetHash()]);
    vpwtx[0]->nTimeReceived = (unsigned int)1333333335;
    vpwtx[0]->nOrderPos = -1;

    ae.nTime = 1333333336;
    ae.strOtherAccount = "c";
    walletdb.WriteAccountingEntry(ae);

    GetR