#include <algorithm>

#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolButton>
#include <QCloseEvent>

#include <Core/CharmCommand.h>
#include <Core/CharmConstants.h>
#include <Core/XmlSerialization.h>
#include <Core/TaskListMerger.h>

#include "Data.h"
#include "MainWindow.h"
#include "ViewHelpers.h"
#include "Application.h"
#include "Idle/IdleDetector.h"
#include "Idle/IdleCorrectionDialog.h"
#include "CharmAboutDialog.h"
#include "Commands/CommandRelayCommand.h"
#include "Commands/CommandExportToXml.h"
#include "Commands/CommandImportFromXml.h"
#include "Commands/CommandModifyEvent.h"
#include "Commands/CommandSetAllTasks.h"
#include "Commands/CommandModifyTask.h"
#include "Reports/ReportConfigurationPage.h"
#include "CharmPreferences.h"

#include "ui_MainWindow.h"

MainWindow::MainWindow()
    : QMainWindow()
    , m_ui( new Ui::MainWindow() )
    , m_viewActionsGroup( this )
    , m_actionQuit( this )
    , m_actionAboutDialog( this )
    , m_actionPreferences( this )
    , m_actionEventView( &m_viewActionsGroup )
    , m_actionTasksView( &m_viewActionsGroup )
    , m_actionToggleView( this )
    , m_actionShowStatusbar( this )
    , m_actionExportToXml( this )
    , m_actionImportFromXml( this )
    , m_actionImportTasks( this )
    , m_eventView( this )
    , m_actionReporting( this )
    , m_reportDialog( this )
    , m_statusBarWidgetsAdded( false )
{
    m_ui->setupUi( this );

    setWindowIcon( Data::charmIcon() );
    // view corrections:
    centralWidget()->layout()->setMargin( 0 );
    centralWidget()->layout()->setSpacing( 0 );
    m_ui->viewStack->layout()->setMargin( 0 );
    m_ui->viewStack->layout()->setSpacing( 0 );

    // "register" view modes:
    m_modes << &m_tasksView << &m_eventView;
    m_ui->viewStack->addWidget( &m_tasksView );
    m_ui->viewStack->addWidget( &m_eventView );
    Q_FOREACH( ViewModeInterface* mode, m_modes ) {
        QWidget* modeWidget = dynamic_cast<QWidget*>( mode );
        Q_ASSERT( modeWidget );
        if ( modeWidget ) {
            connect( modeWidget, SIGNAL( emitCommand( CharmCommand* ) ),
                     SLOT( sendCommand( CharmCommand* ) ) );
        }
    }

    // set up actions
    m_actionQuit.setText( tr( "Quit" ) );
    m_actionQuit.setIcon( Data::quitCharmIcon() );
    connect( &m_actionQuit, SIGNAL( triggered( bool ) ),
             SLOT( slotQuit() ) );

    m_actionAboutDialog.setText( tr( "About Charm" ) );
    connect( &m_actionAboutDialog, SIGNAL( triggered() ),
             SLOT( slotAboutDialog() ) );

    m_actionPreferences.setText( tr( "Preferences" ) );
    m_actionPreferences.setIcon( Data::configureIcon() );
    connect( &m_actionPreferences, SIGNAL( triggered( bool ) ),
             SLOT( slotEditPreferences( bool ) ) );

    // m_actionEventView.setCheckable( true );
//     m_eventView.setVisible( m_actionEventView.isChecked() );
//     connect( &m_actionEventView, SIGNAL( toggled( bool ) ),
//              &m_eventView, SLOT( setVisible( bool ) ) );
//     connect( &m_eventView, SIGNAL( visible( bool ) ),
//              &m_actionEventView, SLOT( setChecked( bool ) ) );

    m_actionImportFromXml.setText( tr( "Import from Previous Export..." ) );
    connect( &m_actionImportFromXml, SIGNAL( triggered() ),
             SLOT( slotImportFromXml() ) );
    m_actionExportToXml.setText( tr( "Export..." ) );
    connect( &m_actionExportToXml, SIGNAL( triggered() ),
             SLOT( slotExportToXml() ) );
    m_actionImportTasks.setText( tr( "Import Task Definitions..." ) );
    connect( &m_actionImportTasks, SIGNAL( triggered() ),
             SLOT( slotImportTasks() ) );
    // set up Charm menu:
    QMenu* appMenu = new QMenu( tr( "File" ), menuBar() );
    appMenu->addAction( &m_actionPreferences );
    m_actionPreferences.setEnabled( true );
    appMenu->addAction( &m_actionAboutDialog );
    appMenu->addSeparator();
    appMenu->addAction( &m_actionExportToXml );
    appMenu->addAction( &m_actionImportFromXml );
    appMenu->addSeparator();
    appMenu->addAction( &m_actionImportTasks );
    appMenu->addSeparator();
    appMenu->addAction( &m_actionQuit );

    // set up view menu:
    m_actionReporting.setText( tr( "Reports..." ) );
    connect( &m_actionReporting, SIGNAL( triggered() ),
             SLOT( slotReportDialog() ) );

    m_actionEventView.setText( tr( "Event Editor" ) );
    m_actionTasksView.setText( tr( "Tasks View" ) );
    Q_FOREACH( QAction* action, m_viewActionsGroup.actions() ) {
        action->setCheckable( true );
    }
    m_actionTasksView.setShortcut( Qt::CTRL + Qt::Key_T );
    m_actionEventView.setShortcut( Qt::CTRL + Qt::Key_E );
    m_actionReporting.setShortcut( Qt::CTRL + Qt::Key_R );
    // set default view mode:
    // FIXME restore view selection from settings:
    m_actionTasksView.setChecked( true );
    slotSelectViewMode( &m_actionTasksView );
    connect( &m_viewActionsGroup, SIGNAL( triggered( QAction* ) ),
             SLOT( slotSelectViewMode( QAction* ) ) );

    QMenu* viewMenu = new QMenu( tr( "View" ), menuBar() );
    viewMenu->addActions( m_viewActionsGroup.actions() );
    viewMenu->addSeparator();
    m_actionToggleView.setShortcut( Qt::CTRL + Qt::Key_S );
    m_actionToggleView.setText( tr( "Switch View" ) );
    connect( &m_actionToggleView, SIGNAL( triggered() ), SLOT( slotToggleView() ) );
    m_actionShowStatusbar.setText( tr( "Show Status Bar" ) );
    m_actionShowStatusbar.setCheckable( true );
    connect( &m_actionShowStatusbar, SIGNAL( triggered( bool ) ),
             SLOT( slotToggleStatusbar( bool ) ) );
    viewMenu->addAction( &m_actionToggleView );
    viewMenu->addSeparator();
    viewMenu->addAction( &m_actionShowStatusbar );
    viewMenu->addSeparator();
    viewMenu->addAction( &m_actionReporting );

    // FIXME this might be a context-sensitive view-mode-menu:
//     QMenu* taskMenu = new QMenu ( tr( "Task" ), menuBar() );
//     taskMenu->addAction( &m_actionEventStarted );
//     taskMenu->addAction( &m_actionEventEnded );
    menuBar()->addMenu( appMenu );
    menuBar()->addMenu( viewMenu );
}

