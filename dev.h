#ifndef _PWR_DEV_H
#define _PWR_DEV_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* pwr_dev_t;
typedef void* pwr_fd_t;

typedef pwr_dev_t (*pwr_init_t)( const char *initstr );
typedef int (*pwr_final_t)( pwr_dev_t dev );

typedef pwr_fd_t (*pwr_open_t)( pwr_dev_t dev, const char *openstr );
typedef int (*pwr_close_t)( pwr_fd_t fd );

typedef int (*pwr_read_t)( pwr_fd_t fd, PWR_AttrName name,
    void *value, unsigned int len, PWR_Time *time );
typedef int (*pwr_write_t)( pwr_fd_t dev, PWR_AttrName name,
    void *value, unsigned int len );

typedef int (*pwr_readv_t)( pwr_fd_t fd, unsigned int arraysize,
    const PWR_AttrName names[], void* ptr, PWR_Time ts[], int status[] );
typedef int (*pwr_writev_t)( pwr_fd_t fd, unsigned int arraysize,
    const PWR_AttrName names[], void* ptr, int status[] );

typedef int (*pwr_time_t)( pwr_fd_t fd, PWR_Time *time );
typedef int (*pwr_clear_t)( pwr_fd_t fd );

typedef struct {
    pwr_init_t  init;
    pwr_final_t final;

    pwr_open_t  open;
    pwr_close_t close;

    pwr_read_t  read;
    pwr_write_t write;

    pwr_readv_t  readv;
    pwr_writev_t writev;

    pwr_time_t  time;
    pwr_clear_t clear;
} plugin_dev_t;

plugin_dev_t* getDev();

#ifdef __cplusplus
}
#endif

#endif
