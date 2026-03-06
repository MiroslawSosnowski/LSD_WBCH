#pragma once
#include <QDockWidget>
#include <QJsonObject>

class QLabel;
class QTableWidget;
class QVBoxLayout;

// ─────────────────────────────────────────────────────────────────────────────
//  MaterialPanel
//  Shows name + parameter list for the selected material, loaded from
//  materials_db.json.  Double-clicking a mat row in ModelTreePanel triggers
//  MainWindow::onMaterialSelected() which calls showMaterial().
// ─────────────────────────────────────────────────────────────────────────────
class MaterialPanel : public QDockWidget
{
    Q_OBJECT
public:
    explicit MaterialPanel(QWidget *parent = nullptr);

    void loadDatabase(const QString &jsonPath);
    void showMaterial(const QString &matKeyword);   // e.g. "MAT_024"
    void clear();

private:
    QWidget      *emptyState;
    QWidget      *contentWidget;
    QLabel       *keywordLabel;
    QLabel       *nameLabel;
    QTableWidget *paramsTable;

    QJsonObject database;
};
