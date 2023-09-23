/**
 * @brief Background transaction completion & recovery
 *   For tlog background scanning with must:
 *   - Just lock for writing the hash (while iterate over) & prepare console results
 *
 * @file background.c
 */
/* -----------------------------------------------------------------------------
 * Enduro/X Middleware Platform for Distributed Transaction Processing
 * Copyright (C) 2009-2016, ATR Baltic, Ltd. All Rights Reserved.
 * Copyright (C) 2017-2023, Mavimax, Ltd. All Rights Reserved.
 * This software is released under one of the following licenses:
 * AGPL (with Java and Go exceptions) or Mavimax's license for commercial use.
 * See LICENSE file for full text.
 * -----------------------------------------------------------------------------
 * AGPL license:
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License, version 3 as published
 * by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU Affero General Public License, version 3
 * for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * -----------------------------------------------------------------------------
 * A commercial use license is available from Mavimax, Ltd
 * contact@mavimax.com
 * -----------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <utlist.h>
#include <dirent.h>
#include <pthread.h>
#include <signal.h>

#include <ndebug.h>
#include <atmi.h>
#include <atmi_int.h>
#include <typed_buf.h>
#include <ndrstandard.h>
#include <ubf.h>
#include <Exfields.h>
#include <tperror.h>
#include <exnet.h>
#include <ndrxdcmn.h>

#include "tmsrv.h"
#include "../libatmisrv/srv_int.h"
#include <xa_cmn.h>
#include <atmi_int.h>
#include <ndrxdiag.h>
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
#define NDRX_TMS_FILE_STATE_INITIAL     0 /**< Intial file check  */
#define NDRX_TMS_FILE_STATE_IGNORE      1 /**< Broken file, cannot loade (wait for housekeep)  */
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
expublic pthread_t G_bacground_thread;
expublic int G_bacground_req_shutdown = EXFALSE;    /* Is shutdown request? */

exprivate MUTEX_LOCKDECL(M_wait_mutex);
exprivate pthread_cond_t M_wait_cond = PTHREAD_COND_INITIALIZER;

exprivate MUTEX_LOCKDECL(M_background_lock); /* Background operations sync        */

exprivate ndrx_stopwatch_t M_chkdisk_stopwatch;    /**< Check disk logs watch */

exprivate ndrx_tms_file_registry_t *M_broken_tmxids=NULL;

/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/**
 * Check if file is already monitored
 * @param tmxid transaction id
 * @return NULL or entry
 */
expublic ndrx_tms_file_registry_t *ndrx_tms_file_registry_get(const char *tmxid)
{
    ndrx_tms_file_registry_t *p_ret = NULL;
    
    EXHASH_FIND_STR(M_broken_tmxids, tmxid, p_ret);
    
    return p_ret;
}

/**
 * Add file to registry (just a tmxid)
 * @tmxid transaction id
 * @state state of the file
 * @return EXSUCCEED/EXFAIL
 */
expublic int ndrx_tms_file_registry_add(const char *tmxid, int state)
{
    int ret = EXSUCCEED;
    ndrx_tms_file_registry_t *p_ret = NULL;
    
    p_ret = ndrx_tms_file_registry_get(tmxid);
    
    if (NULL==p_ret)
    {
        p_ret = (ndrx_tms_file_registry_t *)NDRX_FPMALLOC(sizeof(ndrx_tms_file_registry_t), 0);

        if (NULL==p_ret)
        {
            NDRX_LOG(log_error, "Failed to allocate memory for tmxid: [%s] monitoring", 
                    tmxid);
            EXFAIL_OUT(ret);
        }
        
        NDRX_STRCPY_SAFE(p_ret->tmxid, tmxid);
        p_ret->state = state;

        EXHASH_ADD_STR(M_broken_tmxids, tmxid, p_ret);
    }
out:
    return ret;
}

expublic int ndrx_tms_file_registry_del(ndrx_tms_file_registry_t *ent)
{
    int ret = EXSUCCEED;
    
    EXHASH_DEL(M_broken_tmxids, ent);
    NDRX_FPFREE(ent);
    
    return ret;
}

/**
 * Free up the registry
 */
expublic void ndrx_tms_file_registry_free(void)
{
    ndrx_tms_file_registry_t *el, *tmp;
    
    EXHASH_ITER(hh, M_broken_tmxids, el, tmp)
    {
        ndrx_tms_file_registry_del(el);
    }
}

