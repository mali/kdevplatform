/***************************************************************************
Copyright 2006-2009 David Nolden <david.nolden.kdevelop@art-master.de>
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "patchreview.h"

#include <kmimetype.h>
#include <klineedit.h>
#include <kmimetypechooser.h>
#include <kmimetypetrader.h>
#include <krandom.h>
#include <QTabWidget>
#include <QMenu>
#include <QFile>
#include <QTimer>
#include <QMutexLocker>
#include <QPersistentModelIndex>
#include <kfiledialog.h>
#include <interfaces/idocument.h>
#include <QStandardItemModel>
#include <interfaces/icore.h>
#include <kde_terminal_interface.h>
#include <kparts/part.h>
#include <kparts/factory.h>
#include <kdialog.h>

#include "libdiff2/komparemodellist.h"
#include "libdiff2/kompare.h"
#include <kmessagebox.h>
#include <QMetaType>
#include <QVariant>
#include "libdiff2/diffsettings.h"
#include <ktexteditor/cursor.h>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>
#include <ktexteditor/markinterface.h>
#include <ktexteditor/smartinterface.h>
#include <interfaces/idocumentcontroller.h>
#include <k3process.h>
#include <interfaces/iuicontroller.h>
#include <kaboutdata.h>

///Whether arbitrary exceptions that occurred while diff-parsing within the library should be caught
#define CATCHLIBDIFF

/* Exclude this file from doublequote_chars check as krazy doesn't understand
std::string*/
//krazy:excludeall=doublequote_chars

QStringList splitArgs( const QString& str ) {
    QStringList ret;
    QString current = str;
    int pos = 0;
    while ( ( pos = current.indexOf( ' ', pos ) ) != -1 ) {
        if ( current[ 0 ] == '"' ) {
            int end = current.indexOf( '"' );
            if ( end > pos )
                pos = end;
        }
        QString s = current.left( pos );
        if ( s.length() > 0 )
            ret << s;
        current = current.mid( pos + 1 );
        pos = 0;
    }
    if ( current.length() )
        ret << current;
    return ret;
}

using namespace KDevelop;

Q_DECLARE_METATYPE( const Diff2::DiffModel* )

PatchReviewToolView::PatchReviewToolView( QWidget* parent, PatchReviewPlugin* plugin ) : QWidget( parent ), m_actionState( LocalPatchSource::Unknown ), m_plugin( plugin ), m_reversed( false ) {
    showEditDialog();
    connect( plugin, SIGNAL(patchChanged()), SLOT(patchChanged()) );
    patchChanged();
}

void PatchReviewToolView::patchChanged()
{
    fillEditFromPatch();
    kompareModelChanged();
}

PatchReviewToolView::~PatchReviewToolView() {
}

void PatchReviewToolView::updatePatchFromEdit() {

    LocalPatchSourcePointer ps = m_plugin->patch();

    if ( !ps )
        return;

    ps->filename = m_editPatch.filename->url();
    ps->baseDir = m_editPatch.baseDir->url();
    ps->depth = m_editPatch.depth->value();

    ps->state = editState();

    m_plugin->notifyPatchChanged();
}

LocalPatchSource::State PatchReviewToolView::editState() {
    switch ( m_editPatch.state->currentIndex() ) {
    case 0:
        return LocalPatchSource::Unknown;
    case 1:
        return LocalPatchSource::Applied;
    default:
        return LocalPatchSource::NotApplied;
    }
}

void PatchReviewToolView::fillEditFromPatch() {

    LocalPatchSourcePointer patch = m_plugin->patch();
    if ( !patch )
        return ;

    m_editPatch.filename->setUrl( patch->filename );
    m_editPatch.baseDir->setUrl( patch->baseDir );
    m_editPatch.depth->setValue( patch->depth );

    int stateIndex = 0;

    switch ( patch->state ) {
    case LocalPatchSource::Applied:
        stateIndex = 1;
        break;
    case LocalPatchSource::NotApplied:
        stateIndex = 2;
        break;
    default:
        stateIndex = 0;
    }

    m_editPatch.state->setCurrentIndex( stateIndex );
//   slotStateChanged();

    if ( !patch->filename.isEmpty() )
        m_editPatch.tabWidget->setCurrentIndex( m_editPatch.tabWidget->indexOf( m_editPatch.fileTab ) );
    else
        m_editPatch.tabWidget->setCurrentIndex( m_editPatch.tabWidget->indexOf( m_editPatch.commandTab ) );
}

