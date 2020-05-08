#pragma once

#include "inc.h"

class AvPacketQueue
{
public:
    AvPacketQueue() = default;

public:
    size_t enqueue(AVPacket& packet);

    int dequeue(AVPacket& packet);

    bool isEmpty() const
    {
        return m_queue.empty();
    }

    void clear();

private:
    mutex m_mutex;

    list<AVPacket> m_queue;
};
