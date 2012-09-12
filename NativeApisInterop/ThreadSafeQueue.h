#pragma once
#include <list>
#include "Lock.h"

template<typename T>
ref class ThreadSafeQueue
{
	std::list<T>* m_list;
	Object^ m_lock;
public:
	ThreadSafeQueue() : m_lock(gcnew Object()), m_list(new std::list<T>) {}
	~ThreadSafeQueue() { delete m_list; }

	// Adds an element to the queue.
	void Put(T element) {
		Lock lock(m_lock);
		m_list->push_back(element);
	}

	// Removes and returns the next element in the queue.
	T Take() {
		Lock lock(m_lock);
		T rv = m_list->front();
		m_list->pop_front();
		return rv;
	}

	// Returns the next element if the queue is not empty. Return value indicates success.
	bool TryTake(T& element) {
		if (m_list->size() > 0) {
			Lock lock(m_lock);
			element = m_list->front();
			m_list->pop_front();
			return true;
		}
		else {
			return false;
		}
	}

	// Returns the number of elements in the queue.
	int Count() {
		Lock lock(m_lock);
		return m_list->size();
	}
};