void PatchReviewToolView::slotStateChanged() {
//   updatePatchFromEdit();

    switch ( editState() ) {
    case LocalPatchSource::Unknown:
        m_editPatch.applyButton->setEnabled( true );
        m_editPatch.unApplyButton->setEnabled( true );
        break;
    case LocalPatchSource::Applied:
        m_editPatch.applyButton->setEnabled( false );
        m_editPatch.unApplyButton->setEnabled( true );
        break;
    case LocalPatchSource::NotApplied:
        m_editPatch.applyButton->setEnabled( true );
        m_editPatch.unApplyButton->setEnabled( false );
        break;
    }
}

void PatchReviewToolView::slotEditCommandChanged() {
    m_editPatch.filename->setText( "" );
    updatePatchFromEdit();
}

void PatchReviewToolView::slotEditFileNameChanged() {
    m_editPatch.command->setText( "" );
    updatePatchFromEdit();
}

void PatchReviewToolView::showEditDialog() {

    m_editPatch.setupUi( this );

    m_filesModel = new QStandardItemModel( m_editPatch.filesList );
    m_editPatch.filesList->setModel( m_filesModel );

    connect( m_editPatch.previousHunk, SIGNAL( clicked( bool ) ), this, SLOT( prevHunk() ) );
    connect( m_editPatch.nextHunk, SIGNAL( clicked( bool ) ), this, SLOT( nextHunk() ) );
    connect( m_editPatch.filesList, SIGNAL( doubleClicked( const QModelIndex& ) ), this, SLOT( fileDoubleClicked( const QModelIndex& ) ) );
    connect( m_editPatch.filesList->selectionModel(), SIGNAL( selectionChanged( const QItemSelection&, const QItemSelection& ) ), this, SLOT( fileSelectionChanged() ) );
    //connect( m_editPatch.cancelButton, SIGNAL( pressed() ), this, SLOT( slotEditCancel() ) );

    connect( this, SIGNAL( finished( int ) ), this, SLOT( slotEditDialogFinished( int ) ) );

    connect( m_editPatch.depth, SIGNAL(valueChanged(int)), SLOT(updatePatchFromEdit()) );
    connect( m_editPatch.filename, SIGNAL( textChanged( const QString& ) ), SLOT(slotEditFileNameChanged()) );
    connect( m_editPatch.baseDir, SIGNAL(textChanged(QString)), SLOT(updatePatchFromEdit()) );

    m_editPatch.baseDir->setMode(KFile::Directory);

    connect( m_editPatch.command, SIGNAL( textChanged( const QString& ) ), this, SLOT(slotEditCommandChanged()) );
    connect( m_editPatch.state, SIGNAL( currentIndexChanged( int ) ), this, SLOT( slotStateChanged() ) );
//   connect( m_editPatch.determineState, SIGNAL( clicked( bool ) ), this, SLOT( slotDetermineState() ) );
//   connect( m_editPatch.commandToFile, SIGNAL( clicked( bool ) ), this, SLOT( slotToFile() ) );

    connect( m_editPatch.filename->lineEdit(), SIGNAL( returnPressed() ), this, SLOT(slotEditFileNameChanged()) );
    connect( m_editPatch.filename->lineEdit(), SIGNAL( editingFinished() ), this, SLOT(slotEditFileNameChanged()) );
    connect( m_editPatch.filename, SIGNAL( urlSelected( const QString& ) ), this, SLOT(slotEditFileNameChanged()) );
    connect( m_editPatch.command, SIGNAL( editingFinished() ), this, SLOT(slotEditCommandChanged()) );
    connect( m_editPatch.command, SIGNAL( returnPressed() ), this, SLOT(slotEditCommandChanged()) );
    connect( m_editPatch.command, SIGNAL( () ), this, SLOT(slotEditCommandChanged()) );

    connect( m_editPatch.highlightFiles, SIGNAL(clicked(bool)), m_plugin, SLOT(highlightPatch()) );

    bool blocked = blockSignals( true );

    m_editPatch.state->insertItem( 0, "Unknown" );
    m_editPatch.state->insertItem( 1, "Applied" );
    m_editPatch.state->insertItem( 2, "Not Applied" );

    blockSignals( blocked );
}

