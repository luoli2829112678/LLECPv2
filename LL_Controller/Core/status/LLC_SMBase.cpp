/* 
 */

 
#include "LLC_SMBase.h"

static void goToErrorState( struct llsm_stateMachine *stateMachine,
      struct llsm_event *const event );
static struct llsm_transition *getTransition( struct llsm_stateMachine *stateMachine,
      struct llsm_state *state, struct llsm_event *const event,struct llsm_state*&nextState );

void stateM_init( struct llsm_stateMachine *fsm,
      struct llsm_state *initialState, struct llsm_state *errorState,
      _actionAPI pnoAction)
{
   if ( !fsm )
      return;

   fsm->currentState = initialState;
   fsm->previousState = NULL;
   fsm->errorState = errorState;
   fsm->pnoAction=pnoAction;
}

int stateM_handleEvent( struct llsm_stateMachine *fsm,struct llsm_event *event,bool bCallnoAction)
{
   if ( fsm==nullptr || event==nullptr )
      return stateM_errArg;

   if ( fsm->currentState==nullptr )
   {
      goToErrorState( fsm, event );
      return stateM_errorStateReached;
   }

   if ( fsm->currentState->transitions.size()==0 )
      return stateM_noStateChange;
   // if ( fsm->currentState->numTransitions==0 )
   //    return stateM_noStateChange;

   struct llsm_state *curState = fsm->currentState;
   struct llsm_state* nextState=nullptr;
   do {
      nextState=nullptr;
      struct llsm_transition *transition = getTransition( fsm, curState, event ,nextState);

      /* If there were no transitions for the given event for the current
       * state, check if there are any transitions for any of the parent
       * states (if any): */
      if ( transition==nullptr )
      {
         if (nextState!=nullptr)
         {
            nextState = nextState->parentState;            
         }
         curState = nextState;//LCC
         if (bCallnoAction && (fsm->pnoAction!=nullptr))
         {
            fsm->pnoAction(fsm->currentState->strData, event,"");
         }
         
         continue;
      }

      /* A transition must have a next state defined. If the user has not
       * defined the next state, go to error state: */
      if (( transition->nextState==nullptr )&&(nextState==nullptr))
      {
         goToErrorState( fsm, event );
         return stateM_errorStateReached;
      }
      if (nextState==nullptr)
      {
         nextState = transition->nextState;
      }
      

      /* If the new state is a parent state, enter its entry state (if it has
       * one). Step down through the whole family tree until a state without
       * an entry state is found: */
      while ( nextState->entryState!=nullptr )
         nextState = nextState->entryState;
      
      curState=nextState;//LCC

      bool bAction=true;
      /* Run transition action (if any): */
      if ( transition->action!=nullptr )
         bAction=transition->action( fsm->currentState->strData, event, nextState->strData);

      if (bAction)
      {//action返回true，需要切换状态
         /* Run exit action only if the current state is left (only if it does
       * not return to itself): */
         if ( nextState != fsm->currentState && (fsm->currentState->exitAction!=nullptr) )
            fsm->currentState->exitAction(fsm->currentState->unStateCode,transition, fsm->currentState->strData, event );      

      /* Call the new state's entry action if it has any (only if state does
       * not return to itself): */
         if ( nextState != fsm->currentState && (nextState->entryAction!=nullptr) )
            nextState->entryAction(nextState->unStateCode,transition, nextState->strData, event );

         fsm->previousState = fsm->currentState;
         fsm->currentState = nextState;

         //printf("状态切换至%s\n",fsm->currentState->strData.c_str());
      }     
      
      /* If the state returned to itself: */
      if ( fsm->currentState == fsm->previousState )
         return stateM_stateLoopSelf;

      if ( fsm->currentState == fsm->errorState )
         return stateM_errorStateReached;

      /* If the new state is a final state, notify user that the state
       * machine has stopped: */
      //if ( fsm->currentState->numTransitions==0 )
      if ( fsm->currentState->transitions.empty())
         return stateM_finalStateReached;

      return stateM_stateChanged;
   } while ( curState!=nullptr );

   return stateM_noStateChange;
}

struct llsm_state *stateM_currentState( struct llsm_stateMachine *fsm )
{
   if ( !fsm )
      return NULL;

   return fsm->currentState;
}

struct llsm_state *stateM_previousState( struct llsm_stateMachine *fsm )
{
   if ( !fsm )
      return NULL;

   return fsm->previousState;
}


static void goToErrorState( struct llsm_stateMachine *fsm,
      struct llsm_event *const event )
{
   fsm->previousState = fsm->currentState;
   fsm->currentState = fsm->errorState;

   if ( fsm->currentState && fsm->currentState->entryAction )
      fsm->currentState->entryAction(fsm->currentState->unStateCode,nullptr, fsm->currentState->strData, event );
}

static struct llsm_transition *getTransition( struct llsm_stateMachine *fsm,
      struct llsm_state *state, struct llsm_event *const event,
      struct llsm_state*&nextState )
{
   //size_t i;

   std::map<std::string,struct llsm_transition>::iterator post = state->transitions.find(event->cmd);
   if (post==state->transitions.end())
   {
      return NULL;
   }
   struct llsm_transition *t = &(post->second);
   if ( t->eventType == event->type && t->condition==event->cmd)
   {
      if ( !t->guard )//msg类型
            return t;
         /* If transition is guarded, ensure that the condition is held: */
      else if ( t->guard( t->condition, event,nextState ) )//timer类型
         return t;
   }
   // for ( i = 0; i < state->numTransitions; ++i )
   // {
   //    struct transition *t = &state->transitions[ i ];

   //    /* A transition for the given event has been found: */
      // if ( t->eventType == event->type && t->condition==event->cmd)
      // {
      //    if ( !t->guard )//msg类型
      //       return t;
      //    /* If transition is guarded, ensure that the condition is held: */
      //    else if ( t->guard( t->condition, event ) )//timer类型
      //       return t;
      // }
   // }

   /* No transitions found for given event for given state: */
   return NULL;
}

bool stateM_stopped( struct llsm_stateMachine *stateMachine )
{
   if ( !stateMachine )
      return true;
   return stateMachine->currentState->transitions.empty();
   //return stateMachine->currentState->numTransitions == 0;
}
