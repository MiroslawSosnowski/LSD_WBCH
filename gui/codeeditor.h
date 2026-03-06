#pragma once
#include <QPlainTextEdit>
#include <QWidget>
#include <QTextBlock>

class LineNumberArea;
class FoldManager;
class KeywordCompleter;

// ─────────────────────────────────────────────────────────────────────────────
//  CodeEditor  –  QPlainTextEdit with VS-Code-dark styling + line numbers
//                 + fold markers + keyword autocomplete
// ─────────────────────────────────────────────────────────────────────────────
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT
    friend class LineNumberArea;
    friend class FoldManager;
public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int  lineNumberAreaWidth() const;

    // Set after construction (needs keyword DB loaded first)
    void setKeywordCompleter(KeywordCompleter *kc);

    FoldManager      *foldManager()     const { return folds; }
    KeywordCompleter *keywordCompleter()const { return completer; }

    // Public wrappers for protected QPlainTextEdit methods
    QTextBlock firstVisibleBlock() const { return QPlainTextEdit::firstVisibleBlock(); }
    QRectF blockBoundingGeometry(const QTextBlock &block) const { return QPlainTextEdit::blockBoundingGeometry(block); }
    QPointF contentOffset() const { return QPlainTextEdit::contentOffset(); }
    QRectF blockBoundingRect(const QTextBlock &block) const { return QPlainTextEdit::blockBoundingRect(block); }

public slots:
    void jumpToLine(int lineNumber);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent   *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    int gutterWidth() const;   // line numbers + fold marker column

    QWidget          *lineNumberArea;
    FoldManager      *folds     = nullptr;
    KeywordCompleter *completer = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor)
        : QWidget(editor), codeEditor(editor) {}

    QSize sizeHint() const override
    { return QSize(codeEditor->lineNumberAreaWidth(), 0); }

protected:
    void paintEvent(QPaintEvent *event) override
    { codeEditor->lineNumberAreaPaintEvent(event); }

    void mousePressEvent(QMouseEvent *event) override;

private:
    CodeEditor *codeEditor;
};
