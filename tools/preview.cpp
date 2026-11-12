// Development tool: renders the real popup QML with the real backend into PNG
// frames so the UI can be inspected without a running panel. It also reports
// the Kirigami colors before and after the system color scheme changes.
#include <QAbstractItemModel>
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
            std::fprintf(stderr, "SAVED %s %dx%d\n", path.toUtf8().constData(), image.width(), image.height());
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

    // Scroll / freeze / back-to-top scenario. Each step reports what the user
    // would see, so a regression shows up as a changed log line or a changed
    // frame instead of having to be spotted by hand.
    int modelResets = 0;
    int resetsWhenFrozen = -1;

    QTimer::singleShot(3000, &app, [&]() {
        grab(QStringLiteral("-1"));
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        if (list) {
            if (auto *model = qobject_cast<QAbstractItemModel *>(list->property("model").value<QObject *>())) {
                QObject::connect(model, &QAbstractItemModel::modelReset, &app, [&modelResets]() { ++modelResets; });
            }
            std::fprintf(stderr, "LIVE contentY=%.0f resets=%d max-cpu=%.2f\n", listContentY(list), modelResets,
                     maxProcessCpu(list));
        }
    });

    QTimer::singleShot(4000, &app, [&]() {
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        if (!list) {
            return;
        }
        list->setProperty("contentY", 600.0);
        QQuickItem *badge = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("pausedBadge"));
        std::fprintf(stderr, "SCROLL set=%.0f paused-badge=%d\n", listContentY(list),
                     badge ? badge->isVisible() : -1);
        resetsWhenFrozen = modelResets;
    });

    QTimer::singleShot(5000, &app, [&]() {
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        if (!list) {
            return;
        }
        const qreal before = listContentY(list);
        sendWheelNotch(&view, list, 3);
        std::fprintf(stderr, "WHEEL 3-notches %.0f -> %.0f\n", before, listContentY(list));
    });

    QTimer::singleShot(6500, &app, [&]() {
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

    QTimer::singleShot(7200, &app, [&]() {
        QQuickItem *button = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("backToTopButton"));
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        if (!button || !list) {
            std::fprintf(stderr, "BACK-TO-TOP skipped: no button/list\n");
            return;
        }
        const QPoint click = button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, click);
        std::fprintf(stderr, "BACK-TO-TOP clicked at=%d,%d contentY=%.0f\n", click.x(), click.y(),
                     listContentY(list));
    });

    QTimer::singleShot(8200, &app, [&]() {
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        QQuickItem *badge = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("pausedBadge"));
        std::fprintf(stderr, "RESUMED contentY=%.0f resets=%d paused-badge=%d\n",
                     listContentY(list), modelResets, badge ? badge->isVisible() : -1);
    });

    QTimer::singleShot(9000, &app, [&]() {
        grab(QStringLiteral("-3"));
        app.quit();
    });
    return app.exec();
}
