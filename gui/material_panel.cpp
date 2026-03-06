#include "material_panel.h"
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

MaterialPanel::MaterialPanel(QWidget *parent)
    : QDockWidget("Material Properties", parent)
{
    QWidget     *container = new QWidget(this);
    QVBoxLayout *layout    = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    container->setStyleSheet("background-color: #252526; color: #CCCCCC;");

    // ── Empty / hint state ───────────────────────────────────────────────────
    emptyState = new QWidget(container);
    auto *emptyLayout = new QVBoxLayout(emptyState);
    auto *hint = new QLabel("Double-click a material\nin the Model Tree", emptyState);
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color: #555555; font-style: italic; font-size: 12px;");
    emptyLayout->addStretch();
    emptyLayout->addWidget(hint);
    emptyLayout->addStretch();

    // ── Content state ────────────────────────────────────────────────────────
    contentWidget = new QWidget(container);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(4);

    keywordLabel = new QLabel(contentWidget);
    keywordLabel->setStyleSheet(
        "color: #CE9178; font-weight: bold; font-size: 14px; padding: 4px 0;");

    nameLabel = new QLabel(contentWidget);
    nameLabel->setStyleSheet(
        "color: #9CDCFE; font-size: 11px; margin-bottom: 6px;");
    nameLabel->setWordWrap(true);

    auto *paramsHeader = new QLabel("Parameters", contentWidget);
    paramsHeader->setStyleSheet(
        "color: #858585; font-size: 10px; text-transform: uppercase; "
        "letter-spacing: 1px; margin-top: 6px;");

    paramsTable = new QTableWidget(contentWidget);
    paramsTable->setColumnCount(1);
    paramsTable->setHorizontalHeaderLabels({"Name"});
    paramsTable->horizontalHeader()->setStretchLastSection(true);
    paramsTable->horizontalHeader()->setStyleSheet(
        "QHeaderView::section {"
        "  background-color: #2D2D30; color: #9CDCFE;"
        "  border: none; padding: 4px; font-size: 11px; }");
    paramsTable->setStyleSheet(R"(
        QTableWidget {
            background-color: #1E1E1E;
            color: #D4D4D4;
            gridline-color: #3C3C3C;
            border: 1px solid #3C3C3C;
            font-size: 12px;
        }
        QTableWidget::item          { padding: 4px 6px; }
        QTableWidget::item:selected { background-color: #264F78; color: #D4D4D4; }
    )");
    paramsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    paramsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    paramsTable->verticalHeader()->setVisible(false);
    paramsTable->verticalHeader()->setDefaultSectionSize(24);

    contentLayout->addWidget(keywordLabel);
    contentLayout->addWidget(nameLabel);
    contentLayout->addWidget(paramsHeader);
    contentLayout->addWidget(paramsTable);
    contentWidget->setVisible(false);

    layout->addWidget(emptyState);
    layout->addWidget(contentWidget);
    setWidget(container);
}

// ─── public API ──────────────────────────────────────────────────────────────

void MaterialPanel::loadDatabase(const QString &jsonPath)
{
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isObject())
        database = doc.object();
}

void MaterialPanel::showMaterial(const QString &matKeyword)
{
    keywordLabel->setText("*" + matKeyword);

    if (!database.contains(matKeyword))
    {
        nameLabel->setText("(no entry in materials_db.json)");
        paramsTable->setRowCount(0);
    }
    else
    {
        const QJsonObject mat    = database[matKeyword].toObject();
        const QString     name   = mat["name"].toString();
        const QJsonArray  params = mat["params"].toArray();

        nameLabel->setText(name);
        paramsTable->setRowCount(params.size());

        for (int i = 0; i < params.size(); ++i)
        {
            auto *item = new QTableWidgetItem(params[i].toString());
            item->setForeground(QColor("#B5CEA8"));
            paramsTable->setItem(i, 0, item);
        }
    }

    emptyState->setVisible(false);
    contentWidget->setVisible(true);
}

void MaterialPanel::clear()
{
    contentWidget->setVisible(false);
    emptyState->setVisible(true);
}