MainWindow::~MainWindow()
{
    delete m_ui;
}

void MainWindow::restore()
{
    show();
}

void MainWindow::slotQuit()
{
    // this saves changes:
    // FIXME necessary?
    // m_eventView.close();
    // m_reportDialog.close();
    emit quit();
}

void MainWindow::slotEditPreferences( bool )
{
    CharmPreferences dialog( CONFIGURATION, this );

    if ( dialog.exec() ) {
        CONFIGURATION.eventsInLeafsOnly = dialog.eventsInLeafsOnly();
        CONFIGURATION.oneEventAtATime = dialog.oneEventAtATime();
        CONFIGURATION.taskTrackerFontSize = dialog.taskTrackerFontSize();
        CONFIGURATION.always24hEditing = dialog.always24hEditing();
        CONFIGURATION.toolButtonStyle = dialog.toolButtonStyle();
        CONFIGURATION.detectIdling = dialog.detectIdling();
        slotConfigurationChanged();
        emit saveConfiguration();
    }
}

void MainWindow::slotShowHideView()
{   // hide or restore the view
    if ( isVisible() ) {
        hide();
        m_reportDialog.hide();
    } else {
        restore();
        raise();
    }
}

void MainWindow::stateChanged( State previous )
{
    Q_FOREACH( ViewModeInterface* mode, m_modes ) {
        if ( previous == Constructed ) {
            mode->setModel( & Application::instance().model() );
        }
        mode->stateChanged( previous );
    }

    if ( previous == Constructed ) {
        QAbstractItemModel* model = Application::instance().model().eventModel();
        connect( model, SIGNAL( dataChanged( const QModelIndex&, const QModelIndex& ) ),
                 SLOT( slotUpdateTotal() ) );
        connect( model, SIGNAL( rowsInserted( const QModelIndex&, int, int ) ),
                 SLOT( slotUpdateTotal() ) );
        connect( model, SIGNAL( rowsRemoved( const QModelIndex&, int, int ) ),
                 SLOT( slotUpdateTotal() ) );
        connect( model, SIGNAL( layoutChanged() ),
                 SLOT( slotUpdateTotal() ) );
        connect( model, SIGNAL( modelReset() ),
                 SLOT( slotUpdateTotal() ) );
    }

    switch( Application::instance().state() ) {
    case Connected:
        // FIXME remember in Gui state:
        m_ui->viewStack->setCurrentWidget( &m_tasksView );
        slotConfigurationChanged();
        setEnabled( true );
        break;
    case Disconnecting:
        setEnabled( false );
        saveGuiState();
        m_ui->viewStack->setCurrentWidget( m_ui->openingPage );
        break;
    case StartingUp:
        m_ui->viewStack->setCurrentWidget( m_ui->openingPage );
        break;
    case ShuttingDown:
    case Dead:
    case Connecting:
        setEnabled( false );
        restoreGuiState();
        break;
    default:
        break;
    };
}

