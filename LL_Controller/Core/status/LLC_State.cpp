#include "LLC_State.h"


LL_State::LL_State(/* args */)
{
    m_state.parentState=nullptr;
    m_state.entryState=nullptr;
    m_state.transitions.clear();
    m_state.unStateCode = 0;
    m_state.strData="";
    m_state.entryAction=nullptr;
    m_state.exitAction=nullptr;
}

LL_State::~LL_State()
{
}
LL_State& LL_State::operator=(const LL_State& r)
{
    m_state.parentState=r.m_state.parentState;
    m_state.entryState=r.m_state.entryState;
    m_state.transitions=r.m_state.transitions;
    m_state.unStateCode = r.m_state.unStateCode;
    m_state.strData=r.m_state.strData;
    m_state.entryAction=r.m_state.entryAction;
    m_state.exitAction=r.m_state.exitAction;
    return *this;
}
llsm_state* LL_State::GetState()
{
    return &m_state;
}
void LL_State::RegisterTrans(const string& strCmd,int nMsgType,
            _guardAPI pGuard,_actionAPI pAction,struct llsm_state *nextState)
{
    struct llsm_transition trans;
    trans.eventType=nMsgType;
    trans.condition=strCmd;
    trans.guard=pGuard;
    trans.action=pAction;
    trans.nextState=nextState;
    m_state.transitions.insert(std::make_pair(strCmd,trans));
    if (nMsgType==Cmd_Timer)
    {
        bool bFound=false;
        for (size_t i = 0; i < m_state.checkCmds.size(); i++)
        {
            if (m_state.checkCmds[i]==strCmd)
            {
                bFound=true;
                break;
            }
        }
        if (!bFound)
        {
            m_state.checkCmds.push_back(strCmd);
        }
    }
    
    //g_cmdMap.insert(std::make_pair(strCmd,0));
}
void LL_State::SetState(unsigned int unStateCode,string strData,_stateAPI pEntry,_stateAPI pExit)
{
    m_state.unStateCode = unStateCode;
    m_state.strData=strData;
    m_state.entryAction = pEntry;
    m_state.exitAction = pExit;
}