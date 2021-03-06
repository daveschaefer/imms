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
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <algorithm>

#include "fetcher.h"
#include "strmanip.h"
#include "md5digest.h"
#include "immsutil.h"

using std::endl;
using std::cerr;

InfoFetcher::SongData::SongData(int _position, const string &_path)
    : Song(_path), rating(0), position(_position),
      relation(0), acoustic(0),
      last_played(0), identified(false) {
}

bool InfoFetcher::SongData::get_song_from_playlist()
{
    *static_cast<Song*>(this) = PlaylistDb::playlist_id_from_item(position);
    return isok();
}

bool InfoFetcher::identify_playlist_item(int pos)
{
    Song song(PlaylistDb::get_item_from_playlist(pos));
    int uid = -2;
    if (song.isok())
        uid = song.get_uid();

    PlaylistDb::playlist_update_identity(pos, uid);
    return song.isok();
}

bool InfoFetcher::fetch_song_info(SongData &data)
{
    if (access(data.get_path().c_str(), R_OK))
        return false;

    AutoTransaction at;
    if (!data.get_song_from_playlist())
    {
        if (!identify_playlist_item(data.position))
            return false;
        data.get_song_from_playlist();
    }
    at.commit();

    if (!data.isok()) {
        LOG(INFO) << "Fetch failed: " << data.get_path() << endl;
        return false;
    }

    StringPair info = data.get_info();

    const string &artist = info.first;
    const string &title = info.second;

    if (artist != "" && title != "")
        data.identified = true;
    else if ((data.identified = parse_song_info(data, info)))
        data.set_info(info);

    data.rating = data.get_rating();

#if defined(DEBUG) && 0
    cerr << "path:\t" << data.get_path() << endl;
    cerr << "artist:\t" << artist << endl;
    cerr << "title:\t" << title << endl;
#endif

    data.last_played = time(0) - data.get_last();

    return true;
}

bool InfoFetcher::parse_song_info(const SongData &data, StringPair &info)
{

#define REMIXCLUES "rmx|mix|[^a-z]version|edit|original|remaster|" \
    "cut|instrumental|extended"
#define BADARTIST "artist|^va$|various|collection|^misc"

    //////////////////////////////////////////////
    // Initialize

    bool identified = false;
    bool parser_confident, artist_confirmed = false;

    string path = data.get_path();
    string &artist = info.first;
    string &title = info.second;

    string tag_artist, tag_album, tag_title;
    data.get_tag_info(tag_artist, tag_album, tag_title);

    artist = string_normalize(tag_artist);
    tag_artist = artist;

    StringPair fm = get_simplified_filename_mask(path);
    fm.second = string_normalize(fm.second);

    list<string> file_parts;
    parser_confident = imms_magic_parse_filename(file_parts, fm.first);

    // Can't normalize before we pass it to magic parse
    fm.first = string_normalize(fm.first);

    list<string> path_parts;
    imms_magic_parse_path(path_parts, path_get_dirname(path));

#if defined(DEBUG) && 0
    cerr << "path parts: " << endl;
    for (list<string>::iterator i = path_parts.begin();
            i != path_parts.end(); ++i)
    {
        cerr << " >> " << *i << endl;
    }
    cerr << "name parts: " << endl;
    for (list<string>::iterator i = file_parts.begin();
            i != file_parts.end(); ++i)
    {
        cerr << " >> " << *i << endl;
    }
#endif

    //////////////////////////////////////////////
    // Try to identify the artist
    
    bool bad_tag_artist = tag_artist == "" || Regexx(tag_artist, BADARTIST);

    // See if we can recognize any of the path parts
    if (file_parts.size() > 1)
    {
        for (list<string>::iterator i = file_parts.begin();
                !artist_confirmed && i != file_parts.end(); ++i)
        {
            if (ImmsDb::check_artist(*i)
                    || (!bad_tag_artist && string_like(*i, tag_artist, 4)))
            {
                artist_confirmed = true;
                artist = *i;
            }
        }
    } 

    // See if any of the directories look familiar
    string outermost = "";
    for (list<string>::iterator i = path_parts.begin();
            !artist_confirmed && i != path_parts.end(); ++i)
    {
        if (Regexx(*i, BADARTIST))
            continue;
        if ((artist_confirmed =
                (ImmsDb::check_artist(*i) || string_like(*i, artist, 4))))
            artist = *i;
        // And while we are at it find the uppermost related directory
        else if (outermost == "" && fm.first.find(*i) != string::npos)
            outermost = *i;
    }

    // The path part that is also the outermost directory might be the artist
    if (!artist_confirmed && outermost != "" && fm.second.find(outermost) < 2)
    {
        artist = outermost;
        artist_confirmed = true;
    }

    if (!artist_confirmed && parser_confident && file_parts.size() > 1)
    {
        artist = file_parts.front();
        artist_confirmed = true;
    }

    // Give the tag artist an opportunity to override the guessed artist
    if (!bad_tag_artist && !string_like(tag_artist, artist, 4))
    {
        if (ImmsDb::check_artist(tag_artist)
                || fm.first.find(tag_artist) != string::npos)
        {
            artist = tag_artist;
            artist_confirmed = true;
        }
    }

    // If we still don't know what to do - give up
    if (!artist_confirmed)
        return identified;

    // Erase the first occurrence of the artist
    list<string>::iterator i = find(file_parts.begin(), file_parts.end(),
            artist);
    if (i != file_parts.end())
        file_parts.erase(i);

    //////////////////////////////////////////////
    // By this point the artist had to have been confirmed
    // Try (not very hard) to identify the album

    string album = album_filter(tag_album);
    string directory = album_filter(path_parts.back());
    if (album == "" || Regexx(directory, album))
        album = directory;

    //////////////////////////////////////////////
    // Try to identify the title

    tag_title = string_tolower(tag_title);
    title = title_filter(tag_title);
    // If we know the title and were only missing the artist
    if (title != "" && ImmsDb::check_title(artist, title))
        return true;

    if ((identified = Regexx(title, album + ".*(" REMIXCLUES ")")))
        title = album;
    else if ((identified = Regexx(fm.first, album + "$")))
        title = album;

    // Can recognize the title by matching filename parts to the database?
    for (list<string>::reverse_iterator i = file_parts.rbegin();
            i != file_parts.rend(); ++i)
    {
        if (ImmsDb::check_title(artist, *i)
                || string_like(*i, title, 4)
                || Regexx(title, string("^") + *i))
        {
            title = *i;
            return true;
        }
        else if (Regexx(*i, album + ".*(" REMIXCLUES ")"))
        {
            title = album;
            return true;
        }
        else if (title != "" && Regexx(*i, string("^") + title))
            return true;
    }

    if (identified)
        return identified; 

    // See if we can trust the tag title enough to just default to it
    if (title != "" && !Regexx(title, "(" REMIXCLUES "|^track$|^title$)")
            && tag_artist != "" && !Regexx(tag_artist, BADARTIST)
            && string_like(tag_title, title, 6))
        return true;

    // Filter things that are probably not the title
    for (list<string>::iterator i = file_parts.begin();
            i != file_parts.end(); ++i)
    {
        if (Regexx(*i, "(" REMIXCLUES "|^track$|^title$)"))
            *i = "###";
    }

    file_parts.remove("###");

    // Default to the right most non-suspicious entry as the title
    if (parser_confident && file_parts.size())
    {
        title = file_parts.back();
        return true;
    }
    // nothing left. maybe a remix album??
    if (!file_parts.size() && ImmsDb::check_title(artist, album))
    {
        title = album;
        return true;
    }
    return identified;
}
