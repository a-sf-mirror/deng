/** @file savedsession.h  Saved (game) session.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG2_SAVEDSESSION_H
#define LIBDENG2_SAVEDSESSION_H

#include "../Error"
#include "../Observers"
#include "../PackageFolder"
#include "../Path"
#include "../Record"
#include "../String"

namespace de {
namespace game {

class MapStateReader;

/**
 * Specialized PackageFolder that hosts a serialized game session.

 * @ingroup game
 */
class DENG2_PUBLIC SavedSession : public PackageFolder
{
public:
    /// Notified whenever the cached metadata of the saved session changes.
    DENG2_DEFINE_AUDIENCE2(MetadataChange, void savedSessionMetadataChanged(SavedSession &session))

    /**
     * Session metadata.
     */
    class DENG2_PUBLIC Metadata : public Record
    {
    public:
        /**
         * Parses metadata in Info syntax from a text string.
         *
         * @param source  Source string to be parsed.
         */
        void parse(String const &source);

        /**
         * Generates a textual representation of the session metadata with Info syntax.
         *
         * See the Doomsday Wiki for an example of the syntax:
         * http://dengine.net/dew/index.php?title=Info
         *
         * @todo Use a more generic Record => Info conversion logic.
         */
        String asTextWithInfoSyntax() const;
    };

public:
    SavedSession(File &sourceArchiveFile, String const &name = "");

    virtual ~SavedSession();

    /// @todo remove me
    inline String repoPath() const {
        return parent()->name() / name().fileNameWithoutExtension();
    }

    /**
     * Composes a human-friendly, styled, textual description of the saved session.
     */
    String styledDescription() const;

    /**
     * Attempt to locate the file package for the saved session from the repository.
     *
     * @return  The file package if found. Ownership is unaffected.
     */
    //PackageFolder &locateFile();
    //PackageFolder const &locateFile() const;

    /**
     * Attempt to update the status of the saved session from the file package in the repository.
     * If the file path is invalid, unreachable, or the package is not recognized then the saved
     * session is returned to a valid but non-loadable state.
     */
    void readMetadata();

    /**
     * Replace the file package in the repository with a copy of that associated with the @a source
     * saved session. The copied file package is named according to the @ref path() of "this" saved
     * session.
     *
     * @param source  SavedSession to copy the file package from.
     *
     * @throws MissingFileError  If no source file package is found.
     */
    //void copyFile(SavedSession const &source);

    /**
     * Removes the file package for the saved session from the repository (if configured).
     */
    //void removeFile();

    /**
     * Provides read-only access to a copy of the deserialized saved session metadata.
     *
     * @see replaceMetadata()
     */
    Metadata const &metadata() const;

    /**
     * Replace the cached metadata with a copy of @a original. Note that this will @em not alter
     * the .save package in the repository. The MetadataChange audience is notified.
     *
     * @param copied  Replacement Metadata. A copy is made.
     */
    void cacheMetadata(Metadata const &copied);

    /**
     * Determines whether a serialized map state exists for the saved session.
     *
     * @param mapUri  Unique map identifier.
     */
    bool hasMapState(String mapUriStr) const;

private:
    DENG2_PRIVATE(d)
};

typedef SavedSession::Metadata SessionMetadata;

} // namespace game
} // namespace de

#endif // LIBDENG2_SAVEDSESSION_H
