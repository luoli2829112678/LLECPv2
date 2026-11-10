#include "LLC_Rbt.h"

int LLC_Rbt::InitWaypoint()
{
    m_nWayPoinCount = 0;
    m_nWayPoinRunIndex = 0;
    m_WayPointManager.clear();
    ST_AxisWayPoint_Point point;
    m_WayPointManager.push_back(point);
    m_bWayPoinError = false;
    return REC_Successed;
}

int LLC_Rbt::WayPointPushFinish(int nWayPointID)
{
    m_nWayPoinCount ++;
    m_WayPointManager.back().WayPointID = nWayPointID;
    m_WayPointManager.back().PointStauts = enum_WayPointInitialized;
    ST_AxisWayPoint_Point point;
    point.PointStauts = enum_WayPointUninitialized;
    m_WayPointManager.push_back(point);
    return REC_Successed;
}

int LLC_Rbt::PushWaypoint(vector<ST_AxisWayPointUnit> AxisPoints)
{
    m_WayPointManager[m_nWayPoinCount].AxisPoints.insert(m_WayPointManager[m_nWayPoinCount].AxisPoints.end(),AxisPoints.begin(),AxisPoints.end());
    return REC_Successed;
}

bool LLC_Rbt::CheckIsScriptMoving()
{
    return (m_nWayPoinRunIndex < m_nWayPoinCount)&& m_nWayPoinRunIndex > 0;
}

void LLC_Rbt::WayPoint_RT()
{
    m_bWayPoinRunDone = true;
    bool bBusy = false;
    bool bDone = false;
    bool bError = false;
    int nErrorID;
    bool bCommandAborted = false;
    //所有点位运行完毕
    if(m_nWayPoinRunIndex >= m_nWayPoinCount)
    {
        return;
    }
    if(m_nWayPoinRunIndex < 0)
    {
        return;
    }
    if(m_WayPointManager.size()<1)
    {
        return;
    }
    m_bWayPoinRunDone = false;
    ST_AxisWayPoint_Point* pPoint = &m_WayPointManager[m_nWayPoinRunIndex];
    if(enum_WayPointInitialized == pPoint->PointStauts)//已初始化，先运行起来
    {
        pPoint->PointStauts = enum_WayPointRunning;
        for (size_t i = 0; i < pPoint->AxisPoints.size(); i++)
        {
            if(enum_MoveBaseTypeAxis == pPoint->enMoveBaseType)
            {
                int nAxis = pPoint->AxisPoints[i].nAxisID;
                AxisMtionParam stAxisMtionParam = pPoint->AxisPoints[i].stAxisMtionParam;
                pPoint->AxisPoints[i].pFunBasic = &m_FB_AxisAbsMove[nAxis];
                m_FB_AxisAbsMove[nAxis](
                    m_vAxis[nAxis],
                    false,
                    stAxisMtionParam.dPos,
                    stAxisMtionParam.dVel,
                    stAxisMtionParam.dAcc,
                    stAxisMtionParam.dDec,
                    stAxisMtionParam.dJerk,
                    EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID
                );
                m_FB_AxisAbsMove[nAxis](
                    m_vAxis[nAxis],
                    true,
                    stAxisMtionParam.dPos,
                    stAxisMtionParam.dVel,
                    stAxisMtionParam.dAcc,
                    stAxisMtionParam.dDec,
                    stAxisMtionParam.dJerk,
                    EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID
                );
                pPoint->AxisPoints[i].AxisMtionStauts = enum_WayPointRunning;
            }
            else if(enum_MoveBaseTypeAxis != pPoint->enMoveBaseType)
            {
                printf("WayPointTypeError!\n");
            }
        } 
    }
    else if(enum_WayPointRunning == pPoint->PointStauts)//已经再运行中了，检测其运行状态
    {
        pPoint->PointStauts = EN_WayPointRunState ::enum_WayPointFinished;
        if(enum_MoveBaseTypeAxis == pPoint->enMoveBaseType)
        {
            for (size_t i = 0; i < pPoint->AxisPoints.size(); i++)
            {
                //对于轴运动，只要有一个是运行中，那么就是未结束
                if(pPoint->AxisPoints[i].pFunBasic->Busy())
                {
                    pPoint->PointStauts = EN_WayPointRunState ::enum_WayPointRunning;
                }
                if(pPoint->AxisPoints[i].pFunBasic->Error())
                {
                    pPoint->PointStauts = EN_WayPointRunState ::enum_WayPointError;
                }
            }
        }
        else if(enum_MoveBaseTypeAxis != pPoint->enMoveBaseType)
        {
            printf("WayPointTypeError!\n");
        }
        //点位运动完成
        if(EN_WayPointRunState::enum_WayPointFinished == pPoint->PointStauts)
        {
            m_nWayPoinRunIndex++;
        }
        else if(EN_WayPointRunState::enum_WayPointError == pPoint->PointStauts)
        {
            //错误
            m_nWayPoinRunIndex = -1;
        }
    }
    else if(enum_WayPointError == pPoint->PointStauts)
    {
        InitWaypoint();
        m_bWayPoinError = true;
    }
    return;
}