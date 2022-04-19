/*
* Copyright (C) 2021 ~ 2023 Deepin Technology Co., Ltd.
*
* Author:     caixiangrong <caixiangrong@uniontech.com>
*
* Maintainer: caixiangrong <caixiangrong@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "controllitemsmodel.h"
#include "wireddevice.h"

#include <QPainter>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QWidget>
#include <QApplication>

#include <DStyle>
#include <DSpinner>
#include <DStyleHelper>

DWIDGET_USE_NAMESPACE
using namespace dde::network;

struct ControllItemsAction
{
    DViewItemAction *iconAction;
    DViewItemAction *spinnerAction;
    DSpinner *loadingIndicator;
    DViewItemActionList rightList;
    ControllItems *conn;
    explicit ControllItemsAction(ControllItems *_conn)
        : iconAction(new DViewItemAction(Qt::AlignCenter | Qt::AlignRight, QSize(), QSize(), true))
        , spinnerAction(new DViewItemAction(Qt::AlignCenter | Qt::AlignRight, QSize(), QSize(), false))
        , loadingIndicator(nullptr)
        , conn(_conn)
    {
        iconAction->setData(QVariant::fromValue(conn));
        rightList.append(iconAction);
        spinnerAction->setVisible(false);
    }
    ~ControllItemsAction()
    {
        delete iconAction;
        delete spinnerAction;
        if (loadingIndicator)
            delete loadingIndicator;
    }
    void setLoading(bool isLoading, QWidget *parentView)
    {
        if (spinnerAction->isVisible() == isLoading)
            return;
        if (isLoading) {
            if (!loadingIndicator) {
                loadingIndicator = new DSpinner(parentView);
                loadingIndicator->setFixedSize(24, 24);
                spinnerAction->setWidget(loadingIndicator);
                loadingIndicator->connect(loadingIndicator, &QWidget::destroyed, loadingIndicator, [this]() { loadingIndicator = nullptr; });
            }
            loadingIndicator->setParent(parentView);
            loadingIndicator->start();
        } else if (loadingIndicator) {
            loadingIndicator->stop();
            loadingIndicator->setVisible(false);
        }
        spinnerAction->setVisible(isLoading);
        iconAction->setVisible(!isLoading);
        rightList[0] = isLoading ? spinnerAction : iconAction;
    }
    Q_DISABLE_COPY(ControllItemsAction)
};

ControllItemsModel::ControllItemsModel(QWidget *parent)
    : QAbstractItemModel(parent)
    , m_displayRole(id)
{
    setParentWidget(parent);
}

ControllItemsModel::~ControllItemsModel()
{
    for (auto it = m_data.begin(); it != m_data.end(); ++it) {
        delete (*it);
    }
}

// Basic functionality:
QModelIndex ControllItemsModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    if (row < 0 || row > m_data.size())
        return QModelIndex();
    if (row == m_data.size())
        return createIndex(row, column, nullptr);
    return createIndex(row, column, const_cast<ControllItems *>(m_data.at(row)->conn));
}
QModelIndex ControllItemsModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index)
    return QModelIndex();
}

int ControllItemsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_data.size();
}
int ControllItemsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant ControllItemsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int row = index.row();

    const ControllItems *item = m_data.at(row)->conn;
    switch (role) {
    case Qt::EditRole:
    case Qt::DisplayRole:
        switch (m_displayRole) {
        case id:
            return item->connection()->id();
        case ssid:
            return item->connection()->ssid();
        }
    case Qt::CheckStateRole:
        return item->connected() ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;
    case Dtk::RightActionListRole:
        return QVariant::fromValue(m_data.at(row)->rightList);
    default:
        return QVariant();
    }
}

Qt::ItemFlags ControllItemsModel::flags(const QModelIndex &index) const
{
    Q_UNUSED(index)
    return Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable;
}

void ControllItemsModel::setParentWidget(QWidget *parentWidget)
{
    m_parentWidget = parentWidget;
    if (m_parentWidget) {
        connect(m_parentWidget, &QWidget::destroyed, this, [this]() { setParentWidget(nullptr); });
    }
}

void ControllItemsModel::setDisplayRole(ControllItemsModel::DisplayRole role)
{
    m_displayRole = role;
}

void ControllItemsModel::updateDate(QList<dde::network::ControllItems *> conns)
{
    QList<ControllItemsAction *> newData;
    for (ControllItems *&conn : conns) {
        bool has = false;
        for (auto it = m_data.begin(); it != m_data.end(); ++it) {
            if ((*it)->conn == conn) {
                newData.append(*it);
                m_data.erase(it);
                has = true;
                break;
            }
        }
        if (has) {
            continue;
        } else {
            ControllItemsAction *item = new ControllItemsAction(conn);
            connect(item->iconAction, &DViewItemAction::triggered, this, &ControllItemsModel::onDetailTriggered);
            newData.append(item);
        }
    }
    for (auto it = m_data.begin(); it != m_data.end();) {
        delete (*it);
        it = m_data.erase(it);
    }
    m_data = newData;
    updateStatus();
    beginResetModel();
    endResetModel();
}

void ControllItemsModel::addConnection(QList<ControllItems *> conns)
{
    bool has = false;
    for (ControllItems *&conn : conns) {
        for (auto it = m_data.begin(); it != m_data.end(); ++it) {
            if ((*it)->conn == conn) {
                has = true;
                break;
            }
        }
        if (has)
            continue;

        ControllItemsAction *item = new ControllItemsAction(conn);
        connect(item->iconAction, &DViewItemAction::triggered, this, &ControllItemsModel::onDetailTriggered);
        m_data.append(item);
    }
    updateStatus();
    //    sortList();
    beginResetModel();
    endResetModel();
}

void ControllItemsModel::removeConnection(QList<ControllItems *> conns)
{
    for (ControllItems *&conn : conns) {
        for (auto it = m_data.begin(); it != m_data.end(); ++it) {
            if ((*it)->conn == conn) {
                delete (*it);
                m_data.erase(it);
                break;
            }
        }
    }
    beginResetModel();
    endResetModel();
}

void ControllItemsModel::onDetailTriggered()
{
    DViewItemAction *action = qobject_cast<DViewItemAction *>(sender());
    emit detailClick(action->data().value<ControllItems *>());
}

void ControllItemsModel::updateStatus()
{
    int row = 0;
    for (auto it = m_data.begin(); it != m_data.end(); ++it, ++row) {
        DStyleHelper helper(m_parentWidget ? m_parentWidget->style() : nullptr);
        (*it)->iconAction->setIcon(helper.standardIcon(DStyle::SP_ArrowEnter, nullptr));
        (*it)->setLoading((*it)->conn->status() == ConnectionStatus::Activating, m_parentWidget);
        emit dataChanged(index(row, 0), index(row, 0));
    }
}

void ControllItemsModel::sortList()
{
    std::sort(m_data.begin(), m_data.end(), [](ControllItemsAction *item1, ControllItemsAction *item2) {
        return item1->conn->connection()->path() < item2->conn->connection()->path();
    });
    beginResetModel();
    endResetModel();
}