void MainWindow::commitCommand( CharmCommand* command )
{
    command->finalize();
    delete command;
}

void MainWindow::sendCommand( CharmCommand *cmd)
{
    cmd->prepare();
    CommandRelayCommand* relay = new CommandRelayCommand( this );
    relay->setCommand( cmd );
    emit emitCommand( relay );
}


void MainWindow::showEvent( QShowEvent* )
{
    emit visibilityChanged( true );
    // REFACTOR move to Application
    // m_actionShowHideView.setText( tr( "Hide Charm Window" ) );
}

void MainWindow::hideEvent( QHideEvent* )
{
    emit visibilityChanged( false );
}

void MainWindow::slotAboutDialog()
{
    CharmAboutDialog dialog( this );
    dialog.exec();
}

void MainWindow::restoreGuiState()
{
    // restore geometry
    QSettings settings;
    if ( settings.contains( MetaKey_MainWindowGeometry ) ) {
        restoreGeometry( settings.value( MetaKey_MainWindowGeometry ).toByteArray() );
    }
    // restore visibility
    if ( settings.contains( MetaKey_MainWindowVisible ) ) {
        const bool visible = settings.value( MetaKey_MainWindowVisible ).toBool();
        if ( visible ) {
            show();
        } else {
            hide();
        }
    }
    // call all the view modes:
    for_each( m_modes.begin(), m_modes.end(),
              std::mem_fun( &ViewModeInterface::restoreGuiState ) );
}

void MainWindow::saveGuiState()
{
    QSettings settings;
    // save geometry
    settings.setValue( MetaKey_MainWindowGeometry, saveGeometry() );
    settings.setValue( MetaKey_MainWindowVisible, isVisible() );
    // call all the view modes:
    for_each( m_modes.begin(), m_modes.end(),
              std::mem_fun( &ViewModeInterface::saveGuiState ) );
}

void MainWindow::slotConfigurationChanged()
{
    for_each( m_modes.begin(), m_modes.end(),
              std::mem_fun( &ViewModeInterface::configurationChanged ) );
    QList<QToolButton *> allToolButtons = findChildren<QToolButton *>();
    Q_FOREACH( QToolButton* button, allToolButtons ) {
        button->setToolButtonStyle( CONFIGURATION.toolButtonStyle );
    }
    m_actionShowStatusbar.setChecked( CONFIGURATION.showStatusBar );
    statusBar()->setVisible( CONFIGURATION.showStatusBar );
}

