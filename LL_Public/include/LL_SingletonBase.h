#ifndef _LL_SingletonBase_H
#define _LL_SingletonBase_H

#include "LL_AutoLock.h"


#define DECLARE_Singleton(ClassName) \
    public:\
        static ClassName* instance()\
        {\
            if (nullptr != m_instance)\
            {\
                return m_instance;\
            }\
            AutoSaftMutex InsLocker(&ClassName::m_insLock);\
            if(nullptr == m_instance)\
            {\
                m_instance = new ClassName();\
            }\
            return m_instance;\
        }\
    private:\
        static SaftLock m_insLock;\
        static ClassName* m_instance;\
        ClassName(/* args */);\
        ~ClassName();

//CPP文件静态变量声明
#define Build_Singleton(ClassName)\
    SaftLock ClassName::m_insLock;\
    ClassName* ClassName::m_instance = nullptr;

#define Instance(ClassName) ClassName::instance()

#endif