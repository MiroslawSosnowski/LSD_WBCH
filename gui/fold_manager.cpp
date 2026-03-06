#include "fold_manager.h"
#include "codeeditor.h"
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QPainter>

static bool isFoldableKeyword(const QString &line)
{
    const QString t = line.trimmed().toUpper();
    return t.startsWith("*NODE")          ||
           t.startsWith("*ELEMENT_")      ||
           t.startsWith("*SET_NODE_LIST") ||
           t.startsWith("*SET_SHELL")     ||
           t.startsWith("*SET_SEGMENT");
}

// ─────────────────────────────────────────────────────────────────────────────
FoldManager::FoldManager(QPlainTextEdit *ed, QObject *parent)
    : QObject(parent), editor(ed)
{}

// Helper to cast to CodeEditor for accessing public wrapper methods
static CodeEditor *toCodeEditor(QPlainTextEdit *ed)
{
    return qobject_cast<CodeEditor *>(ed);
}

// ─────────────────────────────────────────────────────────────────────────────
void FoldManager::rescan()
{
    // Unfold everything first so block heights are restored
    for (auto &r : regions)
        if (r.folded)
            applyFold(r, false);
    regions.clear();

    QTextDocument *doc    = editor->document();
    QTextBlock     block  = doc->begin();
    int            lineNum = 1;
    FoldRegion     current;
    bool           inRegion = false;

    while (block.isValid())
    {
        const QString text = block.text();

        if (text.startsWith('*') || text.startsWith('$'))
        {
            if (inRegion && current.lastDataLine >= current.firstDataLine)
                regions[current.keywordLine] = current;
            inRegion = false;

            if (isFoldableKeyword(text))
            {
                current               = FoldRegion{};
                current.keywordLine   = lineNum;
                current.firstDataLine = lineNum + 1;
                current.lastDataLine  = lineNum;
                current.keyword       = text.trimmed().split(' ').first();
                inRegion              = true;
            }
        }
        else if (inRegion)
        {
            current.lastDataLine = lineNum;
        }

        block = block.next();
        ++lineNum;
    }

    if (inRegion && current.lastDataLine >= current.firstDataLine)
        regions[current.keywordLine] = current;

    emit foldChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
void FoldManager::toggleFold(int lineNumber)
{
    auto it = regions.find(lineNumber);
    if (it == regions.end()) return;

    FoldRegion &r = it.value();
    r.folded = !r.folded;
    applyFold(r, r.folded);
    emit foldChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
//  applyFold  –  hide / show data lines by setting block visibility.
//  QTextBlock::setVisible works in both QTextEdit and QPlainTextEdit;
//  markContentsDirty per block forces the layout engine to reflow.
// ─────────────────────────────────────────────────────────────────────────────
void FoldManager::applyFold(FoldRegion &region, bool fold)
{
    QTextDocument *doc   = editor->document();
    QTextCursor    cur   = editor->textCursor();
    cur.beginEditBlock();

    QTextBlock block = doc->findBlockByLineNumber(region.firstDataLine - 1);
    for (int ln = region.firstDataLine; ln <= region.lastDataLine; ++ln)
    {
        if (!block.isValid()) break;

        block.setVisible(!fold);
        // Force layout to recalculate this block's geometry
        doc->markContentsDirty(block.position(), block.length());

        block = block.next();
    }

    cur.endEditBlock();
    editor->viewport()->update();
}

// ─────────────────────────────────────────────────────────────────────────────
const FoldRegion *FoldManager::regionAt(int keywordLine) const
{
    auto it = regions.find(keywordLine);
    return (it != regions.end()) ? &it.value() : nullptr;
}

bool FoldManager::isLineHidden(int lineNumber) const
{
    for (const auto &r : regions)
        if (r.folded &&
            lineNumber >= r.firstDataLine &&
            lineNumber <= r.lastDataLine)
            return true;
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  paintFoldMarkers  –  draw ▶ / ▼ in the gutter for foldable blocks.
//  Called ONCE from CodeEditor::lineNumberAreaPaintEvent.
// ─────────────────────────────────────────────────────────────────────────────
void FoldManager::paintFoldMarkers(QPainter &painter, const QRect &eventRect) const
{
    if (regions.isEmpty()) return;

    auto *codeEditor = toCodeEditor(editor);
    if (!codeEditor) return;

    QTextBlock block  = codeEditor->firstVisibleBlock();
    int        lineNo = block.blockNumber() + 1;

    const QFontMetrics fm    = editor->fontMetrics();
    const int          lineH = fm.height() + 2;

    while (block.isValid())
    {
        const int top = qRound(
            codeEditor->blockBoundingGeometry(block)
                   .translated(codeEditor->contentOffset()).top());

        if (top > eventRect.bottom()) break;

        if (block.isVisible() && top + lineH >= eventRect.top())
        {
            auto it = regions.find(lineNo);
            if (it != regions.end())
            {
                const FoldRegion &r = it.value();
                painter.setPen(r.folded ? QColor("#569CD6") : QColor("#555555"));
                painter.setFont(editor->font());
                painter.drawText(
                    QRect(2, top, 14, lineH),
                    Qt::AlignCenter,
                    r.folded ? QStringLiteral("▶") : QStringLiteral("▼"));
            }
        }

        block = block.next();
        ++lineNo;
    }
}