/**
 * Lock background operations
 */
expublic void background_lock(void)
{
    MUTEX_LOCK_V(M_background_lock);
}

/**
 * Un-lock background operations
 */
expublic void background_unlock(void)
{
    MUTEX_UNLOCK_V(M_background_lock);
}

/**
 * Read the logfiles from the disk (if any we have there...)
 * MUST be during the startup, otherwise if front services start to
 * work, we might re-parse online logs, and for example we might switch the
 * status to ABORTING... if one was preparing...
 * @return 
 */
expublic int background_read_log(void)
{
    int ret=EXSUCCEED;
    struct dirent **namelist = NULL;
    int n, cnt;
    int len;
    char tranmask[256];
    char fnamefull[PATH_MAX+1];
    atmi_xa_log_t *pp_tl = NULL;
    
    snprintf(tranmask, sizeof(tranmask), "TRN-%ld-%hd-%d-", G_tmsrv_cfg.vnodeid, 
            G_atmi_env.xa_rmid, G_server_conf.srv_id);
    len = strlen(tranmask);
    /* List the files here. */
    cnt = scandir(G_tmsrv_cfg.tlog_dir, &namelist, 0, alphasort);
    if (cnt < 0)
    {
       NDRX_LOG(log_error, "Failed to scan [%s]: %s", 
               G_tmsrv_cfg.tlog_dir, strerror(errno));
       ret=EXFAIL;
       goto out;
    }
    else 
    {
       for (n=0; n<cnt; n++)
       {
           if (0==strcmp(namelist[n]->d_name, ".") || 
                       0==strcmp(namelist[n]->d_name, ".."))
           {
               /* memory leak fixes... */
               NDRX_FREE(namelist[n]);
               continue;
           }

           /* If it is transaction then parse & load */
           
           /*
           NDRX_LOG(log_debug, "[%s] vs [%s] %d", 
                       namelist[n]->d_name, tranmask, len);
           */
           
           if (0==strncmp(namelist[n]->d_name, tranmask, len))
           {
               snprintf(fnamefull, sizeof(fnamefull), "%s/%s", G_tmsrv_cfg.tlog_dir, 
                       namelist[n]->d_name);
               NDRX_LOG(log_warn, "Resuming transaction: [%s]", 
                       fnamefull);
               
               if (EXSUCCEED!=tms_load_logfile(fnamefull, 
                       namelist[n]->d_name+len, &pp_tl))
               {
                   NDRX_LOG(log_error, "Failed to resume transaction: [%s]", 
                       fnamefull);
                   NDRX_FREE(namelist[n]); /* mem leak fixes */
                   /* ret=EXFAIL; ??? */

                    /* Do not pick up this anymore... */
                    if (EXSUCCEED!=ndrx_tms_file_registry_add(namelist[n]->d_name+len, 
                            NDRX_TMS_FILE_STATE_IGNORE))
                    {
                        NDRX_LOG(log_error, "Failed to add tmxid: [%s] to registry (malloc err?)", 
                            namelist[n]->d_name+len);
                        EXFAIL_OUT(ret);
                    }
                   continue;
               }
               
           }
           
           NDRX_FREE(namelist[n]);
       }
       
       NDRX_FREE(namelist);
       namelist = NULL;
    }
    
out:
    if (NULL!=namelist)
    {
       NDRX_FREE(namelist);
    }
    return ret;
}

/**
 * Sleep the thread, with option to wake up (by shutdown).
 * @param sleep_sec
 */
exprivate void thread_sleep(int sleep_sec)
{
    struct timespec wait_time;
    struct timeval now;
    int rt;

    gettimeofday(&now,NULL);

    wait_time.tv_sec = now.tv_sec+sleep_sec;
    wait_time.tv_nsec = now.tv_usec*1000;

    MUTEX_LOCK_V(M_wait_mutex);
    rt = pthread_cond_timedwait(&M_wait_cond, &M_wait_mutex, &wait_time);
    MUTEX_UNLOCK_V(M_wait_mutex);
}

/**
 * Wake up the sleeping thread.
 */
expublic void background_wakeup(void)
{
    MUTEX_LOCK_V(M_wait_mutex);
    pthread_cond_signal(&M_wait_cond);
    MUTEX_UNLOCK_V(M_wait_mutex);
}

/**
 * Read the logs directory and verify
 * that we have all the logs in the memory.
 */
