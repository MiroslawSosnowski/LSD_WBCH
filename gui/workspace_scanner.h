#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  workspace_scanner.h
//
//  Fast directory scanner for LS-DYNA workspaces.
//
//  Algorithm:
//    1. Collect all .k / .dyn / .key / .txt files in the directory (non-recursive)
//    2. For each file read only *INCLUDE lines → build an include graph
//    3. Master files = files that are NOT referenced by any other file
//
//  Only *INCLUDE paths are read – no full parsing at this stage.
// ═════════════════════════════════════════════════════════════════════════════
#include <QString>
#include <QStringList>
#include <QMap>
#include <QSet>

struct WsFile {
    QString absPath;
    QString name;        // basename
    bool    isMaster = false;
};

// Include tree node (recursive)
struct WsIncludeNode {
    QString                   absPath;
    QString                   name;
    bool                      exists  = true;   // false if file not found
    QList<WsIncludeNode>      children;          // direct includes
};

class WorkspaceScanner
{
public:
    // Scan directory and build the workspace graph.
    // Returns true if at least one LS-DYNA file was found.
    bool scan(const QString &dirPath);

    const QList<WsFile> &masterFiles() const { return masters; }
    const QString       &directory()   const { return dir; }

    // Build the recursive include tree for one master file.
    // Only the direct includes are resolved; deeper levels resolved lazily by
    // calling this again for each child node's absPath.
    WsIncludeNode buildIncludeTree(const QString &absPath, int maxDepth = 32) const;

    // Quick single-level include scan (used by lazy tree expansion)
    QStringList directIncludes(const QString &absPath) const;

private:
    static QStringList readIncludePaths(const QString &filePath);
    static QString     resolveRelative(const QString &includePath,
                                       const QString &parentDir);

    QString      dir;
    QList<WsFile> masters;

    // absPath → list of absolute included paths
    QMap<QString, QStringList> includeGraph;
    // set of all paths that are included by some other file
    QSet<QString> includedByOther;
};
