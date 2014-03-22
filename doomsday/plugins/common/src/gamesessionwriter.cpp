/** @file gamesessionwriter.cpp  Serializing game state to a saved session.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "gamesessionwriter.h"

#include "g_common.h"
#include "mapstatewriter.h"
#include "p_saveio.h"
#include <de/App>
#include <de/game/SavedSessionRepository>
#include <de/NativeFile>
#include <de/PackageFolder>
#include <de/Time>
#include <de/Writer>
#include <de/ZipArchive>

using namespace de;
using de::game::SavedSession;
using de::game::SessionMetadata;
using de::game::SavedSessionRepository;

DENG2_PIMPL_NOREF(GameSessionWriter)
{
    String repoPath; // Path to the saved session in the repository.

    inline SavedSessionRepository &repo() const
    {
        return G_SavedSessionRepository();
    }

    String composeInfo(SessionMetadata const &metadata) const
    {
        String info;
        QTextStream os(&info);
        os.setCodec("UTF-8");

        // Write header and misc info.
        Time now;
        os <<   "# Doomsday Engine saved game session package.\n#"
           << "\n# Generator: GameSessionWriter (libcommon)"
           << "\n# Generation Date: " + now.asDateTime().toString(Qt::SystemLocaleShortDate);

        // Write metadata.
        os << "\n\n" + metadata.asTextWithInfoSyntax() + "\n";

        return info;
    }
};

GameSessionWriter::GameSessionWriter(String repositoryPath) : d(new Instance())
{
    DENG2_ASSERT(repositoryPath != 0);
    d->repoPath = repositoryPath;
}

void GameSessionWriter::write(SessionMetadata const &metadata)
{
    LOG_AS("GameSessionWriter");
    LOG_RES_VERBOSE("Serializing game state to \"/savegames/%s\"...") << d->repoPath;

    // Write the Info file for this .save package.
    ZipArchive arch;
    arch.add("Info", d->composeInfo(metadata).toUtf8());

#if __JHEXEN__
    // Serialize the world ACScript state.
    {
        Block worldACScriptData;
        de::Writer writer(worldACScriptData);
        Game_ACScriptInterpreter().writeWorldScriptData(writer);
        arch.add("ACScriptState", worldACScriptData);
    }
#endif

    // Serialize the current map state.
    {
        String const mapUriStr(Str_Text(Uri_Compose(gameMapUri)));
        Block mapStateData;
        SV_OpenFileForWrite(mapStateData);
        writer_s *writer = SV_NewWriter();
        MapStateWriter().write(writer);
        arch.add(Path("maps") / mapUriStr + "State", mapStateData);
        Writer_Delete(writer);
    }

    File &outFile = d->repo().folder().replaceFile(d->repoPath + ".save");
    de::Writer(outFile) << arch;
    outFile.setMode(File::ReadOnly);
    outFile.parent()->populate(Folder::PopulateOnlyThisFolder);
    LOG_RES_MSG("Wrote ") << outFile.as<NativeFile>().nativePath().pretty();

    // Generate a saved session for the db.
    //SavedSession *session = new SavedSession(d->repoPath);

    SavedSession &session = d->repo().folder().locate<SavedSession>(d->repoPath + ".save");
    session.cacheMetadata(metadata); // Avoid immediately reopening the .save package.
    G_SavedSessionRepository().add(d->repoPath, &session);
}
