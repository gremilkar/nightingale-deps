/***************************************************************************
    copyright            : (C) 2002, 2003 by Scott Wheeler
    email                : wheeler@kde.org
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it  under the terms of the GNU Lesser General Public License version  *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  *
 *   USA                                                                   *
 ***************************************************************************/

#include "tlocalfileio.h"
#include "tstring.h"
#include "tdebug.h"

#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
# include <wchar.h>
# include <windows.h>
# include <io.h>
# define ftruncate _chsize
#else
# include <unistd.h>
#endif

#ifndef R_OK
# define R_OK 4
#endif
#ifndef W_OK
# define W_OK 2
#endif

using namespace TagLib;

#ifdef _WIN32

typedef FileName FileNameHandle;

#else

struct FileNameHandle : public std::string
{
  FileNameHandle(FileName name) : std::string(name) {}
  operator FileName () const { return c_str(); }
};

#endif

class LocalFileIO::LocalFileIOPrivate
{
public:
  LocalFileIOPrivate(FileName fileName) :
    file(NULL),
    name(fileName),
    readOnly(true),
    valid(true),
    size(0)
    {}

  FILE *file;
  FileNameHandle name;
  bool readOnly;
  bool valid;
  ulong size;
  static const uint bufferSize = 1024;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

LocalFileIO::LocalFileIO(FileName file)
{
  d = new LocalFileIOPrivate(file);

  d->readOnly = !isWritable(file);
#ifdef _WIN32
  if(wcslen((const wchar_t *) file) > 0) {
    d->file = _wfopen(file, d->readOnly ? L"rb" : L"rb+");
  }
#endif
  if (!d->file)
    d->file = fopen(file, d->readOnly ? "rb" : "rb+");

  if(!d->file)
    debug("Could not open file " + String((const char *) file));
}

LocalFileIO::~LocalFileIO()
{
  if(d->file)
    fclose(d->file);
  delete d;
}

FileName LocalFileIO::name() const
{
  return d->name;
}

ByteVector LocalFileIO::readBlock(ulong length)
{
  if(!d->file) {
    debug("LocalFileIO::readBlock() -- Invalid File");
    return ByteVector::null;
  }

  if(length > LocalFileIOPrivate::bufferSize &&
     length > ulong(LocalFileIO::length()))
  {
    length = LocalFileIO::length();
  }

  ByteVector v(static_cast<uint>(length));
  const int count = fread(v.data(), sizeof(char), length, d->file);
  v.resize(count);
  return v;
}

void LocalFileIO::writeBlock(const ByteVector &data)
{
  if(!d->file)
    return;

  if(d->readOnly) {
    debug("LocalFileIO::writeBlock() -- attempted to write to a file that is not writable");
    return;
  }

  fwrite(data.data(), sizeof(char), data.size(), d->file);
}

void LocalFileIO::insert(const ByteVector &data, ulong start, ulong replace)
{
  if(!d->file)
    return;

  if(data.size() == replace) {
    seek(start);
    writeBlock(data);
    return;
  }
  else if(data.size() < replace) {
      seek(start);
      writeBlock(data);
      removeBlock(start + data.size(), replace - data.size());
      return;
  }

  // Woohoo!  Faster (about 20%) than id3lib at last.  I had to get hardcore
  // and avoid TagLib's high level API for rendering just copying parts of
  // the file that don't contain tag data.
  //
  // Now I'll explain the steps in this ugliness:

  // First, make sure that we're working with a buffer that is longer than
  // the *differnce* in the tag sizes.  We want to avoid overwriting parts
  // that aren't yet in memory, so this is necessary.

  ulong bufferLength = bufferSize();
  while(data.size() - replace > bufferLength)
    bufferLength += bufferSize();

  // Set where to start the reading and writing.

  long readPosition = start + replace;
  long writePosition = start;

  ByteVector buffer;
  ByteVector aboutToOverwrite(static_cast<uint>(bufferLength));

  // This is basically a special case of the loop below.  Here we're just
  // doing the same steps as below, but since we aren't using the same buffer
  // size -- instead we're using the tag size -- this has to be handled as a
  // special case.  We're also using LocalFileIO::writeBlock() just for the tag.
  // That's a bit slower than using char *'s so, we're only doing it here.

  seek(readPosition);
  int bytesRead = fread(aboutToOverwrite.data(), sizeof(char), bufferLength, d->file);
  readPosition += bufferLength;

  seek(writePosition);
  writeBlock(data);
  writePosition += data.size();

  buffer = aboutToOverwrite;
 
  // In case we've already reached the end of file...

  buffer.resize(bytesRead);

  // Ok, here's the main loop.  We want to loop until the read fails, which
  // means that we hit the end of the file.
 
  while(!buffer.isEmpty()) {

    // Seek to the current read position and read the data that we're about
    // to overwrite.  Appropriately increment the readPosition.

    seek(readPosition);
    bytesRead = fread(aboutToOverwrite.data(), sizeof(char), bufferLength, d->file);
    aboutToOverwrite.resize(bytesRead);
    readPosition += bufferLength;

    // Check to see if we just read the last block.  We need to call clear()
    // if we did so that the last write succeeds.

    if(ulong(bytesRead) < bufferLength)
      clear();

    // Seek to the write position and write our buffer.  Increment the
    // writePosition.

    seek(writePosition);
    fwrite(buffer.data(), sizeof(char), buffer.size(), d->file);
    writePosition += buffer.size();

    // Make the current buffer the data that we read in the beginning.

    buffer = aboutToOverwrite;

    // Again, we need this for the last write.  We don't want to write garbage
    // at the end of our file, so we need to set the buffer size to the amount
    // that we actually read.

    bufferLength = bytesRead;
  }
}

void LocalFileIO::removeBlock(ulong start, ulong length)
{
  if(!d->file)
    return;

  ulong bufferLength = bufferSize();

  long readPosition = start + length;
  long writePosition = start;

  ByteVector buffer(static_cast<uint>(bufferLength));

  ulong bytesRead = true;

  while(bytesRead != 0) {
    seek(readPosition);
    bytesRead = fread(buffer.data(), sizeof(char), bufferLength, d->file);
    buffer.resize(bytesRead);
    readPosition += bytesRead;

    // Check to see if we just read the last block.  We need to call clear()
    // if we did so that the last write succeeds.

    if(bytesRead < bufferLength)
      clear();

    seek(writePosition);
    fwrite(buffer.data(), sizeof(char), bytesRead, d->file);
    writePosition += bytesRead;
  }
  truncate(writePosition);
}

bool LocalFileIO::readOnly() const
{
  return d->readOnly;
}

bool LocalFileIO::isReadable()
{
  return isReadable(name());
}

bool LocalFileIO::isReadable(FileName file)
{
#ifdef _MSC_VER
  return _waccess(file, R_OK) == 0;
#else
  return access(file, R_OK) == 0;
#endif
}

bool LocalFileIO::isOpen() const
{
  return d->file;
}

int LocalFileIO::seek(long offset, Position p)
{
  if(!d->file) {
    debug("LocalFileIO::seek() -- trying to seek in a file that isn't opened.");
    return -1;
  }

  switch(p) {
  case Beginning:
    fseek(d->file, offset, SEEK_SET);
    break;
  case Current:
    fseek(d->file, offset, SEEK_CUR);
    break;
  case End:
    fseek(d->file, offset, SEEK_END);
    break;
  }

  return 0;
}

void LocalFileIO::clear()
{
  clearerr(d->file);
}

long LocalFileIO::tell() const
{
  return ftell(d->file);
}

long LocalFileIO::length()
{
  // Do some caching in case we do multiple calls.

  if(d->size > 0)
    return d->size;

  if(!d->file)
    return 0;

  long curpos = tell();
  
  seek(0, End);
  long endpos = tell();
  
  seek(curpos, Beginning);

  d->size = endpos;
  return endpos;
}

bool LocalFileIO::isWritable()
{
  return isWritable(name());
}

bool LocalFileIO::isWritable(FileName file)
{
#ifdef _MSC_VER
  return _waccess(file, W_OK) == 0;
#else
  return access(file, W_OK) == 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////
// protected members
////////////////////////////////////////////////////////////////////////////////

void LocalFileIO::truncate(long length)
{
#ifdef _MSC_VER
  _chsize(_fileno(d->file), length);
#else
  ftruncate(fileno(d->file), length);
#endif
}

TagLib::uint LocalFileIO::bufferSize()
{
  return LocalFileIOPrivate::bufferSize;
}