expublic int background_chkdisk(void)
{
    char tmp[PATH_MAX+1];
    int i;
    int ret=EXSUCCEED;
    DIR *dir=NULL;
    struct dirent *entry;
    int len;
    char tranmask[256];
    atmi_xa_log_t *pp_tl = NULL;

    snprintf(tranmask, sizeof(tranmask), "TRN-%ld-%hd-%d-", G_tmsrv_cfg.vnodeid, 
            G_atmi_env.xa_rmid, G_server_conf.srv_id);
    len = strlen(tranmask);

    dir = opendir(G_tmsrv_cfg.tlog_dir);

    if (dir == NULL) {

        NDRX_LOG(log_error, "opendir [%s] failed: %s", 
            G_tmsrv_cfg.tlog_dir, strerror(errno));
        EXFAIL_OUT(ret);
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (0==strncmp(entry->d_name, tranmask, len))
        {
            /* extract transaction id  */
            NDRX_STRCPY_SAFE(tmp, entry->d_name+len);

            if (!tms_log_exists_entry(tmp))
            {
                ndrx_tms_file_registry_t *p_reg = NULL;
                snprintf(tmp, sizeof(tmp), "%s/%s", G_tmsrv_cfg.tlog_dir, 
                        entry->d_name);
                if (ndrx_file_exists(tmp))
                {
                    p_reg=ndrx_tms_file_registry_get(entry->d_name+len);

                    if (NULL==p_reg)
                    {
                        NDRX_LOG(log_error, "ERROR: Unkown transaction log file "
                                "exists [%s] (duplicate processes?) - enqueue for load", 
                            tmp);

                        userlog("ERROR: Unkown transaction log file exists"
                                " [%s] (duplicate processes?) - enqueue for load",
                            tmp);

                        if (EXSUCCEED!=ndrx_tms_file_registry_add(entry->d_name+len, 
                                NDRX_TMS_FILE_STATE_INITIAL))
                        {
                            NDRX_LOG(log_error, "Failed to add tmxid: [%s] to registry (malloc err?)", 
                                entry->d_name+len);
                            EXFAIL_OUT(ret);
                        }
                    }
                    else if (NDRX_TMS_FILE_STATE_INITIAL==p_reg->state)
                    {
                        NDRX_LOG(log_warn, "Loading transaction log [%s]", tmp);
                        userlog("Loading transaction log [%s]", tmp);

                        if (EXSUCCEED!=tms_load_logfile(tmp, entry->d_name+len, &pp_tl))
                        {
                            NDRX_LOG(log_error, "Failed to load transaction log [%s] - ignore log", tmp);
                            userlog("Failed to load transaction log [%s] - ignore log", tmp);
                            /* change state to ignore */
                            p_reg->state = NDRX_TMS_FILE_STATE_IGNORE;
                        }
                        else
                        {
                            NDRX_LOG(log_info, "Transaction log [%s] loaded", tmp);
                            /* remove from hash */
                            ndrx_tms_file_registry_del(p_reg);
                        }
                    }
                } /* still file exists */
            } /* log not found */
        } /* mask matched */
    }

out:
    if (NULL!=dir)
    {
        closedir(dir);
    }

    return ret;

}

/**
 * Continues transaction background loop..
 * Try to complete the transactions.
 * @return  SUCCEED/FAIL
 */
