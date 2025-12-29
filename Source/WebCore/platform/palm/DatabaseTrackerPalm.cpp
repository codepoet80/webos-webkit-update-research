

#include "config.h"

#if PLATFORM(WEBOS)
#include <errno.h>
#include <syslog.h>
#include <CString.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include "Database.h"
#include "FileSystem.h"
#endif // WEBOS

#include "DatabaseTracker.h"

#if ENABLE(DATABASE)

#include "AbstractDatabase.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DatabaseThread.h"
#include "DatabaseTrackerClient.h"
#include "Logging.h"
#include "OriginQuotaManager.h"
#include "Page.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SecurityOriginHash.h"
#include "SQLiteFileSystem.h"
#include "SQLiteStatement.h"
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

void DatabaseTracker::origins(Vector<RefPtr<SecurityOrigin> >& result, const String& condition)
{
	if (condition.isEmpty()) {
		origins(result);
		return;
	}

    //ASSERT(currentThread() == m_thread);
	OwnPtr<QuotaMap> quotaMap;
	quotaMap.set(new QuotaMap);

	MutexLocker lockDatabase(m_databaseGuard);
	openTrackerDatabase(false);
    if (!m_database.isOpen())
        return;

	char* query(NULL);
	if (-1 == ::asprintf(&query, "SELECT origin, quota FROM Origins WHERE (origin LIKE '%s')", condition.utf8().data()))
		return;

	SQLiteStatement statement(m_database, query);

    if (statement.prepare() != SQLResultOk) {
		::free(query);
        return;
	}

    int sqlResult;
    while ((sqlResult = statement.step()) == SQLResultRow) {
        RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromDatabaseIdentifier(statement.getColumnText(0));
        quotaMap->set(origin.get(), statement.getColumnInt64(1));
    }

    if (sqlResult != SQLResultDone)
        LOG_ERROR("Failed to read in all origins from the database");

	copyKeysToVector(*quotaMap, result);

	::free(query);
}

void DatabaseTracker::deleteOrigins(Vector<RefPtr<SecurityOrigin> >& origins)
{
	for (unsigned i = 0; i < origins.size(); ++i)
        deleteOrigin(origins[i].get());
}

void DatabaseTracker::setTrackerQuota(unsigned long long quota)
{
	m_trackerQuota = quota;
}

unsigned long long DatabaseTracker::quotaForTracker()
{
	return m_trackerQuota;
}

unsigned long long DatabaseTracker::defaultQuotaForTracker()
{
	return 10 * 1024 * 1024;
}

/**
 * Copy a file from one location to another.
 */
static bool CopyFile(const char* src, const char* dst)
{
	assert(src != NULL && dst != NULL);

	if (strcmp(src, dst) == 0)
		return false;

	const size_t buffSize = 64*1024;
	bool success(false);
	int srcfd(-1);
	int dstfd(-1);
	void* buff(NULL);

	buff = ::malloc(buffSize);
	if (buff == NULL)
		goto DONE;

	srcfd = ::open(src, O_RDONLY);
	if (srcfd == -1)
		goto DONE;

	dstfd = ::creat(dst, O_CREAT|O_WRONLY|O_TRUNC);
	if (dstfd == -1)
		goto DONE;

    (void)fchmod(dstfd, S_IRUSR|S_IWUSR | S_IRGRP | S_IROTH);

	while(true) {
		ssize_t numRead = ::read(srcfd, buff, buffSize);
		if (numRead < 0) {	// Error
			break;
		}
		else if (numRead == 0) { // EOF
			success = true;
			break;
		}
		else {
			ssize_t numWritten = ::write(dstfd, buff, numRead);
			if (numWritten != numRead) {
				break;
			}
		}
	}

DONE:

	if (buff != NULL)
		::free(buff);
	if (srcfd != -1)
		::close(srcfd);
	if (dstfd != -1) {
		::close(dstfd);
		if (!success) {
			::unlink(dst);
		}
	}

	return success;
}

/**
 * Copy a <strong>closed</strong> database from one origin to another.
 */
