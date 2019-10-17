
#include "avpacketqueue.h"

size_t AvPacketQueue::enqueue(AVPacket *packet)
{
	mutex_lock lock(m_mutex);

    m_queue.push_back(*packet);

    m_condVar.notify_one();

	return m_queue.size();
}

bool AvPacketQueue::dequeue(AVPacket& packet, bool isBlock)
{
	mutex_lock lock(m_mutex);
    while (true)
	{
        if (!m_queue.empty())
		{
           packet = m_queue.front();
            m_queue.pop_front();

			return true;
        }

		if (!isBlock)
		{
            break;
        }
		
        m_condVar.wait(lock);
    }

	return false;
}

void AvPacketQueue::clear()
{
	mutex_lock lock(m_mutex);
    for (auto& packet : m_queue)
	{
		av_packet_unref(&packet);
    }
	m_queue.clear();
}

bool AvPacketQueue::isEmpty()
{
    return m_queue.empty();
}

size_t AvPacketQueue::queueSize()
{
    return m_queue.size();
}
