#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <arpa/inet.h>

#include "tcpEventChannel.h" 
#include "events.h"
#include "debug.h"


std::map<int,TcpEventChannel*> TcpEventChannel::m_chanFdMap;

static void split( const std::string &str, 
            std::map<std::string,std::string>& foo );

TcpEventChannel::TcpEventChannel( AllocFuncPtr func, std::string config, std::string name ) : 
	EventChannel( func, name ), m_fd( -1 ) 
{
    std::map<std::string,std::string> foo;

    split( config, foo );

	DBGX2(DBG_EC,"%s\n",getName().c_str());
	if ( foo.find("server") == foo.end() ) {
		m_fd = initServer( foo["listenPort"] );
		m_chanFdMap[m_fd] = this;
		DBGX2( DBG_EC, "fd=%d name=`%s`\n", m_fd, name.c_str() );
	} else {
		m_clientServer = foo["server"];
		m_clientServerPort = foo["serverPort"];
	}
}


void TcpEventChannel::close()
{
	DBGX2(DBG_EC,"%s fd=%d\n",getName().c_str(),m_fd);
	assert( m_fd != -1 );
	m_chanFdMap.erase(m_fd);
	::close( m_fd );
	m_fd = -1;
}

int TcpEventChannel::xx()
{
	int fd;
	int count = 60;
	do { 
		fd = initClient( m_clientServer, m_clientServerPort );
	} while ( fd < 0 && count-- && sleep(1) == 0  );
	if ( -1 != fd ) {
		m_chanFdMap[fd] = this;
	}
	DBGX2(DBG_EC,"client fd=%d\n",fd);
	return fd;
}

TcpEventChannel::TcpEventChannel( AllocFuncPtr func, int fd, std::string name ) : 
	EventChannel( func, name ), m_fd( fd )
{
	DBGX2(DBG_EC,"%s fd=%d\n",getName().c_str(),m_fd);
	m_chanFdMap[fd] = this;
}

TcpEventChannel::~TcpEventChannel( ) 
{
	DBGX2(DBG_EC,"%s fd=%d\n",getName().c_str(),m_fd);
	if ( m_fd > -1 ) ::close( m_fd );
}
int TcpEventChannel::initClient( std::string hostname, std::string portStr )
{
	DBGX2(DBG_EC,"\n");
	unsigned short port = atoi( portStr.c_str() );
	struct sockaddr_in serv_addr;
	struct hostent *he;
	int fd;

	fd = socket( AF_INET, SOCK_STREAM, 0 );
	assert( fd >= 0 );

	he = gethostbyname( hostname.c_str() );
	assert( he );

    memset(&serv_addr, 0, sizeof(serv_addr));
	memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length ); 

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( port );

	int ret = connect( fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)); 	

	DBGX2(DBG_EC,"serverPort=%d fd=%d\n",port,ret);

	return ret == 0 ? fd : -1; 
}

int TcpEventChannel::initServer( std::string portStr )
{
	unsigned short port = atoi( portStr.c_str() );

	int fd = setupRecv( port );

    DBGX2(DBG_EC,"serverPort=%d listenFd=%d\n", port, fd );
	return fd;
}

#define MAXPENDING 6

int TcpEventChannel::setupRecv( int port )
{
    int fd = socket( AF_INET, SOCK_STREAM, 0 );
    assert( fd >= 0 ); 

	int on = 1;
	int status = setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, 
			 (const char*) &on, sizeof(on)); 
	assert(0==status);

    struct sockaddr_in echoServAddr; /* Local address */

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(port);


    if (bind( fd, (struct sockaddr *) 
                                    &echoServAddr, sizeof(echoServAddr)) < 0) {
		::close(fd);
        assert(0);
    }

    if (listen( fd, MAXPENDING ) < 0) {
		::close(fd);
        assert(0);
    }
	DBGX2(DBG_EC,"fd=%d\n",fd);
    return fd; 
}

TcpEventChannel* TcpEventChannel::selected( int fd )
{
	DBG2(DBG_EC,"fd=%d\n",fd);
	assert( m_chanFdMap.find(fd ) != m_chanFdMap.end() );

	return m_chanFdMap[fd];
}

EventChannel* TcpEventChannel::accept( )
{
   	int cliFd = ::accept( m_fd, NULL, NULL );
	DBGX2(DBG_EC,"fd=%d\n",cliFd);
	assert( cliFd >= 0 );
	
	return new TcpEventChannel( m_allocFunc, cliFd, getName() + "-recv" );
}
#if 1 
#define print(x,y)
#else
static void _print( unsigned char* buf, size_t len )
{ 
	unsigned int z=0;
	DBG2(DBG_EC,"len=%lu\n",len);
	DBG2(DBG_EC," ");
	while ( z < len ) {
		fprintf(stderr, "%02x ", buf[z++] );
		if ( 0 == z % 8 ) {
			fprintf(stderr,"\n");
			DBG2(DBG_EC," ");
		}
	}
	fprintf(stderr,"\n");
}
#endif

