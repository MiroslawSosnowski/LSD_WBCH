#pragma once
#include <QDockWidget>
#include <QTreeWidget>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
//  ModelTreePanel
//  Parses the loaded .k file in-place and builds a tree:
//    ○ Nodes (N)
//    △ Elements
//       SHELL (n)  /  SOLID (n)  /  BEAM (n)  …
//    □ Parts
//       Part 1: name
//    ◈ Materials
//       MAT_024 (ID: 3)
//    ↗ Includes
//       submodel.k
// ─────────────────────────────────────────────────────────────────────────────
class ModelTreePanel : public QDockWidget
{
    Q_OBJECT
public:
    explicit ModelTreePanel(QWidget *parent = nullptr);

    void loadFromContent(const QString &content, const QString &filePath);
    void clear();
    void loadBranches(const QString &masterFilePath);

signals:
    void materialSelected(const QString &matKeyword);       // e.g. "MAT_024"
    void includeFileDoubleClicked(const QString &absPath);
    void jumpToLine(int lineNumber);

private slots:
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);

private:
    QTreeWidget *tree;
};
