/* This file is part of KDevelop
    Copyright 2013 Aleix Pol <aleixpol@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "vcsoverlayproxymodel.h"
#include <projectchangesmodel.h>
#include <interfaces/icore.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/iproject.h>
#include <interfaces/iruncontroller.h>
#include <interfaces/iplugin.h>
#include <vcs/interfaces/ibranchingversioncontrol.h>
#include <vcs/vcsjob.h>
#include <vcs/models/vcsfilechangesmodel.h>
#include <project/projectmodel.h>
#include <util/path.h>

#include <KLocalizedString>


using namespace KDevelop;

using SafeProjectPointer = QPointer<KDevelop::IProject>;
Q_DECLARE_METATYPE(SafeProjectPointer)

VcsOverlayProxyModel::VcsOverlayProxyModel(QObject* parent): QIdentityProxyModel(parent)
{
    connect(ICore::self()->projectController(), &IProjectController::projectOpened,
                                              this, &VcsOverlayProxyModel::addProject);
    connect(ICore::self()->projectController(), &IProjectController::projectClosing,
                                              this, &VcsOverlayProxyModel::removeProject);

    foreach (const auto project, ICore::self()->projectController()->projects()) {
        addProject(project);
    }
}

QVariant VcsOverlayProxyModel::data(const QModelIndex& proxyIndex, int role) const
{
    if(role == VcsStatusRole) {
        if (!proxyIndex.parent().isValid()) {
            IProject* p = qobject_cast<IProject*>(proxyIndex.data(ProjectModel::ProjectRole).value<QObject*>());
            return m_branchName.value(p);
        } else {
            ProjectChangesModel* model = ICore::self()->projectController()->changesModel();
            const QUrl url = proxyIndex.data(ProjectModel::UrlRole).toUrl();
            const auto idx = model->match(model->index(0, 0), ProjectChangesModel::UrlRole, url, 1, Qt::MatchExactly).value(0);
            return idx.sibling(idx.row(), 1).data(Qt::DisplayRole);
        }
    } else
        return QAbstractProxyModel::data(proxyIndex, role);
}

void VcsOverlayProxyModel::addProject(IProject* p)
{
    IPlugin* plugin = p->versionControlPlugin();
    if(!plugin)
        return;

    // TODO: Show revision in case we're in "detached" state
    IBranchingVersionControl* branchingExtension = plugin->extension<KDevelop::IBranchingVersionControl>();
    if(branchingExtension) {
        const QUrl url = p->path().toUrl();
        branchingExtension->registerRepositoryForCurrentBranchChanges(url);
        //can't use new signal/slot syntax here, IBranchingVersionControl is not a QObject
        connect(plugin, SIGNAL(repositoryBranchChanged(QUrl)), SLOT(repositoryBranchChanged(QUrl)));
        repositoryBranchChanged(url);
    }
}

void VcsOverlayProxyModel::repositoryBranchChanged(const QUrl& url)
{
    QList<IProject*> allProjects = ICore::self()->projectController()->projects();
    foreach(IProject* project, allProjects) {
        if( url.isParentOf(project->path().toUrl()) || url.matches(project->path().toUrl(), QUrl::StripTrailingSlash)) {
            IPlugin* v = project->versionControlPlugin();
            Q_ASSERT(v);
            IBranchingVersionControl* branching = v->extension<IBranchingVersionControl>();
            Q_ASSERT(branching);
            VcsJob* job = branching->currentBranch(url);
            connect(job, &VcsJob::resultsReady, this, &VcsOverlayProxyModel::branchNameReady);
            job->setProperty("project", QVariant::fromValue<SafeProjectPointer>(project));
            ICore::self()->runController()->registerJob(job);
        }
    }
}

void VcsOverlayProxyModel::branchNameReady(KDevelop::VcsJob* job)
{
    static const QString noBranchStr = i18nc("Version control: Currently not on a branch", "(no branch)");

    if(job->status()==VcsJob::JobSucceeded) {
        SafeProjectPointer p = job->property("project").value<SafeProjectPointer>();
        QModelIndex index = indexFromProject(p);
        if(index.isValid()) {
            IProject* project = p.data();
            const auto branchName =  job->fetchResults().toString();
            m_branchName[project] = branchName.isEmpty() ? noBranchStr : branchName;
            emit dataChanged(index, index);
        }
    }
}

void VcsOverlayProxyModel::removeProject(IProject* p)
{
    m_branchName.remove(p);
}

QModelIndex VcsOverlayProxyModel::indexFromProject(QObject* project)
{
    for(int i=0; i<rowCount(); i++) {
        QModelIndex idx = index(i,0);
        if(idx.data(ProjectModel::ProjectRole).value<QObject*>()==project) {
            return idx;
        }
    }
    return QModelIndex();
}
