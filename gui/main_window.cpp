#include "main_window.h"
#include "codeeditor.h"
#include "highlighter.h"
#include "fold_manager.h"
#include "keyword_completer.h"
#include "keyword_manager.h"
#include "workspace_panel.h"
#include "model_tree_panel.h"
#include "material_panel.h"
#include "include_navigator.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QKeySequence>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QApplication>
#include <QStatusBar>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("LS-DYNA Workbench");
    resize(1600, 960);
    applyDarkTheme();

    // ── Central: include navigator + editor ──────────────────────────────────
    auto *central = new QWidget(this);
    auto *vl      = new QVBoxLayout(central);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    navigator   = new IncludeNavigator(central);
    editor      = new CodeEditor(central);
    highlighter = new LSDynaHighlighter(editor->document());

    kwCompleter = new KeywordCompleter(editor, this);
    const QString kwDb = findResourceFile("keywords_db.json");
    if (!kwDb.isEmpty())
        kwCompleter->loadDatabase(kwDb);
    editor->setKeywordCompleter(kwCompleter);

    vl->addWidget(navigator);
    vl->addWidget(editor, 1);
    setCentralWidget(central);

    createDocks();
    createMenu();
    createStatusBar();

    connect(navigator, &IncludeNavigator::fileRequested,
            this, [this](const QString &p, int id){ onIncludeFileRequested(p, id); });
}

