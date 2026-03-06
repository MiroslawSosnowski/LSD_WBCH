#include "workspace_panel.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QFileInfo>
#include <QFont>
#include <QDir>

// ─────────────────────────────────────────────────────────────────────────────
WorkspacePanel::WorkspacePanel(QWidget *parent)
    : QDockWidget("Workspace", parent)
{
    auto *container = new QWidget(this);
    auto *layout    = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    container->setStyleSheet("background:#252526;");

    // ── Directory label ───────────────────────────────────────────────────────
    dirLabel = new QLabel("No workspace open", container);
    dirLabel->setStyleSheet(R"(
        QLabel {
            background:#2D2D30;
            color:#858585;
            font-size:11px;
            padding:5px 8px;
            border-bottom:1px solid #3C3C3C;
        }
    )");
    dirLabel->setWordWrap(false);
    dirLabel->setTextFormat(Qt::PlainText);

    // ── Tree ──────────────────────────────────────────────────────────────────
    tree = new QTreeWidget(container);
    tree->setHeaderHidden(true);
    tree->setColumnCount(1);
    tree->setIndentation(16);
    tree->setAnimated(true);
    tree->setStyleSheet(R"(
        QTreeWidget {
            background:#252526;
            color:#CCCCCC;
            border:none;
            font-size:12px;
        }
        QTreeWidget::item             { padding:3px 4px; }
        QTreeWidget::item:selected    { background:#37373D; color:#FFFFFF; }
        QTreeWidget::item:hover       { background:#2A2D2E; }
        QTreeWidget::branch           { background:#252526; }
        QTreeWidget::branch:has-children:!has-siblings:closed,
        QTreeWidget::branch:closed:has-children:has-siblings {
            image: url(none);
        }
    )");

    layout->addWidget(dirLabel);
    layout->addWidget(tree, 1);
    setWidget(container);

    connect(tree, &QTreeWidget::itemExpanded,
            this, &WorkspacePanel::onItemExpanded);
    connect(tree, &QTreeWidget::itemClicked,
            this, &WorkspacePanel::onItemClicked);
}

// ─────────────────────────────────────────────────────────────────────────────
void WorkspacePanel::loadDirectory(const QString &dirPath)
{
    tree->clear();

    // Show scanning indicator
    dirLabel->setText("Scanning  " + QDir(dirPath).dirName() + "…");

    const bool ok = scanner.scan(dirPath);

    // Update label to show just the directory name, full path in tooltip
    dirLabel->setText("📁  " + QDir(dirPath).dirName());
    dirLabel->setToolTip(dirPath);

    if (!ok || scanner.masterFiles().isEmpty())
    {
        auto *placeholder = new QTreeWidgetItem(tree);
        placeholder->setText(0, "No LS-DYNA files found");
        placeholder->setForeground(0, QColor("#858585"));
        placeholder->setFlags(Qt::NoItemFlags);
        return;
    }

    // ── Add master file items ─────────────────────────────────────────────────
    for (const WsFile &wf : scanner.masterFiles())
    {
        QTreeWidgetItem *item = makeMasterItem(wf);
        tree->addTopLevelItem(item);

        // Add a single dummy child so Qt shows the expand arrow
        auto *dummy = new QTreeWidgetItem(item);
        dummy->setText(0, "Loading…");
        dummy->setData(0, NodeTypeRole, "dummy");
        dummy->setForeground(0, QColor("#555555"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void WorkspacePanel::clear()
{
    tree->clear();
    dirLabel->setText("No workspace open");
}

// ─────────────────────────────────────────────────────────────────────────────
//  onItemExpanded  –  lazy-load children when a master or include is expanded
// ─────────────────────────────────────────────────────────────────────────────
void WorkspacePanel::onItemExpanded(QTreeWidgetItem *item)
{
    const QString nodeType = item->data(0, NodeTypeRole).toString();
    const bool loaded      = item->data(0, ChildrenLoadedRole).toBool();

    if (loaded) return;
    if (nodeType != "master" && nodeType != "include") return;

    const QString absPath = item->data(0, AbsPathRole).toString();

    // Remove the dummy placeholder child
    while (item->childCount() > 0)
        delete item->takeChild(0);

    loadChildren(item, absPath);
    item->setData(0, ChildrenLoadedRole, true);
}

// ─────────────────────────────────────────────────────────────────────────────
//  loadChildren  –  populate direct include children for a given node
// ─────────────────────────────────────────────────────────────────────────────
void WorkspacePanel::loadChildren(QTreeWidgetItem *parent, const QString &absPath)
{
    const QStringList children = scanner.directIncludes(absPath);

    if (children.isEmpty())
    {
        // Show explicit "no includes" leaf so user knows it was checked
        auto *leaf = new QTreeWidgetItem(parent);
        leaf->setText(0, "  (no includes)");
        leaf->setForeground(0, QColor("#555555"));
        leaf->setFlags(Qt::NoItemFlags);
        return;
    }

    for (const QString &childPath : children)
    {
        const bool exists = QFileInfo::exists(childPath);
        QTreeWidgetItem *child = makeIncludeItem(childPath, exists);
        parent->addChild(child);

        if (exists)
        {
            // Check if this include itself has further includes
            const QStringList grandChildren = scanner.directIncludes(childPath);
            if (!grandChildren.isEmpty())
            {
                // Add dummy to show expand arrow
                auto *dummy = new QTreeWidgetItem(child);
                dummy->setText(0, "Loading…");
                dummy->setData(0, NodeTypeRole, "dummy");
                dummy->setForeground(0, QColor("#555555"));
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  onItemClicked  –  include nodes load file; master nodes only expand
// ─────────────────────────────────────────────────────────────────────────────
void WorkspacePanel::onItemClicked(QTreeWidgetItem *item, int /*col*/)
{
    const QString nodeType = item->data(0, NodeTypeRole).toString();

    if (nodeType == "include")
    {
        const QString path = item->data(0, AbsPathRole).toString();
        if (QFileInfo::exists(path))
            emit fileOpenRequested(path);
    }
    // master → do nothing (expansion handled by Qt's built-in click-to-expand)
}

// ─────────────────────────────────────────────────────────────────────────────
//  Item factories
// ─────────────────────────────────────────────────────────────────────────────
QTreeWidgetItem *WorkspacePanel::makeMasterItem(const WsFile &wf)
{
    auto *item = new QTreeWidgetItem();
    item->setText(0, "🔑  " + wf.name);
    item->setData(0, AbsPathRole,         wf.absPath);
    item->setData(0, NodeTypeRole,        "master");
    item->setData(0, ChildrenLoadedRole,  false);
    item->setToolTip(0, wf.absPath);

    QFont f = item->font(0);
    f.setBold(true);
    item->setFont(0, f);
    item->setForeground(0, QColor("#DCDCAA"));   // VS Code yellow

    return item;
}

QTreeWidgetItem *WorkspacePanel::makeIncludeItem(const QString &absPath, bool exists)
{
    const QString name = QFileInfo(absPath).fileName();

    auto *item = new QTreeWidgetItem();
    item->setData(0, AbsPathRole,         absPath);
    item->setData(0, NodeTypeRole,        "include");
    item->setData(0, ChildrenLoadedRole,  false);
    item->setToolTip(0, exists ? absPath : "⚠  File not found:\n" + absPath);

    if (exists)
    {
        item->setText(0, "📄  " + name);
        item->setForeground(0, QColor("#9CDCFE"));   // VS Code light blue
    }
    else
    {
        item->setText(0, "⚠  " + name);
        item->setForeground(0, QColor("#F48771"));   // VS Code error red
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }

    return item;
}
