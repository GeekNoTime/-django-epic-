/*
 * Qt4 bitcoin GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin Developers 2011-2012
 */
#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "editaddressdialog.h"
#include "optionsmodel.h"
#include "tutoStackDialog.h"
#include "tutoWriteDialog.h"
#include "transactiondescdialog.h"
#include "addresstablemodel.h"
#include "transactionview.h"
#include "overviewpage.h"
#include "statisticspage.h"
#include "blockbrowser.h"
#include "poolbrowser.h"
#include "chatwindow.h"
#include "bitcoinunits.h"
#include "guiconstants.h"
#include "askpassphrasedialog.h"
#include "notificator.h"
#include "guiutil.h"
#include "rpcconsole.h"
#include "wallet.h"

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QIcon>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QLocale>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QStackedWidget>
#include <QDateTime>
#include <QMovie>
#include <QFileDialog>
#include <QDesktopServices>
#include <QTimer>
#include <QDragEnterEvent>
#include <QUrl>
#include <QStyle>
#include <QFont>
#include <QFontDatabase>

#include <iostream>

extern CWallet* pwalletMain;
extern int64_t nLastCoinStakeSearchInterval;
extern unsigned int nTargetSpacing;
double GetPoSKernelPS();
int convertmode = 0;

BitcoinGUI::BitcoinGUI(QWidget *parent):
    QMainWindow(parent),
    clientModel(0),
    walletModel(0),
    encryptWalletAction(0),
    changePassphraseAction(0),
    unlockWalletAction(0),
    lockWalletAction(0),
    aboutQtAction(0),
    trayIcon(0),
    notificator(0),
    rpcConsole(0)
{
    setFixedSize(1000, 600);
    setWindowFlags(Qt::FramelessWindowHint);
    setWindowTitle(tr("HYC") + " " + tr("Wallet"));
    setObjectName("HYC-qt");
    setStyleSheet("#HYC-qt {background-color:#fbf9f6; font-family:'Open Sans';}");

#ifndef Q_OS_MAC
    qApp->setWindowIcon(QIcon(":icons/bitcoin"));
    setWindowIcon(QIcon(":icons/bitcoin"));
#else
    setUnifiedTitleAndToolBarOnMac(true);
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    // Include Fonts
    includeFonts();

    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    createActions();

    // Create the tray icon (or setup the dock icon)
    createTrayIcon();

    // Create additional content for pages
    createContent();

    // Create navigation tabs
    overviewPage = new OverviewPage();
    statisticsPage = new StatisticsPage(this);
    chatWindow = new ChatWindow(this);
	blockBrowser = new BlockBrowser(this);
	poolBrowser = new PoolBrowser(this);
    transactionsPage = new QWidget(this);
    QVBoxLayout *transactionVbox = new QVBoxLayout();
    transactionView = new TransactionView(this);
    transactionVbox->addWidget(transactionView);
    transactionVbox->setContentsMargins(20,0,20,20);
    transactionsPage->setLayout(transactionVbox);
    transactionsPage->setStyleSheet("background:rgb(255,249,247)");
    addressBookPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab);
    receiveCoinsPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPa