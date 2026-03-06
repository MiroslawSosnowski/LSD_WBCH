#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  file_mapper.h  –  Layer 1: Memory-Mapped File Reader
//
//  Wraps Qt's QFile::map() (mmap on POSIX, MapViewOfFile on Windows).
//  Provides a zero-copy const char* view over a .k file plus line-index for
//  O(1) line→offset lookups.
// ═════════════════════════════════════════════════════════════════════════════
#include <QString>
#include <QFile>
#include <QVector>
#include <cstdint>

class MappedFile
{
public:
    MappedFile() = default;
    ~MappedFile();

    // Non-copyable (owns the mapping)
    MappedFile(const MappedFile &) = delete;
    MappedFile &operator=(const MappedFile &) = delete;

    // ── Open / close ─────────────────────────────────────────────────────────
    bool        open(const QString &path);
    void        close();
    bool        isOpen() const { return mapped != nullptr; }

    // ── Raw access ───────────────────────────────────────────────────────────
    const char *data()       const { return reinterpret_cast<const char *>(mapped); }
    size_t      size()       const { return fileSize; }
    QString     filePath()   const { return path; }

    // ── Line index (built on open) ────────────────────────────────────────────
    int         lineCount()                            const;
    const char *lineStart(int lineNumber)              const; // 1-based
    int         lineLength(int lineNumber)             const;
    QByteArray  lineBytes(int lineNumber)              const;
    QString     lineString(int lineNumber)             const;
    size_t      lineOffset(int lineNumber)             const; // byte offset
    int         offsetToLine(size_t byteOffset)        const; // binary search

private:
    void buildLineIndex();

    QFile       file;
    QString     path;
    uchar      *mapped    = nullptr;
    size_t      fileSize  = 0;
    QVector<size_t> lineOffsets; // lineOffsets[i] = byte offset of line i+1
};
