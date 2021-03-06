/* This file is part of the KDE project
   Copyright 2015 Maciej Cencora

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

#include <cstdlib>

#include "texteditorhelpers.h"

#include "KTextEditor/View"

namespace KDevelop {

namespace {

// TODO: this is a hack, but Kate does not provide interface for this
int getLineHeight(const KTextEditor::View* view, int curLine)
{
  KTextEditor::Cursor c(curLine, 0);
  int currentHeight = view->cursorToCoordinate(c).y();
  c.setLine(curLine + 1);
  if (view->cursorToCoordinate(c).y() < 0) {
    c.setLine(curLine - 1);
  }
  return std::abs(view->cursorToCoordinate(c).y() - currentHeight);
}

}

QRect getItemBoundingRect(const KTextEditor::View* view, KTextEditor::Range itemRange)
{
  QPoint startPoint = view->mapToGlobal(view->cursorToCoordinate(itemRange.start()));
  QPoint endPoint = view->mapToGlobal(view->cursorToCoordinate(itemRange.end()));
  endPoint.ry() += getLineHeight(view, itemRange.start().line());
  return QRect(startPoint, endPoint);
}

}
