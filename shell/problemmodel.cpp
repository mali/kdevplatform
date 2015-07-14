/*
 * KDevelop Problem Reporter
 *
 * Copyright 2007 Hamish Rodda <rodda@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "problemmodel.h"
#include <QThread>
#include <KLocalizedString>
#include <interfaces/icore.h>
#include <interfaces/idocument.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/ilanguagecontroller.h>
#include <interfaces/icompletionsettings.h>
#include <interfaces/iassistant.h>
#include <QIcon>
#include <shell/problem.h>

#include <shell/problemstore.h>
#include <shell/problemstorenode.h>
#include <shell/filteredproblemstore.h>
#include <shell/problemconstants.h>
#include <shell/watcheddocumentset.h>

namespace {
QIcon iconForSeverity(KDevelop::IProblem::Severity severity)
{
    switch (severity) {
    case KDevelop::IProblem::Hint:
        return QIcon::fromTheme(QStringLiteral("dialog-information"));
    case KDevelop::IProblem::Warning:
        return QIcon::fromTheme(QStringLiteral("dialog-warning"));
    case KDevelop::IProblem::Error:
        return QIcon::fromTheme(QStringLiteral("dialog-error"));
    }
    return QIcon();
}

}


struct ProblemModelPrivate
{
    ProblemModelPrivate(KDevelop::ProblemStore *store)
        : m_problems(store)
        , m_features(KDevelop::ProblemModel::NoFeatures)
    {
    }

    // Class that actually stores the problems
    QScopedPointer<KDevelop::ProblemStore> m_problems;

    // Supported features
    KDevelop::ProblemModel::Features m_features;
};

namespace KDevelop
{

ProblemModel::ProblemModel(QObject * parent, ProblemStore *store)
  : QAbstractItemModel(parent)
  , d(new ProblemModelPrivate(store))
{
    if (!d->m_problems) {
        d->m_problems.reset(new FilteredProblemStore());
        d->m_features = ScopeFilter | SeverityFilter | Grouping;
    }

    setScope(CurrentDocument);

    connect(ICore::self()->documentController(), &IDocumentController::documentActivated, this, &ProblemModel::setCurrentDocument);
    // CompletionSettings include a list of todo markers we care for, so need to update
    connect(ICore::self()->languageController()->completionSettings(), &ICompletionSettings::settingsChanged, this, &ProblemModel::forceFullUpdate);

    if (ICore::self()->documentController()->activeDocument()) {
        setCurrentDocument(ICore::self()->documentController()->activeDocument());
    }

    connect(d->m_problems.data(), &ProblemStore::beginRebuild, this, &ProblemModel::onBeginRebuild);
    connect(d->m_problems.data(), &ProblemStore::endRebuild, this, &ProblemModel::onEndRebuild);
}

ProblemModel::~ ProblemModel()
{
}

int ProblemModel::rowCount(const QModelIndex & parent) const
{
    if(!parent.isValid())
        return d->m_problems->count();
    else
        return d->m_problems->count(reinterpret_cast<ProblemStoreNode*>(parent.internalPointer()));
}

static QString displayUrl(const QUrl &url, const QUrl &base)
{
    QUrl location = url;
    if ( location.isParentOf(base) ) {
        return location.toDisplayString(QUrl::PreferLocalFile).mid(base.toDisplayString(QUrl::PreferLocalFile).length());
    } else {
        return location.toDisplayString(QUrl::PreferLocalFile);
    }
}

QVariant ProblemModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid())
        return QVariant();

    QUrl baseDirectory = d->m_problems->currentDocument().toUrl().adjusted(QUrl::RemoveFilename);
    IProblem::Ptr p = problemForIndex(index);
    if(p.constData() == NULL)
    {
        if((role == Qt::DisplayRole) && (index.column() == Error))
        {
            ProblemStoreNode *node = reinterpret_cast<ProblemStoreNode*>(index.internalPointer());
            if(node == NULL)
                return QVariant();
            else
                return node->label();
        }

        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case Source:
            return p->sourceString();
        case Error:
            return p->description();
        case File:
            return displayUrl(p->finalLocation().document.toUrl(), baseDirectory);
        case Line:
            if(p->finalLocation().isValid())
                return QString::number(p->finalLocation().start().line() + 1);
            break;
        case Column:
            if(p->finalLocation().isValid())
                return QString::number(p->finalLocation().start().column() + 1);
            break;
        }
        break;

    case Qt::DecorationRole:
        if (index.column() == 0) {
            return iconForSeverity(p->severity());
        }
        break;
    case Qt::ToolTipRole:
        return p->explanation();

    default:
        break;
    }

    return QVariant();
}

QModelIndex ProblemModel::parent(const QModelIndex& index) const
{
    ProblemStoreNode *node = reinterpret_cast<ProblemStoreNode*>(index.internalPointer());
    if(node == NULL)
        return QModelIndex();

    ProblemStoreNode *parent = node->parent();
    if(parent == NULL)
        return QModelIndex();

    if(parent->isRoot())
        return QModelIndex();

    int idx = parent->index();

    return createIndex(idx, 0, parent);

}

QModelIndex ProblemModel::index(int row, int column, const QModelIndex & parent) const
{
    if (row < 0 || column < 0 || column >= LastColumn)
        return QModelIndex();


    ProblemStoreNode *parentNode = reinterpret_cast<ProblemStoreNode*>(parent.internalPointer());
    const ProblemStoreNode *node = d->m_problems->findNode(row, parentNode);
    return createIndex(row, column, (void*)node);
}

int ProblemModel::columnCount(const QModelIndex & parent) const
{
    Q_UNUSED(parent)
    return LastColumn;
}

IProblem::Ptr ProblemModel::problemForIndex(const QModelIndex & index) const
{
    ProblemStoreNode *node = reinterpret_cast<ProblemStoreNode*>(index.internalPointer());
    if(node == NULL)
        return IProblem::Ptr(NULL);
    else
        return node->problem();
}

ProblemModel::Features ProblemModel::features() const
{
    return d->m_features;
}

void ProblemModel::setFeatures(Features features)
{
    d->m_features = features;
}

void ProblemModel::addProblem(const IProblem::Ptr &problem)
{
    int c = d->m_problems->count();
    beginInsertRows(QModelIndex(), c, c + 1);
    d->m_problems->addProblem(problem);
    endInsertRows();
}

void ProblemModel::clearProblems()
{
    beginResetModel();
    d->m_problems->clear();
    endResetModel();
}


QVariant ProblemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(orientation);

    if (role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
        case Source:
            return i18nc("@title:column source of problem", "Source");
        case Error:
            return i18nc("@title:column problem description", "Problem");
        case File:
            return i18nc("@title:column file where problem was found", "File");
        case Line:
            return i18nc("@title:column line number with problem", "Line");
        case Column:
            return i18nc("@title:column column number with problem", "Column");
    }

    return QVariant();
}

void ProblemModel::setCurrentDocument(IDocument* document)
{
    Q_ASSERT(thread() == QThread::currentThread());

    QUrl currentDocument = document->url();
    // Will trigger signals beginRebuild(), endRebuild() if problems change and are rebuilt
    d->m_problems->setCurrentDocument(IndexedString(currentDocument));
}

void ProblemModel::onBeginRebuild()
{
    beginResetModel();
}

void ProblemModel::onEndRebuild()
{
    endResetModel();
}

void ProblemModel::setScope(int scope)
{
    Q_ASSERT(thread() == QThread::currentThread());

    // Will trigger signals beginRebuild(), endRebuild() if problems change and are rebuilt
    d->m_problems->setScope(scope);
}

void ProblemModel::setSeverity(int severity)
{
    Q_ASSERT(thread() == QThread::currentThread());

    // Will trigger signals beginRebuild(), endRebuild() if problems change and are rebuilt
    d->m_problems->setSeverity(severity);
}

void ProblemModel::setGrouping(int grouping)
{
    // Will trigger signals beginRebuild(), endRebuild() if problems change and are rebuilt
    d->m_problems->setGrouping(grouping);
}

ProblemStore* ProblemModel::store() const
{
    return d->m_problems.data();
}

}


