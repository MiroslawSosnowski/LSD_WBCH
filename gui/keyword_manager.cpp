#include "keyword_manager.h"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QMap>
#include <QFont>

// ─────────────────────────────────────────────────────────────────────────────
KeywordManager::KeywordManager(QWidget *parent)
    : QDockWidget("Keyword Manager", parent)
{
    QWidget     *container = new QWidget(this);
    auto        *layout    = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    container->setStyleSheet("background:#252526;");

    // ── Search box ────────────────────────────────────────────────────────────
    searchBox = new QLineEdit(container);
    searchBox->setPlaceholderText("  🔍  Filter keywords…");
    searchBox->setClearButtonEnabled(true);
    searchBox->setStyleSheet(R"(
        QLineEdit {
            background:#3C3C3C; color:#D4D4D4;
            border:none; border-bottom:1px solid #454545;
            padding:6px 8px; font-size:12px;
        }
        QLineEdit:focus { border-bottom:1px solid #007ACC; }
    )");

    // ── Tree ──────────────────────────────────────────────────────────────────
    tree = new QTreeWidget(container);
    tree->setHeaderHidden(true);
    tree->setColumnCount(1);
    tree->setIndentation(14);
    tree->setStyleSheet(R"(
        QTreeWidget {
            background:#252526; color:#CCCCCC;
            border:none; font-size:12px;
        }
        QTreeWidget::item            { padding:2px 4px; }
        QTreeWidget::item:selected   { background:#37373D; }
        QTreeWidget::item:hover      { background:#2A2D2E; }
        QTreeWidget::branch          { background:#252526; }
    )");
    tree->setToolTipDuration(8000);

    layout->addWidget(searchBox);
    layout->addWidget(tree, 1);
    setWidget(container);

    connect(searchBox, &QLineEdit::textChanged,
            this,      &KeywordManager::onFilterChanged);
    connect(tree, &QTreeWidget::itemDoubleClicked,
            this, &KeywordManager::onItemDoubleClicked);
}

// ─────────────────────────────────────────────────────────────────────────────
void KeywordManager::setEntries(const QList<KeywordEntry> &entries)
{
    allEntries = entries;
    rebuild();
}

// ─────────────────────────────────────────────────────────────────────────────
void KeywordManager::rebuild(const QString &filter)
{
    tree->clear();

    const QString f = filter.trimmed().toUpper();

    // Group by category preserving insertion order
    QMap<QString, QList<const KeywordEntry *>> byCategory;
    QStringList catOrder;

    for (const auto &e : allEntries)
    {
        if (!f.isEmpty())
        {
            if (!e.keyword.toUpper().contains(f) &&
                !e.desc.toUpper().contains(f) &&
                !e.category.toUpper().contains(f))
                continue;
        }
        if (!byCategory.contains(e.category))
            catOrder << e.category;
        byCategory[e.category].append(&e);
    }

    for (const QString &cat : catOrder)
    {
        const auto &list = byCategory[cat];
        if (list.isEmpty()) continue;

        // ── Category header ───────────────────────────────────────────────────
        auto *catItem = new QTreeWidgetItem(tree);
        catItem->setText(0, cat + "  (" + QString::number(list.size()) + ")");
        catItem->setFlags(catItem->flags() & ~Qt::ItemIsSelectable);
        QFont cf = catItem->font(0);
        cf.setBold(true);
        cf.setPointSize(cf.pointSize() + 1);
        catItem->setFont(0, cf);
        catItem->setForeground(0, list.first()->color);
        catItem->setBackground(0, QColor("#2D2D30"));
        tree->addTopLevelItem(catItem);

        for (const KeywordEntry *e : list)
        {
            // ── Keyword item ──────────────────────────────────────────────────
            auto *kwItem = new QTreeWidgetItem(catItem);
            kwItem->setText(0, e->keyword);
            kwItem->setForeground(0, e->color);
            kwItem->setData(0, Qt::UserRole, e->keyword);
            kwItem->setToolTip(0,
                "<b>" + e->keyword + "</b><br>" +
                e->desc + "<br><br>" +
                "<span style='color:#858585'>Fields: </span>" +
                e->fields.join("  "));

            // ── Description sub-item ──────────────────────────────────────────
            auto *descItem = new QTreeWidgetItem(kwItem);
            descItem->setText(0, "  " + e->desc);
            descItem->setForeground(0, QColor("#858585"));
            descItem->setFlags(descItem->flags() & ~Qt::ItemIsSelectable);

            // ── Fields sub-item ───────────────────────────────────────────────
            if (!e->fields.isEmpty())
            {
                auto *fieldItem = new QTreeWidgetItem(kwItem);
                fieldItem->setText(0, "  " + e->fields.join("  ·  "));
                fieldItem->setForeground(0, QColor("#6A9955"));
                QFont ff = fieldItem->font(0);
                ff.setPointSize(ff.pointSize() - 1);
                fieldItem->setFont(0, ff);
                fieldItem->setFlags(fieldItem->flags() & ~Qt::ItemIsSelectable);
            }
        }

        catItem->setExpanded(f.isEmpty() ? false : true);
    }

    if (!f.isEmpty())
        tree->expandAll();
}

// ─────────────────────────────────────────────────────────────────────────────
void KeywordManager::onFilterChanged(const QString &text)
{
    rebuild(text);
}

// ─────────────────────────────────────────────────────────────────────────────
void KeywordManager::onItemDoubleClicked(QTreeWidgetItem *item, int)
{
    if (!editor) return;

    const QString keyword = item->data(0, Qt::UserRole).toString();
    if (keyword.isEmpty()) return;

    // Find template
    for (const auto &e : allEntries)
    {
        if (e.keyword == keyword)
        {
            QTextCursor c = editor->textCursor();
            // Insert at start of current line
            c.movePosition(QTextCursor::StartOfLine);
            c.insertText(e.templateText.isEmpty() ? keyword + "\n" : e.templateText);
            editor->setTextCursor(c);
            editor->setFocus();
            return;
        }
    }
}
