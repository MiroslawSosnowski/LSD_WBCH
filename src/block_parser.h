#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  block_parser.h  –  Layer 3: Intelligent Incremental Parser
//
//  Strategy:
//    1. scan()  – single mmap pass, records BlockEntry list only (O(n) I/O)
//    2. parseBlock(block) – demand parse one block's data into ModelDatabase
//    3. parseAll()  – convenience: parse every block
//
//  Include files are resolved recursively during scan().
// ═════════════════════════════════════════════════════════════════════════════
#include "model_index.h"
#include "model_database.h"
#include "file_mapper.h"
#include <QString>
#include <QMap>
#include <functional>

class BlockParser
{
public:
    using ProgressCb = std::function<void(int /*percent*/, const QString &/*msg*/)>;

    explicit BlockParser(ModelDatabase &db);

    // ── Phase 1: scan ─────────────────────────────────────────────────────────
    // Open file, build block index, recurse into *INCLUDE files.
    bool scan(const QString &rootFilePath, ProgressCb cb = {});

    // ── Phase 2: demand parse ─────────────────────────────────────────────────
    void parseBlock(BlockEntry &block);
    void parseAll(ProgressCb cb = {});

    // ── Helpers ───────────────────────────────────────────────────────────────
    static BlockType  classifyKeyword(const QString &kw);
    static ElemType   classifyElement(const QString &subtype);

private:
    ModelDatabase               &db;
    QMap<int, MappedFile *>      mappers;   // fileId → owned MappedFile

    MappedFile *mapperFor(int fileId);
    void        scanFile(int fileId, const QString &path, ProgressCb &cb);
    int         resolveInclude(const QString &includePath, int parentId, int atLine, ProgressCb &cb);

    // Block-type parsers (invoked during demand-parse)
    void parseNodeBlock    (BlockEntry &, MappedFile &);
    void parseElementBlock (BlockEntry &, MappedFile &);
    void parsePartBlock    (BlockEntry &, MappedFile &);
    void parseMatBlock     (BlockEntry &, MappedFile &);
    void parseSectionBlock (BlockEntry &, MappedFile &);
    void parseContactBlock (BlockEntry &, MappedFile &);
    void parseSetBlock     (BlockEntry &, MappedFile &);

    // Fixed-width LS-DYNA field splitter
    static QStringList splitFixed(const QByteArray &line, int fieldWidth = 8);
    static QStringList splitFree (const QByteArray &line);
};
