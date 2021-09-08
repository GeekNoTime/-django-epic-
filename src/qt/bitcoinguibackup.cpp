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
    receiveCoinsPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab);
    sendCoinsPage = new SendCoinsDialog(this);
    signVerifyMessageDialog = new SignVerifyMessageDialog(this);

    //Define Central Widget
    centralWidget = new QStackedWidget(this);
    centralWidget->addWidget(overviewPage);
    centralWidget->addWidget(statisticsPage);
    centralWidget->addWidget(chatWindow);
    centralWidget->addWidget(blockBrowser);
	centralWidget->addWidget(poolBrowser);
    centralWidget->addWidget(transactionsPage);
    centralWidget->addWidget(addressBookPage);
    centralWidget->addWidget(receiveCoinsPage);
    centralWidget->addWidget(sendCoinsPage);
    centralWidget->addWidget(settingsPage);
    centralWidget->setMaximumWidth(1000);
    centralWidget->setMaximumHeight(600);
    setCentralWidget(centralWidget);

    // Create status bar notification icons
    labelEncryptionIcon = new QLabel();
    labelStakingIcon = new QLabel();
    labelConnectionsIcon = new QLabel();
    labelBlocksIcon = new QLabel();
    actionConvertIcon = new QAction(QIcon(":/icons/changevalHYC"), tr(""), this);
    actionConvertIcon->setToolTip("Convert currency");

    // Get current staking status
    if (GetBoolArg("-staking", true))
    {
        QTimer *timerStakingIcon = new QTimer(labelStakingIcon);
        connect(timerStakingIcon, SIGNAL(timeout()), this, SLOT(updateStakingIcon()));
        timerStakingIcon->start(30 * 1000);
        updateStakingIcon();
    }

    // Progress bar and label for blocks download, disabled for current HYC wallet
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new QProgressBar();

    // Create toolbars
    createToolBars();

    // When clicking the current currency logo, the currency will be converted into HYC, BTC or USD
    connect(actionConvertIcon, SIGNAL(triggered()), this, SLOT(sConvert()));

    // Clicking on a transaction on the overview page simply sends you to transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    // RPC Console
    rpcConsole = new RPCConsole(this);
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));
    connect(openRPCConsoleAction2, SIGNAL(triggered()), rpcConsole, SLOT(show()));

    // Clicking on "Verify Message" in the address book sends you to the verify message tab
    connect(addressBookPage, SIGNAL(verifyMessage(QString)), this, SLOT(gotoVerifyMessageTab(QString)));

    // Clicking on "Sign Message" in the receive coins page sends you to the sign message tab
    connect(receiveCoinsPage, SIGNAL(signMessage(QString)), this, SLOT(gotoSignMessageTab(QString)));

    //Go to overview page
    gotoOverviewPage();
}

BitcoinGUI::~BitcoinGUI()
{
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
}

void BitcoinGUI::includeFonts()
{
    QStringList list;
    list << "OpenSans-Regular.ttf" << "OpenSans-Bold.ttf" << "OpenSans-ExtraBold.ttf";
    int fontID(-1);
    bool fontWarningShown(false);
    for (QStringList::const_iterator constIterator = list.constBegin(); constIterator != list.constEnd(); ++constIterator) {
        QFile res(":/fonts/" + *constIterator);
        if (res.open(QIODevice::ReadOnly) == false) {
            if (fontWarningShown == false) {
                QMessageBox::warning(0, "Application", (QString)"Impossible to open " + QChar(0x00AB) + *constIterator + QChar(0x00BB) + ".");
                fontWarningShown = true;
            }
        } else {
            fontID = QFontDatabase::addApplicationFontFromData(res.readAll());
            if (fontID == -1 && fontWarningShown == false) {
                QMessageBox::warning(0, "Application", (QString)"Impossible to open " + QChar(0x00AB) + *constIterator + QChar(0x00BB) + ".");
                fontWarningShown = true;
            }
        }
    }
}

void BitcoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    // Navigation bar actions
    overviewAction = new QAction("Dashboard", this);
    overviewAction->setToolTip(tr("Show general overview"));
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction("Send", this);
    sendCoinsAction->setToolTip(tr("Send coins to a HYC address"));
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    sendCoinsAction->setObjectName("send");
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QAction("Receive", this);
    receiveCoinsAction->setToolTip(tr("Receive addresses list"));
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    receiveCoinsAction->setCheckable(true);
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction("Transactions", this);
    historyAction->setToolTip(tr("Browse transaction history"));
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

    addressBookAction = new QAction("Address Book", this);
    addressBookAction->setToolTip(tr("Edit the list of stored addresses and labels"));
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(addressBookAction);

    poolAction = new QAction("Market Data", this);
    poolAction->setToolTip(tr("Show market data"));
    poolAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
    poolAction->setCheckable(true);
    tabGroup->addAction(poolAction);

    chatAction = new QAction("Social", this);
    chatAction->setToolTip(tr("View social media info"));
    chatAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_7));
    chatAction->setCheckable(true);
    tabGroup->addAction(chatAction);

    blockAction = new QAction("Block Explorer", this);
    blockAction->setToolTip(tr("Explore the HYC blockchain"));
    blockAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_8));
    blockAction->setCheckable(true);
    tabGroup->addAction(blockAction);

    actionmenuAction = new QAction("Actions", this);
    actionmenuAction->setToolTip(tr("Multiple wallet actions"));
    actionmenuAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_9));
    actionmenuAction->setCheckable(true);
    tabGroup->addAction(actionmenuAction);

    settingsAction = new QAction("Actions", this);
    settingsAction->setToolTip(tr("Multiple wallet actions"));
    settingsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_9));
    settingsAction->setCheckable(true);
    tabGroup->addAction(settingsAction);

    statisticsAction = new QAction("Statistics", this);
    statisticsAction->setToolTip(tr("View HYC statistics"));
    statisticsAction->setCheckable(true);
    tabGroup->addAction(statisticsAction);

    optionsAction = new QAction("Settings", this);
    optionsAction->setToolTip(tr("Modify settings for HYC wallet"));
    tabGroup->addAction(optionsAction);

    // Connect actions to functions
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(blockAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(blockAction, SIGNAL(triggered()), this, SLOT(gotoBlockBrowser()));
    connect(poolAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
	connect(poolAction, SIGNAL(triggered()), this, SLOT(gotoPoolBrowser()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(statisticsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(statisticsAction, SIGNAL(triggered()), this, SLOT(gotoStatisticsPage()));
    connect(chatAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(chatAction, SIGNAL(triggered()), this, SLOT(gotoChatPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(gotoAddressBookPage()));
    connect(settingsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(settingsAction, SIGNAL(triggered()), this, SLOT(gotoSettingsPage()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));

    // Upper right toolbar actions
    toggleHideAction = new QAction(QIcon(":/icons/mini"), tr("&Show / Hide"), this);

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setToolTip(tr("Quit application"));

    quitAction2 = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction2->setToolTip(tr("Quit application"));
    quitAction2->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));

    // ACTIONS tab actions
    tutoStackAction = new QAction(QIcon(":/images/howtostake"),tr(""), this);
    aboutAction = new QAction(QIcon(":/images/aboutHYC"), tr(""), this);
    aboutAction->setToolTip(tr("Show information about HYC"));
    aboutQtAction = new QAction(QIcon(":/trolltech/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
    aboutQtAction->setToolTip(tr("Show information about Qt"));
    encryptWalletAction = new QAction(QIcon(":/images/encryptwallet"), tr(""), this);
    encryptWalletAction->setToolTip(tr("Encrypt or decrypt wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(":/images/backupwallet"), tr(""), this);
    backupWalletAction->setToolTip(tr("Backup wallet to another location"));
    changePassphraseAction = new QAction(QIcon(":/images/newpassphrase"), tr(""), this);
    changePassphraseAction->setToolTip(tr("Change the passphrase used for wallet encryption"));
    unlockWalletAction = new QAction(QIcon(":/images/unlockwallet"), tr(""), this);
    unlockWalletAction->setToolTip(tr("Unlock wallet"));
    lockWalletAction = new QAction(QIcon(":/images/lockwallet"), tr(""), this);
    lockWalletAction->setToolTip(tr("Lock wallet"));
    signMessageAction = new QAction(QIcon(":/images/signmessage"), tr(""), this);
    verifyMessageAction = new QAction(QIcon(":/images/verifymessage"), tr(""), this);
    exportAction = new QAction(QIcon(":/icons/export"), tr("&Export..."), this);
    exportAction->setToolTip(tr("Export the data in the current tab to a file"));
    openRPCConsoleAction = new QAction(QIcon(":/images/debugwindow"), tr(""), this);
    openRPCConsoleAction->setToolTip(tr("Open debugging and diagnostic console"));
    signMessageAction2 = new QAction(QIcon(":/icons/edit"), tr("Sign &message..."), this);
    verifyMessageAction2 = new QAction(QIcon(":/icons/transaction_0"), tr("&Verify message..."), this);
    openRPCConsoleAction2 = new QAction(QIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction2->setToolTip(tr("Open debugging and diagnostic console"));

#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    file->addAction(backupWalletAction);
    file->addAction(exportAction);
    file->addAction(signMessageAction);
    file->addAction(verifyMessageAction);
    file->addSeparator();
    file->addAction(quitAction2);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    settings->addAction(encryptWalletAction);
    settings->addAction(changePassphraseAction);
    settings->addAction(unlockWalletAction);
    settings->addAction(lockWalletAction);
    settings->addSeparator();
    settings->addAction(optionsAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
#endif

    // Connect actions to slots
    connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));
    connect(quitAction2, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(tutoStackAction, SIGNAL(triggered()), this, SLOT(tutoStackClicked()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(encryptWalletAction, SIGNAL(triggered(bool)), this, SLOT(encryptWallet(bool)));
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(backupWallet()));
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(changePassphrase()));
    connect(unlockWalletAction, SIGNAL(triggered()), this, SLOT(unlockWallet()));
    connect(lockWalletAction, SIGNAL(triggered()), this, SLOT(lockWallet()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
    connect(signMessageAction2, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction2, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
}

void BitcoinGUI::createContent()
{
    //Create Actions Page Content
    settingsPage = new QWidget(this);
    settingsPage->setFixedSize(