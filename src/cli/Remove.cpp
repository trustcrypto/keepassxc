/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdlib>
#include <stdio.h>

#include "Remove.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QStringList>

#include "cli/TextStream.h"
#include "cli/Utils.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/Tools.h"

Remove::Remove()
{
    name = QString("rm");
    description = QString("Remove an entry from the database.");
}

Remove::~Remove()
{
}

int Remove::execute(const QStringList& arguments)
{
    TextStream errorTextStream(Utils::STDERR, QIODevice::WriteOnly);

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Remove an entry from the database."));
    parser.addPositionalArgument("database", QObject::tr("Path of the database."));
    parser.addOption(Command::QuietOption);
    parser.addOption(Command::KeyFileOption);
    parser.addPositionalArgument("entry", QObject::tr("Path of the entry to remove."));
    parser.addHelpOption();
    parser.process(arguments);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 2) {
        errorTextStream << parser.helpText().replace("keepassxc-cli", "keepassxc-cli rm");
        return EXIT_FAILURE;
    }

    auto db = Utils::unlockDatabase(args.at(0),
                                    parser.value(Command::KeyFileOption),
                                    parser.isSet(Command::QuietOption) ? Utils::DEVNULL : Utils::STDOUT,
                                    Utils::STDERR);
    if (!db) {
        return EXIT_FAILURE;
    }

    return removeEntry(db.data(), args.at(0), args.at(1), parser.isSet(Command::QuietOption));
}

int Remove::removeEntry(Database* database, const QString& databasePath, const QString& entryPath, bool quiet)
{
    TextStream outputTextStream(quiet ? Utils::DEVNULL : Utils::STDOUT, QIODevice::WriteOnly);
    TextStream errorTextStream(Utils::STDERR, QIODevice::WriteOnly);

    QPointer<Entry> entry = database->rootGroup()->findEntryByPath(entryPath);
    if (!entry) {
        errorTextStream << QObject::tr("Entry %1 not found.").arg(entryPath) << endl;
        return EXIT_FAILURE;
    }

    QString entryTitle = entry->title();
    bool recycled = true;
    auto* recycleBin = database->metadata()->recycleBin();
    if (!database->metadata()->recycleBinEnabled() || (recycleBin && recycleBin->findEntryByUuid(entry->uuid()))) {
        delete entry;
        recycled = false;
    } else {
        database->recycleEntry(entry);
    };

    QString errorMessage;
    if (!database->save(databasePath, &errorMessage, true, false)) {
        errorTextStream << QObject::tr("Unable to save database to file: %1").arg(errorMessage) << endl;
        return EXIT_FAILURE;
    }

    if (recycled) {
        outputTextStream << QObject::tr("Successfully recycled entry %1.").arg(entryTitle) << endl;
    } else {
        outputTextStream << QObject::tr("Successfully deleted entry %1.").arg(entryTitle) << endl;
    }

    return EXIT_SUCCESS;
}
