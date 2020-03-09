#pragma once

#include "inc.h"

class AvPacketQueue
{
public:
    AvPacketQueue() {}

public:
    size_t enqueue(AVPacket *packet);

    bool dequeue(AVPacket& packet, bool isBlock);

    bool isEmpty() const
    {
        return m_queue.empty();
    }

    void clear();

private:
    mutex m_mutex;
    condition_variable m_condition;

    list<AVPacket> m_queue;
};
