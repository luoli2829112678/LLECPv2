#ifndef _LLC_STATE_H
#define _LLC_STATE_H

#include <string>
using namespace std;

//#include "LLStruct_Robot.h"

#include "LLC_SMBase.h"
class LL_State
{
private:
    llsm_state m_state;
private:
    LL_State& operator=(const LL_State&);
public:
    LL_State(/* args */);
    ~LL_State();
    llsm_state* GetState();
    void RegisterTrans(const string& strCmd,int nMsgType,
            _guardAPI pGuard,_actionAPI pAction,struct llsm_state *nextState);
    void SetState(unsigned int unStateCode,string strData,_stateAPI pEntry,_stateAPI pExit);
};




#endif  //_LL_STATE_H
