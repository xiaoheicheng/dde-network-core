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
#include "wirelessdevicemodel.h"
#include "wirelessdevice.h"

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

struct ItemAction
{
    DViewItemAction *secureAction;
    DViewItemAction *iconAction;
    DViewItemAction *spinnerAction;
    DSpinner *loadingIndicator;
    DViewItemActionList leftList;
    DViewItemActionList rightList;
    AccessPoints *ap;
    explicit ItemAction(AccessPoints *_ap)
        : secureAction(new DViewItemAction(Qt::AlignCenter, QSize(), QSize(), false))
        , iconAction(new DViewItemAction(Qt::AlignCenter | Qt::AlignRight, QSize(), QSize(), true))
        , spinnerAction(new DViewItemAction(Qt::AlignCenter | Qt::AlignRight, QSize(), QSize(), false))
        , loadingIndicator(nullptr)
        , ap(_ap)
    {
        iconAction->setData(QVariant::fromValue(ap));
        leftList.append(secureAction);
        rightList.append(iconAction);
        spinnerAction->setVisible(false);
    }
    ~ItemAction()
    {
        delete secureAction;
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
            }
            loadingIndicator->start();
        } else if (loadingIndicator) {
            loadingIndicator->stop();
            loadingIndicator->setVisible(false);
        }
        spinnerAction->setVisible(isLoading);
        iconAction->setVisible(!isLoading);
        rightList[0] = isLoading ? spinnerAction : iconAction;
    }
    Q_DISABLE_COPY(ItemAction)
};

WirelessDeviceModel::WirelessDeviceModel(const WirelessDevice *dev, QWidget *parent)
    : QAbstractItemModel(parent)
    , m_device(dev)
    , m_parent(parent)
    , m_hiddenItem(new ItemAction(nullptr))
{
    m_hiddenItem->iconAction->setIcon(m_parent->style()->standardIcon(QStyle::SP_ArrowRight));
    connect(m_hiddenItem->iconAction, &DViewItemAction::triggered, this, &WirelessDeviceModel::onDetailTriggered);

    addAccessPoints(m_device->accessPointItems());
    connect(m_device, &WirelessDevice::networkAdded, this, &WirelessDeviceModel::addAccessPoints);
    connect(m_device, &WirelessDevice::networkRemoved, this, &WirelessDeviceModel::removeAccessPoints);
    connect(m_device, &WirelessDevice::accessPointInfoChanged, this, &WirelessDeviceModel::updateApStatus);
    connect(m_device, &WirelessDevice::connectionSuccess, this, &WirelessDeviceModel::updateApStatus);
    connect(m_device, &WirelessDevice::connectionChanged, this, &WirelessDeviceModel::updateApStatus);
    connect(m_device, &WirelessDevice::deviceStatusChanged, this, &WirelessDeviceModel::updateApStatus);
    connect(m_device, &WirelessDevice::activeConnectionChanged, this, &WirelessDeviceModel::updateApStatus);
}

WirelessDeviceModel::~WirelessDeviceModel()
{
    for (auto it = m_data.begin(); it != m_data.end(); ++it) {
        delete (*it);
    }
    delete m_hiddenItem;
}

QModelIndex WirelessDeviceModel::index(const AccessPoints *ap)
{
    if (!ap)
        return index(m_data.size(), 0);
    int row = 0;
    for (auto it = m_data.begin(); it != m_data.end(); ++it, ++row) {
        if ((*it)->ap == ap) {
            return index(row, 0);
        }
    }
    return QModelIndex();
}
// Basic functionality:
QModelIndex WirelessDeviceModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    if (row < 0 || row > m_data.size())
        return QModelIndex();
    if (row == m_data.size())
        return createIndex(row, column, nullptr);
    return createIndex(row, column, const_cast<AccessPoints *>(m_data.at(row)->ap));
}
QModelIndex WirelessDeviceModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index)
    return QModelIndex();
}

int WirelessDeviceModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_data.size() + 1;
}
int WirelessDeviceModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant WirelessDeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int row = index.row();

    if (row >= m_data.size()) {
        switch (role) {
        case Qt::DisplayRole:
            return tr("Connect to hidden network");
        case Dtk::LeftActionListRole:
            return QVariant::fromValue(m_hiddenItem->leftList);
        case Dtk::RightActionListRole:
            return QVariant::fromValue(m_hiddenItem->rightList);
        }
        return QVariant();
    }
    const AccessPoints *ap = m_data.at(row)->ap;
    switch (role) {
    case Qt::EditRole:
    case Qt::DisplayRole:
        return ap->ssid();
    case Qt::DecorationRole: {
        int strength = ap->strength();
        QString icon = ap->type() == AccessPoints::WlanType::wlan6 ? QLatin1String("dcc_wireless6-") : QLatin1String("dcc_wireless-");
        if (strength > 65)
            icon += "8";
        else if (strength > 55)
            icon += "6";
        else if (strength > 30)
            icon += "4";
        else if (strength > 5)
            icon += "2";
        else
            icon += "0";

        return QIcon::fromTheme(icon);
    }
    case Qt::CheckStateRole:
        return ap->connected() ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;
    case Dtk::LeftActionListRole:
        return QVariant::fromValue(m_data.at(row)->leftList);
    case Dtk::RightActionListRole:
        return QVariant::fromValue(m_data.at(row)->rightList);
    default:
        return QVariant();
    }
}

Qt::ItemFlags WirelessDeviceModel::flags(const QModelIndex &index) const
{
    Q_UNUSED(index)
    return Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable;
}

void WirelessDeviceModel::addAccessPoints(QList<AccessPoints *> aps)
{
    for (AccessPoints *ap : aps) {
        if (std::any_of(m_data.begin(), m_data.end(), [ap](const ItemAction *item) { return item->ap == ap; }))
            continue;
        connect(ap, &AccessPoints::strengthChanged, this, &WirelessDeviceModel::sortAPList, Qt::UniqueConnection);

        ItemAction *item = new ItemAction(ap);
        connect(item->iconAction, &DViewItemAction::triggered, this, &WirelessDeviceModel::onDetailTriggered);
        m_data.append(item);
    }
    // 排序
    sortAPList();
    updateApStatus();
    beginResetModel();
    endResetModel();
}
void WirelessDeviceModel::removeAccessPoints(QList<dde::network::AccessPoints *> aps)
{
    for (AccessPoints *ap : aps) {
        for (auto it = m_data.begin(); it != m_data.end(); ++it) {
            if ((*it)->ap == ap) {
                delete (*it);
                m_data.erase(it);
                break;
            }
        }
    }
    beginResetModel();
    endResetModel();
}

void WirelessDeviceModel::onDetailTriggered()
{
    DViewItemAction *action = qobject_cast<DViewItemAction *>(sender());
    emit detailClick(action->data().value<AccessPoints *>());
}

void WirelessDeviceModel::updateApStatus()
{
    sortAPList();
    int row = 0;
    for (auto it = m_data.begin(); it != m_data.end(); ++it, ++row) {
        DStyleHelper helper(m_parent->style());
        (*it)->iconAction->setIcon(helper.standardIcon(DStyle::SP_ArrowEnter, nullptr));
        if ((*it)->ap->secured()) {
            (*it)->secureAction->setIcon(helper.standardIcon(DStyle::SP_LockElement, nullptr, nullptr));
        }
        (*it)->setLoading((*it)->ap->status() == ConnectionStatus::Activating, m_parent);
        emit dataChanged(index(row, 0), index(row, 0));
    }
}

void WirelessDeviceModel::sortAPList()
{
    std::sort(m_data.begin(), m_data.end(), [](ItemAction *item1, ItemAction *item2) {
        AccessPoints *ap1 = item1->ap;
        AccessPoints *ap2 = item2->ap;
        bool ap1Connected = ap1->status() == ConnectionStatus::Activating || ap1->status() == ConnectionStatus::Activated;
        bool ap2Connected = ap2->status() == ConnectionStatus::Activating || ap2->status() == ConnectionStatus::Activated;
        if (ap1Connected != ap2Connected) {
            return ap1Connected;
        }
        if (ap1->strength() == ap2->strength())
            return ap1->ssid() <= ap2->ssid();
        return ap1->strength() > ap2->strength();
    });
    beginResetModel();
    endResetModel();
}
