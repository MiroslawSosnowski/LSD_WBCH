#include "codeeditor.h"
#include "fold_manager.h"
#include "keyword_completer.h"
#include <QPainter>
#include <QTextBlock>
#include <QKeyEvent>
#include <QMouseEvent>

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    lineNumberArea = new LineNumberArea(this);

    QPalette p = palette();
    p.setColor(QPalette::Base,            QColor("#1E1E1E"));
    p.setColor(QPalette::Text,            QColor("#D4D4D4"));
    p.setColor(QPalette::Highlight,       QColor("#264F78"));
    p.setColor(QPalette::HighlightedText, QColor("#D4D4D4"));
    setPalette(p);

    QFont font("Courier New", 10);
    font.setFixedPitch(true);
    setFont(font);
    setLineWrapMode(QPlainTextEdit::NoWrap);

    folds = new FoldManager(this, this);

    connect(this,  &CodeEditor::blockCountChanged,
            this,  &CodeEditor::updateLineNumberAreaWidth);
    connect(this,  &CodeEditor::updateRequest,
            this,  &CodeEditor::updateLineNumberArea);
    connect(this,  &CodeEditor::cursorPositionChanged,
            this,  &CodeEditor::highlightCurrentLine);
    connect(folds, &FoldManager::foldChanged,
            lineNumberArea, QOverload<>::of(&QWidget::update));

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

void CodeEditor::setKeywordCompleter(KeywordCompleter *kc)
{
    completer = kc;
}

// ── Gutter = 18 px fold column + line-number digits + 6 px padding ───────────
int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max    = qMax(1, blockCount());
    while (max >= 10) { max /= 10; ++digits; }
    return 18 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 6;
}

void CodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    lineNumberArea->setGeometry(
        QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

// ── Key events ────────────────────────────────────────────────────────────────
void CodeEditor::keyPressEvent(QKeyEvent *e)
{
    // While popup is open, navigation keys must reach the popup, not the editor
    if (completer && completer->popup()->isVisible())
    {
        switch (e->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            e->ignore();   // QCompleter's event filter will handle this
            return;
        default:
            break;
        }
    }

    QPlainTextEdit::keyPressEvent(e);

    // Only update completer for printable characters or backspace
    if (completer)
    {
        const int k = e->key();
        const bool ignore =
            e->modifiers() & (Qt::ControlModifier | Qt::AltModifier) ||
            (k == Qt::Key_Left   || k == Qt::Key_Right  ||
             k == Qt::Key_Up     || k == Qt::Key_Down   ||
             k == Qt::Key_Home   || k == Qt::Key_End    ||
             k == Qt::Key_PageUp || k == Qt::Key_PageDown);
        if (!ignore)
            completer->handleKeyPress(k, e->modifiers());
    }
}

void CodeEditor::mousePressEvent(QMouseEvent *event)
{
    if (completer) completer->hidePopup();
    QPlainTextEdit::mousePressEvent(event);
}

// ── Current-line highlight ────────────────────────────────────────────────────
void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extras;
    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(QColor("#2D2D30"));
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = textCursor();
        sel.cursor.clearSelection();
        extras.append(sel);
    }
    setExtraSelections(extras);
}

// ── Paint gutter ──────────────────────────────────────────────────────────────
void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), QColor("#252526"));

    // Right border
    painter.setPen(QColor("#3C3C3C"));
    painter.drawLine(lineNumberArea->width() - 1, event->rect().top(),
                     lineNumberArea->width() - 1, event->rect().bottom());

    // ── Line numbers ──────────────────────────────────────────────────────────
    QTextBlock block       = firstVisibleBlock();
    int        blockNumber = block.blockNumber();
    int        top    = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int        bottom = top + qRound(blockBoundingRect(block).height());

    const int numAreaLeft  = 18;
    const int numAreaWidth = lineNumberArea->width() - 22;

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            painter.setPen(blockNumber == textCursor().blockNumber()
                               ? QColor("#C6C6C6")
                               : QColor("#858585"));
            painter.drawText(numAreaLeft, top,
                             numAreaWidth, fontMetrics().height(),
                             Qt::AlignRight,
                             QString::number(blockNumber + 1));
        }
        block = block.next();
        top    = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }

    // ── Fold markers (drawn once, after line numbers) ─────────────────────────
    if (folds)
        folds->paintFoldMarkers(painter, event->rect());
}

// ── Jump to line ──────────────────────────────────────────────────────────────
void CodeEditor::jumpToLine(int lineNumber)
{
    QTextBlock block = document()->findBlockByLineNumber(lineNumber - 1);
    if (block.isValid())
    {
        QTextCursor cursor(block);
        setTextCursor(cursor);
        centerCursor();
    }
}

// ── LineNumberArea mouse: toggle fold on left 16px ────────────────────────────
void LineNumberArea::mousePressEvent(QMouseEvent *event)
{
    FoldManager *fm = codeEditor->foldManager();
    if (!fm) return;

    if (event->position().toPoint().x() > 16) return;

    QTextBlock block  = codeEditor->firstVisibleBlock();
    int        lineNo = block.blockNumber() + 1;

    while (block.isValid())
    {
        const int top = qRound(
            codeEditor->blockBoundingGeometry(block)
                       .translated(codeEditor->contentOffset()).top());
        const int bottom = top + qRound(codeEditor->blockBoundingRect(block).height());

        if (event->position().toPoint().y() >= top &&
            event->position().toPoint().y() <  bottom)
        {
            fm->toggleFold(lineNo);
            return;
        }
        block = block.next();
        ++lineNo;
    }
}
