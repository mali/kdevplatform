/**************************************************************************
*   Copyright 2009 Andreas Pakulat <apaku@gmx.de>                         *
*   Copyright 2010 Niko Sams <niko.sams@gmail.com>                        *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU Library General Public License as       *
*   published by the Free Software Foundation; either version 2 of the    *
*   License, or (at your option) any later version.                       *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU Library General Public     *
*   License along with this program; if not, write to the                 *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
***************************************************************************/

#include "loadedpluginsdialog.h"

#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include <KAboutData>
#include <KAboutApplicationDialog>
#include <KIconLoader>
#include <KLocalizedString>
#include <KTitleWidget>
#include <KWidgetItemDelegate>

#include "core.h"
#include "plugincontroller.h"

#define MARGIN 5

namespace {

KPluginMetaData pluginInfo(KDevelop::IPlugin* plugin)
{
    return KDevelop::Core::self()->pluginControllerInternal()->pluginInfo(plugin);
};

QString displayName(KDevelop::IPlugin* plugin)
{
    const auto name = pluginInfo(plugin).name();
    return !name.isEmpty() ? name : plugin->componentName();
}

bool sortPlugins(KDevelop::IPlugin* l, KDevelop::IPlugin* r)
{
    return displayName(l) < displayName(r);
}

}

class PluginsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum ExtraRoles {
        DescriptionRole = Qt::UserRole+1
    };
    PluginsModel(QObject* parent = 0)
        : QAbstractListModel(parent)
    {
        m_plugins = KDevelop::Core::self()->pluginControllerInternal()->loadedPlugins();
        std::sort(m_plugins.begin(), m_plugins.end(), sortPlugins);
    }

    KDevelop::IPlugin *pluginForIndex(const QModelIndex& index) const
    {
        if (!index.isValid()) return 0;
        if (index.parent().isValid()) return 0;
        if (index.column() != 0) return 0;
        if (index.row() >= m_plugins.count()) return 0;
        return m_plugins[index.row()];
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        KDevelop::IPlugin* plugin = pluginForIndex(index);
        if (!plugin)
            return QVariant();

        switch (role) {
        case Qt::DisplayRole:
            return displayName(plugin);
        case DescriptionRole:
            return pluginInfo(plugin).description();
        case Qt::DecorationRole:
            return pluginInfo(plugin).iconName();
        default:
            return QVariant();
        };
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (!parent.isValid()) {
            return m_plugins.count();
        }
        return 0;
    }

private:
    QList<KDevelop::IPlugin*> m_plugins;
};

class LoadedPluginsDelegate : public KWidgetItemDelegate
{
    Q_OBJECT

public:

    LoadedPluginsDelegate(QAbstractItemView *itemView, QObject *parent = 0)
        : KWidgetItemDelegate(itemView, parent)
        , pushButton(new QPushButton)
    {
        pushButton->setIcon(QIcon::fromTheme(QStringLiteral("dialog-information"))); // only for getting size matters
    }

    ~LoadedPluginsDelegate() override
    {
        delete pushButton;
    }

    QList<QWidget *> createItemWidgets(const QModelIndex &/*index*/) const override
    {
        return QList<QWidget *>();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        int i = 5;
        int j = 1;

        QFont font = titleFont(option.font);
        QFontMetrics fmTitle(font);

        return QSize(qMax(fmTitle.width(index.model()->data(index, Qt::DisplayRole).toString()),
                        option.fontMetrics.width(index.model()->data(index, PluginsModel::DescriptionRole).toString())) +
                        KIconLoader::SizeMedium + MARGIN * i + pushButton->sizeHint().width() * j,
                    qMax(KIconLoader::SizeMedium + MARGIN * 2, fmTitle.height() + option.fontMetrics.height() + MARGIN * 2));

    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        if (!index.isValid()) {
            return;
        }

        painter->save();

        QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0);

        int iconSize = option.rect.height() - MARGIN * 2;
        QIcon icon = QIcon::fromTheme(index.model()->data(index, Qt::DecorationRole).toString());
        icon.paint(painter, QRect(dependantLayoutValue(MARGIN + option.rect.left(), iconSize, option.rect.width()), MARGIN + option.rect.top(), iconSize, iconSize));
        QRect contentsRect(dependantLayoutValue(MARGIN * 2 + iconSize + option.rect.left(), option.rect.width() - MARGIN * 3 - iconSize, option.rect.width()), MARGIN + option.rect.top(), option.rect.width() - MARGIN * 3 - iconSize, option.rect.height() - MARGIN * 2);

        int lessHorizontalSpace = MARGIN * 2 + pushButton->sizeHint().width();

        contentsRect.setWidth(contentsRect.width() - lessHorizontalSpace);

        if (option.state & QStyle::State_Selected) {
            painter->setPen(option.palette.highlightedText().color());
        }

