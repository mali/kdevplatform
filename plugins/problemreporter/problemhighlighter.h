/*
 * KDevelop Problem Reporter
 *
 * Copyright 2008 Hamish Rodda <rodda@kde.org>
 * Copyright 2008-2009 David Nolden <david.nolden.kdevelop@art-master.de>
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

#ifndef KDEVPLATFORM_PLUGIN_PROBLEM_HIGHLIGHT_H
#define KDEVPLATFORM_PLUGIN_PROBLEM_HIGHLIGHT_H

#include <language/duchain/problem.h>
#include <qpointer.h>
#include <ktexteditor/movingrange.h>
#include <KTextEditor/TextHintInterface>
#include <interfaces/iproblem.h>

class ProblemHighlighter;

class ProblemTextHintProvider : public KTextEditor::TextHintProvider
{
public:
    explicit ProblemTextHintProvider(ProblemHighlighter* highlighter);

    QString textHint(KTextEditor::View* view, const KTextEditor::Cursor& position) override;

private:
    ProblemHighlighter* m_highlighter;
    KTextEditor::Range m_currentHintRange;
};

class ProblemHighlighter : public QObject
{
    Q_OBJECT
public:
    explicit ProblemHighlighter(KTextEditor::Document* document);
    ~ProblemHighlighter() override;

    void setProblems(const QVector<KDevelop::IProblem::Ptr>& problems);

private slots:
    void viewCreated(KTextEditor::Document*, KTextEditor::View*);
    void aboutToRemoveText(const KTextEditor::Range& range);
    void clearProblems();

private:
    QPointer<KTextEditor::Document> m_document;
    QList<KTextEditor::MovingRange*> m_topHLRanges;
    QVector<KDevelop::IProblem::Ptr> m_problems;
    QMap<KTextEditor::MovingRange*, KDevelop::IProblem::Ptr> m_problemsForRanges;

    friend class ProblemTextHintProvider;
    ProblemTextHintProvider m_textHintProvider;

public slots:
    void settingsChanged();
};

#endif // KDEVPLATFORM_PLUGIN_PROBLEM_HIGHLIGHT_H
