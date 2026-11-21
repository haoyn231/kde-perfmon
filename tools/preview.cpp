// Development tool: renders the real popup QML with the real backend into PNG
// frames so the UI can be inspected without a running panel. It also reports
// the Kirigami colors before and after the system color scheme changes.
#include <QAbstractItemModel>

#include <algorithm>
#include <QGuiApplication>
#include <QImage>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTest>
#include <QTimer>
#include <QWheelEvent>

#include <cstdio>
#include <functional>

namespace {

void dumpTexts(QQuickItem *item, int depth)
{
    if (!item) {
        return;
    }
    // Delegate details/confirmation rows exist in the item tree even when
    // collapsed; only report what is actually visible.
    if (!item->isVisible()) {
        return;
    }
    if (item->metaObject()->indexOfProperty("text") >= 0) {
        const QString text = item->property("text").toString();
        if (!text.isEmpty()) {
            const QPointF pos = item->mapToScene(QPointF(0, 0));
            std::fprintf(stderr, "TEXT %s  x=%.0f y=%.0f w=%.0f h=%.0f  \"%s\"\n",
                         item->metaObject()->className(), pos.x(), pos.y(),
                         item->width(), item->height(), text.toUtf8().constData());
        }
    }
    if (item->metaObject()->indexOfProperty("status") >= 0 && item->metaObject()->indexOfProperty("source") >= 0) {
        const QPointF pos = item->mapToScene(QPointF(0, 0));
        std::fprintf(stderr, "IMAGE %s x=%.0f y=%.0f w=%.0f h=%.0f status=%d source=\"%s\"\n",
                     item->objectName().toUtf8().constData(), pos.x(), pos.y(),
                     item->width(), item->height(), item->property("status").toInt(),
                     item->property("source").toUrl().toString().toUtf8().constData());
    }
    const QList<QQuickItem *> children = item->childItems();
    for (QQuickItem *child : children) {
        dumpTexts(child, depth + 1);
    }
}

void collectItemsWithProperty(QQuickItem *item, const char *name, QList<QQuickItem *> &out)
{
    if (!item) {
        return;
    }
    if (item->metaObject()->indexOfProperty(name) >= 0) {
        out.append(item);
    }
    for (QQuickItem *child : item->childItems()) {
        collectItemsWithProperty(child, name, out);
    }
}

QQuickItem *topmostProcessRow(QQuickItem *root)
{
    QList<QQuickItem *> rows;
    collectItemsWithProperty(root, "pid", rows);
    QQuickItem *best = nullptr;
    qreal bestY = 1e9;
    for (QQuickItem *row : rows) {
        const qreal y = row->mapToScene(QPointF(0, 0)).y();
        if (y < bestY) {
            bestY = y;
            best = row;
        }
    }
    return best;
}

qreal listContentY(QQuickItem *list)
{
    return list ? list->property("contentY").toDouble() : -1.0;
}

// Highest CPU value in the model: proves a frozen frame kept live numbers
// instead of the zeroed rows a fresh sampling window produces.
double maxProcessCpu(QQuickItem *list)
{
    auto *model = list ? qobject_cast<QAbstractItemModel *>(list->property("model").value<QObject *>()) : nullptr;
    if (!model) {
        return -1.0;
    }
    double best = 0.0;
    for (int row = 0; row < model->rowCount(); ++row) {
        best = qMax(best, model->index(row, 0).data(Qt::UserRole + 5).toDouble());
    }
    return best;
}

// One mouse-wheel notch, delivered to the view like a real wheel event.
void sendWheelNotch(QQuickView *view, QQuickItem *list, int notches)
{
    const QPointF origin = list->mapToScene(QPointF(0, 0));
    const QPointF target = origin + QPointF(list->width() / 2, list->height() / 2);
    QWheelEvent event(target, view->mapToGlobal(target.toPoint()), QPoint(),
                      QPoint(0, -120 * notches), Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(view, &event);
}

// Delegates must never overlap: a row that keeps a stale y (or a recycled
// delegate that lost track of its position) draws two processes on top of each
// other, which is invisible in a moving screenshot but obvious to the eye.
int countOverlappingRows(QQuickItem *root)
{
    QList<QQuickItem *> rows;
    collectItemsWithProperty(root, "pid", rows);
    struct Span {
        qreal top;
        qreal bottom;
        int pid;
    };
    QList<Span> spans;
    for (QQuickItem *row : rows) {
        const qreal y = row->mapToScene(QPointF(0, 0)).y();
        spans.append({y, y + row->height(), row->property("pid").toInt()});
    }
    std::sort(spans.begin(), spans.end(), [](const Span &a, const Span &b) { return a.top < b.top; });
    int overlaps = 0;
    for (int i = 1; i < spans.size(); ++i) {
        if (spans.at(i).top < spans.at(i - 1).bottom - 1.5) {
            ++overlaps;
        }
    }
    return overlaps;
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <FullRepresentation.qml> [output-prefix]\n", argv[0]);
        return 2;
    }
    const QString qmlFile = QString::fromLocal8Bit(argv[1]);
    const QString outputPrefix = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QStringLiteral("/tmp/perfmon-preview");

    QQuickView view;
    view.setResizeMode(QQuickView::SizeViewToRootObject);

    // Plasma provides i18n() to applets; reproduce it for the standalone render.
    const QJSValue i18n = view.engine()->evaluate(QStringLiteral(
        "(function(text) {"
        "  var args = Array.prototype.slice.call(arguments, 1);"
        "  for (var i = 0; i < args.length; ++i) text = text.replace('%' + (i + 1), String(args[i]));"
        "  return text;"
        "})"));
    view.engine()->rootContext()->setContextProperty(QStringLiteral("i18n"), QVariant::fromValue(i18n));

    view.setSource(QUrl::fromLocalFile(qmlFile));
    if (view.status() != QQuickView::Ready) {
        for (const QQmlError &error : view.errors()) {
            std::fprintf(stderr, "QML ERROR: %s\n", error.toString().toUtf8().constData());
        }
        return 1;
    }

    QQmlComponent themeProbe(view.engine());
    themeProbe.setData(QByteArrayLiteral(
        "import QtQuick\n"
        "import org.kde.kirigami as Kirigami\n"
        "QtObject {\n"
        "  property color bg: Kirigami.Theme.backgroundColor\n"
        "  property color fg: Kirigami.Theme.textColor\n"
        "  property color hl: Kirigami.Theme.highlightColor\n"
        "}\n"), QUrl());
    QObject *theme = themeProbe.create();
    if (!theme) {
        for (const QQmlError &error : themeProbe.errors()) {
            std::fprintf(stderr, "THEME ERROR: %s\n", error.toString().toUtf8().constData());
        }
        return 1;
    }
    view.setColor(theme->property("bg").value<QColor>());
    view.show();

    auto grab = [&](const QString &suffix) {
        const QColor background = theme->property("bg").value<QColor>();
        const QColor foreground = theme->property("fg").value<QColor>();
        const QColor highlight = theme->property("hl").value<QColor>();
        std::fprintf(stderr, "THEME bg=#%s fg=#%s hl=#%s\n",
                     background.name(QColor::HexRgb).mid(1).toUtf8().constData(),
                     foreground.name(QColor::HexRgb).mid(1).toUtf8().constData(),
                     highlight.name(QColor::HexRgb).mid(1).toUtf8().constData());
        view.setColor(background);
        const QImage image = view.grabWindow();
        const QString path = outputPrefix + suffix + QStringLiteral(".png");
        if (!image.save(path)) {
            std::fprintf(stderr, "SAVE FAILED %s\n", path.toUtf8().constData());
        } else {
            std::fprintf(stderr, "SAVED %s %dx%d overlaps=%d\n", path.toUtf8().constData(), image.width(),
                         image.height(), countOverlappingRows(view.rootObject()));
        }
        dumpTexts(view.rootObject(), 0);
        if (QQuickItem *scrollBar = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processScrollBar"))) {
            const QPointF pos = scrollBar->mapToScene(QPointF(0, 0));
            std::fprintf(stderr, "SCROLLBAR x=%.0f y=%.0f w=%.0f h=%.0f visible=%d\n",
                         pos.x(), pos.y(), scrollBar->width(), scrollBar->height(), scrollBar->isVisible());
        }
    };

    if (qEnvironmentVariableIsSet("PERFMON_PREVIEW_CLICK")) {
        QTimer::singleShot(1800, &app, [&]() {
            QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
            QList<QQuickItem *> rows;
            collectItemsWithProperty(view.rootObject(), "pid", rows);
            if (!list || rows.isEmpty()) {
                std::fprintf(stderr, "CLICK failed: no list/rows\n");
                return;
            }
            const QPointF listOrigin = list->mapToScene(QPointF(0, 0));
            const QRectF listRect(listOrigin, QSizeF(list->width(), list->height()));
            QQuickItem *target = nullptr;
            for (QQuickItem *row : rows) {
                const QPointF p = row->mapToScene(QPointF(30, 10));
                if (listRect.contains(p)) {
                    target = row;
                    break;
                }
            }
            if (!target) {
                std::fprintf(stderr, "CLICK failed: no row inside the viewport\n");
                return;
            }
            const int pid = target->property("pid").toInt();
            const QPoint clickPoint = target->mapToScene(QPointF(30, 10)).toPoint();
            QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, clickPoint);
            std::fprintf(stderr, "CLICK1 pid=%d at=%d,%d expandedPid=%s\n", pid, clickPoint.x(), clickPoint.y(),
                         view.rootObject()->property("expandedPid").toString().toUtf8().constData());
            QTimer::singleShot(400, &app, [&app, &view, pid]() {
                std::fprintf(stderr, "CLICK1-LATER expandedPid=%s\n",
                             view.rootObject()->property("expandedPid").toString().toUtf8().constData());
                QList<QQuickItem *> currentRows;
                collectItemsWithProperty(view.rootObject(), "pid", currentRows);
                QQuickItem *again = nullptr;
                QPoint secondPoint;
                for (QQuickItem *row : currentRows) {
                    if (row->property("pid").toInt() == pid) {
                        again = row;
                        secondPoint = row->mapToScene(QPointF(30, 10)).toPoint();
                        break;
                    }
                }
                if (!again) {
                    std::fprintf(stderr, "CLICK2 failed: row %d gone\n", pid);
                    return;
                }
                std::fprintf(stderr, "CLICK2-TARGET pid=%d expanded=%d y=%d\n", pid,
                             again->property("expanded").toBool(), secondPoint.y());
                QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, secondPoint);
                std::fprintf(stderr, "CLICK2 expandedPid=%s\n",
                             view.rootObject()->property("expandedPid").toString().toUtf8().constData());
            });
        });
    }

    if (qEnvironmentVariableIsSet("PERFMON_PREVIEW_EXPAND")) {
        QTimer::singleShot(1500, &app, [&]() {
            QQuickItem *row = topmostProcessRow(view.rootObject());
            if (!row) {
                std::fprintf(stderr, "EXPAND failed: no process row\n");
                return;
            }
            const int pid = row->property("pid").toInt();
            view.rootObject()->setProperty("expandedPid", QString::number(pid));
            const QString confirm = qEnvironmentVariable("PERFMON_PREVIEW_CONFIRM");
            if (confirm == QStringLiteral("term") || confirm == QStringLiteral("kill")) {
                view.rootObject()->setProperty("pendingKillPid", pid);
                view.rootObject()->setProperty("pendingKillForce", confirm == QStringLiteral("kill"));
            }
            std::fprintf(stderr, "EXPANDED pid=%d confirm=%s\n", pid, confirm.toUtf8().constData());
        });
        QTimer::singleShot(2600, &app, [&]() {
            std::fprintf(stderr, "ROOT expandedPid=%s pending=%d\n",
                         view.rootObject()->property("expandedPid").toString().toUtf8().constData(),
                         view.rootObject()->property("pendingKillPid").toInt());
            QList<QQuickItem *> rows;
            collectItemsWithProperty(view.rootObject(), "pid", rows);
            int expandedCount = 0;
            for (QQuickItem *row : rows) {
                if (!row->property("expanded").toBool()) {
                    continue;
                }
                ++expandedCount;
                std::fprintf(stderr, "ROW-EXPANDED pid=%d confirm=%d height=%.0f visible=%d\n",
                             row->property("pid").toInt(),
                             row->property("confirmPending").toBool(),
                             row->height(), row->isVisible());
            }
            std::fprintf(stderr, "ROWS total=%lld expanded=%d\n", qint64(rows.size()), expandedCount);
        });
    }

    // Everything the freeze heuristic looks at, in one line: contentY alone is
    // not enough to tell "pinned to the top" from "shifted a few pixels and
    // stuck", and originY explains offsets the reader never produced.
    auto logListState = [&](const char *label) {
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        QQuickItem *badge = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("pausedBadge"));
        QQuickItem *button = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("backToTopButton"));
        QQuickItem *scrollbar = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processScrollBar"));
        if (!list) {
            std::fprintf(stderr, "%s: no list\n", label);
            return;
        }
        std::fprintf(stderr,
                     "%s contentY=%.0f originY=%.0f contentHeight=%.0f height=%.0f "
                     "paused-badge=%d back-to-top=%d scrolled=%d dragging=%d bar-pressed=%d\n",
                     label, list->property("contentY").toDouble(), list->property("originY").toDouble(),
                     list->property("contentHeight").toDouble(), list->height(),
                     badge ? badge->isVisible() : -1, button ? button->isVisible() : -1,
                     view.rootObject()->property("readerScrolled").toBool(),
                     view.rootObject()->property("readerDragging").toBool(),
                     scrollbar && scrollbar->property("pressed").toBool());
    };

    // The timers below outlive this function's scopes, so everything they touch
    // has to live here rather than inside an if block. A table that should be
    // live must show its first row without a frozen badge; a table the reader
    // scrolled away from must stay frozen. Both used to be mixed up after a
    // re-sort, so a mismatch fails the run (and CI).
    int sortFailures = 0;
    // Timer callbacks outlive the block scopes they are scheduled from, so any
    // state they share has to be declared out here as well.
    QPoint backToTopPoint;
    int barDragX = 0;
    int barDragTopY = 0;

    auto checkListState = [&](const char *label, bool shouldBeLive) {
        logListState(label);
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        QQuickItem *badge = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("pausedBadge"));
        if (!list) {
            ++sortFailures;
            return;
        }
        const qreal offset = list->property("contentY").toDouble() - list->property("originY").toDouble();
        const bool frozen = badge && badge->isVisible();
        const bool ok = shouldBeLive ? (qAbs(offset) <= 1.0 && !frozen) : (offset > 1.0 && frozen);
        if (!ok) {
            ++sortFailures;
            std::fprintf(stderr, "preview: %s expected %s, offset=%.0f frozen=%d\n", label,
                         shouldBeLive ? "live at the top" : "frozen while scrolled", offset, frozen ? 1 : 0);
        }
    };

    // Sort-toggle scenario: switching CPU/memory or the sort direction has to
    // leave the table pinned to the live top. It drives the model the way the
    // sort chip does (reset the view, then re-sort) and then checks that the
    // frozen state follows the reader, not ListView's own re-anchoring: the view
    // rewrites contentY when rows move, which used to look like "the reader
    // scrolled away" and left the badge stuck on Paused.
    if (qEnvironmentVariableIsSet("PERFMON_PREVIEW_SORT")) {
        // Poll instead of listening: contentYChanged lives on QQuickFlickable,
        // which is not a public header. 16 ms is enough to see whether a jump
        // survives a frame or is undone before the frame is drawn.
        if (qEnvironmentVariableIsSet("PERFMON_PREVIEW_TRACE")) {
            auto *trace = new QTimer(&app);
            trace->setInterval(16);
            qreal lastY = -1.0;
            qreal lastOrigin = -1.0;
            QObject::connect(trace, &QTimer::timeout, &app, [&]() {
                QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
                if (!list) {
                    return;
                }
                const qreal y = list->property("contentY").toDouble();
                const qreal origin = list->property("originY").toDouble();
                if (qFuzzyCompare(y + 1.0, lastY + 1.0) && qFuzzyCompare(origin + 1.0, lastOrigin + 1.0)) {
                    return;
                }
                lastY = y;
                lastOrigin = origin;
                logListState("SORT-trace");
            });
            trace->start();
        }

        QTimer::singleShot(3000, &app, [&]() {
            checkListState("SORT-live", true);
            if (QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"))) {
                sendWheelNotch(&view, list, 40);
            }
            checkListState("SORT-reader-scrolled", false);
        });

        const auto toggle = [&](const char *key) {
            QObject *backend = view.engine()->singletonInstance<QObject *>(QStringLiteral("Hao.PerfMon"),
                                                                          QStringLiteral("PerfBackend"));
            QMetaObject::invokeMethod(view.rootObject(), "resetViewToTop");
            if (backend) {
                QMetaObject::invokeMethod(backend, "toggleSort", Q_ARG(QString, QString::fromLatin1(key)));
            }
        };

        QTimer::singleShot(3600, &app, [&]() { toggle("cpu"); });
        QTimer::singleShot(4200, &app, [&]() { checkListState("SORT-settled-after-cpu", true); });
        QTimer::singleShot(4800, &app, [&]() { toggle("memory"); });
        QTimer::singleShot(5400, &app, [&]() { checkListState("SORT-settled-after-memory", true); });
        QTimer::singleShot(6000, &app, [&]() { toggle("memory"); }); // direction flip
        QTimer::singleShot(6600, &app, [&]() {
            checkListState("SORT-settled-after-flip", true);
            if (QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"))) {
                sendWheelNotch(&view, list, 40);
            }
            checkListState("SORT-scrolled-again", false);
        });
        // The floating button, clicked the way a pointer does it: hover, hold,
        // release. A single press/release pair with no gap in between is
        // occasionally dropped by the offscreen platform, which made this check
        // flaky for reasons that had nothing to do with the view.
        QTimer::singleShot(7000, &app, [&]() {
            QQuickItem *button = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("backToTopButton"));
            if (!button || !button->isVisible()) {
                std::fprintf(stderr, "SORT-back-to-top skipped: button hidden\n");
                return;
            }
            backToTopPoint = button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
            QTest::mouseMove(&view, backToTopPoint);
        });
        QTimer::singleShot(7150, &app, [&]() {
            logListState("SORT-before-back-to-top");
            if (!backToTopPoint.isNull()) {
                QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, backToTopPoint);
            }
        });
        QTimer::singleShot(7300, &app, [&]() {
            if (backToTopPoint.isNull()) {
                return;
            }
            QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, backToTopPoint);
            // Park the pointer outside the rows, so the screenshots show no
            // hover highlight on the row that happened to sit there.
            QTest::mouseMove(&view, QPoint(4, 4));
        });
        QTimer::singleShot(7900, &app, [&]() { checkListState("SORT-back-to-top", true); });

        // Second half: the same recovery through the scrollbar instead of the
        // floating button, which is the other way a reader gets back to the top.
        QTimer::singleShot(8200, &app, [&]() {
            if (QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"))) {
                sendWheelNotch(&view, list, 40);
            }
            checkListState("SORT-frozen-again", false);
        });
        // Drag the handle to the very top. The steps are separate timer ticks so
        // the view actually processes press/move/release instead of receiving
        // all three inside one callback.
        QTimer::singleShot(8700, &app, [&]() {
            QQuickItem *bar = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processScrollBar"));
            if (!bar || !bar->isVisible()) {
                std::fprintf(stderr, "SORT-bar-drag skipped: scrollbar hidden\n");
                return;
            }
            const QPointF origin = bar->mapToScene(QPointF(0, 0));
            const qreal handle = bar->property("size").toDouble();
            const qreal position = bar->property("position").toDouble();
            barDragX = int(origin.x() + bar->width() / 2);
            barDragTopY = int(origin.y() + handle * bar->height() / 2);
            const int grabY = int(origin.y() + (position + handle / 2) * bar->height());
            std::fprintf(stderr, "SORT-bar-drag from=%d to=%d\n", grabY, barDragTopY);
            QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, QPoint(barDragX, grabY));
        });
        QTimer::singleShot(8900, &app, [&]() {
            QTest::mouseMove(&view, QPoint(barDragX, barDragTopY + 40));
        });
        QTimer::singleShot(9100, &app, [&]() {
            QTest::mouseMove(&view, QPoint(barDragX, barDragTopY));
        });
        QTimer::singleShot(9300, &app, [&]() {
            QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, QPoint(barDragX, barDragTopY));
        });
        QTimer::singleShot(9900, &app, [&]() { checkListState("SORT-bar-to-top", true); });
        QTimer::singleShot(10400, &app, [&]() {
            checkListState("SORT-final", true);
            grab(QStringLiteral("-sort"));
            std::fprintf(stderr, "SORT-RESULT failures=%d\n", sortFailures);
            app.exit(sortFailures == 0 ? 0 : 1);
        });
    }

    // Scroll / freeze / back-to-top scenario. Each step reports what the user
    // would see, so a regression shows up as a changed log line or a changed
    // frame instead of having to be spotted by hand.
    // The table refreshes by diffing into the same model (moves + in-place
    // role updates), because a reset would destroy every delegate and with it
    // the row-move animation. Both are counted so a regression to reset-based
    // rebuilding shows up as a log line instead of a janky animation.
    int modelResets = 0;
    int rowMoves = 0;
    int roleUpdates = 0;
    int resetsWhenFrozen = -1;

    // Both scenarios drive the same popup, so only one of them runs per process.
    const bool sortScenario = qEnvironmentVariableIsSet("PERFMON_PREVIEW_SORT");
    auto scrollRunLater = [&](int ms, std::function<void()> fn) {
        if (!sortScenario) {
            QTimer::singleShot(ms, &app, fn);
        }
    };

    scrollRunLater(3000, [&]() {
        grab(QStringLiteral("-1"));
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        if (list) {
            if (auto *model = qobject_cast<QAbstractItemModel *>(list->property("model").value<QObject *>())) {
                QObject::connect(model, &QAbstractItemModel::modelReset, &app, [&modelResets]() { ++modelResets; });
                QObject::connect(model, &QAbstractItemModel::rowsMoved, &app, [&rowMoves]() { ++rowMoves; });
                QObject::connect(model, &QAbstractItemModel::dataChanged, &app, [&roleUpdates]() { ++roleUpdates; });
            }
            std::fprintf(stderr, "LIVE contentY=%.0f resets=%d moves=%d updates=%d overlaps=%d max-cpu=%.2f\n",
                         listContentY(list), modelResets, rowMoves, roleUpdates, countOverlappingRows(view.rootObject()),
                         maxProcessCpu(list));
        }
    });

    scrollRunLater(4000, [&]() {
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        if (!list) {
            return;
        }
        // Scroll like a reader, not with a raw contentY write: a live table
        // treats any move it did not cause as its own and re-pins the top, so a
        // property write would be undone on purpose.
        sendWheelNotch(&view, list, 5);
        QQuickItem *badge = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("pausedBadge"));
        std::fprintf(stderr, "SCROLL wheel-5 contentY=%.0f paused-badge=%d\n", listContentY(list),
                     badge ? badge->isVisible() : -1);
        resetsWhenFrozen = modelResets;
    });

    scrollRunLater(5000, [&]() {
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        if (!list) {
            return;
        }
        const qreal before = listContentY(list);
        sendWheelNotch(&view, list, 3);
        std::fprintf(stderr, "WHEEL 3-notches %.0f -> %.0f\n", before, listContentY(list));
    });

    scrollRunLater(6500, [&]() {
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        if (!list) {
            return;
        }
        QQuickItem *badge = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("pausedBadge"));
        QQuickItem *button = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("backToTopButton"));
        std::fprintf(stderr, "FROZEN contentY=%.0f resets-since-pause=%d paused-badge=%d back-to-top=%d max-cpu=%.2f\n",
                     listContentY(list), modelResets - resetsWhenFrozen,
                     badge ? badge->isVisible() : -1, button ? button->isVisible() : -1,
                     maxProcessCpu(list));
        grab(QStringLiteral("-2"));
    });

    scrollRunLater(7200, [&]() {
        QQuickItem *button = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("backToTopButton"));
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        if (!button || !list) {
            std::fprintf(stderr, "BACK-TO-TOP skipped: no button/list\n");
            return;
        }
        const QPoint click = button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, click);
        // Park the pointer outside the rows again, so the final frames show no
        // hover highlight on whichever row happened to sit under the click.
        QTest::mouseMove(&view, QPoint(4, 4));
        std::fprintf(stderr, "BACK-TO-TOP clicked at=%d,%d contentY=%.0f\n", click.x(), click.y(),
                     listContentY(list));
    });

    scrollRunLater(8200, [&]() {
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        QQuickItem *badge = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("pausedBadge"));
        std::fprintf(stderr, "RESUMED contentY=%.0f resets=%d moves=%d updates=%d overlaps=%d paused-badge=%d\n",
                     listContentY(list), modelResets, rowMoves, roleUpdates, countOverlappingRows(view.rootObject()),
                     badge ? badge->isVisible() : -1);
    });

    scrollRunLater(9000, [&]() {
        grab(QStringLiteral("-3"));
        app.quit();
    });
    return app.exec();
}