void PatchReviewToolView::nextHunk() {
//   updateKompareModel();
    m_plugin->seekHunk( true );
}

void PatchReviewToolView::prevHunk() {
//   updateKompareModel();
    m_plugin->seekHunk( false );
}

void PatchReviewPlugin::seekHunk( bool forwards, const QString& fileName ) {
    try {
        if ( !m_modelList.get() )
            throw "no model";

        for (uint a = 0; a < m_modelList->modelCount(); ++a) {

            const Diff2::DiffModel* model = m_modelList->modelAt(a);
            if ( !model || !model->differences() )
                continue;

            KUrl file = m_patch->baseDir;
            if ( isSource() ) {
                file.addPath( model->sourcePath() );
                file.addPath( model->sourceFile() );
            } else {
                file.addPath( model->destinationPath() );
                file.addPath( model->destinationFile() );
            }
            if ( !fileName.isEmpty() && fileName != file.toLocalFile() )
                continue;

            IDocument* doc = ICore::self()->documentController()->documentForUrl( file );

            if ( doc ) {
                ICore::self()->documentController()->activateDocument( doc );
                if ( doc->textDocument() ) {
                    KTextEditor::View * v = doc->textDocument() ->activeView();
                    int bestLine = -1;
                    if ( v ) {
                        KTextEditor::Cursor c = v->cursorPosition();
                        for ( Diff2::DifferenceList::const_iterator it = model->differences() ->begin(); it != model->differences() ->end(); ++it ) {
                            int line;
                            Diff2::Difference* diff = *it;
                            if ( isSource() )
                                line = diff->sourceLineNumber();
                            else
                                line = diff->destinationLineNumber();
                            if ( line > 0 )
                                line -= 1;

                            if ( forwards ) {
                                if ( line > c.line() && ( bestLine == -1 || line < bestLine ) )
                                    bestLine = line;
                            } else {
                                if ( line < c.line() && ( bestLine == -1 || line > bestLine ) )
                                    bestLine = line;
                            }
                        }
                        if ( bestLine != -1 ) {
                            v->setCursorPosition( KTextEditor::Cursor( bestLine, 0 ) );
                            return ;
                        }
                    }
                }
            }
        }

    } catch ( const QString & str ) {
        kDebug() << "seekHunk():" << str;
    } catch ( const char * str ) {
        kDebug() << "seekHunk():" << str;
    }
    kDebug() << "no matching hunk found";
}

void PatchReviewPlugin::highlightPatch() {
//   updateKompareModel();
    try {
        if ( !modelList() )
            throw "no model";

        for (uint a = 0; a < modelList()->modelCount(); ++a) {
            const Diff2::DiffModel* model = modelList()->modelAt(a);
            if ( !model )
                continue;

            KUrl file = m_patch->baseDir;
            if ( isSource() ) {
                file.addPath( model->sourcePath() );
                file.addPath( model->sourceFile() );
            } else {
                file.addPath( model->destinationPath() );
                file.addPath( model->destinationFile() );
            }

            kDebug() << "highlighting" << file.toLocalFile();

            IDocument* doc = ICore::self()->documentController()->documentForUrl( file );

            if ( !doc ) {
                doc = ICore::self()->documentController()->openDocument( file, KTextEditor::Cursor() );
                if (!doc) {
                    kWarning() << "failed to open" << file;
                    return;
                }
                seekHunk( true, file.toLocalFile() );
            }
            removeHighlighting( file.toLocalFile() );

            m_highlighters[ file.toLocalFile() ] = new DocumentHighlighter( model, doc, isSource() );
        }

    } catch ( const QString & str ) {
        kDebug() << "highlightFile():" << str;
    } catch ( const char * str ) {
        kDebug() << "highlightFile():" << str;
    }
}

