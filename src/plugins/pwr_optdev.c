/*
 * Copyright 2014-2015 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000, there is a non-exclusive license for use of this work
 * by or on behalf of the U.S. Government. Export of this program may require
 * a license from the United States Government.
 *
 * This file is part of the Power API Prototype software package. For license
 * information, see the LICENSE file in the top level directory of the
 * distribution.
*/

#include "pwr_optdev.h"
#include "pwr_dev.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#define OPT_CPU_MODEL_6272               1
#define OPT_CPU_MODEL_A10_5800           16

/* D18F0x0[15:0] */
#define MSR_OPT_CORE_REG                 0x0
#define MSR_OPT_CORE_OFFSET              0x18

/* D18F0x0[15:0] */
#define MSR_OPT_VENDOR_REG               0x0
#define MSR_OPT_VENDOR_OFFSET            0x0
#define MSR_OPT_VENDOR_ID                0x1022
#define MSR_OPT_VENDOR_MASK              0xffff
#define MSR_OPT_VENDOR_SHIFT             0

/* D18F3xE8[31:29] */
#define MSR_OPT_MULTINODE_REG            0x3
#define MSR_OPT_MULTINODE_OFFSET         0xe8
#define MSR_OPT_MULTINODE_BIT            29
#define MSR_OPT_INTERNAL_BIT1            30
#define MSR_OPT_INTERNAL_BIT2            31

/* D18F4x1B8[15:0] */
#define MSR_OPT_PROC_TDP_REG             0x4
#define MSR_OPT_PROC_TDP_OFFSET          0x1b8
#define MSR_OPT_PROC_TDP_MASK            0xffff
#define MSR_OPT_BASE_TDP_SHIFT           16

/* D18F5xE0[3:0] */
#define MSR_OPT_RUN_AVG_RANGE_REG        0x5
#define MSR_OPT_RUN_AVG_RANGE_OFFSET     0xe0
#define MSR_OPT_RUN_AVG_RANGE_MASK       0x000f

/* D18F5xE8[15:0] */
#define MSR_OPT_TDP_TO_WATT_REG          0x5
#define MSR_OPT_TDP_TO_WATT_OFFSET       0xe8
#define MSR_OPT_TDP_TO_WATT_HI_MASK      0x03ff
#define MSR_OPT_TDP_TO_WATT_LO_MASK      0xfc00
#define MSR_OPT_TDP_TO_WATT_HI_SHIFT     6
#define MSR_OPT_TDP_TO_WATT_LO_SHIFT     10
#define MSR_OPT_TDP_TO_WATT_DIVIDE_BY    65536
 
/* D18F5xE8[28:16] */
#define MSR_OPT_APM_TDP_LIMIT_REG        0x5
#define MSR_OPT_APM_TDP_LIMIT_OFFSET     0xe8
#define MSR_OPT_APM_TDP_LIMIT_MASK       0x1fffffff
#define MSR_OPT_APM_TDP_LIMIT_SHIFT      16

/* D18F5xE8[25:4] */
#define MSR_OPT_RUN_AVG_ACC_CAP_REG      0x5
#define MSR_OPT_RUN_AVG_ACC_CAP_OFFSET   0xe0
#define MSR_OPT_RUN_AVG_ACC_CAP_MASK     0x03ffffff
#define MSR_OPT_RUN_AVG_ACC_CAP_SHIFT    4
#define MSR_OPT_RUN_AVG_ACC_CAP_NEG      0x00200000
#define MSR_OPT_RUN_AVG_ACC_CAP_NEG_MASK 0x001fffff

#define MSR(X,Y,Z) ((X&Y)>>Z)
#define LMSR(X,Y,Z) ((X&Y)<<Z)
#define MSR_BIT(X,Y) ((X&(1LL<<Y))?1:0)

#define OPT_NODE_MAX 8

typedef struct {
    int number;

    double tdp_to_watt;
    double run_avg_range;
    double avg_divide_by;
    double proc_tdp;
    double base_tdp;
    double off_core_pwr_watt;
} node_t;

typedef struct {
    int cpu_model;
    int node_count;
    node_t node[OPT_NODE_MAX];

    int core;
} pwr_optdev_t;
#define PWR_OPTDEV(X) ((pwr_optdev_t *)(X))

typedef struct {
    pwr_optdev_t *dev;
} pwr_optfd_t;
#define PWR_OPTFD(X) ((pwr_optfd_t *)(X))

static plugin_devops_t devops = {
    .open         = pwr_optdev_open,
    .close        = pwr_optdev_close,
    .read         = pwr_optdev_read,
    .write        = pwr_optdev_write,
    .readv        = pwr_optdev_readv,
    .writev       = pwr_optdev_writev,
    .time         = pwr_optdev_time,
    .clear        = pwr_optdev_clear,
#if 0
    .stat_get     = pwr_dev_stat_get,
    .stat_start   = pwr_dev_stat_start,
    .stat_stop    = pwr_dev_stat_stop,
    .stat_clear   = pwr_dev_stat_clear,
#endif
    .private_data = 0x0
};

