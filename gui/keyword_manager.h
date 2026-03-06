#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  keyword_manager.h
// ═════════════════════════════════════════════════════════════════════════════
#include <QDockWidget>
#include "keyword_completer.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QPlainTextEdit;

class KeywordManager : public QDockWidget
{
    Q_OBJECT
public:
    explicit KeywordManager(QWidget *parent = nullptr);

    void setEntries(const QList<KeywordEntry> &entries);
    void setEditor(QPlainTextEdit *ed) { editor = ed; }

private slots:
    void onFilterChanged(const QString &text);
    void onItemDoubleClicked(QTreeWidgetItem *item, int col);

private:
    void rebuild(const QString &filter = {});

    QLineEdit      *searchBox;
    QTreeWidget    *tree;
    QPlainTextEdit *editor = nullptr;

    QList<KeywordEntry> allEntries;
};
