#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  keyword_completer.h
// ═════════════════════════════════════════════════════════════════════════════
#include <QObject>
#include <QString>
#include <QStringList>
#include <QColor>
#include <QMap>
#include <QCompleter>
#include <QPlainTextEdit>
#include <QAbstractItemView>

struct KeywordEntry {
    QString     keyword;
    QString     desc;
    QString     category;
    QColor      color;
    QString     templateText;
    QStringList fields;
};

class KeywordCompleter : public QObject
{
    Q_OBJECT
public:
    explicit KeywordCompleter(QPlainTextEdit *editor, QObject *parent = nullptr);

    bool loadDatabase(const QString &jsonPath);

    // Called by CodeEditor::keyPressEvent after the key is processed
    void handleKeyPress(int key, Qt::KeyboardModifiers mods);

    // Forwarding accessors so CodeEditor can manage popup lifecycle
    QAbstractItemView *popup() const;
    void               hidePopup();

    const QList<KeywordEntry> &entries()                          const { return entries_; }
    const KeywordEntry        *findEntry(const QString &keyword)  const;

private slots:
    void insertCompletion(const QModelIndex &index);   // uses UserRole data

private:
    void    updateCompleter();
    QString currentWord()             const;
    bool    cursorAtKeywordPosition() const;

    QPlainTextEdit     *editor;
    QCompleter         *completer;
    QList<KeywordEntry> entries_;
    QMap<QString, int>  index_;
};