MainWindow::~MainWindow() = default;

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::applyDarkTheme()
{
    setStyleSheet(R"(
        QMainWindow, QWidget { background:#1E1E1E; color:#CCCCCC; }
        QMenuBar              { background:#3C3C3C; color:#CCCCCC; }
        QMenuBar::item:selected { background:#505050; }
        QMenu { background:#252526; color:#CCCCCC; border:1px solid #454545; }
        QMenu::item:selected  { background:#094771; }
        QMenu::separator      { background:#454545; height:1px; margin:2px 4px; }
        QDockWidget           { color:#CCCCCC; }
        QDockWidget::title    { background:#2D2D30; padding:5px 8px; font-weight:bold; }
        QStatusBar            { background:#007ACC; color:#FFFFFF; font-size:11px; }
        QProgressBar {
            background:#005F9E; border:none; color:#FFFFFF;
            text-align:center; font-size:10px; max-height:14px;
        }
        QProgressBar::chunk   { background:#1C97EA; }
        QScrollBar:vertical   { background:#1E1E1E; width:10px; }
        QScrollBar::handle:vertical   { background:#424242; border-radius:5px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QScrollBar:horizontal { background:#1E1E1E; height:10px; }
        QScrollBar::handle:horizontal { background:#424242; border-radius:5px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }
    )");
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::createDocks()
{
    // ── Left: workspace panel (top) ───────────────────────────────────────────
    workspacePanel = new WorkspacePanel(this);
    workspacePanel->setMinimumWidth(220);
    addDockWidget(Qt::LeftDockWidgetArea, workspacePanel);

    // ── Left: model tree (tabbed below workspace) ─────────────────────────────
    modelTree = new ModelTreePanel(this);
    modelTree->setMinimumWidth(220);
    addDockWidget(Qt::LeftDockWidgetArea, modelTree);
    tabifyDockWidget(workspacePanel, modelTree);
    workspacePanel->raise();   // workspace tab active by default

    // ── Right-top: keyword manager ────────────────────────────────────────────
    kwManager = new KeywordManager(this);
    kwManager->setEditor(editor);
    kwManager->setMinimumWidth(260);
    kwManager->setEntries(kwCompleter->entries());
    addDockWidget(Qt::RightDockWidgetArea, kwManager);

    // ── Right-bottom: material panel ──────────────────────────────────────────
    materialPanel = new MaterialPanel(this);
    materialPanel->setMinimumWidth(220);
    addDockWidget(Qt::RightDockWidgetArea, materialPanel);
    splitDockWidget(kwManager, materialPanel, Qt::Vertical);

    const QString matDb = findResourceFile("materials_db.json");
    if (!matDb.isEmpty())
        materialPanel->loadDatabase(matDb);

    // ── Signals ───────────────────────────────────────────────────────────────
    connect(workspacePanel, &WorkspacePanel::fileOpenRequested,
            this,           &MainWindow::loadFile);

    connect(modelTree, &ModelTreePanel::materialSelected,
            this,      &MainWindow::onMaterialSelected);
    connect(modelTree, &ModelTreePanel::includeFileDoubleClicked,
            this,      [this](const QString &p){ onIncludeFileRequested(p, -1); });
    connect(modelTree, &ModelTreePanel::jumpToLine,
            editor,    &CodeEditor::jumpToLine);
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::createStatusBar()
{
    statusLabel = new QLabel("Ready", this);
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setVisible(false);
    progressBar->setFixedWidth(180);
    statusBar()->addWidget(statusLabel, 1);
    statusBar()->addPermanentWidget(progressBar);
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::createMenu()
{
    // ── File ──────────────────────────────────────────────────────────────────
    QMenu *fileMenu   = menuBar()->addMenu("&File");

    auto *openDirAct  = new QAction("Open &Directory…", this);
    auto *openFileAct = new QAction("Open &File…",      this);
    auto *exitAct     = new QAction("E&xit",             this);

    openDirAct->setShortcut(QKeySequence("Ctrl+Shift+O"));
    openFileAct->setShortcut(QKeySequence::Open);

    fileMenu->addAction(openDirAct);
    fileMenu->addAction(openFileAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    connect(openDirAct,  &QAction::triggered, this, &MainWindow::openDirectory);
    connect(openFileAct, &QAction::triggered, this, &MainWindow::openFile);
    connect(exitAct,     &QAction::triggered, this, &MainWindow::close);

    // ── View ──────────────────────────────────────────────────────────────────
    QMenu *viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(workspacePanel->toggleViewAction());
    viewMenu->addAction(modelTree->toggleViewAction());
    viewMenu->addAction(kwManager->toggleViewAction());
    viewMenu->addAction(materialPanel->toggleViewAction());

    // ── Edit ──────────────────────────────────────────────────────────────────
    QMenu *editMenu     = menuBar()->addMenu("&Edit");
    auto *foldAllAct    = new QAction("Fold all data blocks",   this);
    auto *unfoldAllAct  = new QAction("Unfold all data blocks", this);
    foldAllAct->setShortcut(QKeySequence("Ctrl+["));
    unfoldAllAct->setShortcut(QKeySequence("Ctrl+]"));
    editMenu->addAction(foldAllAct);
    editMenu->addAction(unfoldAllAct);

    connect(foldAllAct, &QAction::triggered, this, [this]() {
        auto *fm = editor->foldManager();
        if (!fm) return;
        for (int i = 1; i <= editor->document()->blockCount(); ++i)
            if (const FoldRegion *r = fm->regionAt(i); r && !r->folded)
                fm->toggleFold(i);
    });
    connect(unfoldAllAct, &QAction::triggered, this, [this]() {
        auto *fm = editor->foldManager();
        if (!fm) return;
        for (int i = 1; i <= editor->document()->blockCount(); ++i)
            if (const FoldRegion *r = fm->regionAt(i); r && r->folded)
                fm->toggleFold(i);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::openDirectory()
{
    const QString dirPath = QFileDialog::getExistingDirectory(
        this, "Open LS-DYNA Workspace Directory",
        currentFilePath.isEmpty()
            ? QDir::homePath()
            : QFileInfo(currentFilePath).absolutePath());

    if (dirPath.isEmpty()) return;

    workspacePanel->loadDirectory(dirPath);
    workspacePanel->raise();   // switch to workspace tab
    setWindowTitle("LS-DYNA Workbench  —  " + QDir(dirPath).dirName());
    statusLabel->setText("Workspace: " + dirPath);
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::openFile()
{
    const QString fn = QFileDialog::getOpenFileName(
        this, "Open LS-DYNA file",
        currentFilePath.isEmpty()
            ? QDir::homePath()
            : QFileInfo(currentFilePath).absolutePath(),
        "LS-DYNA files (*.k *.dyn *.key *.inc *.txt);;All files (*)");
    if (!fn.isEmpty()) loadFile(fn);
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::loadFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Error", "Cannot open file:\n" + filePath);
        return;
    }

    const QString content = QTextStream(&file).readAll();
    editor->setPlainText(content);
    currentFilePath = filePath;
    setWindowTitle("LS-DYNA Workbench  —  " + QFileInfo(filePath).fileName());

    if (editor->foldManager())
        editor->foldManager()->rescan();

    // Switch to model tree tab to show block summary
    modelTree->loadFromContent(content, filePath);
    modelTree->raise();

    materialPanel->clear();

    // Parse (populates db.files)
    runParser(filePath);

    // Update navigator after parse
    {
        int fid = 0;
        for (const auto &fe : db.files)
            if (QString::fromStdString(fe.path) == filePath)
                { fid = fe.id; break; }

        if (db.files.isEmpty())
            navigator->setRoot(0, filePath, db.files);
        else
            navigator->navigateTo(fid, filePath, db.files);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::runParser(const QString &filePath)
{
    progressBar->setValue(0);
    progressBar->setVisible(true);
    statusLabel->setText("Scanning…");
    QApplication::processEvents();

    parser = std::make_unique<BlockParser>(db);
    parser->scan(filePath, [this](int pct, const QString &msg) {
        progressBar->setValue(pct / 2);
        statusLabel->setText(msg);
        QApplication::processEvents();
    });

    parser->parseAll([this](int pct, const QString &msg) {
        progressBar->setValue(50 + pct / 2);
        statusLabel->setText(msg);
        QApplication::processEvents();
    });

    // buildCrossReferenceGraph already called inside parseAll
    query = std::make_unique<QueryEngine>(db);

    progressBar->setVisible(false);
    statusLabel->setText(query->summaryText().replace('\n', "   "));
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onMaterialSelected(const QString &matKeyword)
{
    materialPanel->showMaterial(matKeyword);
    materialPanel->show();
    materialPanel->raise();
}

void MainWindow::onIncludeFileRequested(const QString &absPath, int /*fileId*/)
{
    if (QFileInfo::exists(absPath))
        loadFile(absPath);
    else
        QMessageBox::information(this, "Include File",
            "File not found:\n" + absPath);
}

// ─────────────────────────────────────────────────────────────────────────────
QString MainWindow::findResourceFile(const QString &filename) const
{
    for (const QString &d : {
            QDir::currentPath(),
            QApplication::applicationDirPath(),
            QApplication::applicationDirPath() + "/..",
            QApplication::applicationDirPath() + "/../.." })
    {
        const QString full = QDir(d).absoluteFilePath(filename);
        if (QFileInfo::exists(full)) return full;
    }
    return {};
}
