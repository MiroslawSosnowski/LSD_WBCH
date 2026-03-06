#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  include_navigator.h
// ═════════════════════════════════════════════════════════════════════════════
#include <QWidget>
#include <QVector>
#include <QString>

class QToolButton;
class QLabel;

struct FileEntry;

class IncludeNavigator : public QWidget
{
    Q_OBJECT
public:
    explicit IncludeNavigator(QWidget *parent = nullptr);

    void setRoot(int fileId, const QString &absPath, const QVector<FileEntry> &fileRegistry);
    void navigateTo(int fileId, const QString &absPath, const QVector<FileEntry> &fileRegistry);

signals:
    void fileRequested(const QString &filePath, int fileId);

public slots:
    void goBack();
    void goForward();

private:
    void rebuildBreadcrumb(const QVector<FileEntry> &reg);
    void updateButtons();
    QVector<FileEntry> buildChain(int fileId, const QVector<FileEntry> &reg) const;

    QToolButton *backBtn;
    QToolButton *fwdBtn;
    QToolButton *upBtn;
    QLabel      *breadcrumbLabel;

    QString currentFilePath;
    int      currentId = -1;

    QVector<QString>    history;
    QVector<int>         historyIds;
    int                  histPos = -1;
    const QVector<FileEntry> *lastRegistry = nullptr;
};