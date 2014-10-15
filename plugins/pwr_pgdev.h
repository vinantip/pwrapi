#ifndef PWR_PGDEV_H 
#define PWR_PGDEV_H 

#include "dev.h"

#ifdef __cplusplus
extern "C" {
#endif

plugin_devops_t *pwr_pgdev_init( const char *initstr );
int pwr_pgdev_final( plugin_devops_t *dev );

pwr_fd_t pwr_pgdev_open( plugin_devops_t *dev, const char *openstr );
int pwr_pgdev_close( pwr_fd_t fd );

int pwr_pgdev_read( pwr_fd_t fd, PWR_AttrName attr,
    void *value, unsigned int len, PWR_Time *timestamp );
int pwr_pgdev_write( pwr_fd_t fd, PWR_AttrName attr,
    void *value, unsigned int len );

int pwr_pgdev_readv( pwr_fd_t fd, unsigned int arraysize,
    const PWR_AttrName attrs[], void *values, PWR_Time timestamp[], int status[] );
int pwr_pgdev_writev( pwr_fd_t fd, unsigned int arraysize,
    const PWR_AttrName attrs[], void *values, int status[] );

int pwr_pgdev_time( pwr_fd_t fd, PWR_Time *timestamp );
int pwr_pgdev_clear( pwr_fd_t fd );

#ifdef __cplusplus
}
#endif

#endif