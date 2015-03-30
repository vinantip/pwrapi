/*
 * Copyright 2014 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000, there is a non-exclusive license for use of this work
 * by or on behalf of the U.S. Government. Export of this program may require
 * a license from the United States Government.
 *
 * This file is part of the Power API Prototype software package. For license
 * information, see the LICENSE file in the top level directory of the
 * distribution.
*/

#include "pwr_xtpmdev.h"
#include "pwr_dev.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

static int xtpmdev_verbose = 0;

typedef struct {
    int fd;
} pwr_xtpmdev_t;
#define PWR_XTPMDEV(X) ((pwr_xtpmdev_t *)(X))

typedef struct {
    pwr_xtpmdev_t *dev;
    double generation;
} pwr_xtpmfd_t;
#define PWR_XTPMFD(X) ((pwr_xtpmfd_t *)(X))

static plugin_devops_t devops = {
    .open         = pwr_xtpmdev_open,
    .close        = pwr_xtpmdev_close,
    .read         = pwr_xtpmdev_read,
    .write        = pwr_xtpmdev_write,
    .readv        = pwr_xtpmdev_readv,
    .writev       = pwr_xtpmdev_writev,
    .time         = pwr_xtpmdev_time,
    .clear        = pwr_xtpmdev_clear,
    .stat_get     = pwr_dev_stat_get,
    .stat_start   = pwr_dev_stat_start,
    .stat_stop    = pwr_dev_stat_stop,
    .stat_clear   = pwr_dev_stat_clear,
    .private_data = 0x0
};

static int xtpmdev_read( const char *name, double *val )
{
    char path[256] = "", strval[20] = "";
    int offset = 0;
    int fd;

    sprintf( path, "/sys/cray/pm_counters/%s", name );
    fd = open( path, O_RDONLY );
    if( fd < 0 ) {
        printf( "Error: unable to open counter file at %s\n", path );
        return -1;
    }

    while( read( fd, strval+offset, 1 ) != EOF ) {
        if( strval[offset] == ' ' ) {
            *val = atof(strval);
            close( fd );
            return 0;
        }
        offset++;
    }

    printf( "Error: unable to parse PM counter value\n" );
    close( fd );
    return -1;
}

static int xtpmdev_write( const char *name, double val )
{
    char path[256] = "", strval[20] = "";
    int fd;

    sprintf( path, "/sys/cray/pm_counters/%s", name );
    fd = open( path, O_WRONLY );
    if( fd < 0 ) {
        printf( "Error: unable to open PM counter file at %s\n", path );
        return -1;
    }

    sprintf( strval, "%lf", val ); 
    if( write( fd, strval, strlen(strval) ) < 0 ) {
        printf( "Error: unable to write PM counter\n" );
        close( fd );
        return -1;
    }

    close( fd );
    return 0;
}

plugin_devops_t *pwr_xtpmdev_init( const char *initstr )
{
    plugin_devops_t *dev = malloc( sizeof(plugin_devops_t) );
    *dev = devops;

    dev->private_data = malloc( sizeof(pwr_xtpmdev_t) );
    bzero( dev->private_data, sizeof(pwr_xtpmdev_t) );

    if( xtpmdev_verbose )
        printf( "Info: initializing PWR XTPM device\n" );

    return dev;
}

int pwr_xtpmdev_final( plugin_devops_t *dev )
{
    if( xtpmdev_verbose )
        printf( "Info: finalizing PWR XTPM device\n" );

    free( dev->private_data );
    free( dev );
    return 0;
}

pwr_fd_t pwr_xtpmdev_open( plugin_devops_t *dev, const char *openstr )
{
    pwr_fd_t *fd = malloc( sizeof(pwr_xtpmfd_t) );
    bzero( fd, sizeof(pwr_xtpmfd_t) );

    if( xtpmdev_verbose )
        printf( "Info: opening PWR XTPM device\n" );

    PWR_XTPMFD(fd)->dev = PWR_XTPMDEV(dev->private_data);

    if( xtpmdev_read( "generation", &(PWR_XTPMFD(fd)->generation) ) < 0 ) {
        printf( "Error: unable to open generation counter\n" );
        return 0x0;
    }

    return fd;
}

int pwr_xtpmdev_close( pwr_fd_t fd )
{
    if( xtpmdev_verbose )
        printf( "Info: closing PWR XTPM device\n" );

    PWR_XTPMFD(fd)->dev = 0x0;
    free( fd );

    return 0;
}

