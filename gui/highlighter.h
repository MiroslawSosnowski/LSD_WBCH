#pragma once
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

class LSDynaHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit LSDynaHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightRule
    {
        QRegularExpression pattern;
        QTextCharFormat    format;
    };
    QVector<HighlightRule> rules;
};
