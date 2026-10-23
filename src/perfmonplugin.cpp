#include "perfbackend.h"

#include <QQmlEngine>
#include <QQmlExtensionPlugin>

class PerfMonPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override
    {
        qmlRegisterSingletonType<PerfBackend>(uri, 1, 0, "PerfBackend",
                                              [](QQmlEngine *, QJSEngine *) -> QObject * {
                                                  return new PerfBackend;
                                              });
    }
};

#include "perfmonplugin.moc"
