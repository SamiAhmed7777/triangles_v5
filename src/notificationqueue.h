// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIANGLES_NOTIFICATIONQUEUE_H
#define TRIANGLES_NOTIFICATIONQUEUE_H

#include <string>
#include <deque>
#include <vector>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

/**
 * Thread-safe notification queue for SSE (Server-Sent Events) clients.
 *
 * Producers (block acceptance, mempool acceptance) push JSON event strings.
 * Consumer threads (SSE HTTP handlers) wait on the condition variable and
 * drain events as they arrive.
 *
 * The queue keeps the last MAX_QUEUED_EVENTS events so late-joining clients
 * can get a small backlog. Each SSE client tracks its own read position.
 */
class CNotificationQueue
{
private:
    mutable boost::mutex cs;
    boost::condition_variable cond;

    struct Event {
        uint64_t id;
        std::string data; // JSON payload
    };

    std::deque<Event> events;
    uint64_t nNextId;

    static const size_t MAX_QUEUED_EVENTS = 256;

public:
    CNotificationQueue() : nNextId(1) {}

    /** Push a new event. Wakes all waiting SSE clients. */
    void Push(const std::string& strData)
    {
        boost::mutex::scoped_lock lock(cs);
        events.push_back(Event{nNextId++, strData});
        while (events.size() > MAX_QUEUED_EVENTS)
            events.pop_front();
        cond.notify_all();
    }

    /**
     * Wait for events newer than nLastId.
     * Returns new events and updates nLastId to the newest seen.
     * Returns false if timed out with no new events, true if events were returned.
     * Also returns false if fShutdown becomes true.
     */
    bool WaitForEvents(uint64_t& nLastId, std::vector<std::string>& vEvents, int nTimeoutMs, const volatile bool& fShutdown)
    {
        vEvents.clear();
        boost::mutex::scoped_lock lock(cs);

        // Check for events already in the queue past our read position
        bool fHasNew = false;
        for (std::deque<Event>::const_iterator it = events.begin(); it != events.end(); ++it)
        {
            if (it->id > nLastId)
            {
                fHasNew = true;
                break;
            }
        }

        if (!fHasNew)
        {
            // Wait for new events or timeout
            cond.timed_wait(lock, boost::posix_time::milliseconds(nTimeoutMs));
        }

        // Drain all events newer than nLastId
        for (std::deque<Event>::const_iterator it = events.begin(); it != events.end(); ++it)
        {
            if (it->id > nLastId)
            {
                vEvents.push_back(it->data);
                nLastId = it->id;
            }
        }

        if (fShutdown)
            return false;

        return !vEvents.empty();
    }

    /** Get the current latest event ID (for clients that want to skip history). */
    uint64_t GetLatestId() const
    {
        boost::mutex::scoped_lock lock(cs);
        return nNextId - 1;
    }
};

/** Global notification queue instance */
extern CNotificationQueue* pNotificationQueue;

#endif // TRIANGLES_NOTIFICATIONQUEUE_H