void MainWindow::slotSelectViewMode( QAction* action )
{
    if ( action == &m_actionEventView ) {
        m_ui->viewStack->setCurrentWidget( &m_eventView );
    } else if ( action == &m_actionTasksView ) {
        m_ui->viewStack->setCurrentWidget( &m_tasksView );
    }
}

void MainWindow::slotReportDialog()
{
    m_reportDialog.back();
    if ( m_reportDialog.exec() ) {
        ReportConfigurationPage* page = m_reportDialog.selectedPage();
        QDialog* preview = page->makeReportPreviewDialog( this );
        // preview is destroy-on-close and non-modal:
        preview->show();
    }
}

void MainWindow::slotToggleView()
{
    ViewModeInterface* current = dynamic_cast<ViewModeInterface*>( m_ui->viewStack->currentWidget() );
    if ( current ) {
        Q_ASSERT( m_modes.size() == m_ui->viewStack->count() -1 ); // -1: startup page, never shown again
        QList<ViewModeInterface*>::iterator mode = std::find( m_modes.begin(), m_modes.end(), current );
        Q_ASSERT( mode != m_modes.end() ); // how could that be if m_modes is not empty?
        ++mode;
        if ( mode == m_modes.end() ) {
            mode = m_modes.begin();
        }
        QWidget* widget = dynamic_cast<QWidget*>( *mode );
        // baah:
        if ( widget == &m_eventView ) {
            m_actionEventView.trigger();
        } else if ( widget == &m_tasksView ) {
            m_actionTasksView.trigger();
        } else { // cannot happen:
            Q_ASSERT( widget && false );
        }
    }
}

void MainWindow::slotExportToXml()
{
    // ask for a filename:
    QSettings settings;
    QString path;
    if ( settings.contains( MetaKey_ExportToXmlRecentSavePath ) ) {
        path = settings.value( MetaKey_ExportToXmlRecentSavePath ).toString();
        QDir dir( path );
        if ( !dir.exists() ) path = QString();
    }

    QString filename = QFileDialog::getSaveFileName( this, tr( "Enter File Name" ), path );
    if ( filename.isEmpty() ) return;

    QFileInfo fileinfo( filename );
    path = fileinfo.absolutePath();

    if ( !path.isEmpty() ) {
        settings.setValue( MetaKey_ExportToXmlRecentSavePath, path );
    }

    if ( fileinfo.suffix().isEmpty() ) {
        filename+=".charmdatabaseexport";
    }

    // get a XML export:
    CommandExportToXml* command = new CommandExportToXml( filename, this );
    sendCommand( command );
}

void MainWindow::slotImportFromXml()
{
    // ask for the filename:
    QSettings settings;
    QString path;
    if ( settings.contains( MetaKey_ExportToXmlRecentSavePath ) ) {
        path = settings.value( MetaKey_ExportToXmlRecentSavePath ).toString();
        QDir dir( path );
        if ( !dir.exists() ) path = QString();
    }

    QString filename = QFileDialog::getOpenFileName( this, tr( "Please Select File" ), path );
    if ( filename.isEmpty() ) return;
    QFileInfo fileinfo( filename );
    Q_ASSERT( fileinfo.exists() );

    // warn the user about the consequences:
    if ( QMessageBox::warning( this, tr("Watch out!" ),
                               tr("During import, all existing tasks and events will be deleted"
                                  " and replaced with the imported ones. Are you sure?" ),
                               QMessageBox::Yes | QMessageBox::No ) != QMessageBox::Yes )
        return;

    // ask the controller to import the file:
    CommandImportFromXml* cmd = new CommandImportFromXml( filename, this );
    sendCommand( cmd );
}

