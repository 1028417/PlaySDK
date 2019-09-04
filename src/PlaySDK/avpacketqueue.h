#pragma once

#include "inc.h"

class AvPacketQueue
{
public:
    AvPacketQueue() {}

public:
    size_t enqueue(AVPacket *packet);

    bool dequeue(AVPacket& packet, bool isBlock);

    bool isEmpty();

    void clear();

    size_t queueSize();

private:
    mutex m_mutex;
    condition_variable m_condVar;

    list<AVPacket> m_queue;
};
