#include "highlighter.h"
#include <QFont>

LSDynaHighlighter::LSDynaHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // ── 1. Numbers ───────────────────────────────────────────────────────────
    {
        HighlightRule r;
        QTextCharFormat f;
        f.setForeground(QColor("#B5CEA8"));
        r.pattern = QRegularExpression(R"(\b-?[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\b)");
        r.format  = f;
        rules.append(r);
    }

    // ── 2. General keywords  (*NODE, *ELEMENT_*, *PART, *CONTROL_*, …) ──────
    {
        HighlightRule r;
        QTextCharFormat f;
        f.setForeground(QColor("#569CD6"));
        f.setFontWeight(QFont::Bold);
        r.pattern = QRegularExpression(R"(^\*[A-Za-z_0-9]+)");
        r.format  = f;
        rules.append(r);
    }

    // ── 3. Material keywords  (*MAT_…) ───────────────────────────────────────
    {
        HighlightRule r;
        QTextCharFormat f;
        f.setForeground(QColor("#CE9178"));
        f.setFontWeight(QFont::Bold);
        r.pattern = QRegularExpression(R"(^\*MAT[_A-Za-z0-9]*)");
        r.format  = f;
        rules.append(r);
    }

    // ── 4. Include keywords  (*INCLUDE, *INCLUDE_TRANSFORM) ─────────────────
    {
        HighlightRule r;
        QTextCharFormat f;
        f.setForeground(QColor("#C586C0"));
        f.setFontWeight(QFont::Bold);
        r.pattern = QRegularExpression(R"(^\*INCLUDE(?:_TRANSFORM)?\b)");
        r.format  = f;
        rules.append(r);
    }

    // ── 5. End keyword  (*END) ───────────────────────────────────────────────
    {
        HighlightRule r;
        QTextCharFormat f;
        f.setForeground(QColor("#F48771"));
        f.setFontWeight(QFont::Bold);
        r.pattern = QRegularExpression(R"(^\*END\b)");
        r.format  = f;
        rules.append(r);
    }

    // ── 6. Comment lines  ($…) ────────────────────── must be last ──────────
    {
        HighlightRule r;
        QTextCharFormat f;
        f.setForeground(QColor("#6A9955"));
        f.setFontItalic(true);
        r.pattern = QRegularExpression(R"(^\$.*$)");
        r.format  = f;
        rules.append(r);
    }
}

void LSDynaHighlighter::highlightBlock(const QString &text)
{
    for (const auto &rule : rules)
    {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext())
        {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), rule.format);
        }
    }
}
