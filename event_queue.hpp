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

	void push(T& event)
	{
		const std::lock_guard lock(m_mutex);
		m_items.push_back(std::move(event));
		SetEvent(m_hEvent);
	}

	bool pop(T& event)
	{
		const std::lock_guard lock(m_mutex);
		if (m_items.empty()) {
			return false;
		}

		event = std::move(m_items.front());

		for (size_t i = 1; i < m_items.size(); ++i) {
			m_items[i - 1] = std::move(m_items[i]);
		}
		m_items.resize(m_items.size() - 1);

		return true;
	}

	void* getHandle() const
	{
		return m_hEvent;
	}

private:
	std::mutex m_mutex;
	HANDLE m_hEvent;
	std::vector<T> m_items;
};