        if (itemView()->layoutDirection() == Qt::RightToLeft) {
            contentsRect.translate(lessHorizontalSpace, 0);
        }

        painter->save();

        painter->save();
        QFont font = titleFont(option.font);
        QFontMetrics fmTitle(font);
        painter->setFont(font);
        painter->drawText(contentsRect, Qt::AlignLeft | Qt::AlignTop, fmTitle.elidedText(index.model()->data(index, Qt::DisplayRole).toString(), Qt::ElideRight, contentsRect.width()));
        painter->restore();

        painter->drawText(contentsRect, Qt::AlignLeft | Qt::AlignBottom, option.fontMetrics.elidedText(index.model()->data(index, PluginsModel::DescriptionRole).toString(), Qt::ElideRight, contentsRect.width()));

        painter->restore();
        painter->restore();
    }

    QList<QWidget*> createItemWidgets() const
    {
        QPushButton *button = new QPushButton();
        button->setIcon(QIcon::fromTheme(QStringLiteral("dialog-information")));
        setBlockedEventTypes(button, QList<QEvent::Type>() << QEvent::MouseButtonPress
                             << QEvent::MouseButtonRelease << QEvent::MouseButtonDblClick);

        connect(button, &QPushButton::clicked, this, &LoadedPluginsDelegate::info);
        return QList<QWidget*>()
            << button;
    }

    void updateItemWidgets(const QList<QWidget*> widgets,
                           const QStyleOptionViewItem &option,
                           const QPersistentModelIndex &index) const override
    {
        Q_UNUSED(index);
        if ( widgets.isEmpty() ) {
            qDebug() << "Fixme: missing button?";
            return;
        }
        QPushButton *aboutPushButton = static_cast<QPushButton*>(widgets[0]);
        QSize aboutPushButtonSizeHint = aboutPushButton->sizeHint();
        aboutPushButton->resize(aboutPushButtonSizeHint);
        aboutPushButton->move(dependantLayoutValue(option.rect.width() - MARGIN - aboutPushButtonSizeHint.width(), aboutPushButtonSizeHint.width(), option.rect.width()), option.rect.height() / 2 - aboutPushButtonSizeHint.height() / 2);
    }

    int dependantLayoutValue(int value, int width, int totalWidth) const
    {
        if (itemView()->layoutDirection() == Qt::LeftToRight) {
            return value;
        }
        return totalWidth - width - value;
    }

    QFont titleFont(const QFont &baseFont) const
    {
        QFont retFont(baseFont);
        retFont.setBold(true);
        return retFont;
    }

private Q_SLOTS:
    void info()
    {
        PluginsModel *m = static_cast<PluginsModel*>(itemView()->model());
        KDevelop::IPlugin *p = m->pluginForIndex(focusedIndex());
        if (p) {
//             TODO KF5: Port
//             const K4AboutData *aboutData = p->componentData().aboutData();
//             if (!aboutData->programName().isEmpty()) { // Be sure the about data is not completely empty
//                 KAboutApplicationDialog aboutPlugin(aboutData, itemView());
//                 aboutPlugin.exec();
//                 return;
//             }
        }
    }
private:
    QPushButton *pushButton;
};

class PluginsView : public QListView
{
    Q_OBJECT
public:
    PluginsView(QWidget* parent = 0)
        :QListView(parent)
    {
        setModel(new PluginsModel());
        setItemDelegate(new LoadedPluginsDelegate(this));
        setVerticalScrollMode(QListView::ScrollPerPixel);
    }

    ~PluginsView() override
    {
        // explicitly delete the delegate here since otherwise
        // we get spammed by warnings that the QPushButton we return
        // in createItemWidgets is deleted before the delegate
        // *sigh* - even dfaure says KWidgetItemDelegate is a crude hack
        delete itemDelegate();
    }

    QSize sizeHint() const override
    {
        QSize ret = QListView::sizeHint();
        ret.setWidth(qMax(ret.width(), sizeHintForColumn(0) + 30));
        return ret;
    }
};

LoadedPluginsDialog::LoadedPluginsDialog( QWidget* parent )
    : QDialog( parent )
{
    setWindowTitle(i18n("Loaded Plugins"));

    QVBoxLayout* vbox = new QVBoxLayout(this);

    KTitleWidget* title = new KTitleWidget(this);
    title->setPixmap(QIcon::fromTheme(KAboutData::applicationData().programIconName()),
                     KTitleWidget::ImageLeft);
    title->setText(i18n("<html><font size=\"4\">Plugins loaded for <b>%1</b></font></html>",
                        KAboutData::applicationData().displayName()));
    vbox->addWidget(title);
    vbox->addWidget(new PluginsView());

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &LoadedPluginsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &LoadedPluginsDialog::reject);
    buttonBox->button(QDialogButtonBox::Close)->setDefault(true);
    vbox->addWidget(buttonBox);
}

#include "moc_loadedpluginsdialog.cpp"
#include "loadedpluginsdialog.moc"
