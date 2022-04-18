
#include "poolbrowser.h"
#include "ui_poolbrowser.h"
#include "main.h"
#include "wallet.h"
#include "base58.h"
#include "clientmodel.h"
#include "bitcoinrpc.h"
#include <QDesktopServices>

#include <sstream>
#include <string>

using namespace json_spirit;

#define QSTRING_DOUBLE(var) QString::number(var, 'f', 10)

// Markets pages
const QString kBittrexPage = "https://www.bittrex.com/Market/Index?MarketName=BTC-HYC";
const QString kMintPalPage = "https://www.mintpal.com/market/HYC/BTC";
const QString kCryptsyPage = "https://www.cryptsy.com/markets/view/221";

// Bitcoin to USD
const QString kCurrencyUSDUrl    = "http://blockchain.info/tobtc?currency=USD&value=1";

// Bittrex API urls
const QString kBittrexSummaryUrl        = "http://bittrex.com/api/v1/public/getmarketsummaries";
const QString kBittrexOrdersUrl         = "http://bittrex.com/api/v1/public/getorderbook?market=BTC-HYC&type=both&depth=50";
const QString kBittrexHistoryUrl        = "http://bittrex.com/api/v1/public/getmarkethistory?market=BTC-HYC&count=100";

// MintPal API urls
const QString kMintPalSummaryUrl    = "https://api.mintpal.com/v2/market/stats/HYC/BTC";
const QString kMintPalOrdersUrl     = "https://api.mintpal.com/v2/market/orders/HYC/BTC/ALL";
const QString kMintPalHistoryUrl    = "https://api.mintpal.com/v2/market/trades/HYC/BTC/";

// Cryptsy API urls
const QString kCryptsySummaryUrl    = "http://pubapi.cryptsy.com/api.php?method=singlemarketdata&marketid=221";
const QString kCryptsyOrdersUrl     = "http://pubapi.cryptsy.com/api.php?method=singleorderdata&marketid=221";

QString bitcoinp = "";
double bitcoinToUSD;
double lastuG;
QString bitcoing;
QString dollarg;
int mode=1;
QString lastp = "";
QString askp = "";
QString bidp = "";
QString highp = "";
QString lowp = "";
QString volumebp = "";
QString volumesp = "";
QString bop = "";
QString sop = "";
QString lastp2 = "";
QString askp2 = "";
QString bidp2 = "";
QString highp2 = "";
QString lowp2 = "";
QString volumebp2 = "";
QString volumesp2 = "";
QString bop2 = "";
QString sop2 = "";

double lastBittrex = 0.0;