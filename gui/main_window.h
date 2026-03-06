#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H
#include <QMainWindow>
#include <QString>
#include <memory>

#include "model_database.h"
#include "block_parser.h"
#include "query_engine.h"

class CodeEditor;
class LSDynaHighlighter;
class WorkspacePanel;
class ModelTreePanel;
class MaterialPanel;
class KeywordManager;
class KeywordCompleter;
class IncludeNavigator;
class QProgressBar;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void openFile();
    void openDirectory();
    void onMaterialSelected(const QString &matKeyword);
    void onIncludeFileRequested(const QString &absPath, int fileId = -1);

private:
    void createMenu();
    void createDocks();
    void createStatusBar();
    void applyDarkTheme();

    void loadFile(const QString &filePath);
    void runParser(const QString &filePath);
    QString findResourceFile(const QString &filename) const;

    // ── Backend ───────────────────────────────────────────────────────────────
    ModelDatabase                db;
    std::unique_ptr<BlockParser> parser;
    std::unique_ptr<QueryEngine> query;

    // ── UI ────────────────────────────────────────────────────────────────────
    IncludeNavigator  *navigator;
    CodeEditor        *editor;
    LSDynaHighlighter *highlighter;
    KeywordCompleter  *kwCompleter;

    WorkspacePanel    *workspacePanel;
    ModelTreePanel    *modelTree;
    MaterialPanel     *materialPanel;
    KeywordManager    *kwManager;

    QLabel            *statusLabel;
    QProgressBar      *progressBar;

    QString currentFilePath;
};

#endif
