/*
 * This file is part of KDevelop
 * Copyright 2010 Aleix Pol Gonzalez <aleixpol@kde.org>
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


#ifndef KDEVPLATFORM_PLUGIN_GITJOB_H
#define KDEVPLATFORM_PLUGIN_GITJOB_H

#include <vcs/dvcs/dvcsjob.h>

class GitJob : public KDevelop::DVcsJob
{
    Q_OBJECT
    public:
        explicit GitJob(const QDir& workingDir, KDevelop::IPlugin* parent = 0, KDevelop::OutputJob::OutputJobVerbosity verbosity = KDevelop::OutputJob::Verbose);
        
};

#endif // KDEVPLATFORM_PLUGIN_GITJOB_H