static int optdev_parse( const char *initstr, int *core )
{
    char *token;

    DBGP( "Info: received initialization string %s\n", initstr );

    if( (token = strtok( (char *)initstr, ":" )) == 0x0 ) {
        fprintf( stderr, "Error: missing core separator in initialization string %s\n", initstr );
        return -1;
    }
    *core = atoi(token);

    DBGP( "Info: extracted initialization string (CORE=%d)\n", *core );

    return 0;
}

static int optdev_identify( int *cpu_model )
{
    FILE *file;
    char *str = 0x0;
    char cpuinfo[80] = "";
    int retval = 0;

    int family;
    char vendor[80] = "";

    if( (file=fopen( "/proc/cpuinfo", "r" )) == 0x0 ) {
        fprintf( stderr, "Error: OPT cpuinfo open failed\n" );
        return -1;
    }

    *cpu_model = -1;
    while( (str=fgets( cpuinfo, 80, file )) != 0x0 ) {
        if( strncmp( str, "vendor_id", 8) == 0 ) {
            sscanf( str, "%*s%*s%s", vendor );
            if( strncmp( vendor, "AuthenticAMD", 12 ) != 0 ) {
                fprintf( stderr, "Warning: %s, only AMD supported\n", vendor );
                retval = -1;
            }
        }
        else if( strncmp( str, "cpu_family", 10) == 0 ) {
            sscanf( str, "%*s%*s%*s%d", &family );
            if( family != 21 ) {
                fprintf( stderr, "Warning: Unsupported CPU family %d\n", family );
                retval = -1;
            }
        }
        else if( strncmp( str, "model", 5) == 0 ) {
            sscanf( str, "%*s%*s%d", cpu_model );

            switch( *cpu_model ) {
                case OPT_CPU_MODEL_6272:
                case OPT_CPU_MODEL_A10_5800:
                    break;
                default:
                    fprintf( stderr, "Warning: Unsupported model %d\n", *cpu_model );
                    retval = -1;
                    break;
            }
        }
    }

    fclose( file );
    return retval;
}

static int optdev_read( unsigned long node, unsigned long reg, unsigned long offset, unsigned long *msr )
{
    uint64_t value;
    char cmd[255] = "", buf[255] = "";
    FILE *pipe = 0x0;

    sprintf( cmd, "setpci -f -s 0:%x.%x 0x%x.l 2>&1", node+MSR_OPT_CORE_OFFSET, reg, offset );
    DBGP( "Info: %s\n", cmd );

    if( (pipe = popen( cmd, "r" )) && fgets( buf, 255, pipe ) ) {
        sscanf( buf, "%x", msr );
        DBGP( "Info: 0x%08x\n", *msr );
    } else {
        fprintf( stderr, "Error: call to setpci failed\n" );
        return -1;
    }
 
    return 0;
}

static int optdev_gather( node_t node, double *power )
{
    unsigned long msr;
    double apm_tdp_limit;
    unsigned long run_avg_acc_cap;

    if( optdev_read( node.number, MSR_OPT_APM_TDP_LIMIT_REG, MSR_OPT_APM_TDP_LIMIT_OFFSET, &msr ) < 0 ) {
        fprintf( stderr, "Error: PWR OPT device read failed\n" );
        return -1;
    }
    apm_tdp_limit =
        (double)(MSR(msr, MSR_OPT_APM_TDP_LIMIT_MASK, MSR_OPT_APM_TDP_LIMIT_SHIFT));
    DBGP( "Info: node[%d].apm_tdp_limit     - %g\n", node.number, apm_tdp_limit );

    if( optdev_read( node.number, MSR_OPT_RUN_AVG_ACC_CAP_REG, MSR_OPT_RUN_AVG_ACC_CAP_OFFSET, &msr ) < 0 ) {
        fprintf( stderr, "Error: PWR OPT device read failed\n" );
        return -1;
    }
    run_avg_acc_cap =
        (unsigned long)(MSR(msr, MSR_OPT_RUN_AVG_ACC_CAP_MASK, MSR_OPT_RUN_AVG_ACC_CAP_SHIFT));

    if( run_avg_acc_cap >= MSR_OPT_RUN_AVG_ACC_CAP_NEG )
        run_avg_acc_cap = 
            -((~(run_avg_acc_cap & MSR_OPT_RUN_AVG_ACC_CAP_NEG_MASK) & MSR_OPT_RUN_AVG_ACC_CAP_NEG_MASK) + 1);
    DBGP( "Info: node[%d].run_avg_acc_cap   - %u\n", node.number, run_avg_acc_cap );

    *power = (apm_tdp_limit - (run_avg_acc_cap / node.avg_divide_by)) * node.tdp_to_watt;
    DBGP( "Info: node[%d].power             - %g\n", node.number, *power );

    return 0;
}

