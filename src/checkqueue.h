// Copyright (c) 2012-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIANGLES_CHECKQUEUE_H
#define TRIANGLES_CHECKQUEUE_H

#include <algorithm>
#include <deque>
#include <vector>

#include <condition_variable>
#include <mutex>
#include <thread>

template<typename T>
class CCheckQueue
{
private:
    std::mutex mutex;
    std::condition_variable condWorker;
    std::condition_variable condMaster;

    std::deque<T> queue;
    unsigned int nIdle;
    unsigned int nTotal;
    bool fAllOk;
    unsigned int nTodo;
    bool fQuit;

    unsigned int nBatchSize;

    bool Loop(bool fMaster)
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (!fMaster)
            nTotal++;
        nIdle++;

        bool fOk = true;

        for (;;)
        {
            while (queue.empty())
            {
                if (fQuit)
                {
                    nIdle--;
                    if (!fMaster)
                        nTotal--;
                    return false;
                }
                if (fMaster && nTodo == 0)
                {
                    bool fRet = fAllOk;
                    nIdle--;
                    return fRet;
                }
                if (fMaster)
                    condMaster.wait(lock);
                else
                    condWorker.wait(lock);
            }

            unsigned int nNow = std::max(1U, std::min((unsigned int)queue.size() / (nTotal + 1), nBatchSize));
            std::vector<T> vChecks(nNow);
            for (unsigned int i = 0; i < nNow; i++)
            {
                vChecks[i].swap(queue.front());
                queue.pop_front();
            }
            nIdle--;
            lock.unlock();

            for (unsigned int i = 0; i < vChecks.size(); i++)
            {
                if (fOk)
                    fOk = vChecks[i]();
            }
            vChecks.clear();

            lock.lock();
            nIdle++;
            nTodo -= nNow;
            if (!fOk)
                fAllOk = false;

            if (nTodo == 0)
                condMaster.notify_one();
        }
    }

public:
    CCheckQueue(unsigned int nBatchSizeIn = 128)
        : nIdle(0), nTotal(0), fAllOk(true), nTodo(0), fQuit(false),
          nBatchSize(nBatchSizeIn) {}

    void Thread()
    {
        Loop(false);
    }

    void StartBatch()
    {
        std::unique_lock<std::mutex> lock(mutex);
        fAllOk = true;
        nTodo = 0;
    }

    void Add(std::vector<T>& vChecks)
    {
        if (vChecks.empty())
            return;

        std::unique_lock<std::mutex> lock(mutex);
        for (typename std::vector<T>::iterator it = vChecks.begin(); it != vChecks.end(); ++it)
        {
            queue.push_back(T());
            queue.back().swap(*it);
        }
        nTodo += vChecks.size();
        if (vChecks.size() == 1)
            condWorker.notify_one();
        else
            condWorker.notify_all();
    }

    bool Wait()
    {
        return Loop(true);
    }

    void Quit()
    {
        std::unique_lock<std::mutex> lock(mutex);
        fQuit = true;
        condWorker.notify_all();
        condMaster.notify_all();
    }
};

template<typename T>
class CCheckQueueControl
{
private:
    CCheckQueue<T>* pqueue;
    bool fDone;

    CCheckQueueControl(const CCheckQueueControl&);
    CCheckQueueControl& operator=(const CCheckQueueControl&);

public:
    CCheckQueueControl(CCheckQueue<T>* pqueueIn)
        : pqueue(pqueueIn), fDone(false)
    {
        if (pqueue)
            pqueue->StartBatch();
    }

    bool Wait()
    {
        if (!pqueue || fDone)
            return true;
        fDone = true;
        return pqueue->Wait();
    }

    void Add(std::vector<T>& vChecks)
    {
        if (pqueue)
            pqueue->Add(vChecks);
    }

    ~CCheckQueueControl()
    {
        if (!fDone)
            Wait();
    }
};

#endif // TRIANGLES_CHECKQUEUE_H
