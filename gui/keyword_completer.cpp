#include "keyword_completer.h"
#include <QCompleter>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextBlock>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStandardItemModel>

// ─────────────────────────────────────────────────────────────────────────────
KeywordCompleter::KeywordCompleter(QPlainTextEdit *ed, QObject *parent)
    : QObject(parent), editor(ed)
{
    completer = new QCompleter(this);
    completer->setWidget(editor);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);

    completer->popup()->setStyleSheet(R"(
        QListView {
            background-color: #252526;
            color: #D4D4D4;
            border: 1px solid #454545;
            font-family: 'Courier New';
            font-size: 12px;
            selection-background-color: #094771;
        }
        QListView::item { padding: 3px 8px; }
        QScrollBar:vertical { background:#1E1E1E; width:8px; }
        QScrollBar::handle:vertical { background:#424242; border-radius:4px; }
    )");

    // Use QModelIndex overload to reliably get UserRole keyword string
    connect(completer,
            QOverload<const QModelIndex &>::of(&QCompleter::activated),
            this, &KeywordCompleter::insertCompletion);
}

// ─────────────────────────────────────────────────────────────────────────────
bool KeywordCompleter::loadDatabase(const QString &jsonPath)
{
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;

    const QJsonObject cats = doc.object()["categories"].toObject();

    for (auto catIt = cats.begin(); catIt != cats.end(); ++catIt)
    {
        const QString    catName = catIt.key();
        const QJsonObject cat    = catIt.value().toObject();
        const QColor     color   = QColor(cat["color"].toString("#D4D4D4"));
        const QJsonObject kws    = cat["keywords"].toObject();

        for (auto kwIt = kws.begin(); kwIt != kws.end(); ++kwIt)
        {
            const QJsonObject kw = kwIt.value().toObject();
            KeywordEntry e;
            e.keyword      = kwIt.key();
            e.desc         = kw["desc"].toString();
            e.category     = catName;
            e.color        = color;
            e.templateText = kw["template"].toString();
            for (const auto &fv : kw["fields"].toArray())
                e.fields << fv.toString();

            index_[e.keyword] = entries_.size();
            entries_.append(e);
        }
    }

    auto *model = new QStandardItemModel(completer);
    for (const auto &e : entries_)
    {
        auto *item = new QStandardItem(e.keyword + "   \u2014 " + e.desc);
        item->setData(e.keyword, Qt::UserRole);   // keyword stored in UserRole
        item->setForeground(e.color);
        model->appendRow(item);
    }
    completer->setModel(model);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Forwarding accessors
// ─────────────────────────────────────────────────────────────────────────────
QAbstractItemView *KeywordCompleter::popup() const
{
    return completer->popup();
}

void KeywordCompleter::hidePopup()
{
    completer->popup()->hide();
}

// ─────────────────────────────────────────────────────────────────────────────
void KeywordCompleter::handleKeyPress(int key, Qt::KeyboardModifiers /*mods*/)
{
    // Hide popup when user leaves a keyword line
    if (!cursorAtKeywordPosition())
    {
        completer->popup()->hide();
        return;
    }
    updateCompleter();
}

// ─────────────────────────────────────────────────────────────────────────────
void KeywordCompleter::updateCompleter()
{
    const QString prefix = currentWord();
    if (prefix.isEmpty())
    {
        completer->popup()->hide();
        return;
    }

    completer->setCompletionPrefix(prefix);

    if (completer->completionCount() == 0)
    {
        completer->popup()->hide();
        return;
    }

    QRect cr = editor->cursorRect();
    cr.setWidth(qMin(
        completer->popup()->sizeHintForColumn(0)
            + completer->popup()->verticalScrollBar()->sizeHint().width(),
        520));
    completer->complete(cr);
}

// ─────────────────────────────────────────────────────────────────────────────
//  insertCompletion  – triggered via QModelIndex signal; keyword from UserRole
// ─────────────────────────────────────────────────────────────────────────────
void KeywordCompleter::insertCompletion(const QModelIndex &index)
{
    const QString keyword = index.data(Qt::UserRole).toString();
    if (keyword.isEmpty()) return;

    const KeywordEntry *entry = findEntry(keyword);

    QTextCursor c = editor->textCursor();
    c.select(QTextCursor::LineUnderCursor);
    c.removeSelectedText();

    c.insertText(entry && !entry->templateText.isEmpty()
                     ? entry->templateText
                     : keyword + "\n");

    editor->setTextCursor(c);
}

// ─────────────────────────────────────────────────────────────────────────────
bool KeywordCompleter::cursorAtKeywordPosition() const
{
    return editor->textCursor().block().text().startsWith('*');
}

QString KeywordCompleter::currentWord() const
{
    return editor->textCursor().block().text().trimmed();
}

const KeywordEntry *KeywordCompleter::findEntry(const QString &keyword) const
{
    auto it = index_.find(keyword);
    return (it != index_.end()) ? &entries_[it.value()] : nullptr;
}