plugin_devops_t *pwr_optdev_init( const char *initstr )
{
    char file[80] = "";
    int core = 0, i;
    unsigned long msr;
    plugin_devops_t *dev = malloc( sizeof(plugin_devops_t) );
    *dev = devops;

    dev->private_data = malloc( sizeof(pwr_optdev_t) );
    bzero( dev->private_data, sizeof(pwr_optdev_t) );

    DBGP( "Info: PWR OPT device open\n" );
    if( optdev_parse( initstr, &core ) < 0 ) {
        fprintf( stderr, "Error: PWR OPT device initialization string %s invalid\n", initstr );
        return 0x0;
    }

    if( optdev_identify( &(PWR_OPTDEV(dev->private_data)->cpu_model) ) < 0 ) {
        fprintf( stderr, "Error: PWR OPT device model identification failed\n" );
        return 0x0;
    }
    DBGP( "Info: cpu_model                 - %d\n", PWR_OPTDEV(dev->private_data)->cpu_model );

    for( i=0; i<OPT_NODE_MAX; i++ ) {
        if( optdev_read( i, MSR_OPT_VENDOR_REG, MSR_OPT_VENDOR_OFFSET, &msr ) < 0 ) {
            fprintf( stderr, "Error: PWR OPT device read failed\n" );
            return 0x0;
        }

        if( (unsigned long)(MSR(msr, MSR_OPT_VENDOR_MASK, MSR_OPT_VENDOR_SHIFT)) == MSR_OPT_VENDOR_ID ) {
            DBGP( "Info: Vendor ID verified\n" );
            if( optdev_read( i, MSR_OPT_MULTINODE_REG, MSR_OPT_MULTINODE_OFFSET, &msr ) < 0 ) {
                fprintf( stderr, "Error: PWR OPT device read failed\n" );
                return 0x0;
            }
            if( (unsigned long)(MSR_BIT(msr, MSR_OPT_MULTINODE_BIT)) ) {
                if( (unsigned long)(MSR_BIT(msr, MSR_OPT_INTERNAL_BIT1)) == 0 &&
                    (unsigned long)(MSR_BIT(msr, MSR_OPT_INTERNAL_BIT2)) == 0 ) {
                    PWR_OPTDEV(dev->private_data)->node[PWR_OPTDEV(dev->private_data)->node_count++].number = i;
                }
                DBGP( "Info: Ignored multinode\n" );
            } else {
                PWR_OPTDEV(dev->private_data)->node[PWR_OPTDEV(dev->private_data)->node_count++].number = i;
            }
        } else {
            DBGP( "Info: Vendor ID not supported\n" );
        }
    }
    DBGP( "Info: node_count                - %d\n", PWR_OPTDEV(dev->private_data)->node_count );

    for( i=0; i<PWR_OPTDEV(dev->private_data)->node_count; i++ ) {
        DBGP( "Info: node[%d].number            - %d\n", i, PWR_OPTDEV(dev->private_data)->node[i].number );

        if( optdev_read( PWR_OPTDEV(dev->private_data)->node[i].number, MSR_OPT_TDP_TO_WATT_REG, MSR_OPT_TDP_TO_WATT_OFFSET, &msr ) < 0 ) {
            fprintf( stderr, "Error: PWR OPT device read failed\n" );
            return 0x0;
        }
        PWR_OPTDEV(dev->private_data)->node[i].tdp_to_watt =
            (double)((unsigned long)(LMSR(msr, MSR_OPT_TDP_TO_WATT_HI_MASK, MSR_OPT_TDP_TO_WATT_HI_SHIFT)) |
            (unsigned long)(MSR(msr, MSR_OPT_TDP_TO_WATT_LO_MASK, MSR_OPT_TDP_TO_WATT_LO_SHIFT))) / MSR_OPT_TDP_TO_WATT_DIVIDE_BY;
        DBGP( "Info: node[%d].tdp_to_watt       - %g\n", i, PWR_OPTDEV(dev->private_data)->node[i].tdp_to_watt );

        if( optdev_read( PWR_OPTDEV(dev->private_data)->node[i].number, MSR_OPT_RUN_AVG_RANGE_REG, MSR_OPT_RUN_AVG_RANGE_OFFSET, &msr ) < 0 ) {
            fprintf( stderr, "Error: PWR OPT device read failed\n" );
            return 0x0;
        }
        PWR_OPTDEV(dev->private_data)->node[i].run_avg_range =
            (unsigned long)(MSR(msr, MSR_OPT_RUN_AVG_RANGE_MASK, 0));
        PWR_OPTDEV(dev->private_data)->node[i].avg_divide_by =
            pow( 2.0, PWR_OPTDEV(dev->private_data)->node[i].run_avg_range + 1 );
        DBGP( "Info: node[%d].run_avg_range     - %g\n", i, PWR_OPTDEV(dev->private_data)->node[i].run_avg_range );

        if( optdev_read( PWR_OPTDEV(dev->private_data)->node[i].number, MSR_OPT_PROC_TDP_REG, MSR_OPT_PROC_TDP_OFFSET, &msr ) < 0 ) {
            fprintf( stderr, "Error: PWR OPT device read failed\n" );
            return 0x0;
        }
        PWR_OPTDEV(dev->private_data)->node[i].proc_tdp =
            (unsigned long)(MSR(msr, MSR_OPT_PROC_TDP_MASK, 0));
        DBGP( "Info: node[%d].avg_divide_by     - %g\n", i, PWR_OPTDEV(dev->private_data)->node[i].avg_divide_by );

        /* TODO - unnecessary values to read in but possibly useful */
        DBGP( "Info: node[%d].proc_tdp          - %g\n", i, PWR_OPTDEV(dev->private_data)->node[i].proc_tdp );
        DBGP( "Info: node[%d].base_tdp          - %g\n", i, PWR_OPTDEV(dev->private_data)->node[i].base_tdp );
        DBGP( "Info: node[%d].off_core_pwr_watt - %g\n", i, PWR_OPTDEV(dev->private_data)->node[i].off_core_pwr_watt );
    }

    return dev;
}

