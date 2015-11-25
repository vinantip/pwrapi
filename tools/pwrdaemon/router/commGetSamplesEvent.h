
#ifndef _RTR_COMM_GET_SAMPLES_REQ_EVENT_H
#define _RTR_COMM_GET_SAMPLES_REQ_EVENT_H

#include <events.h>
#include <eventChannel.h>
#include <debug.h>
#include "router.h"

namespace PWR_Router {

class RtrCommGetSamplesReqEvent: public  CommGetSamplesReqEvent {
  public:
   	RtrCommGetSamplesReqEvent( SerialBuf& buf ) : CommGetSamplesReqEvent( buf ) {}  

	bool process(EventGenerator* _rtr, EventChannel* ec) {
		Router& rtr = *static_cast<Router*>(_rtr);
		Router::Client& client = *rtr.getClient( ec );

    	CommReqInfo* info = new CommReqInfo;
    	info->src = ec;
    	info->ev = this;
    	info->id = id;

    	id = (EventId) info;

    	DBGX("commID=%llu eventId=%#llx new eventId=%p\n",
                                commID, id, info );

		DBGX("period=%f count=%d\n",period,count);

		std::vector<ObjID>& commList= client.getCommList( commID );
    	std::vector<ObjID>::iterator iter = commList.begin();
    	for ( ; iter != commList.end(); ++iter ) {

        	DBGX("%s %d\n",(*iter).c_str(), attrName );
			rtr.sendEvent( (*iter), this );
    	}
		return false;
	}
};

class RtrCommGetSamplesRespEvent: public  CommGetSamplesRespEvent {
  public:
   	RtrCommGetSamplesRespEvent( SerialBuf& buf ) : CommGetSamplesRespEvent( buf ) {}  

	bool process(EventGenerator* _rtr, EventChannel* ec) {

        DBGX("id=%p status=%d \n",(void*)id, status );
        CommReqInfo* info = (CommReqInfo*) id;

        id = info->id;
        info->src->sendEvent( this );
        delete info;
		return false;
	}
};

}

#endif