Event* TcpEventChannel::getEvent( bool blocking ) 
{
	if ( -1 == m_fd ) {
		m_fd = xx();
	}

	EventType type;
	size_t nbytes = read( m_fd, &type, sizeof(type) );
	print( (unsigned char*) &type, nbytes );
		
	if ( 0 == nbytes ) {
		return NULL;
	}
	assert( nbytes == sizeof(type) );

	size_t length;
	nbytes = read( m_fd, &length, sizeof(length) );
	assert( nbytes == sizeof(length) );
	print( (unsigned char*) &length, nbytes );


	SerialBuf buf(length);	
	nbytes = read( m_fd, buf.addr(), buf.length() );
	assert( nbytes == buf.length() );
	print( (unsigned char*) buf.addr(), nbytes );

	DBGX2(DBG_EC,"%s length=%lu \n",getName().c_str(), length);

    return m_allocFunc( type, buf );
}

bool TcpEventChannel::sendEvent( Event* event )
{
	if ( -1 == m_fd ) {
		m_fd = xx();
	}

	SerialBuf buf;
	event->serialize_out(buf);

	size_t nbytes = write( m_fd, &event->type, sizeof(event->type) ); 
	assert( nbytes == sizeof(event->type) );
	print( (unsigned char*) &event->type, nbytes );

	size_t length = buf.length(); 
	nbytes = write( m_fd, &length, sizeof(length) ); 
	assert( nbytes == sizeof(length) );
	print( (unsigned char*) &length, nbytes );
	
	nbytes = write( m_fd, buf.addr(), buf.length() ); 
	assert( nbytes == buf.length() );
	print( (unsigned char*) buf.addr(), nbytes );

	DBGX2(DBG_EC,"%s length=%lu \n",getName().c_str(), length);

    return true;
}

/************************************************************************/

TcpChannelSelect::TcpChannelSelect()
{
}

bool TcpChannelSelect::addChannel( EventChannel* chan, Data* ptr )
{
	DBGX2(DBG_EC,"name='%s'\n",chan->getName().c_str() );
    assert( m_chanMap.find( chan ) == m_chanMap.end() );

    m_chanMap[chan] = ptr;

    return false;
}

bool TcpChannelSelect::delChannel( EventChannel* chan )
{
	DBGX2(DBG_EC,"\n");
    assert( m_chanMap.find( chan ) != m_chanMap.end() );

    m_chanMap.erase(chan);

    return false;
}

ChannelSelect::Data* TcpChannelSelect::wait()
{
    int     fdmax = 0;;
    fd_set  read_fds;
    fd_set  write_fds;
	EventChannel* chan;

	do {
    	FD_ZERO( &read_fds );
    	FD_ZERO( &write_fds );

		std::map<EventChannel*,Data*>::iterator iter = m_chanMap.begin();

		while ( iter != m_chanMap.end() ) {
    		int fd = static_cast<TcpEventChannel*>(iter->first)->getSelectFd();

			DBGX2(DBG_EC,"fd=%d %s\n",fd, 
				static_cast<TcpEventChannel*>(iter->first)->getName().c_str());
			if ( fd > -1 ) {
    			fdmax = fd > fdmax ? fd : fdmax; 
    			FD_SET( fd, &read_fds );
    			FD_SET( fd, &write_fds );
			}
			++iter;
		}

		DBGX2(DBG_EC,"calling select\n");

    	int ret = select( fdmax+1, &read_fds, NULL, NULL, NULL );
    	assert( ret > 0 );

    	for ( int i = 0; i <= fdmax; i++ ) {
        	if ( FD_ISSET( i, &read_fds ) ) {
				DBGX2(DBG_EC,"selected %d\n",i);
				chan = TcpEventChannel::selected( i );
				break;
        	}
    	} 
	} while ( ! chan );

    return m_chanMap[chan];
}

/************************************************************************/
static void split( const std::string &str,
            std::map<std::string,std::string>& foo )
{
    //printf("%s\n",str.c_str());

    std::string tmp = str;
    while ( ! tmp.empty() ) {
        tmp = tmp.substr( tmp.find_first_not_of(' '));
        size_t pos = tmp.find_first_of('=');
        std::string key = tmp.substr( 0, pos );

        tmp = tmp.substr( pos + 1 );

        pos = tmp.find_first_of(' ');

        foo[ key ] = tmp.substr( 0, pos);
        //printf("%s %s\n",key.c_str(), foo[key].c_str());

        if ( pos == std::string::npos ) {
            tmp.clear();
        } else {
            tmp = tmp.substr( pos + 1 );
        }
    }
}