int pwr_optdev_final( plugin_devops_t *dev )
{
    DBGP( "Info: PWR OPT device close\n" );

    free( dev->private_data );
    free( dev );

    return 0;
}

pwr_fd_t pwr_optdev_open( plugin_devops_t *dev, const char *openstr )
{
    pwr_fd_t *fd = malloc( sizeof(pwr_optfd_t) );
    PWR_OPTFD(fd)->dev = PWR_OPTDEV(dev->private_data);

    return fd;
}

int pwr_optdev_close( pwr_fd_t fd )
{
    PWR_OPTFD(fd)->dev = 0x0;
    free( fd );

    return 0;
}

int pwr_optdev_read( pwr_fd_t fd, PWR_AttrName attr, void *value, unsigned int len, PWR_Time *timestamp )
{
    double power = 0.0;
    struct timeval tv;

    DBGP( "Info: PWR OPT device read\n" );

    if( len != sizeof(double) ) {
        fprintf( stderr, "Error: value field size of %u incorrect, should be %ld\n", len, sizeof(double) );
        return -1;
    }

    if( optdev_gather( (PWR_OPTFD(fd)->dev)->node[(PWR_OPTFD(fd)->dev)->core], &power ) < 0 ) {
        fprintf( stderr, "Error: PWR OPT device gather failed\n" );
        return -1;
    }

    switch( attr ) {
        case PWR_ATTR_POWER:
            *((double *)value) = power;
            break;
        default:
            fprintf( stderr, "Warning: unknown PWR reading attr requested\n" );
            break;
    }
    gettimeofday( &tv, NULL );
    *timestamp = tv.tv_sec*1000000000ULL + tv.tv_usec*1000;

    return 0;
}

int pwr_optdev_write( pwr_fd_t fd, PWR_AttrName attr, void *value, unsigned int len )
{
    return 0;
}

int pwr_optdev_readv( pwr_fd_t fd, unsigned int arraysize,
    const PWR_AttrName attrs[], void *values, PWR_Time timestamp[], int status[] )
{
    unsigned int i;

    for( i = 0; i < arraysize; i++ )
        status[i] = pwr_optdev_read( fd, attrs[i], (double *)values+i, sizeof(double), timestamp+i );

    return 0;
}

int pwr_optdev_writev( pwr_fd_t fd, unsigned int arraysize,
    const PWR_AttrName attrs[], void *values, int status[] )
{
    unsigned int i;

    for( i = 0; i < arraysize; i++ )
        status[i] = pwr_optdev_write( fd, attrs[i], (double *)values+i, sizeof(double) );

    return 0;
}

int pwr_optdev_time( pwr_fd_t fd, PWR_Time *timestamp )
{
    double value;

    DBGP( "Info: PWR OPT device time\n" );

    return pwr_optdev_read( fd, PWR_ATTR_POWER, &value, sizeof(double), timestamp );
}

int pwr_optdev_clear( pwr_fd_t fd )
{
    return 0;
}

static plugin_dev_t dev = {
    .init   = pwr_optdev_init,
    .final  = pwr_optdev_final,
};

plugin_dev_t* getDev() {
    return &dev;
}
