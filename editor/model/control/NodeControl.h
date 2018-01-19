#pragma once

#include "../GridItem.h"
#include "../connection/ConnectionSink.h"

namespace AxiomModel {

    class Node;

    class NodeControl : public GridItem {
    Q_OBJECT

    public:
        enum class Channel {
            LEFT = 1 << 0,
            RIGHT = 1 << 1,
            BOTH = LEFT | RIGHT
        };

        const Channel channel;

        ConnectionSink sink;

        Node *node;

        NodeControl(Node *node, Channel channel, QPoint pos, QSize size);

        QString name() const { return m_name; }

        bool isMovable() const override { return true; }

    public slots:

        void setName(const QString &name);

    signals:

        void nameChanged(const QString &newName);

    private:

        QString m_name = "";

    private slots:

        void recalcSinkPos();
    };

}
