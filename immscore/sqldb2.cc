/*
 IMMS: Intelligent Multimedia Management System
 Copyright (C) 2001-2009 Michael Grigoriev

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/
#include <iostream>
#include <sstream>

#include <unistd.h>
#include <sys/stat.h>

#include <sqlite3.h>

#include "strmanip.h"
#include "sqldb2.h"
#include "immsutil.h"

using std::endl;
using std::cerr;
using std::ostringstream;

// Fuzzy compare function using levenshtein string distance
static void fuzzy_like(sqlite3_context *context, int argc, sqlite3_value** val)
{
    if (argc != 2)
        throw SQLException("fuzzy_like", "argc != 2");
    sqlite3_result_int(context, string_like(
                (char*)sqlite3_value_text(val[0]),
                (char*)sqlite3_value_text(val[1]), 4));
}

extern sqlite3 *db();

SqlDb::SqlDb()
    : correlations(new AttachedDatabase()), acoustic(new AttachedDatabase())
{
    if (!access(get_imms_root("imms.db").c_str(), R_OK)
            && access(get_imms_root("imms2.db").c_str(), F_OK))
    {
        cerr << string(60, '*') << endl;
        cerr << "Old database format detected, "
            "will attempt an upgrade..." << endl;
        ostringstream command;
        command << "sqlite " << get_imms_root("imms.db")
            << " .dump | sqlite3 " << get_imms_root("imms2.db") << endl;
        cerr << "Running: " << command.str() << endl;
        system(command.str().c_str());
        cerr << "If you see errors above verify that you have *both*"
            " sqlite 2.8.x" << endl;
        cerr << "and 3.0.x installed and rerun the command by hand." << endl;
        cerr << string(60, '*') << endl;
    }

    dbcon.open(get_imms_root("imms2.db"));
    sqlite3_create_function(db(), "similar", 2, 1, 0, fuzzy_like, 0, 0);

    correlations->attach(get_imms_root("imms.correlations.db"), "C");
    acoustic->attach(get_imms_root("imms.acoustic.db"), "A");
}

SqlDb::~SqlDb()
{
    close_database();
}

void SqlDb::close_database()
{
    correlations.release();
    acoustic.release();
    dbcon.close();
}

int SqlDb::changes()
{
    return db() ? sqlite3_changes(db()) : 0;
}
