/*
   Copyright 2009 David Nolden <david.nolden.kdevelop@art-master.de>

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

#include "problemnavigationcontext.h"

#include <QHBoxLayout>
#include <QMenu>

#include <KLocalizedString>

#include <language/duchain/declaration.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/problem.h>
#include <interfaces/iassistant.h>
#include <util/richtextpushbutton.h>

using namespace KDevelop;

ProblemNavigationContext::ProblemNavigationContext(const IProblem::Ptr& problem)
  : m_problem(problem)
  , m_widget(nullptr)
{
  QExplicitlySharedDataPointer< IAssistant > solution = problem->solutionAssistant();
  if(solution && !solution->actions().isEmpty()) {
    m_widget = new QWidget;
    QHBoxLayout* layout = new QHBoxLayout(m_widget);
    RichTextPushButton* button = new RichTextPushButton;
//     button->setPopupMode(QToolButton::InstantPopup);
    if(!solution->title().isEmpty())
      button->setHtml(i18n("Solve: %1", solution->title()));
    else
      button->setHtml(i18n("Solve"));

    QMenu* menu = new QMenu;
    menu->setFocusPolicy(Qt::NoFocus);
    foreach(IAssistantAction::Ptr action, solution->actions()) {
      menu->addAction(action->toKAction(this));
    }
    button->setMenu(menu);

    layout->addWidget(button);
    layout->setAlignment(button, Qt::AlignLeft);
    m_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  }
}

ProblemNavigationContext::~ProblemNavigationContext()
{
  delete m_widget;
}

QWidget* ProblemNavigationContext::widget() const
{
    return m_widget;
}

bool ProblemNavigationContext::isWidgetMaximized() const
{
    return false;
}

QString ProblemNavigationContext::name() const
{
  return i18n("Problem");
}


QString ProblemNavigationContext::html(bool shorten)
{
  clear();
  m_shorten = shorten;
  modifyHtml() += QStringLiteral("<html><body><p>");

  modifyHtml() += i18n("Problem in <b>%1</b>:<br/>", m_problem->sourceString());
  modifyHtml() += m_problem->description().toHtmlEscaped();
  modifyHtml() += QStringLiteral("<br/>");
  modifyHtml() += "<i style=\"white-space:pre-wrap\">" + m_problem->explanation().toHtmlEscaped() + "</i>";

  const QVector<IProblem::Ptr> diagnostics = m_problem->diagnostics();
  if (!diagnostics.isEmpty()) {
    modifyHtml() += QStringLiteral("<br/>");

    DUChainReadLocker lock;
    for (auto diagnostic : diagnostics) {
      modifyHtml() += labelHighlight(QStringLiteral("%1: ").arg(diagnostic->severityString()));
      modifyHtml() += diagnostic->description();

      const DocumentRange range = diagnostic->finalLocation();
      Declaration* declaration = DUChainUtils::itemUnderCursor(range.document.toUrl(), range.start());
      if (declaration) {
        modifyHtml() += i18n("<br/>See: ");
        makeLink(declaration->toString(), DeclarationPointer(declaration), NavigationAction::NavigateDeclaration);
        modifyHtml() += i18n(" in ");
        makeLink(QStringLiteral("%1 :%2")
                  .arg(declaration->url().toUrl().fileName())
                  .arg(declaration->rangeInCurrentRevision().start().line() + 1),
                 DeclarationPointer(declaration), NavigationAction::NavigateDeclaration);
      } else if (range.start().isValid()) {
        modifyHtml() += i18n("<br/>See: ");
        const auto url = range.document.toUrl();
        makeLink(QStringLiteral("%1 :%2")
                   .arg(url.fileName())
                   .arg(range.start().line() + 1),
                 url.toDisplayString(QUrl::PreferLocalFile), NavigationAction(url, range.start()));
      }
      modifyHtml() += QStringLiteral("<br/>");
    }
  }

  modifyHtml() += QStringLiteral("</p></body></html>");
  return currentHtml();
}
