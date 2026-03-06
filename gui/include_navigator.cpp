#include "include_navigator.h"
#include "model_index.h"

#include <QHBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QFileInfo>
#include <QFont>

// ─────────────────────────────────────────────────────────────────────────────
IncludeNavigator::IncludeNavigator(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(34);
    setStyleSheet("background-color: #2D2D30; border-bottom: 1px solid #3C3C3C;");

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(4);

    auto makeBtn = [&](const QString &icon, const QString &tip) {
        auto *b = new QToolButton(this);
        b->setText(icon);
        b->setToolTip(tip);
        b->setFixedSize(24, 24);
        b->setStyleSheet(R"(
            QToolButton {
                background: transparent; color: #CCCCCC;
                border: none; border-radius: 3px; font-size: 14px;
            }
            QToolButton:hover   { background: #3E3E42; }
            QToolButton:pressed { background: #094771; }
            QToolButton:disabled{ color: #555555; }
        )");
        return b;
    };

    backBtn        = makeBtn("←", "Back (Alt+Left)");
    fwdBtn         = makeBtn("→", "Forward (Alt+Right)");
    upBtn          = makeBtn("⬆", "Go to parent file");

    breadcrumbLabel = new QLabel(this);
    breadcrumbLabel->setStyleSheet(
        "color: #9CDCFE; font-size: 12px; padding: 0 6px;");
    breadcrumbLabel->setTextFormat(Qt::RichText);
    breadcrumbLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);

    layout->addWidget(backBtn);
    layout->addWidget(fwdBtn);
    layout->addWidget(new QLabel("|", this));   // divider
    layout->addWidget(breadcrumbLabel, 1);
    layout->addWidget(upBtn);

    backBtn->setEnabled(false);
    fwdBtn->setEnabled(false);
    upBtn->setEnabled(false);

    connect(backBtn, &QToolButton::clicked, this, &IncludeNavigator::goBack);
    connect(fwdBtn,  &QToolButton::clicked, this, &IncludeNavigator::goForward);
    connect(upBtn,   &QToolButton::clicked, this, [this]() {
        if (!lastRegistry) return;
        // find parent
        for (const auto &fe : *lastRegistry)
            if (fe.id == currentId && fe.parentId >= 0)
            {
                for (const auto &pf : *lastRegistry)
                    if (pf.id == fe.parentId)
                    {
                        navigateTo(pf.id, QString::fromStdString(pf.path), *lastRegistry);
                        return;
                    }
            }
    });

    connect(breadcrumbLabel, &QLabel::linkActivated, this, [this](const QString &link) {
        emit fileRequested(link, -1);   // let MainWindow resolve id
    });
}

// ─────────────────────────────────────────────────────────────────────────────
void IncludeNavigator::setRoot(int fileId, const QString &absPath,
                               const QVector<FileEntry> &fileRegistry)
{
    history.clear();
    historyIds.clear();
    histPos = -1;
    navigateTo(fileId, absPath, fileRegistry);
}

void IncludeNavigator::navigateTo(int fileId, const QString &absPath,
                                  const QVector<FileEntry> &fileRegistry)
{
    lastRegistry = &fileRegistry;

    // Truncate forward history
    if (histPos < static_cast<int>(history.size()) - 1)
    {
        history    = history.mid(0, histPos + 1);
        historyIds = historyIds.mid(0, histPos + 1);
    }

    history.append(absPath);
    historyIds.append(fileId);
    histPos = history.size() - 1;

    currentFilePath = absPath;
    currentId       = fileId;

    rebuildBreadcrumb(fileRegistry);
    updateButtons();
}

// ─────────────────────────────────────────────────────────────────────────────
void IncludeNavigator::goBack()
{
    if (histPos <= 0 || !lastRegistry) return;
    --histPos;
    currentFilePath = history[histPos];
    currentId       = historyIds[histPos];
    rebuildBreadcrumb(*lastRegistry);
    updateButtons();
    emit fileRequested(currentFilePath, currentId);
}

void IncludeNavigator::goForward()
{
    if (histPos >= static_cast<int>(history.size()) - 1 || !lastRegistry) return;
    ++histPos;
    currentFilePath = history[histPos];
    currentId       = historyIds[histPos];
    rebuildBreadcrumb(*lastRegistry);
    updateButtons();
    emit fileRequested(currentFilePath, currentId);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Build the ancestry chain for the breadcrumb
// ─────────────────────────────────────────────────────────────────────────────
QVector<FileEntry> IncludeNavigator::buildChain(int fileId,
    const QVector<FileEntry> &reg) const
{
    QVector<FileEntry> chain;
    int current = fileId;
    while (current >= 0)
    {
        for (const auto &fe : reg)
        {
            if (fe.id == current)
            {
                chain.prepend(fe);
                current = fe.parentId;
                goto next;
            }
        }
        break;
        next:;
    }
    return chain;
}

void IncludeNavigator::rebuildBreadcrumb(const QVector<FileEntry> &reg)
{
    const QVector<FileEntry> chain = buildChain(currentId, reg);

    QStringList parts;
    for (int i = 0; i < chain.size(); ++i)
    {
        const QString name = QString::fromStdString(chain[i].name);
        const QString path = QString::fromStdString(chain[i].path);
        const bool isCurrent = (i == chain.size() - 1);

        if (isCurrent)
            parts << QString("<b style='color:#DCDCAA'>%1</b>").arg(name.toHtmlEscaped());
        else
            parts << QString("<a href='%1' style='color:#4FC1FF;text-decoration:none'>%2</a>")
                         .arg(path.toHtmlEscaped(), name.toHtmlEscaped());
    }
    breadcrumbLabel->setText(parts.join("  <span style='color:#555'>›</span>  "));

    // Enable "up" button only if there is a parent
    upBtn->setEnabled(chain.size() > 1);
}

void IncludeNavigator::updateButtons()
{
    backBtn->setEnabled(histPos > 0);
    fwdBtn->setEnabled(histPos < static_cast<int>(history.size()) - 1);
}
