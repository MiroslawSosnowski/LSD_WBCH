#include "workspace_scanner.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>

// ─────────────────────────────────────────────────────────────────────────────
//  scan()
// ─────────────────────────────────────────────────────────────────────────────
bool WorkspaceScanner::scan(const QString &dirPath)
{
    dir = dirPath;
    masters.clear();
    includeGraph.clear();
    includedByOther.clear();

    // ── 1. Collect all candidate files ───────────────────────────────────────
    const QStringList filters = {"*.k", "*.dyn", "*.key", "*.inc"};
    QDir d(dirPath);
    const QFileInfoList entries =
        d.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name);

    if (entries.isEmpty()) return false;

    QStringList allPaths;
    for (const QFileInfo &fi : entries)
        allPaths << fi.absoluteFilePath();

    // ── 2. Build include graph ───────────────────────────────────────────────
    for (const QString &p : allPaths)
    {
        const QStringList includes = readIncludePaths(p);
        QStringList resolved;
        for (const QString &inc : includes)
        {
            const QString abs = resolveRelative(inc, QFileInfo(p).absolutePath());
            resolved << abs;
            includedByOther.insert(abs.toLower());   // case-insensitive match
        }
        includeGraph[p] = resolved;
    }

    // ── 3. Identify master files ─────────────────────────────────────────────
    for (const QString &p : allPaths)
    {
        const bool isIncluded = includedByOther.contains(p.toLower());
        if (!isIncluded)
        {
            WsFile wf;
            wf.absPath  = p;
            wf.name     = QFileInfo(p).fileName();
            wf.isMaster = true;
            masters.append(wf);
        }
    }

    return !masters.isEmpty() || !allPaths.isEmpty();
}

// ─────────────────────────────────────────────────────────────────────────────
//  buildIncludeTree()  –  recursive, with cycle guard
// ─────────────────────────────────────────────────────────────────────────────
static WsIncludeNode buildNode(const QString &absPath,
                               const QMap<QString, QStringList> &graph,
                               QSet<QString> &visited,
                               int depth, int maxDepth)
{
    WsIncludeNode node;
    node.absPath = absPath;
    node.name    = QFileInfo(absPath).fileName();
    node.exists  = QFileInfo::exists(absPath);

    if (depth >= maxDepth || visited.contains(absPath.toLower()))
        return node;
    visited.insert(absPath.toLower());

    const QStringList includes = graph.value(absPath);
    for (const QString &child : includes)
        node.children.append(
            buildNode(child, graph, visited, depth + 1, maxDepth));

    return node;
}

WsIncludeNode WorkspaceScanner::buildIncludeTree(const QString &absPath,
                                                  int maxDepth) const
{
    QSet<QString> visited;
    return buildNode(absPath, includeGraph, visited, 0, maxDepth);
}

// ─────────────────────────────────────────────────────────────────────────────
//  directIncludes()  –  returns the immediate children (resolved absolute paths)
// ─────────────────────────────────────────────────────────────────────────────
QStringList WorkspaceScanner::directIncludes(const QString &absPath) const
{
    // If we already scanned this file, use the cached graph
    if (includeGraph.contains(absPath))
        return includeGraph.value(absPath);

    // Otherwise do a fresh read (file may be outside the workspace dir)
    const QStringList includes = readIncludePaths(absPath);
    const QString parentDir    = QFileInfo(absPath).absolutePath();
    QStringList resolved;
    for (const QString &inc : includes)
        resolved << resolveRelative(inc, parentDir);
    return resolved;
}

// ─────────────────────────────────────────────────────────────────────────────
//  readIncludePaths()  –  reads only *INCLUDE lines, returns raw path strings
// ─────────────────────────────────────────────────────────────────────────────
QStringList WorkspaceScanner::readIncludePaths(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&f);
    QStringList paths;
    bool nextLineIsPath = false;

    while (!in.atEnd())
    {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('$'))
        {
            if (nextLineIsPath) continue;   // skip comments between *INCLUDE and path
            continue;
        }

        if (nextLineIsPath)
        {
            paths << line;
            nextLineIsPath = false;
            continue;
        }

        const QString upper = line.toUpper();
        if (upper.startsWith("*INCLUDE"))
            nextLineIsPath = true;
    }

    return paths;
}

// ─────────────────────────────────────────────────────────────────────────────
QString WorkspaceScanner::resolveRelative(const QString &includePath,
                                          const QString &parentDir)
{
    if (QFileInfo(includePath).isAbsolute())
        return QDir::cleanPath(includePath);
    return QDir::cleanPath(QDir(parentDir).absoluteFilePath(includePath));
}
