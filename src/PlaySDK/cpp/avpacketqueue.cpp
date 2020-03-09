
#include "avpacketqueue.h"

size_t AvPacketQueue::enqueue(AVPacket *packet)
{
    m_mutex.lock();

    m_queue.push_back(*packet);

    m_condition.notify_one();

    cauto size = m_queue.size();

    m_mutex.unlock();

    return size;
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
		
        m_condition.wait(lock);
    }

	return false;
}

void AvPacketQueue::clear()
{
    m_mutex.lock();

    for (auto& packet : m_queue)
	{
		av_packet_unref(&packet);
    }
	m_queue.clear();

    m_mutex.unlock();
}
