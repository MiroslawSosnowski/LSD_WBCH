#include "block_parser.h"
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>

// ─────────────────────────────────────────────────────────────────────────────
BlockParser::BlockParser(ModelDatabase &db) : db(db) {}

// ─────────────────────────────────────────────────────────────────────────────
//  scan()  – public entry point
// ─────────────────────────────────────────────────────────────────────────────
bool BlockParser::scan(const QString &rootFilePath, ProgressCb cb)
{
    db.clear();
    qDeleteAll(mappers);
    mappers.clear();

    int rootId = db.registerFile(rootFilePath);
    scanFile(rootId, rootFilePath, cb);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  scanFile()  – first-pass keyword scanner for one file
// ─────────────────────────────────────────────────────────────────────────────
void BlockParser::scanFile(int fileId, const QString &filePath, ProgressCb &cb)
{
    MappedFile *mf = mapperFor(fileId);
    if (!mf || !mf->isOpen())
        return;

    if (cb) cb(0, "Scanning: " + QFileInfo(filePath).fileName());

    const int nLines = mf->lineCount();
    BlockEntry current;
    current.fileId = fileId;
    bool inBlock   = false;

    auto closeBlock = [&](int endLine) {
        if (inBlock && current.type != BlockType::UNKNOWN)
        {
            current.endLine = endLine;
            current.endByte = mf->lineOffset(endLine);
            db.blocks.push_back(current);
        }
        inBlock = false;
    };

    for (int ln = 1; ln <= nLines; ++ln)
    {
        const QString line = mf->lineString(ln).trimmed();

        if (line.isEmpty() || line.startsWith('$'))
            continue;

        if (line.startsWith('*'))
        {
            closeBlock(ln - 1);

            const QString kw = line.split(QRegularExpression(R"(\s+)"),
                                          Qt::SkipEmptyParts)
                                   .first().toUpper();

            current           = BlockEntry{};
            current.fileId    = fileId;
            current.keyword   = kw.toStdString();
            current.startLine = ln;
            current.startByte = mf->lineOffset(ln);
            current.type      = classifyKeyword(kw);
            current.parsed    = false;
            inBlock           = true;

            // ── Subtype extraction ────────────────────────────────────────────
            // *ELEMENT_SHELL → "SHELL"  |  *MAT_024 → "024"
            int under = kw.indexOf('_', 1);
            if (under != -1)
                current.subtype = kw.mid(under + 1).toStdString();

            // ── Resolve *INCLUDE immediately (need the path on same/next line)
            if (current.type == BlockType::INCLUDE ||
                current.type == BlockType::INCLUDE_TRANSFORM)
            {
                // Path is on the next non-comment data line
                for (int pl = ln + 1; pl <= qMin(nLines, ln + 3); ++pl)
                {
                    QString pathLine = mf->lineString(pl).trimmed();
                    if (pathLine.isEmpty() || pathLine.startsWith('$'))
                        continue;
                    resolveInclude(pathLine, fileId, ln, cb);
                    break;
                }
                current.type = BlockType::INCLUDE;  // normalise TRANSFORM→INCLUDE
                inBlock = true;
            }

            continue;
        }

        // data line – just update end
        if (inBlock)
            current.endLine = ln;
    }

    closeBlock(nLines);

    if (cb) cb(100, "Scanned: " + QFileInfo(filePath).fileName());
}

// ─────────────────────────────────────────────────────────────────────────────
int BlockParser::resolveInclude(const QString &includePath, int parentId, int atLine, ProgressCb &cb)
{
    const FileEntry *parent = db.fileById(parentId);
    if (!parent) return -1;

    QString parentDir = QFileInfo(QString::fromStdString(parent->path)).absolutePath();
    QString absPath   = QDir(parentDir).absoluteFilePath(includePath);

    if (!QFileInfo::exists(absPath))
        return -1;   // missing include — will appear as unresolved in model tree

    int newId = db.registerFile(absPath, parentId, atLine);
    scanFile(newId, absPath, cb);
    return newId;
}

// ─────────────────────────────────────────────────────────────────────────────
//  parseAll / parseBlock  – Phase 2
// ─────────────────────────────────────────────────────────────────────────────
void BlockParser::parseAll(ProgressCb cb)
{
    const int n = static_cast<int>(db.blocks.size());
    for (int i = 0; i < n; ++i)
    {
        auto &b = db.blocks[i];
        if (!b.parsed)
            parseBlock(b);
        if (cb && i % 50 == 0)
            cb(i * 100 / n, "Parsing blocks…");
    }
    db.buildCrossReferenceGraph();
    if (cb) cb(100, "Done.");
}

void BlockParser::parseBlock(BlockEntry &block)
{
    if (block.parsed) return;
    MappedFile *mf = mapperFor(block.fileId);
    if (!mf) return;

    switch (block.type)
    {
    case BlockType::NODE:    parseNodeBlock(block, *mf);    break;
    case BlockType::ELEMENT: parseElementBlock(block, *mf); break;
    case BlockType::PART:    parsePartBlock(block, *mf);    break;
    case BlockType::MAT:     parseMatBlock(block, *mf);     break;
    case BlockType::SECTION: parseSectionBlock(block, *mf); break;
    case BlockType::CONTACT: parseContactBlock(block, *mf); break;
    case BlockType::SET:     parseSetBlock(block, *mf);     break;
    default: break;
    }
    block.parsed = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Block-type parsers
// ─────────────────────────────────────────────────────────────────────────────
void BlockParser::parseNodeBlock(BlockEntry &block, MappedFile &mf)
{
    for (int ln = block.startLine + 1; ln <= block.endLine; ++ln)
    {
        QByteArray raw = mf.lineBytes(ln);
        if (raw.isEmpty() || raw[0] == '$' || raw[0] == '*') continue;

        QStringList f = raw.trimmed().contains(',') ? splitFree(raw) : splitFixed(raw, 8);
        if (f.size() < 4) continue;

        bool ok;
        int id = f[0].trimmed().toInt(&ok);
        if (!ok || db.node_index.count(id))
            continue;

        NodeMeta nm;
        nm.id     = id;
        nm.x      = f[1].trimmed().toDouble();
        nm.y      = f[2].trimmed().toDouble();
        nm.z      = f[3].trimmed().toDouble();
        nm.source = { block.fileId, ln, mf.lineOffset(ln) };
        db.node_index[id] = nm;
    }
}

void BlockParser::parseElementBlock(BlockEntry &block, MappedFile &mf)
{
    ElemType et = classifyElement(QString::fromStdString(block.subtype));
    int numNodes = 0;
    switch (et) {
    case ElemType::SHELL:    numNodes = 4;  break;
    case ElemType::SOLID:    numNodes = 8;  break;
    case ElemType::BEAM:     numNodes = 3;  break;
    case ElemType::DISCRETE: numNodes = 2;  break;
    case ElemType::TSHELL:   numNodes = 4;  break;
    default:                 numNodes = 0;  break;
    }

    for (int ln = block.startLine + 1; ln <= block.endLine; ++ln)
    {
        QByteArray raw = mf.lineBytes(ln);
        if (raw.isEmpty() || raw[0] == '$' || raw[0] == '*') continue;

        QStringList f = raw.trimmed().contains(',') ? splitFree(raw) : splitFixed(raw, 8);
        if (f.size() < 2) continue;

        bool ok;
        int eid = f[0].trimmed().toInt(&ok); if (!ok) continue;
        int pid = f[1].trimmed().toInt(&ok); if (!ok) continue;

        ElementMeta em;
        em.id     = eid;
        em.partId = pid;
        em.type   = et;
        em.source = { block.fileId, ln, mf.lineOffset(ln) };

        for (int k = 2; k < qMin(f.size(), 2 + numNodes); ++k)
        {
            int nid = f[k].trimmed().toInt(&ok);
            if (ok && nid != 0) em.nodeIds.push_back(nid);
        }
        db.element_index[eid] = std::move(em);
    }
}

void BlockParser::parsePartBlock(BlockEntry &block, MappedFile &mf)
{
    // *PART
    // <name string>
    // pid secid mid eosid hgid grav adpopt tmid
    QString name;
    for (int ln = block.startLine + 1; ln <= block.endLine; ++ln)
    {
        QByteArray raw = mf.lineBytes(ln);
        if (raw.isEmpty() || raw[0] == '$' || raw[0] == '*') continue;
        QString s = QString::fromLatin1(raw).trimmed();

        bool ok;
        int firstNum = s.split(QRegularExpression(R"([,\s]+)"), Qt::SkipEmptyParts)
                        .first().toInt(&ok);
        if (!ok)
        {
            // Name line
            name = s;
            continue;
        }

        QStringList f = s.contains(',') ? splitFree(raw) : splitFixed(raw, 10);
        if (f.isEmpty()) continue;

        int pid = f[0].trimmed().toInt(&ok); if (!ok) continue;

        PartMeta pm;
        pm.id        = pid;
        pm.name      = name.toStdString();
        pm.sectionId = f.size() > 1 ? f[1].trimmed().toInt() : 0;
        pm.matId     = f.size() > 2 ? f[2].trimmed().toInt() : 0;
        pm.eosId     = f.size() > 3 ? f[3].trimmed().toInt() : 0;
        pm.hgId      = f.size() > 4 ? f[4].trimmed().toInt() : 0;
        pm.source    = { block.fileId, ln, mf.lineOffset(ln) };
        db.part_index[pid] = std::move(pm);
        name.clear();
    }
}

void BlockParser::parseMatBlock(BlockEntry &block, MappedFile &mf)
{
    // First data line: MID RO E PR …
    for (int ln = block.startLine + 1; ln <= block.endLine; ++ln)
    {
        QByteArray raw = mf.lineBytes(ln);
        if (raw.isEmpty() || raw[0] == '$' || raw[0] == '*') continue;

        QStringList f = raw.trimmed().contains(',') ? splitFree(raw) : splitFixed(raw, 10);
        if (f.isEmpty()) continue;

        bool ok;
        int mid = f[0].trimmed().toInt(&ok); if (!ok) continue;

        MaterialMeta mm;
        mm.id      = mid;
        mm.keyword = QString::fromStdString(block.keyword).mid(1).toStdString(); // strip '*'
        mm.source  = { block.fileId, ln, mf.lineOffset(ln) };

        const QString kw = QString::fromStdString(block.keyword).toUpper();
        mm.hasThermal = kw.contains("THERMAL");
        db.material_index[mid] = std::move(mm);
        break;   // only first data line needed for meta
    }
}

void BlockParser::parseSectionBlock(BlockEntry &block, MappedFile &mf)
{
    for (int ln = block.startLine + 1; ln <= block.endLine; ++ln)
    {
        QByteArray raw = mf.lineBytes(ln);
        if (raw.isEmpty() || raw[0] == '$' || raw[0] == '*') continue;

        QStringList f = raw.trimmed().contains(',') ? splitFree(raw) : splitFixed(raw, 10);
        if (f.isEmpty()) continue;

        bool ok;
        int sid = f[0].trimmed().toInt(&ok); if (!ok) continue;

        SectionMeta sm;
        sm.id     = sid;
        sm.type   = block.subtype;
        sm.source = { block.fileId, ln, mf.lineOffset(ln) };
        db.section_index[sid] = std::move(sm);
        break;
    }
}

void BlockParser::parseContactBlock(BlockEntry &block, MappedFile &mf)
{
    ContactMeta cm;
    cm.type   = block.subtype;
    cm.source = { block.fileId, block.startLine, block.startByte };

    // Second data line has ssid, msid
    int dataLine = 0;
    for (int ln = block.startLine + 1; ln <= block.endLine; ++ln)
    {
        QByteArray raw = mf.lineBytes(ln);
        if (raw.isEmpty() || raw[0] == '$' || raw[0] == '*') continue;
        ++dataLine;
        if (dataLine == 1)   // first data line: SSID MSID SSTYP MSTYP …
        {
            QStringList f = splitFree(raw);
            if (f.size() >= 2) {
                cm.ssid = f[0].trimmed().toInt();
                cm.msid = f[1].trimmed().toInt();
            }
            break;
        }
    }
    db.contacts.push_back(cm);
}

void BlockParser::parseSetBlock(BlockEntry &block, MappedFile &mf)
{
    for (int ln = block.startLine + 1; ln <= block.endLine; ++ln)
    {
        QByteArray raw = mf.lineBytes(ln);
        if (raw.isEmpty() || raw[0] == '$' || raw[0] == '*') continue;

        QStringList f = raw.trimmed().contains(',') ? splitFree(raw) : splitFixed(raw, 10);
        if (f.isEmpty()) continue;

        bool ok;
        int sid = f[0].trimmed().toInt(&ok); if (!ok) continue;

        SetMeta sm;
        sm.id     = sid;
        sm.type   = block.subtype;
        sm.source = { block.fileId, ln, mf.lineOffset(ln) };
        db.set_index[sid] = std::move(sm);
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
MappedFile *BlockParser::mapperFor(int fileId)
{
    if (mappers.contains(fileId))
        return mappers[fileId];

    const FileEntry *fe = db.fileById(fileId);
    if (!fe) return nullptr;

    auto *mf = new MappedFile();
    if (!mf->open(QString::fromStdString(fe->path)))
    {
        delete mf;
        return nullptr;
    }
    mappers[fileId] = mf;
    return mf;
}

BlockType BlockParser::classifyKeyword(const QString &kw)
{
    if (kw == "*NODE")                  return BlockType::NODE;
    if (kw.startsWith("*ELEMENT_"))     return BlockType::ELEMENT;
    if (kw == "*PART" ||
        kw.startsWith("*PART_"))        return BlockType::PART;
    if (kw.startsWith("*MAT_"))         return BlockType::MAT;
    if (kw.startsWith("*SECTION_"))     return BlockType::SECTION;
    if (kw.startsWith("*CONTACT_"))     return BlockType::CONTACT;
    if (kw.startsWith("*CONTROL_"))     return BlockType::CONTROL;
    if (kw.startsWith("*DATABASE_"))    return BlockType::DATABASE;
    if (kw.startsWith("*SET_"))         return BlockType::SET;
    if (kw.startsWith("*EOS_"))         return BlockType::EOS;
    if (kw.startsWith("*DEFINE_"))      return BlockType::DEFINE;
    if (kw.startsWith("*BOUNDARY_"))    return BlockType::BOUNDARY;
    if (kw.startsWith("*LOAD_"))        return BlockType::LOAD;
    if (kw.startsWith("*INITIAL_"))     return BlockType::INITIAL;
    if (kw == "*INCLUDE")               return BlockType::INCLUDE;
    if (kw == "*INCLUDE_TRANSFORM")     return BlockType::INCLUDE_TRANSFORM;
    if (kw == "*KEYWORD")               return BlockType::KEYWORD;
    if (kw == "*END")                   return BlockType::END;
    return BlockType::UNKNOWN;
}

ElemType BlockParser::classifyElement(const QString &sub)
{
    const QString s = sub.toUpper();
    if (s == "SHELL" || s == "SHELL4N")   return ElemType::SHELL;
    if (s == "SOLID")                     return ElemType::SOLID;
    if (s == "BEAM")                      return ElemType::BEAM;
    if (s == "DISCRETE")                  return ElemType::DISCRETE;
    if (s == "TSHELL")                    return ElemType::TSHELL;
    return ElemType::UNKNOWN;
}

QStringList BlockParser::splitFixed(const QByteArray &line, int fw)
{
    QStringList result;
    for (int i = 0; i < line.size(); i += fw)
        result << QString::fromLatin1(line.mid(i, fw));
    return result;
}

QStringList BlockParser::splitFree(const QByteArray &line)
{
    return QString::fromLatin1(line)
               .split(QRegularExpression(R"([,\s]+)"), Qt::SkipEmptyParts);
}
