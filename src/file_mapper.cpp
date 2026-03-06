#include "file_mapper.h"
#include <QFileInfo>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
MappedFile::~MappedFile()
{
    close();
}

bool MappedFile::open(const QString &filePath)
{
    close();

    path = filePath;
    file.setFileName(filePath);

    if (!file.open(QIODevice::ReadOnly))
        return false;

    fileSize = static_cast<size_t>(file.size());
    if (fileSize == 0)
    {
        file.close();
        return true;   // empty file is valid
    }

    mapped = file.map(0, static_cast<qint64>(fileSize));
    if (!mapped)
    {
        file.close();
        return false;
    }

    buildLineIndex();
    return true;
}

void MappedFile::close()
{
    if (mapped)
    {
        file.unmap(mapped);
        mapped = nullptr;
    }
    file.close();
    fileSize = 0;
    lineOffsets.clear();
    path.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Build a vector of byte offsets, one per line (1-based indexing).
//  lineOffsets[0] = 0  (start of line 1)
//  lineOffsets[n] = offset of line n+1
// ─────────────────────────────────────────────────────────────────────────────
void MappedFile::buildLineIndex()
{
    lineOffsets.clear();
    lineOffsets.reserve(static_cast<int>(fileSize / 40));  // rough estimate

    lineOffsets.append(0);  // line 1 starts at offset 0

    const char *p   = reinterpret_cast<const char *>(mapped);
    const char *end = p + fileSize;

    while (p < end)
    {
        if (*p == '\n')
        {
            size_t offset = static_cast<size_t>(p + 1 - reinterpret_cast<const char *>(mapped));
            if (offset < fileSize)
                lineOffsets.append(offset);
        }
        ++p;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
int MappedFile::lineCount() const
{
    return lineOffsets.size();
}

size_t MappedFile::lineOffset(int lineNumber) const
{
    if (lineNumber < 1 || lineNumber > lineOffsets.size())
        return 0;
    return static_cast<size_t>(lineOffsets[lineNumber - 1]);
}

const char *MappedFile::lineStart(int lineNumber) const
{
    if (!mapped || lineNumber < 1 || lineNumber > lineOffsets.size())
        return nullptr;
    return reinterpret_cast<const char *>(mapped) + lineOffsets[lineNumber - 1];
}

int MappedFile::lineLength(int lineNumber) const
{
    if (!mapped || lineNumber < 1 || lineNumber > lineOffsets.size())
        return 0;

    size_t start = lineOffsets[lineNumber - 1];
    size_t end   = (lineNumber < lineOffsets.size())
                       ? static_cast<size_t>(lineOffsets[lineNumber])
                       : fileSize;

    // strip trailing \r\n
    int len = static_cast<int>(end - start);
    const char *p = reinterpret_cast<const char *>(mapped) + start;
    while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r'))
        --len;
    return len;
}

QByteArray MappedFile::lineBytes(int lineNumber) const
{
    const char *s = lineStart(lineNumber);
    int         n = lineLength(lineNumber);
    if (!s || n <= 0)
        return {};
    return QByteArray(s, n);
}

QString MappedFile::lineString(int lineNumber) const
{
    return QString::fromLatin1(lineBytes(lineNumber));
}

int MappedFile::offsetToLine(size_t byteOffset) const
{
    // Binary search in lineOffsets (sorted ascending)
    auto it = std::upper_bound(lineOffsets.begin(), lineOffsets.end(),
                               static_cast<size_t>(byteOffset));
    return static_cast<int>(it - lineOffsets.begin());  // 1-based
}
