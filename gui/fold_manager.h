#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  fold_manager.h
// ═════════════════════════════════════════════════════════════════════════════
#include <QObject>
#include <QMap>
#include <QString>
#include <QRect>

class QPlainTextEdit;
class QPainter;

struct FoldRegion {
    int     keywordLine   = 0;
    int     firstDataLine = 0;
    int     lastDataLine  = 0;
    bool    folded        = false;
    QString keyword;
    int     lineCount() const { return lastDataLine - firstDataLine + 1; }
};

class FoldManager : public QObject
{
    Q_OBJECT
public:
    explicit FoldManager(QPlainTextEdit *editor, QObject *parent = nullptr);

    void rescan();
    void toggleFold(int lineNumber);

    // Returns fold region for a keyword line (nullptr if none)
    const FoldRegion *regionAt(int keywordLine) const;

    // Paint ▶ / ▼ markers – called from LineNumberArea::paintEvent
    // Pass the gutter painter and the visible event rect.
    void paintFoldMarkers(QPainter &painter, const QRect &eventRect) const;

    bool isLineHidden(int lineNumber) const;
    int  regionCount()               const { return regions.size(); }

signals:
    void foldChanged();

private:
    void applyFold(FoldRegion &region, bool fold);

    QPlainTextEdit      *editor;
    QMap<int, FoldRegion> regions;   // keywordLine → region
};