void PatchReviewToolView::fileDoubleClicked( const QModelIndex& i ) {
    try {
        if ( !m_plugin->modelList() )
            throw "no model";
        QVariant v = m_filesModel->data( i, Qt::UserRole );
        if ( !v.canConvert<const Diff2::DiffModel*>() )
            throw "cannot convert";
        const Diff2::DiffModel* model = v.value<const Diff2::DiffModel*>();
        if ( !model )
            throw "bad model-value";

        KUrl file = m_plugin->patch()->baseDir;
        if ( m_plugin->isSource() ) {
            file.addPath( model->sourcePath() );
            file.addPath( model->sourceFile() );
        } else {
            file.addPath( model->destinationPath() );
            file.addPath( model->destinationFile() );
        }

        kDebug() << "opening" << file.toLocalFile();

        ICore::self()->documentController()->openDocument( file, KTextEditor::Cursor() );

        m_plugin->seekHunk( true, file.toLocalFile() );
    } catch ( const QString & str ) {
        kDebug() << "fileDoubleClicked():" << str;
    } catch ( const char * str ) {
        kDebug() << "fileDoubleClicked():" << str;
    }
}

void PatchReviewToolView::kompareModelChanged()
{
    m_filesModel->clear();
    m_filesModel->insertColumns( 0, 1 );

    if (!m_plugin->modelList())
        return;

    const Diff2::DiffModelList* models = m_plugin->modelList()->models();
    if ( !models )
        throw "no diff-models";
    Diff2::DiffModelList::const_iterator it = models->begin();
    while ( it != models->end() ) {
        Diff2::DifferenceList * diffs = ( *it ) ->differences();
        int cnt = 0;
        if ( diffs )
            cnt = diffs->count();

        KUrl file = m_plugin->patch()->baseDir;
        if ( m_plugin->isSource() ) {
            file.addPath( ( *it ) ->sourcePath() );
            file.addPath( ( *it ) ->sourceFile() );
        } else {
            file.addPath( ( *it ) ->destinationPath() );
            file.addPath( ( *it ) ->destinationFile() );
        }

        m_filesModel->insertRow( 0 );
        QModelIndex i = m_filesModel->index( 0, 0 );
        if ( i.isValid() ) {
            //m_filesModel->setData( i, file, Qt::DisplayRole );
            m_filesModel->setData( i, QString( "%1 (%2 hunks)" ).arg( file.toLocalFile() ).arg( cnt ), Qt::DisplayRole );
            QVariant v;
            v.setValue<const Diff2::DiffModel*>( *it );
            m_filesModel->setData( i, v, Qt::UserRole );
        }
        ++it;
    }
    fileSelectionChanged();
}

void PatchReviewToolView::fileSelectionChanged() {
    QModelIndexList i = m_editPatch.filesList->selectionModel() ->selectedIndexes();
    m_editPatch.nextHunk->setEnabled( false );
    m_editPatch.previousHunk->setEnabled( false );
    m_editPatch.highlightFiles->setEnabled( false );
    if ( !m_plugin->modelList() )
        return ;
    for ( QModelIndexList::iterator it = i.begin(); it != i.end(); ++it ) {
        QVariant v = m_filesModel->data( *it, Qt::UserRole );
        if ( v.canConvert<const Diff2::DiffModel*>() ) {
            const Diff2::DiffModel * model = v.value<const Diff2::DiffModel*>();
            if ( model ) {
                if ( model->differenceCount() != 0 ) {
                    m_editPatch.nextHunk->setEnabled( true );
                    m_editPatch.previousHunk->setEnabled( true );
                    m_editPatch.highlightFiles->setEnabled( true );
                }
            }
        }
    }
}

