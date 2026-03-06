#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  workspace_panel.h
//
//  Left-dock panel showing the working directory structure.
//
//  Tree layout:
//    📁 /home/user/project/                     ← header (not clickable)
//    ├─ 🔑 crash_main.k                         ← master file
//    │   ├─ 📄 geometry.k                       ← include (click → load)
//    │   └─ 📄 material_lib.k                   ← include (click → load)
//    └─ 🔑 pedestrian.k                         ← another master
//        └─ 📄 ped_mesh.k
//
//  Rules:
//    • Master file single-click → expand/collapse, does NOT load into editor
//    • Include node single-click → load file into editor
//    • Include nodes are loaded lazily (only when parent is first expanded)
//    • Missing include files shown in red with ⚠ icon
// ═════════════════════════════════════════════════════════════════════════════
#include <QDockWidget>
#include "workspace_scanner.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;

class WorkspacePanel : public QDockWidget
{
    Q_OBJECT
public:
    explicit WorkspacePanel(QWidget *parent = nullptr);

    // Load a workspace directory (triggers scan)
    void loadDirectory(const QString &dirPath);

    void clear();

signals:
    // Emitted when user clicks an include node (or a master file that should be
    // loaded directly, e.g. via double-click).
    void fileOpenRequested(const QString &absPath);

private slots:
    void onItemExpanded(QTreeWidgetItem *item);
    void onItemClicked(QTreeWidgetItem *item, int col);

private:
    enum Role {
        AbsPathRole  = Qt::UserRole,
        NodeTypeRole = Qt::UserRole + 1,  // "master" | "include" | "dir"
        ChildrenLoadedRole = Qt::UserRole + 2  // bool
    };

    QTreeWidgetItem *makeMasterItem(const WsFile &wf);
    QTreeWidgetItem *makeIncludeItem(const QString &absPath, bool exists);
    void             loadChildren(QTreeWidgetItem *parent, const QString &absPath);

    QLabel      *dirLabel;
    QTreeWidget *tree;
    WorkspaceScanner scanner;
};
