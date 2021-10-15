/*Copyright (C) 2009 Cleriot Simon
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA*/

#include "chatwindow.h"
#include "ui_chatwindow.h"

ChatWindow::ChatWindow(QWidget *parent)
    : QWidget(parent), ui(new Ui::ChatWindowClass)
{
    ui->setupUi(this);
    setFixedSize(750,600);
    ui->splitter->hide();

    connect(ui->buttonConnect, SIGNAL(clicked()), this, SLOT(connectToIrc()));
    connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->actionCloseTab, SIGNAL(triggered()), this, SLOT(closeTab()));
    connect(ui->lineEdit, SIGNAL(returnPressed()), this, SLOT(sendCommand()));
    connect(ui->disconnect, SIGNAL(clicked()), this, SLOT(disconnectFromServer()));
    connect(ui->tab, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)) );
    connect(ui->tab, SIGNAL(tabCloseRequested(int)), this, SLOT(tabClosing(int)) );
}

void ChatWindow::tabChanged(int index)
{
	if(index!=0 && joining == false)
		currentTab()->updateUsersList(ui->tab->tabText(index));
}

void ChatWindow::tabClosing(int index)
{
	currentTab()->leave(ui->tab->tabText(index));
}
/*void ChatWindow::tabRemoved(int index)
{
	currentTab()->leave(ui->tab->tabText(index));
}*/

void ChatWindow::disconnectFromServer() {

    QMapIterator<QString, ServerIrc*> i(servers);

    while(i.hasNext())
    {
        i.next();
        QMapIterator<QString, QTextEdit*> i2(i.value()->conversations);
        while(i2.hasNext())
        {
            i2.next();
            i.value()->sendData("QUIT "+i2.key() + " ");
        }
    }

    ui->splitter->hide();
    ui->hide3->show();

}

ServerIrc *ChatWindow::currentTab()
{
	QString tooltip=ui->tab->tabToolTip(ui->tab->currentIndex());
    return servers[tooltip];
	//return ui->tab->currentWidget()->findChild<Serveur *>();
}

void ChatWindow::closeTab()
{
	QString tooltip=ui->tab->tabToolTip(ui->tab->currentIndex());
	QString txt=ui->tab->tabText(ui->tab->currentIndex());

	if(txt==tooltip)
	{
        QMapIterator<QString, QTextEdit*> i(servers[tooltip]->conversations);

		int count=ui->tab->currentIndex()+1;

		while(i.hasNext())
		{
			i.next();
			ui->tab->removeTab(count);
		}

		currentTab()->abort();
		ui->tab->removeTab(ui->tab->currentIndex());
	}
	else
	{
        ui->tab->removeTab(ui->tab->currentIndex());
		currentTab()->conversations.re