bool DatabaseTracker::copyOriginDatabase(const String& dbName, SecurityOrigin* srcOrigin,
		SecurityOrigin* dstOrigin, DatabaseTracker* dstTracker)
{
	assert(dstTracker != NULL);

	// First look and see if the database is open
	Vector<RefPtr<AbstractDatabase> > openDbs;
	{
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
        if (m_openDatabaseMap) {										// At least one open database
            DatabaseNameMap* nameMap = m_openDatabaseMap->get(srcOrigin);	// Get all db's for this origin
            if (nameMap && nameMap->size()) {
                DatabaseSet* databaseSet = nameMap->get(dbName);		// Get all matching db name supplied
                if (databaseSet && databaseSet->size()) {				// If got at least one.
                    DatabaseSet::const_iterator end = databaseSet->end();
                    for (DatabaseSet::const_iterator it = databaseSet->begin(); it != end; ++it) {
						ASSERT(*it != NULL);
                        openDbs.append(*it);
					}
                }
            }
        }
    }

	if (openDbs.size() > 0) {
		return false; // We can't copy open databases.
	}

	String srcFullPath = fullPathForDatabase(srcOrigin, dbName, false);
	if (srcFullPath.isEmpty())
		return false;
	
	if (fileExists(srcFullPath)) {
		if (!dstTracker->hasEntryForOrigin(dstOrigin)) {
			// It doesn't matter what this is set to because once it's exceeded it will be
			// reset to the correct value.
			dstTracker->setQuota(dstOrigin, 1024);
		}
		String dstFullPath = dstTracker->fullPathForDatabase(dstOrigin, dbName, true);
		if (dstFullPath.isEmpty())
			return false;

		return CopyFile(srcFullPath.utf8().data(), dstFullPath.utf8().data());
	}
	else {
		// A database is referenced in our tracker database, but it doesn't exist on disk.
		// Clean this up by creating an empty file
		int fd = ::creat(srcFullPath.utf8().data(), O_CREAT|O_WRONLY|O_TRUNC);
		if (fd != -1) {
			::close(fd);
		}
		deleteDatabase(srcOrigin, dbName);
		return true;
	}
}

/**
 * Copy all databases from one origin to another in the specified tracker.
 *
 * @param srcOrigin The source origin containing databases to be copied.
 * @param dstOrigin The destination origin.
 * @param dstTracker The destination tracker. Can be this tracker if the origins don't match.
 *
 * @return true if successful, false if not.
 */
bool DatabaseTracker::copyOriginDatabases(SecurityOrigin* srcOrigin, SecurityOrigin* dstOrigin, DatabaseTracker* dstTracker)
{
	ASSERT(dstTracker != NULL);

	if (srcOrigin == NULL || dstOrigin == NULL) {
		return false;
	}

	//ASSERT(currentThread() == m_thread);
    openTrackerDatabase(false);
    if (!m_database.isOpen())
        return false;

    Vector<String> databaseNames;
    if (!databaseNamesForOrigin(srcOrigin, databaseNames)) {
        LOG_ERROR("Unable to retrieve list of database names for origin %s", srcOrigin->toString().ascii().data());
        return false;
    }

	bool success(true);
    for (unsigned i = 0; success && i < databaseNames.size(); ++i) {
		success = copyOriginDatabase(databaseNames[i], srcOrigin, dstOrigin, dstTracker);
		if (!success) {
			LOG_ERROR("Unable to copy database %s from origin %s to origin %s", databaseNames[i].ascii().data(),
					srcOrigin->toString().ascii().data(), dstOrigin->toString().ascii().data());
		}
    }

	return success;
}

/**
 * Copy all databases from this tracker to dstTracker. Any databases with identical
 * names in dstTracker will be overwritten by databases in this tracker.
 *
 * @note If there is an error then any previously successfully copied databases
 *       will be left intact.
 *
 * @return true if all copied successfully, false upon error. 
 */
bool DatabaseTracker::copyTrackerDatabases(DatabaseTracker* dstTracker, Vector<RefPtr<SecurityOrigin> >& origins)
{
	if (dstTracker == this)
		return false;

    MutexLocker lockDatabase(m_databaseGuard);
    //ASSERT(currentThread() == m_thread);
    openTrackerDatabase(true);
    if (!m_database.isOpen())
        return false;

    dstTracker->openTrackerDatabase(true);
    if (!dstTracker->m_database.isOpen())
        return false;

	{ // First make sure there is no open database.
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
        if (m_openDatabaseMap && !m_openDatabaseMap->isEmpty()) {
			return false;
		}
	}

	bool succeeded(true);
    for (unsigned i = 0; succeeded && i < origins.size(); ++i) {
		SecurityOrigin* orig = origins[i].get();
        succeeded = copyOriginDatabases(orig, orig, dstTracker);
	}

	return succeeded;
}