int pwr_xtpmdev_read( pwr_fd_t fd, PWR_AttrName attr, void *value, unsigned int len, PWR_Time *timestamp )
{
    double generation;
    struct timeval tv;

    if( xtpmdev_verbose )
        printf( "Info: reading from PWR XTPM device\n" );

    if( len != sizeof(double) ) {
        printf( "Error: value field size of %u incorrect, should be %ld\n", len, sizeof(double) );
        return -1;
    }

    switch( attr ) {
        case PWR_ATTR_ENERGY:
            if( xtpmdev_read( "energy", (double *)value) < 0 ) {
                printf( "Error: unable to read energy counter\n" );
                return -1;
            }
            break;
        case PWR_ATTR_POWER:
            if( xtpmdev_read( "power", (double *)value) < 0 ) {
                printf( "Error: unable to read power counter\n" );
                return -1;
            }
            break;
        case PWR_ATTR_MAX_POWER:
            if( xtpmdev_read( "power_cap", (double *)value) < 0 ) {
                printf( "Error: unable to read power_cap counter\n" );
                return -1;
            }
            break;
        default:
            printf( "Warning: unknown PWR XTPM reading attr (%u) requested\n", attr );
            break;
    }
    gettimeofday( &tv, NULL );
    *timestamp = tv.tv_sec*1000000000ULL + tv.tv_usec*1000;

    if( xtpmdev_read( "generation", &generation) < 0 ) {
        printf( "Error: unable to open generation counter\n" );
        return 0x0;
    }
    if( PWR_XTPMFD(fd)->generation != generation ) {
        printf( "Warning: generation counter rolled over" );
        PWR_XTPMFD(fd)->generation = generation;
    }

    if( xtpmdev_verbose )
        printf( "Info: reading of type %u at time %llu with value %lf\n",
                attr, *(unsigned long long *)timestamp, *(double *)value );
    return 0;
}

int pwr_xtpmdev_write( pwr_fd_t fd, PWR_AttrName attr, void *value, unsigned int len )
{
    if( xtpmdev_verbose )
        printf( "Info: writing to PWR XTPM device\n" );

    if( len != sizeof(double) ) {
        printf( "Error: value field size of %u incorrect, should be %ld\n", len, sizeof(double) );
        return -1;
    }

    switch( attr ) {
        case PWR_ATTR_MAX_POWER:
            if( xtpmdev_write( "power_cap", *((double *)value) ) < 0 ) {
                printf( "Error: unable to write power_cap counter\n" );
                return -1;
            }
            break;
        default:
            printf( "Warning: unknown PWR XTPM writing attr (%u) requested\n", attr );
            break;
    }

    if( xtpmdev_verbose )
        printf( "Info: reading of type %u with value %lf\n", attr, *(double *)value );
    return 0;
}

int pwr_xtpmdev_readv( pwr_fd_t fd, unsigned int arraysize,
    const PWR_AttrName attrs[], void *values, PWR_Time timestamp[], int status[] )
{
    unsigned int i;

    for( i = 0; i < arraysize; i++ )
        status[i] = pwr_xtpmdev_read( fd, attrs[i], (double *)values+i, sizeof(double), timestamp+i );

    return 0;
}

int pwr_xtpmdev_writev( pwr_fd_t fd, unsigned int arraysize,
    const PWR_AttrName attrs[], void *values, int status[] )
{
    unsigned int i;

    for( i = 0; i < arraysize; i++ )
        status[i] = pwr_xtpmdev_write( fd, attrs[i], (double *)values+i, sizeof(double) );

    return 0;
}

int pwr_xtpmdev_time( pwr_fd_t fd, PWR_Time *timestamp )
{
    double value;

    if( xtpmdev_verbose )
        printf( "Info: reading time from PWR XTPM device\n" );

    return pwr_xtpmdev_read( fd, PWR_ATTR_ENERGY, &value, sizeof(double), timestamp );;
}

int pwr_xtpmdev_clear( pwr_fd_t fd )
{
    return 0;
}

static plugin_dev_t dev = {
    .init   = pwr_xtpmdev_init,
    .final  = pwr_xtpmdev_final,
};

plugin_dev_t* getDev() {
    return &dev;
}
