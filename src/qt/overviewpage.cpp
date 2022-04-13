#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "walletmodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "poolbrowser.h"
#include "bitcoingui.h"
#include "clientmodel.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QMovie>
#include <QFrame>

#define DECORATION_SIZE 45
#define NUM_ITEMS 10

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(BitcoinUnits::BTC)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        //QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        //QRect decorationRect(mainRect.left()-8,mainRect.top()-12, DECORATION_SIZE, DECORATION_SIZE);
        //QRect decorationRect(0, 0, 0, 0);
        //int xspace = DECORATION_SIZE - 8;
        int xspace = 0;
        int ypad = 0;
        int halfheight = (mainRect.height() - 2*ypad)/2;

        QRect amountRect(mainRect.left() + xspace+455, mainRect.top(), mainRect.width() - xspace - 10, halfheight);
        QRect addressRect(mainRect.left() + xspace+140, mainRect.top(), mainRect.width() - xspace, halfheight);
        QRect dateRect(mainRect.left() + 20, mainRect.top(), mai