/**
 * Move all databases for all origins from the source directory to the destination directory.
 *
 * @param srcPath      The source tracker database path.
 * @param srcExtPath   The path to where the source tracker puts it's "external" media databases.
 * @param srcCondition The origin name SQL condition used to filter out the origin's to move. If
 *                     this string is empty then all origins are moved.
 * @param dstPath      The destination tracker database path.
 * @param dstExtPath   The path to where the destination tracker puts it's "external" media databases.
 *
 * @return true if move is successful, false if not. Upon error all successfully copied databases
 *         will be deleted. If successfull then all source databases will be deleted.
 */
bool DatabaseTracker::moveTrackerDatabases(const String& srcPath, const String& srcExtPath,
											const String& srcCondition, const String& dstPath,
											const String& dstExtPath)
{
    DatabaseTracker srcTracker(srcPath + DB_PATH_SEPARATOR + srcExtPath);
    DatabaseTracker dstTracker(dstPath + DB_PATH_SEPARATOR + dstExtPath);

	Vector<RefPtr<SecurityOrigin> > dbOrigins;
    srcTracker.origins(dbOrigins, srcCondition);

	bool successful = srcTracker.copyTrackerDatabases(&dstTracker, dbOrigins);
	if (successful) {
		srcTracker.deleteOrigins(dbOrigins);
	}
	else {
		dstTracker.deleteOrigins(dbOrigins);
	}

	return successful;
}

/**
 * Return the number of origins managed by this tracker.
 *
 * @param dirPath   The directory path containing the tracker database and managed databases.
 * @param condition The condition used when matching the origin name.
 */
int DatabaseTracker::numOriginsInTracker(const String& dirPath, const String& condition)
{
	DatabaseTracker	tracker(dirPath);

	// Do a filesystem check for the database because DatabaseTracker::orgins
	// will create the database if it doesn't exist.
	if (!fileExists(tracker.trackerDatabasePath()))
		return 0;

	Vector<RefPtr<SecurityOrigin> > dbOrigins;
    tracker.origins(dbOrigins, condition);

	return dbOrigins.size();
}

bool DatabaseTracker::startDatabaseDump(PassRefPtr<SecurityOrigin> passOrig, const String& dbName, 
										const String& dumpFile, PassRefPtr<DatabaseWatcher> passWatcher)
{
	RefPtr<DatabaseWatcher> watcher = passWatcher;
	RefPtr<SecurityOrigin> origin = passOrig;
	if (watcher == NULL || origin == NULL)
		return false;
	LOG(StorageAPI, "Starting backup of database %s-%s", origin->toString().ascii().data(), dbName.ascii().data());
	Vector<RefPtr<AbstractDatabase> > openDbs;

	// First look and see if the database is open
	{
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
        if (m_openDatabaseMap) {										// At least one open database
            DatabaseNameMap* nameMap = m_openDatabaseMap->get(origin);	// Get all db's for this origin
            if (nameMap && nameMap->size()) {
                DatabaseSet* databaseSet = nameMap->get(dbName);		// Get all matching db name supplied
                if (databaseSet && databaseSet->size()) {				// If got at least one.
                    DatabaseSet::const_iterator end = databaseSet->end();
                    for (DatabaseSet::const_iterator it = databaseSet->begin(); it != end; ++it) {
						ASSERT(*it != NULL);
                        openDbs.append(*it);
					}
                }
            }
        }
    }

	if (openDbs.size() > 1) {
		return false; // We only support backing up one database.
	}

	int statusCode(0);
	if (openDbs.size()) {
		ASSERT(openDbs.size() == 1);
		for (unsigned i = 0; i < openDbs.size(); ++i) {
			LOG(StorageAPI, "Dumping open database to %s", dumpFile.ascii().data());
			RefPtr<AbstractDatabase> db = openDbs[i];
			watcher->dumpStarted(0);
			statusCode = db->dump(dumpFile, watcher);	// dump calls dumpStopped
		}
	}
	else {
		// Backing up a closed database.
		watcher->dumpStarted(0);
		String fullPath = fullPathForDatabase(origin.get(), dbName, false);
		if (!fullPath.isEmpty()) {
			// guard the map which prevents this (or any) database from being 
			// opened while this backup takes place.
			LOG(StorageAPI, "Dumping closed database to %s", dumpFile.ascii().data());
			MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
			statusCode = Database::dump(fullPath, dumpFile);	
		}
		else {
			LOG_ERROR("Can't find database for %s-%s", origin->toString().utf8().data(), dumpFile.utf8().data());
			statusCode = ENOENT;
		}

		watcher->dumpStopped(statusCode);
	}

	return true;
}

