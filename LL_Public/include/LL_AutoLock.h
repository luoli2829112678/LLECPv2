#ifndef _LL_AutoLock_H
#define _LL_AutoLock_H

#include <string>
#include <vector>
#include <pthread.h>

class SaftLock
{
public:
    SaftLock(){pthread_mutex_init(&m_lock,nullptr);}
    ~SaftLock(){pthread_mutex_destroy(&m_lock);}
    void Lock(){pthread_mutex_lock(&m_lock);}
    void UnLock(){pthread_mutex_unlock(&m_lock);}
#ifndef _WIN32
    int GetOwner() { return m_lock.__data.__owner; } //这句话在Windows VS下编译不过
#endif
private:
    pthread_mutex_t m_lock;
};

class AutoSaftMutex
{
public:
    AutoSaftMutex(SaftLock *hLock) :m_Lock(hLock)
    {
        m_Lock->Lock();
    }
    ~AutoSaftMutex()
    {
        m_Lock->UnLock();
    }
private:
    SaftLock *m_Lock;
};

class wrLock
{
public:
    wrLock(){pthread_rwlock_init(&m_lock,nullptr);}
    ~wrLock(){pthread_rwlock_destroy(&m_lock);}
    void rLock(){pthread_rwlock_rdlock(&m_lock);}
    void wLock(){pthread_rwlock_wrlock(&m_lock);}
    void UnLock(){pthread_rwlock_unlock(&m_lock);}
    int GetOwner(){return 0;}//m_lock.__data.__owner;}
private:
    pthread_rwlock_t m_lock;
};

class AutoReadGuard
{
public:
    AutoReadGuard(wrLock *hLock) :m_Lock(hLock)
    {
        m_Lock->rLock();
    }
    ~AutoReadGuard()
    {
        m_Lock->UnLock();
    }
private:
    wrLock *m_Lock;
};

class AutoWriteGuard
{
public:
    AutoWriteGuard(wrLock *hLock) :m_Lock(hLock)
    {
        m_Lock->wLock();
    }
    ~AutoWriteGuard()
    {
        m_Lock->UnLock();
    }
private:
    wrLock *m_Lock;
};

#endif