DocumentHighlighter::DocumentHighlighter( const Diff2::DiffModel* model, IDocument* kdoc, bool isSource ) throw( QString ) : m_doc( kdoc ) {
//  connect( kdoc, SIGNAL( destroyed( QObject* ) ), this, SLOT( documentDestroyed() ) );
    connect( kdoc->textDocument(), SIGNAL( destroyed( QObject* ) ), this, SLOT( documentDestroyed() ) );
    connect( model, SIGNAL( destroyed( QObject* ) ), this, SLOT( documentDestroyed() ) );

    KTextEditor::Document* doc = kdoc->textDocument();
    if ( doc->lines() == 0 )
        return ;
    if ( doc->activeView() == 0 ) return;

    if ( !model->differences() )
        return ;
    KTextEditor::SmartInterface* smart = dynamic_cast<KTextEditor::SmartInterface*>( doc );
    if ( !smart )
        throw QString( "no smart-interface" );

    QMutexLocker lock(smart->smartMutex());

    KTextEditor::SmartRange* topRange = smart->newSmartRange(doc->documentRange(), 0, KTextEditor::SmartRange::ExpandLeft | KTextEditor::SmartRange::ExpandRight);

    for ( Diff2::DifferenceList::const_iterator it = model->differences() ->begin(); it != model->differences() ->end(); ++it ) {
        Diff2::Difference* diff = *it;
        int line, lineCount;
        if ( isSource ) {
            line = diff->sourceLineNumber();
            lineCount = diff->sourceLineCount();
        } else {
            line = diff->destinationLineNumber();
            lineCount = diff->destinationLineCount();
        }
        if ( line > 0 )
            line -= 1;

        KTextEditor::Cursor c( line, 0 );
        KTextEditor::Cursor endC( line + lineCount, 0 );
        if ( doc->lines() <= c.line() )
            c.setLine( doc->lines() - 1 );
        if ( doc->lines() <= endC.line() )
            endC.setLine( doc->lines() - 1 );
        endC.setColumn( doc->lineLength( endC.line() ) ) ;

        if ( endC.isValid() && c.isValid() ) {
            KTextEditor::SmartRange * r = smart->newSmartRange( c, endC );
            r->setParentRange(topRange);
            KSharedPtr<KTextEditor::Attribute> t( new KTextEditor::Attribute() );

            t->setProperty( QTextFormat::BackgroundBrush, QBrush( QColor( 200, 200, 255 ) ) );
            r->setAttribute( t );
        }
    }

    m_ranges << topRange;

    smart->addHighlightToDocument(topRange);
}

DocumentHighlighter::~DocumentHighlighter() {
    for ( QList<KTextEditor::SmartRange*>::iterator it = m_ranges.begin(); it != m_ranges.end(); ++it ) {
        delete *it; ///@todo smart-lock
    }

    m_ranges.clear();
}

IDocument* DocumentHighlighter::doc() {
    return m_doc;
}

void DocumentHighlighter::documentDestroyed() {
    m_ranges.clear();
}

void PatchReviewPlugin::removeHighlighting( const QString& file ) {
    if ( file.isEmpty() ) {
        ///Remove all highlighting
        for ( HighlightMap::iterator it = m_highlighters.begin(); it != m_highlighters.end(); ++it ) {
            delete *it;
        }
        m_highlighters.clear();
    } else {
        HighlightMap::iterator it = m_highlighters.find( file );
        if ( it != m_highlighters.end() ) {
            delete * it;
            m_highlighters.erase( it );
        }
    }
}

void PatchReviewPlugin::notifyPatchChanged()
{
    kDebug() << "notifying patch change: " << m_patch->filename;
    m_updateKompareTimer->start(500);
}

void PatchReviewPlugin::updateKompareModel() {
    kDebug() << "updating model";
    try {
        LocalPatchSourcePointer l = m_patch;
        if ( l ) {
//       if ( l->state != LocalPatchSource::Applied )
//         return;
            /*
            if ( l->filename == m_lastModelFile && m_modelList.get() )
              return ; ///We already have the correct model
              */
            m_lastModelFile = l->filename;
        }

        m_modelList.reset( 0 );
        qRegisterMetaType<const Diff2::DiffModel*>( "const Diff2::DiffModel*" );
        if ( m_diffSettings )
            delete m_diffSettings;
        m_diffSettings = new DiffSettings( 0 );
        m_kompareInfo.reset( new Kompare::Info() );
        removeHighlighting();
        m_modelList.reset( new Diff2::KompareModelList( m_diffSettings, *m_kompareInfo, ( QObject* ) this ) );
        KUrl diffFile = l->filename;
        if ( diffFile.isEmpty() )
            return ;
        try {
            ///@todo does not work with remote URLs
            if ( !m_modelList->openDirAndDiff( l->baseDir.toLocalFile(), diffFile.toLocalFile() ) )
                throw "could not open diff " + diffFile.toLocalFile();
        } catch ( const QString & str ) {
            throw;
        } catch ( ... ) {
            throw QString( "lib/libdiff2 crashed, memory may be corrupted. Please restart kdevelop." );
        }

        emit patchChanged();
        return;
    } catch ( const QString & str ) {
        kWarning() << "updateKompareModel:" << str;
    } catch ( const char * str ) {
        kWarning() << "updateKompareModel:" << str;
    }
    m_modelList.reset( 0 );
    delete m_diffSettings;
    m_kompareInfo.reset( 0 );
}