expublic int background_loop(void)
{
    int ret = EXSUCCEED;
    atmi_xa_log_list_t *tx_list;
    atmi_xa_log_list_t *el, *tmp;
    atmi_xa_tx_info_t xai;
    atmi_xa_log_t *p_tl;
    
    memset(&xai, 0, sizeof(xai));

    ndrx_stopwatch_reset(&M_chkdisk_stopwatch);
    
    while(!G_bacground_req_shutdown)
    {
        /* run ping... */
        if (G_tmsrv_cfg.ping_time > 0)
        {
            tm_ping_db(NULL, NULL);
        }

        /* Check against any logs that we are not aware of
         * in that case process reloads (restart) and perform fresh start actions.
         * so that we protect us from duplicate runs.
         */
        if (G_tmsrv_cfg.chkdisk_time > 0 && 
                ndrx_stopwatch_get_delta_sec(&M_chkdisk_stopwatch) > G_tmsrv_cfg.chkdisk_time)
        {
            /* reset is initiated by the func (if required) */
            background_chkdisk();
            ndrx_stopwatch_reset(&M_chkdisk_stopwatch);
        }
        
        /* Check the list of transactions (iterate over...) 
         * Seems anyway, we need a list of background ops here...
         */
        
        /* Lock for processing... (cose xadmin might want to do some stuff in middle
         * Might want to think something better (so that it does not lock all process)
         */
        background_lock();
        tx_list = tms_copy_hash2list(COPY_MODE_BACKGROUND | COPY_MODE_ACQLOCK);
        
        LL_FOREACH_SAFE(tx_list,el,tmp)
        {
            /* el->p_tl.trycount++; moved to COPY_MODE_INCCOUNTER */
            NDRX_LOG(log_info, "XID: [%s] stage: [%hd]. Try: %ld, max: %d.", 
                    el->p_tl.tmxid, el->p_tl.txstage, el->p_tl.trycount, 
                    G_tmsrv_cfg.max_tries);
            
            if (el->p_tl.trycount>=G_tmsrv_cfg.max_tries)
            {
                NDRX_LOG(log_warn, "Skipping try %ld of %ld...", 
                        el->p_tl.trycount,  G_tmsrv_cfg.max_tries);
                /* Have some housekeep. */
                LL_DELETE(tx_list, el);
                NDRX_FREE(el);
                continue;
            }
            
            /* Now try to get transaction for real (with a lock!) */
            if (NULL!=(p_tl = tms_log_get_entry(el->p_tl.tmxid, 0, NULL)))
            {
                p_tl->trycount++;
                
                NDRX_LOG(log_info, "XID: [%s] try counter increased to: %d",
                        el->p_tl.tmxid, p_tl->trycount);
                
                XA_TX_COPY((&xai), p_tl);

                /* If we have transaction in background, then do something with it
                 * The master_op does not matter, as we ignore the error code.
                 */
                tm_drive(&xai, p_tl, XA_OP_COMMIT, EXFAIL, 0L);
            }
            else
            {
                NDRX_LOG(log_debug, "Transaction locked or already "
                        "processed by foreground...");
            }
            /* Have some housekeep. */
            LL_DELETE(tx_list, el);
            NDRX_FREE(el);
        }
        
        background_unlock();
        NDRX_LOG(log_debug, "background - sleep %d", 
                G_tmsrv_cfg.scan_time);
        
        if (!G_bacground_req_shutdown)
            thread_sleep(G_tmsrv_cfg.scan_time);
    }
    
out:
    return ret;
}

/**
 * Background processing of the transactions (Complete them).
 * @return 
 */
expublic void * background_process(void *arg)
{
    NDRX_LOG(log_error, "***********BACKGROUND PROCESS START ********");
    
    tm_thread_init();
    
/*    background_read_log();*/
    
    
   /* Loop over the transactions and:
    * - Check for in-progress timeouts
    * - Try to abort abortable
    * - Try co commit commitable
    * - Use timers counters from the cli params. 
    */
    
    background_loop();
    
    tm_thread_uninit();
    
    NDRX_LOG(log_error, "***********BACKGROUND PROCESS END **********");
    
    return NULL;
}

/**
 * Initialize background process
 * @return EXSUCCEED/EXFAIL
 */
expublic int background_process_init(void)
{
    int ret=EXSUCCEED;
    pthread_attr_t pthread_custom_attr;
    
    /* Read the transaction records from disk
     * shall be done before services open
     * otherwise we might start to read logs
     * from online txns, and if they are preparing,
     * we might set here them to aborting..
     */ 
    if (EXSUCCEED!=background_read_log())
    {
        NDRX_LOG(log_error, "Failed to recover logs");
        userlog("Failed to recover logs");
        EXFAIL_OUT(ret);
    }
    
    pthread_attr_init(&pthread_custom_attr);
    /* clean up resources after exit.. 
    pthread_attr_setdetachstate(&pthread_custom_attr, PTHREAD_CREATE_DETACHED);
    */
    /* set some small stacks size, 1M should be fine! */
    ndrx_platf_stack_set(&pthread_custom_attr);
    if (EXSUCCEED!=pthread_create(&G_bacground_thread, &pthread_custom_attr, 
            background_process, NULL))
    {
        NDRX_PLATF_DIAG(NDRX_DIAG_PTHREAD_CREATE, errno, "background_process_init");
        EXFAIL_OUT(ret);
    }
out:
    return ret;
      
}
/* vim: set ts=4 sw=4 et smartindent: */