void MainWindow::slotImportTasks()
{
    QString filename = QFileDialog::getOpenFileName( this, tr( "Please Select File" ) );
    if ( filename.isEmpty() ) return;
    QFileInfo fileinfo( filename );
    Q_ASSERT( fileinfo.exists() );


    TaskExport exporter;
    TaskListMerger merger;
    try {
        exporter.readFrom( filename );
        merger.setOldTasks( DATAMODEL->getAllTasks() );
        merger.setNewTasks( exporter.tasks() );
        if ( merger.modifiedTasks().count() == 0 && merger.addedTasks().count() == 0 ) {
            QMessageBox::information( this, tr( "Tasks Import" ), tr( "The selected task file does not contain any updates." ) );
        } else {
            QString detailsText(
                tr( "Importing this task list will result in %1 modified and %2 added tasks. Do you want to continue?" )
                .arg( merger.modifiedTasks().count() )
                .arg( merger.addedTasks().count() ) );
            if ( QMessageBox::warning( this, tr( "Tasks Import" ), detailsText,
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes )
                 == QMessageBox::Yes ) {
                CommandSetAllTasks* cmd = new CommandSetAllTasks( merger.mergedTaskList(), this );
                sendCommand( cmd );
            }
        }
    } catch(  InvalidTaskListException& e ) {
        QMessageBox::critical( this, tr(  "Invalid Task Definitions" ),
                               tr( "The selected task definitions are invalid and cannot be imported." ) );
        return;
    } catch( XmlSerializationException& e ) {
        QMessageBox::critical( this, tr(  "Invalid Task Definitions" ),
                               tr( "The selected task definitions are invalid and cannot be imported." ) );
        return;
    }
}

static int totalDuration( const EventIdList& eventList )
{
    int total = 0;
    Q_FOREACH( EventId id, eventList ) {
        const Event& event = DATAMODEL->eventForId( id );
        total += event.duration();
    }
    return total;
}

void MainWindow::slotUpdateTotal()
{
    if ( !m_statusBarWidgetsAdded ) {
        m_statusBarWidgetsAdded = true;
        statusBar()->addPermanentWidget( &m_statusBarLabelDayTotal );
        statusBar()->addPermanentWidget( &m_statusBarLabelWeekTotal );
    }

    const QDate today = QDate::currentDate();
    const EventIdList todayEvents = DATAMODEL->eventsThatStartInTimeFrame(
        QDateTime( today ), QDateTime( today.addDays(1) ) );
    m_statusBarLabelDayTotal.setText( tr("Day total: %1").arg(hoursAndMinutes(totalDuration(todayEvents))) );

    const TimeSpan thisWeek( today.addDays( - today.dayOfWeek() + 1 ),
                             today.addDays( 7 - today.dayOfWeek() + 1 ) );
    const EventIdList weekEvents = DATAMODEL->eventsThatStartInTimeFrame(
        QDateTime( thisWeek.first ), QDateTime( thisWeek.second ) );
    m_statusBarLabelWeekTotal.setText( tr("Week total: %1").arg(hoursAndMinutes(totalDuration(weekEvents))) );
}

void MainWindow::slotToggleStatusbar( bool show )
{
    CONFIGURATION.showStatusBar = show;
    statusBar()->setVisible( show );
    emit saveConfiguration();
}

QAction* MainWindow::actionQuit()
{
    return &m_actionQuit;
}

void MainWindow::maybeIdle()
{
    if ( Application::instance().idleDetector() == 0 ) return;

    IdleDetector* detector = Application::instance().idleDetector();

    Q_FOREACH( const IdleDetector::IdlePeriod& p, detector->idlePeriods() ) {
        qDebug() << "Application::slotMaybeIdle: computer might be have been idle from"
                 << p.first
                 << "to" << p.second;
    }

    // handle idle merging:
    IdleCorrectionDialog dialog( this );
    const bool wasVisible = isVisible();
    if ( ! wasVisible ) show();

    dialog.exec();
    switch( dialog.result() ) {
    case IdleCorrectionDialog::Idle_Ignore:
        break;
    case IdleCorrectionDialog::Idle_EndEvent: {
        EventIdList activeEvents = DATAMODEL->activeEvents();
        DATAMODEL->endAllEventsRequested();
        // FIXME with this option, we can only change the events to
        // the start time of one idle period, I chose to use the last
        // one:
        const IdleDetector::IdlePeriod& period = detector->idlePeriods().last();
        Q_FOREACH ( EventId eventId, activeEvents ) {
            Event event = DATAMODEL->eventForId( eventId );
            if ( event.isValid() ) {
                event.setEndDateTime( qMax( event.startDateTime(), period.first ) );
                Q_ASSERT( event.isValid() );
                CommandModifyEvent* cmd = new CommandModifyEvent( event, this );
                emit emitCommand( cmd );
            }
        }
        break;
    }
    default:
        break; // should not happen
    }
    detector->clear();
    if ( ! wasVisible ) hide();
}

#include "MainWindow.moc"