K_PLUGIN_FACTORY(KDevProblemReporterFactory, registerPlugin<PatchReviewPlugin>(); )
K_EXPORT_PLUGIN(KDevProblemReporterFactory(KAboutData("kdevpatchreview","kdevpatchreview", ki18n("Patch Review"), "0.1", ki18n("Highlights code affected by a patch"), KAboutData::License_GPL)))

class PatchReviewToolViewFactory : public KDevelop::IToolViewFactory
{
public:
    PatchReviewToolViewFactory(PatchReviewPlugin *plugin): m_plugin(plugin) {}

    virtual QWidget* create(QWidget *parent = 0)
    {
        return m_plugin->createToolView(parent);
    }

    virtual Qt::DockWidgetArea defaultPosition()
    {
        return Qt::BottomDockWidgetArea;
    }

    virtual QString id() const
    {
        return "org.kdevelop.PatchReviewToolview";
    }

private:
    PatchReviewPlugin *m_plugin;
};

PatchReviewPlugin::~PatchReviewPlugin()
{
    removeHighlighting();
}

PatchReviewPlugin::PatchReviewPlugin(QObject *parent, const QVariantList &) : KDevelop::IPlugin(KDevProblemReporterFactory::componentData(), parent), m_factory(new PatchReviewToolViewFactory(this)), m_isSource(false) {

    m_patch = LocalPatchSourcePointer(new LocalPatchSource);
    core()->uiController()->addToolView(i18n("Patch Review"), m_factory);
    setXMLFile("kdevpatchreview.rc");

    m_updateKompareTimer = new QTimer( this );
    m_updateKompareTimer->setSingleShot( true );
    connect( m_updateKompareTimer, SIGNAL( timeout() ), this, SLOT( updateKompareModel() ) );

}

void PatchReviewPlugin::unload()
{
    core()->uiController()->removeToolView(m_factory);

    KDevelop::IPlugin::unload();
}

QWidget* PatchReviewPlugin::createToolView(QWidget* parent)
{
    return new PatchReviewToolView(parent, this);
}

void PatchReviewPlugin::determineState() {
    LocalPatchSourcePointer lpatch = m_patch;
    if ( !lpatch ) {
        kDebug() <<"determineState(..) could not lock patch";
    }
    try {
        if ( lpatch->filename.isEmpty() )
            throw "state can only be determined for file-patches";

        KUrl fileUrl = lpatch->filename;

        {
            K3Process proc;
            ///Try to apply, if it works, the patch is not applied
            QString cmd =  "patch --dry-run -s -f -i " + fileUrl.toLocalFile();
            proc << splitArgs( cmd );

            kDebug() << "calling " << cmd;

            if ( !proc.start( K3Process::Block ) )
                throw "could not start process";

            if ( !proc.normalExit() )
                throw "process did not exit normally";

            kDebug() << "exit-status:" << proc.exitStatus();

            if ( proc.exitStatus() == 0 ) {
                lpatch->state = LocalPatchSource::NotApplied;
                return;
            }
        }

        {
            ///Try to revert, of it works, the patch is applied
            K3Process proc;
            QString cmd =  "patch --dry-run -s -f -i --reverse " + fileUrl.toLocalFile();
            proc << splitArgs( cmd );

            kDebug() << "calling " << cmd;

            if ( !proc.start( K3Process::Block ) )
                throw "could not start process";

            if ( !proc.normalExit() )
                throw "process did not exit normally";

            kDebug() << "exit-status:" << proc.exitStatus();

            if ( proc.exitStatus() == 0 ) {
                lpatch->state = LocalPatchSource::Applied;
                return;
            }
        }
    } catch ( const QString& str ) {
        kWarning() <<"Error:" << str;
    } catch ( const char* str ) {
        kWarning() << "Error:" << str;
    }

    lpatch->state = LocalPatchSource::Unknown;
}
#include "patchreview.moc"

// kate: space-indent on; indent-width 2; tab-width 2; replace-tabs on