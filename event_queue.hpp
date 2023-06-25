#pragma once

#include <Windows.h>

#include <deque>
#include <mutex>

template<typename T>
class EventQueue
{
public:
	EventQueue() {
		m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	}

	~EventQueue() {
		CloseHandle(m_hEvent);
	}

	void push(const T& event)
	{
		const std::lock_guard lock(m_mutex);
		m_items.push_back(event);
		SetEvent(m_hEvent);
	}

	bool pop(T& event)
	{
		const std::lock_guard lock(m_mutex);
		if (m_items.empty()) {
			return false;
		}

		event = m_items.front();
		m_items.pop_front();
		return true;
	}

	void* getHandle() const
	{
		return m_hEvent;
	}

private:
	std::mutex m_mutex;
	HANDLE m_hEvent;
	std::deque<T> m_items;
};
