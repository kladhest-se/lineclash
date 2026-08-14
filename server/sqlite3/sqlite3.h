/* sqlite3.h — minimal stub for LineClash server auth module.
 *
 * This file provides the minimal SQLite3 declarations used by auth.c.
 * The actual implementation comes from your system's libsqlite3:
 *
 *   macOS:   already included in the SDK (no action needed)
 *   Linux:   sudo apt install libsqlite3-dev     (Debian/Ubuntu)
 *            sudo dnf install sqlite-devel        (Fedora/RHEL)
 *   Windows: download sqlite-dll + sqlite3.h from https://sqlite.org/download.html
 *
 * If the full sqlite3.h is already on your include path, this file
 * is not used (the Makefile puts -Isqlite3 after system include paths
 * so the system header takes priority when available).
 */
#ifndef SQLITE3_H
#define SQLITE3_H
#include <stddef.h>

#define SQLITE_OK     0
#define SQLITE_ERROR  1
#define SQLITE_ROW  100
#define SQLITE_DONE 101

typedef struct sqlite3       sqlite3;
typedef struct sqlite3_stmt  sqlite3_stmt;
typedef long long            sqlite3_int64;

int           sqlite3_open(const char *filename, sqlite3 **ppDb);
int           sqlite3_close(sqlite3 *db);
int           sqlite3_exec(sqlite3 *db, const char *sql,
                           int (*cb)(void*,int,char**,char**),
                           void *arg, char **errmsg);
void          sqlite3_free(void *p);
int           sqlite3_prepare_v2(sqlite3 *db, const char *sql, int nByte,
                                 sqlite3_stmt **ppStmt, const char **pzTail);
int           sqlite3_step(sqlite3_stmt *pStmt);
int           sqlite3_finalize(sqlite3_stmt *pStmt);
int           sqlite3_bind_text(sqlite3_stmt*, int, const char*, int, void(*)(void*));
int           sqlite3_bind_int(sqlite3_stmt*, int, int);
int           sqlite3_bind_int64(sqlite3_stmt*, int, sqlite3_int64);
const unsigned char *sqlite3_column_text(sqlite3_stmt*, int iCol);
int           sqlite3_column_int(sqlite3_stmt*, int iCol);
sqlite3_int64 sqlite3_last_insert_rowid(sqlite3 *db);
const char   *sqlite3_errmsg(sqlite3 *db);

#define SQLITE_STATIC    ((void(*)(void*))0)
#define SQLITE_TRANSIENT ((void(*)(void*))(-1))
#endif /* SQLITE3_H */
