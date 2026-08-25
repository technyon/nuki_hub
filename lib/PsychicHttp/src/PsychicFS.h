// src/PsychicFS.h — INTERNAL USE ONLY
// Do NOT include this in user-facing headers or sketches.
//
// psychic::FS and psychic::File provide a minimal uniform interface over:
//   — Arduino fs::FS / fs::File   (ARDUINO builds, zero-overhead delegation)
//   — POSIX fopen/fstat/fread     (native ESP-IDF builds)
//
// The interface covers exactly the 8 operations used by PsychicHttp:
//   FS::open(), FS::exists()
//   File::operator bool(), File::isDirectory(), File::size(),
//   File::name(), File::readBytes(), File::close()
//
// On Arduino, psychic::File's operator bool() returns
//   (bool)fs::File && !isDirectory()
// which absorbs the FILE_IS_REAL(f) macro semantics — that macro is not
// needed anywhere once the shim is in use.

#pragma once

#ifdef ARDUINO

  #include <FS.h>

namespace psychic
{

  class File
  {
      mutable fs::File _f; // mutable: Arduino fs::File lacks const qualifiers on isDirectory()

    public:
      File() = default;
      explicit File(fs::File f) : _f(f) {}

      // true only when open AND not a directory — absorbs FILE_IS_REAL semantics
      explicit operator bool() const { return (bool)_f && !_f.isDirectory(); }
      bool isDirectory() const { return _f.isDirectory(); }
      size_t size() const { return _f.size(); }
      const char* name() const { return _f.name(); }
      size_t readBytes(char* buf, size_t len) { return _f.readBytes(buf, len); }
      void close() { _f.close(); }
  };

  class FS
  {
      fs::FS* _fs = nullptr;

    public:
      FS() = default;
      explicit FS(fs::FS& fs) : _fs(&fs) {}

      File open(const char* path, const char* mode = "r")
      {
        if (!_fs)
          return File{};
        return File{_fs->open(path, mode)};
      }

      bool exists(const char* path)
      {
        if (!_fs)
          return false;
        return _fs->exists(path);
      }
  };

} // namespace psychic

#else // native ESP-IDF — POSIX VFS

  #include <stdio.h>
  #include <string>
  #include <sys/stat.h>

namespace psychic
{

  // Non-copyable, moveable POSIX file wrapper.
  class File
  {
      FILE* _fp = nullptr;
      size_t _size = 0;
      std::string _path;

    public:
      File() = default;
      File(FILE* fp, size_t size, const char* path) : _fp(fp), _size(size), _path(path ? path : "") {}

      // non-copyable — owns the FILE*
      File(const File&) = delete;
      File& operator=(const File&) = delete;

      // moveable
      File(File&& o) noexcept : _fp(o._fp), _size(o._size), _path(std::move(o._path)) { o._fp = nullptr; }
      File& operator=(File&& o) noexcept
      {
        if (this != &o) {
          close();
          _fp = o._fp;
          _size = o._size;
          _path = std::move(o._path);
          o._fp = nullptr;
        }
        return *this;
      }

      ~File() { close(); }

      // true only when a regular file was successfully opened
      explicit operator bool() const { return _fp != nullptr; }
      // FS::open() returns an empty File{} for directories, so this is always false
      bool isDirectory() const { return false; }
      size_t size() const { return _size; }
      const char* name() const { return _path.c_str(); }
      size_t readBytes(char* buf, size_t len) { return _fp ? fread(buf, 1, len, _fp) : 0; }
      void close()
      {
        if (_fp) {
          fclose(_fp);
          _fp = nullptr;
        }
      }
  };

  // Stateless POSIX filesystem — users must have mounted their VFS partition
  // (e.g. via esp_vfs_littlefs_register()) before calling open() or exists().
  class FS
  {
    public:
      File open(const char* path, const char* mode = "r")
      {
        struct stat st;
        if (stat(path, &st) != 0)
          return File{};
        if (S_ISDIR(st.st_mode))
          return File{}; // directories → empty (false) File
        FILE* fp = fopen(path, mode);
        return fp ? File{fp, (size_t)st.st_size, path} : File{};
      }

      bool exists(const char* path)
      {
        struct stat st;
        return stat(path, &st) == 0;
      }
  };

} // namespace psychic

#endif // ARDUINO / native IDF