bool DatabaseTracker::startDatabaseRestore(PassRefPtr<SecurityOrigin> passOrig, const String& dbName, 
										const String& dumpFile, PassRefPtr<DatabaseWatcher> watcher)
{
	RefPtr<SecurityOrigin> origin = passOrig;
	if (watcher == NULL || origin == NULL)
		return false;
	LOG(StorageAPI, "Starting restore of database %s-%s", origin->toString().ascii().data(), dbName.ascii().data());
	syslog(LOG_INFO, "Starting restore of database %s", origin->toString().utf8().data());
	Vector<RefPtr<AbstractDatabase> > openDbs;

	// First look and see if the database is open
	{
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
        if (m_openDatabaseMap) {										// At least one open database
            DatabaseNameMap* nameMap = m_openDatabaseMap->get(origin);	// Get all db's for this origin
            if (nameMap && nameMap->size()) {
                DatabaseSet* databaseSet = nameMap->get(dbName);		// Get all matching db name supplied
                if (databaseSet && databaseSet->size()) {				// If got at least one.
                    DatabaseSet::const_iterator end = databaseSet->end();
                    for (DatabaseSet::const_iterator it = databaseSet->begin(); it != end; ++it) {
						ASSERT(*it != NULL);
                        openDbs.append(*it);
					}
                }
            }
        }
    }

	watcher->restoreStarted(0);

	int statusCode(0);

	if (!hasEntryForOrigin(origin.get())) {
		// It doesn't matter what this is set to because once it's exceeded it will be
		// reset to the correct value.
		setQuota(origin.get(), 1024);
	}
	String fullPath = fullPathForDatabase(origin.get(), dbName, true /* create if it doesn't exist */);
	if (!fullPath.isEmpty()) {
		// guard the map which prevents this (or any) database from being
		// opened while this restore takes place.
		MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
		if (openDbs.isEmpty()) {
			statusCode = Database::restore(dumpFile, fullPath);	
		}
		else if (openDbs.size() == 1) {
			// Generally this is a *BAD* idea because there is no existing way for WebKit to inform
			// the page that it's databases have been modified so that they can respond. However 
			// we know that these db's are only restored when LunaSysMgr is running in "minimal
			// mode" and once this restoration has happened it will restart.
			syslog(LOG_INFO, "Restoring open database: %s", dbName.utf8().data());
			statusCode = openDbs[0]->restore(dumpFile);
		}
		else {
			syslog(LOG_ERR, "Can only restore db's for origins with zero or 1 open db");
			statusCode = EMFILE; // Really too many databases, but closest error code.
		}
	}
	else {
		ASSERT(false);
		statusCode = ENOENT;
	}
	
	watcher->restoreStopped(statusCode);

	return true;
}

/**
 * If this database doesn't exist then true is returned and mtime is zero.
 */
bool DatabaseTracker::getDatabaseModInfo(PassRefPtr<SecurityOrigin> passOrig, const String& dbName,
			time_t& mtime, int& numOpenMods)
{
	mtime = 0;
	numOpenMods = 0;

	RefPtr<SecurityOrigin> origin = passOrig;
	if (origin == NULL)
		return false;

	// Look and see if the database is open
	Vector<RefPtr<AbstractDatabase> > openDbs;
	{
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
        if (m_openDatabaseMap) {										// At least one open database
            DatabaseNameMap* nameMap = m_openDatabaseMap->get(origin);	// Get all db's for this origin
            if (nameMap && nameMap->size()) {
                DatabaseSet* databaseSet = nameMap->get(dbName);		// Get all matching db name supplied
                if (databaseSet && databaseSet->size()) {				// If got at least one.
                    DatabaseSet::const_iterator end = databaseSet->end();
                    for (DatabaseSet::const_iterator it = databaseSet->begin(); it != end; ++it) {
						ASSERT(*it != NULL);
                        openDbs.append(*it);
					}
                }
            }
        }
    }

	bool success = false;
	String fullPath = fullPathForDatabase(origin.get(), dbName, false /* Don't create */);
	if (!fullPath.isEmpty()) {
		struct stat sinfo;
		if (stat(fullPath.utf8().data(), &sinfo) == 0) {
			mtime = sinfo.st_mtime;
			success = true;
			if (openDbs.size() == 1) {
				RefPtr<AbstractDatabase> db = openDbs[0];
				success = (db->getTotalChanges(numOpenMods) == 0);
			}
		}
	}

	return success;
}

void DatabaseTracker::setDatabaseExtDirectoryPath(const String& path)
{
    MutexLocker lockDatabase(m_databaseGuard);
    //ASSERT(currentThread() == m_thread);
    ASSERT(!m_database.isOpen());
    m_databaseExtDirectoryPath = path.threadsafeCopy();
}


} // WebCore


#endif // ENABLE(DATABASE)

