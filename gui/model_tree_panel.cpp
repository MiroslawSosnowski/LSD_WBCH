#include "model_tree_panel.h"
#include <QVBoxLayout>
#include <QFileInfo>
#include <QDir>
#include <QFont>
#include <QRegularExpression>

// ─── helpers ─────────────────────────────────────────────────────────────────

static QTreeWidgetItem *makeCategory(const QString &icon, const QString &name)
{
    auto *item = new QTreeWidgetItem();
    item->setText(0, icon + "  " + name);
    QFont f = item->font(0);
    f.setBold(true);
    item->setFont(0, f);
    item->setForeground(0, QColor("#9CDCFE"));
    return item;
}

static QStringList splitFields(const QString &line)
{
    // Handles both whitespace-separated and comma-separated fields
    return line.split(QRegularExpression(R"([,\s]+)"), Qt::SkipEmptyParts);
}

// ─── constructor ─────────────────────────────────────────────────────────────

ModelTreePanel::ModelTreePanel(QWidget *parent)
    : QDockWidget("Model Tree", parent)
{
    QWidget *container = new QWidget(this);
    auto    *layout    = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    tree = new QTreeWidget(container);
    tree->setHeaderHidden(true);
    tree->setColumnCount(1);
    tree->setIndentation(16);
    tree->setStyleSheet(R"(
        QTreeWidget {
            background-color: #252526;
            color: #CCCCCC;
            border: none;
            font-size: 12px;
        }
        QTreeWidget::item { padding: 2px 0; }
        QTreeWidget::item:selected  { background-color: #37373D; }
        QTreeWidget::item:hover     { background-color: #2A2D2E; }
        QTreeWidget::branch         { background-color: #252526; }
    )");

    layout->addWidget(tree);
    setWidget(container);

    connect(tree, &QTreeWidget::itemDoubleClicked,
            this, &ModelTreePanel::onItemDoubleClicked);
}

// ─── public API ──────────────────────────────────────────────────────────────

void ModelTreePanel::clear()
{
    tree->clear();
}

void ModelTreePanel::loadFromContent(const QString &content, const QString &filePath)
{
    tree->clear();

    const QString dir = QFileInfo(filePath).absolutePath();
    
    // Load branches if this is a master file
    loadBranches(filePath);

    // ── category items ───────────────────────────────────────────────────────
    auto *nodesItem    = makeCategory("○", "Nodes");
    auto *elementsItem = makeCategory("△", "Elements");
    auto *partsItem    = makeCategory("□", "Parts");
    auto *matsItem     = makeCategory("◈", "Materials");
    auto *includesItem = makeCategory("↗", "Includes");

    for (auto *cat : {nodesItem, elementsItem, partsItem, matsItem, includesItem})
        tree->addTopLevelItem(cat);

    // ── single-pass parser ───────────────────────────────────────────────────
    enum Block { NONE, NODE, ELEMENT, PART, MAT, INCLUDE_FILE };
    Block   block           = NONE;
    QString elementSubtype;                    // e.g. "SHELL"
    QString matKeyword;                        // e.g. "MAT_024"

    // Per-element-type child items (created lazily)
    QMap<QString, QTreeWidgetItem *> elementTypeItems;
    QMap<QString, int>               elementTypeCounts;

    int  nodeCount       = 0;

    // PART state
    QString partHeaderText;
    bool    awaitPartIds = false;

    // MAT state
    bool awaitMatId = false;

    // INCLUDE state
    bool awaitIncludePath = false;

    const QStringList lines = content.split('\n');
    for (int i = 0; i < lines.size(); ++i)
    {
        const QString &raw     = lines[i];
        const QString  trimmed = raw.trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith('$'))
            continue;

        // ── keyword line ─────────────────────────────────────────────────────
        if (trimmed.startsWith('*'))
        {
            // Reset pending states when a new keyword begins
            awaitPartIds      = false;
            awaitMatId        = false;
            awaitIncludePath  = false;
            partHeaderText.clear();

            // Normalise: take the first token, uppercase
            const QString kw = trimmed.split(QRegularExpression(R"(\s+)"),
                                              Qt::SkipEmptyParts)
                                       .first()
                                       .toUpper();

            if (kw == "*NODE")
            {
                block = NODE;
            }
            else if (kw.startsWith("*ELEMENT_"))
            {
                block          = ELEMENT;
                elementSubtype = kw.mid(9);   // after *ELEMENT_
                if (!elementTypeCounts.contains(elementSubtype))
                    elementTypeCounts[elementSubtype] = 0;
            }
            else if (kw == "*PART")
            {
                block        = PART;
                awaitPartIds = true;   // next non-comment = header string
            }
            else if (kw.startsWith("*MAT"))
            {
                block      = MAT;
                matKeyword = kw.mid(1);   // strip leading '*'
                awaitMatId = true;
            }
            else if (kw == "*INCLUDE" || kw == "*INCLUDE_TRANSFORM")
            {
                block            = INCLUDE_FILE;
                awaitIncludePath = true;
            }
            else
            {
                block = NONE;
            }
            continue;
        }

        // ── data line ────────────────────────────────────────────────────────
        switch (block)
        {
        // ── Nodes ─────────────────────────────────────────────────────────
        case NODE:
            ++nodeCount;
            break;

        // ── Elements ──────────────────────────────────────────────────────
        case ELEMENT:
            elementTypeCounts[elementSubtype]++;
            break;

        // ── Parts ─────────────────────────────────────────────────────────
        case PART:
            if (partHeaderText.isEmpty())
            {
                // First data line is the part title
                partHeaderText = trimmed;
            }
            else if (awaitPartIds)
            {
                // Second data line: pid secid mid …
                const QStringList vals = splitFields(trimmed);
                if (!vals.isEmpty())
                {
                    bool ok;
                    int pid = vals[0].toInt(&ok);
                    if (ok)
                    {
                        QString label = QString("Part %1").arg(pid);
                        if (!partHeaderText.isEmpty())
                            label += ":  " + partHeaderText;

                        auto *pi = new QTreeWidgetItem(partsItem);
                        pi->setText(0, label);
                        pi->setData(0, Qt::UserRole,   i + 1);   // 1-based line
                        pi->setForeground(0, QColor("#DCDCAA"));
                    }
                }
                awaitPartIds = false;
                block        = NONE;   // one part per *PART keyword
            }
            break;

        // ── Materials ─────────────────────────────────────────────────────
        case MAT:
            if (awaitMatId)
            {
                const QStringList vals = splitFields(trimmed);
                if (!vals.isEmpty())
                {
                    const QString matId = vals[0];
                    const QString label = matKeyword + "  (ID: " + matId + ")";

                    auto *mi = new QTreeWidgetItem(matsItem);
                    mi->setText(0, label);
                    mi->setData(0, Qt::UserRole,     i + 1);      // line
                    mi->setData(0, Qt::UserRole + 1, matKeyword);  // for signal
                    mi->setForeground(0, QColor("#CE9178"));
                    awaitMatId = false;
                }
            }
            break;

        // ── Includes ──────────────────────────────────────────────────────
        case INCLUDE_FILE:
            if (awaitIncludePath)
            {
                const QString includePath = trimmed;
                const QString absPath =
                    QDir(dir).absoluteFilePath(includePath);
                const bool exists = QFileInfo::exists(absPath);

                auto *ii = new QTreeWidgetItem(includesItem);
                ii->setText(0, QFileInfo(includePath).fileName());
                ii->setData(0, Qt::UserRole,     absPath);
                ii->setData(0, Qt::UserRole + 1, QString("include"));
                ii->setToolTip(0, exists ? absPath
                                         : "⚠ File not found:\n" + absPath);
                ii->setForeground(0, exists ? QColor("#C586C0")
                                            : QColor("#F48771"));
                awaitIncludePath = false;
                block = NONE;
            }
            break;

        default:
            break;
        }
    }

    // ── Build element-type child items ────────────────────────────────────────
    for (auto it = elementTypeCounts.constBegin();
         it != elementTypeCounts.constEnd(); ++it)
    {
        auto *ei = new QTreeWidgetItem(elementsItem);
        ei->setText(0, it.key() + "  (" + QString::number(it.value()) + ")");
        ei->setForeground(0, QColor("#4EC9B0"));
    }

    // ── Update category labels with counts ────────────────────────────────────
    nodesItem->setText(0, "○  Nodes  (" + QString::number(nodeCount) + ")");

    // ── Expand / hide categories ──────────────────────────────────────────────
    tree->expandAll();

    nodesItem->setHidden(nodeCount == 0);
    elementsItem->setHidden(elementsItem->childCount() == 0);
    partsItem->setHidden(partsItem->childCount()    == 0);
    matsItem->setHidden(matsItem->childCount()      == 0);
    includesItem->setHidden(includesItem->childCount() == 0);
}

// ─── slots ────────────────────────────────────────────────────────────────────

void ModelTreePanel::onItemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    const QString tag = item->data(0, Qt::UserRole + 1).toString();

    if (tag == "include")
    {
        emit includeFileDoubleClicked(item->data(0, Qt::UserRole).toString());
        return;
    }
    else if (tag == "master")
    {
        loadBranches(item->data(0, Qt::UserRole).toString());
        return;
    }

    // Material keyword stored in UserRole+1 for mat items
    if (!tag.isEmpty())
        emit materialSelected(tag);

    // Jump to source line (stored in UserRole for non-include items)
    const int line = item->data(0, Qt::UserRole).toInt();
    if (line > 0)
        emit jumpToLine(line);
}

void ModelTreePanel::loadBranches(const QString &masterFilePath)
{
    QFile file(masterFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("*INCLUDE")) {
            QStringList parts = line.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                QString includePath = parts[1].trimmed().replace("=", "").trimmed();
                QString absPath = QFileInfo(QFileInfo(masterFilePath).absolutePath() + QDir::separator() + includePath).absoluteFilePath();
                
                if (QFileInfo::exists(absPath)) {
                    auto *includesItem = makeCategory("↗", "Includes");
                    tree->addTopLevelItem(includesItem);
                    
                    auto *ii = new QTreeWidgetItem(includesItem);
                    ii->setText(0, QFileInfo(includePath).fileName());
                    ii->setData(0, Qt::UserRole, absPath);
                    ii->setData(0, Qt::UserRole + 1, QString("include"));
                    ii->setToolTip(0, absPath);
                    ii->setForeground(0, QColor("#C586C0"));
                }
            }
        }
    }
}
