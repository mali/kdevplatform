/* This file is part of the KDE project
   Copyright (C) xxxx KFile Authors
   Copyright (C) 2002 Anders Lund <anders.lund@lund.tdcadsl.dk>
   Copyright (C) 2007 Mirko Stocker <me@misto.ch>
   Copyright (C) 2009 Dominik Haumann <dhaumann kde org>
   Copyright (C) 2012 Niko Sams <niko.sams@gmai.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KDEVPLATFORM_PLUGIN_BOOKMARKHANDLER_H
#define KDEVPLATFORM_PLUGIN_BOOKMARKHANDLER_H

#include <KBookmarkManager>
#include <KBookmarkMenu>

#include <QObject>

class FileManager;

class BookmarkHandler : public QObject, public KBookmarkOwner
{
Q_OBJECT

public:
    explicit BookmarkHandler( FileManager *parent, QMenu *kpopupmenu = 0 );
    ~BookmarkHandler() override;

    // KBookmarkOwner interface:
    QUrl currentUrl() const override;
    QString currentTitle() const override;

    QMenu *menu() const
    {
      return m_menu;
    }
    void openBookmark( const KBookmark &, Qt::MouseButtons, Qt::KeyboardModifiers ) override;

Q_SIGNALS:
    void openUrl( const QUrl& url );

private:
    FileManager *m_parent;
    QMenu *m_menu;
    KBookmarkMenu *m_bookmarkMenu;
};

#endif // KDEVPLATFORM_PLUGIN_BOOKMARKHANDLER_H

