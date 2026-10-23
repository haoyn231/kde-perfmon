// Development tool: renders the real popup QML with the real backend into PNG
// frames so the UI can be inspected without a running panel. It also reports
// the Kirigami colors before and after the system color scheme changes.
#include <QGuiApplication>
#include <QImage>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTimer>

#include <cstdio>

namespace {

void dumpTexts(QQuickItem *item, int depth)
{
    if (!item) {
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

    QTimer::singleShot(4000, &app, [&]() {
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        if (list) {
            list->setProperty("contentY", 600.0);
            std::fprintf(stderr, "SCROLL set=%.0f\n", list->property("contentY").toDouble());
        }
    });
    QTimer::singleShot(6500, &app, [&]() {
        QQuickItem *list = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("processList"));
        if (list) {
            std::fprintf(stderr, "SCROLL after-two-resets=%.0f count=%d\n",
                         list->property("contentY").toDouble(),
                         list->property("count").toInt());
        }
    });

    QTimer::singleShot(3000, &app, [&]() { grab(QStringLiteral("-1")); });
    QTimer::singleShot(9000, &app, [&]() {
        grab(QStringLiteral("-2"));
        app.quit();
    });
    return app.exec();
}
