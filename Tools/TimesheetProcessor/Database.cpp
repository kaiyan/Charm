#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QObject>
#include <QVariant>
#include <QStringList>

#include "Core/CharmExceptions.h"

#include "Database.h"
#include "Exceptions.h"

#include <cstdlib>

Database::Database()
{
}

Database::~Database()
{
}

void Database::checkUserid( int id ) throw (TimesheetProcessorException )
{
	User user = m_storage.getUser( id );
	if ( !user.isValid() ) {
		throw TimesheetProcessorException( "No such user" );
	}
}

User Database::getOrCreateUserByName( QString name ) throw (TimesheetProcessorException )
{
	User user;
	QSqlQuery query( database() );
	const char* statement = "SELECT user_id from Users WHERE name = :user_name;";
	query.prepare( statement );
	query.bindValue( ":user_name", name );
	bool result = query.exec();
	if ( result ) {
		if ( query.next() ) {
			int userIdPosition = query.record().indexOf( "user_id" );
			Q_ASSERT( userIdPosition != -1 );
			int userId = query.value( userIdPosition ).toInt();
			user = m_storage.getUser( userId );
		} else { // user with this name does not exist:
			user = m_storage.makeUser( name ); // that should work
			if( ! user.isValid() ) {
				throw TimesheetProcessorException( "Cannot create the new user" );
			}
		}
	} else {
		throw TimesheetProcessorException( "Cannot execute query for user name" );
	}
	return user;
}

Task Database::getTask( int taskid ) throw (TimesheetProcessorException )
{
	Task task = m_storage.getTask( taskid );
	if ( !task.isValid() ) {
		throw TimesheetProcessorException( QObject::tr( "Invalid task %1 in report" ).arg( taskid ) );
	}
	return task;
}

TaskList Database::getAllTasks() throw(TimesheetProcessorException )
{
	return m_storage.getAllTasks();
}

QSqlDatabase& Database::database()
{
	return m_storage.database();
}

void Database::login() throw (TimesheetProcessorException )
{
    MySqlStorage::Parameters parameters;
    try {
        parameters = MySqlStorage::parseParameterEnvironmentVariable();
    } catch( ParseError& e ) {
        throw TimesheetProcessorException( e.what() );
    }
    m_storage.configure( parameters );
    bool ok = m_storage.database().open();
    if ( !ok ) {
        QSqlError error = m_storage.database().lastError();

        QString msg = QObject::tr( "Cannot connect to database %1 on host %2, database said "
                                   "\"%3\", driver said \"%4\"" )
                .arg( parameters.database ) .arg( parameters.host )
                .arg( error.driverText() )
                .arg( error.databaseText() );
        throw TimesheetProcessorException( msg );
    }
    // check if the driver has transaction support
    if( ! m_storage.database().driver()->hasFeature( QSqlDriver::Transactions ) ) {
        QString msg = QObject::tr( "The database driver in use does not support transactions. Transactions are required." );
        throw TimesheetProcessorException( msg );
    }
}

void Database::initializeDatabase() throw (TimesheetProcessorException )
{
	try {
		QStringList tables = m_storage.database().tables();
		if ( !tables.empty() ) {
			throw TimesheetProcessorException( "The database is not empty. Only "
				"empty databases can be automatically initialized." );
		}
		if ( !m_storage.createDatabaseTables() ) {
			throw TimesheetProcessorException( "Cannot create database contents, please double-check permissions." );
		}
	} catch ( UnsupportedDatabaseVersionException& e ) {
		throw TimesheetProcessorException( e.what() );
	}
}

void Database::addEvent( const Event& event )
{
	Event newEvent = m_storage.makeEvent();
	int id = newEvent.id();
	newEvent = event;
	newEvent.setId( id );
	if ( !m_storage.modifyEvent( newEvent ) ) {
		throw TimesheetProcessorException( "Cannot add event" );
	}
}

void Database::deleteEventsForReport( int userid, int index )
{
	// delete the time sheet: pretty straightforward
	QString statement = QString::fromLocal8Bit( "DELETE FROM Events WHERE report_id = :index and user_id = :userid" );
	QSqlQuery query( m_storage.database() );
	query.prepare( statement );
	query.bindValue( ":index", index );
	query.bindValue( ":userid", userid );
	bool result = query.exec();
	if ( !result ) {
		throw TimesheetProcessorException( "Failed to delete report" );
	}
}
