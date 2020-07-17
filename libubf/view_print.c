/**
 * @brief View print & parse functions to/from UBF style output
 *
 * @file view_print.c
 */
/* -----------------------------------------------------------------------------
 * Enduro/X Middleware Platform for Distributed Transaction Processing
 * Copyright (C) 2009-2016, ATR Baltic, Ltd. All Rights Reserved.
 * Copyright (C) 2017-2019, Mavimax, Ltd. All Rights Reserved.
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

/* needed for asprintf */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>

#include <ndrstandard.h>
#include <ubfview.h>
#include <ndebug.h>

#include <userlog.h>
#include <view_cmn.h>
#include <atmi_tls.h>
#include <cf.h>
#include "Exfields.h"
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
#define OUTPUT_FORMAT_WDATA fmt_wdata, f->cname, p
#define OUTPUT_FORMAT_NDATA fmt_ndata, f->cname
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/


/**
 * Read the view data from the data input
 * @param cstruct view structure to fill up
 * @param view view name to read
 * @param inf input stream
 * @param p_readf read data from function
 * @param dataptr1 RFU
 * @param level current read level
 * @param p_readbuf_buffered any buffered data returned (i.e. if read from
 *  UBF buffer dump
 * @return EXSUCCEED/EXFAIL
 */
expublic int ndrx_Bvextread (char *cstruct, char *view, FILE *inf,
        long (*p_readf)(char *buffer, long bufsz, void *dataptr1), 
        void *dataptr1, int level, char **p_readbuf_buffered)
{
    int ret=EXSUCCEED;
    int line=0;
    char *readbuf=NULL;
    size_t readbuf_len;
    char fldnm[NDRX_VIEW_CNAME_LEN+1];
    char *value=NULL;
    size_t value_len;
    char flag;
    char *p;
    char *tok;
    BFLDID bfldid;
    BFLDID bfldid_from;
    int fldtype;
    int cpylen;
    int len;
    char *readbuf_buffered=NULL;
    int nr_lead_tabs;
    int is_eof=EXFALSE;
    ndrx_typedview_t *v = NULL;
    
    
    NDRX_USYSBUF_MALLOC_WERR_OUT(readbuf, readbuf_len, ret);
    NDRX_USYSBUF_MALLOC_WERR_OUT(value, value_len, ret);
    
    /* Resolve view descriptor */
    
    if (NULL==(v = ndrx_view_get_view(view)))
    {
        ndrx_Bset_error_fmt(BBADVIEW, "View [%s] not found!", view);
        EXFAIL_OUT(ret);
    }
    
    /* TODO: Init the view to NULL, so that we can count occurrences 
     * Question about initialization of the occurrences, if occurrence
     * follows after some other field -> shall be set first or still added
     * to last. If added to last, then we need to build a hash list of each
     * parsed field to count the occurrences.
     * String Hash... of integer
     */
    
    /* Read line by line */
    while(1)
    {
        if (NULL!=p_readf)
        {
            /* read the data from callback */
            ret = (int)p_readf(readbuf, readbuf_len, dataptr1);
            
            if (0==ret)
            {
                /* eof reached */
                break;
            }
            if (ret < 0)
            {
                ndrx_Bset_error_fmt(BEUNIX, "p_readf() user callback failed");
                
                EXFAIL_OUT(ret);
            }
            ret = EXSUCCEED;
        }
        else 
        {
            if (NULL==fgets(readbuf, readbuf_len, inf))
            {
                /* terminate the loop */
                /*
                 * Check errors on file.
                 */
                if (!feof(inf))
                {
                   /* some other error happened!?! */
                   ndrx_Bset_error_fmt(BEUNIX, "Failed to read from file "
                           "with error: [%s]", strerror(errno));
                   EXFAIL_OUT(ret);
                }

                break;
            }
        }
                
        len = strlen(readbuf);
        line++;
        bfldid=BBADFLDID;
        value[0] = EXEOS;
        fldnm[0] = EXEOS;
        p = readbuf;

        if ('#'==p[0])
        {
            continue; /* <<<< nothing to do - continue */
        }

        /* Ignore any newline we get, so that we are backwards compatible
         * with original logic
         */
        if (0==strcmp(p, "\n"))
        {
            continue; /* <<<< nothing to do - continue */
        }
        
        /* check the leading tabs, to see the current nesting level */
        nr_lead_tabs=0;
        while (*p=='\t')
        {
            nr_lead_tabs++;
            p++;
        }
        
        /* terminate the function if found less tabs than current level*/
        if (nr_lead_tabs < level)
        {
            UBF_LOG(log_debug, "Found tab level %d current %d, popping up line %d", 
                    nr_lead_tabs, level, line);
            if (NULL!=p_readbuf_buffered)
            {
                /* pass to upper level */
                *p_readbuf_buffered=readbuf;
                readbuf=NULL;
            }
            /* terminate the read */
            goto out;
        }
        else if (nr_lead_tabs > level)
        {
            ndrx_Bset_error_fmt(BSYNTAX, "Tab level %d expected %d or less - "
                    "invalid data at line %d", nr_lead_tabs, level, line);
            EXFAIL_OUT(ret);
        }
        /* else: it is current level and process ok */
        
        flag = 0;

        if ('-'==p[0] || '+'==p[0] || '='==p[0])
        {
            /* Check syntax with flags... */

            flag=p[0];

            if (' '!=p[1])
            {
                ndrx_Bset_error_fmt(BSYNTAX, "Space does not follow the flag on "
                                            "line %d!", line);
                
                EXFAIL_OUT(ret);
            }
            else
            {
                /* step forward, flag + eos*/
                p+=2;
            }
        }
        
        tok = strchr(p, '\t');
        if (NULL==tok)
        {
            ndrx_Bset_error_fmt(BSYNTAX, "No tab on "
                                        "line %d!", line);
            EXFAIL_OUT(ret);
        }
        else if (tok==readbuf)
        {
            ndrx_Bset_error_fmt(BSYNTAX, "Line should not start with tab on "
                                        "line %d!", line);
            EXFAIL_OUT(ret);
        } 
        else
        {
            int tmpl = strlen(p);
            /* seems to be ok, remove trailing newline */
            
            if (p[tmpl-1]!='\n')
            {
                /* new line at the end is optional for callbacks... */
                if (NULL==p_readf)
                {
                    ndrx_Bset_error_fmt(BSYNTAX, "Line %d does not "
                            "terminate with newline!", line);
                    EXFAIL_OUT(ret);
                }
            }
            else
            {
                p[tmpl-1]=EXEOS;
            }
        }

        /* now read field number + value */
        cpylen = (tok-p);
        /* Copy off field name */
        NDRX_STRNCPY_EOS(fldnm, p, cpylen, sizeof(fldnm));
        
        /* Copy off value */
        NDRX_STRCPY_SAFE_DST(value, tok+1, value_len);
        UBF_LOG(log_debug, "Got [%s]:[%s]", fldnm, value);

#if 0
        /*
         * Resolve field name
         */
        bfldid = ndrx_Bfldid_int(fldnm);
        if (BBADFLDID==bfldid)
        {
            ndrx_Bset_error_fmt(BBADNAME, "Cannot resolve field ID from [%s] on"
                                        "line %d!", fldnm, line);
            EXFAIL_OUT(ret);
        }
#endif
        
        /* get field descriptor... */
        
        /* Check field type */
        if ((BFLD_STRING == fldtype || BFLD_CARRAY == fldtype) && '='!=flag)
        {
            if (EXFAIL==ndrx_normalize_string(value, &len))
            {
                ndrx_Bset_error_fmt(BSYNTAX, "Cannot normalize value on line %d", 
                        line);
                EXFAIL_OUT(ret);
            }
        }
        
        /* now about to execute command */
        if (0==flag)
        {
            /* TODO: Resolve occurrence */
            if (EXSUCCEED!=(ret=CBadd(p_ub, bfldid, value, len, BFLD_CARRAY)))
            {
                EXFAIL_OUT(ret);
            }
        }
        else if ('+'==flag)
        {
            if (EXSUCCEED!=(ret=CBchg(p_ub, bfldid, 0, value, len, BFLD_CARRAY)))
            {
                EXFAIL_OUT(ret);
            }
        }
        else if ('-'==flag)
        {
            if (EXSUCCEED!=(ret=Bdel(p_ub, bfldid, 0)))
            {
                EXFAIL_OUT(ret);
            }
        }
        else if ('='==flag)
        {
            /* Resolve field to-field id */
            bfldid_from = ndrx_Bfldid_int(value);
            if (BBADFLDID==bfldid_from)
            {
                ndrx_Bset_error_fmt(BBADNAME, "Cannot resolve field ID from [%s] on"
                                            "line %d!", value, line);
                EXFAIL_OUT(ret);
            }
            else
            {
                BFLDLEN len_from=0;
                /* TODO: use Bgetalloc to copy value from */
                char *copy_form = Bgetalloc (p_ub, bfldid_from, 0, &len_from);
                
                /* Find the value and put into buffer. */
                if (NULL!=copy_form)
                {
                    
                    /* WARNING! This might move the source buffer before setting
                     * Say: We set field_id=1 from field_id=2. Thus we will move
                     * the 2 to get space for 1.
                     * Fixed: moved from Bfind to ndrx_Bgetalloc
                     */
                    if (EXSUCCEED!=(ret=Bchg(p_ub, bfldid, 0, copy_form, len_from)))
                    {
                        NDRX_FREE(copy_form);
                        EXFAIL_OUT(ret);
                    }
                    
                    NDRX_FREE(copy_form);
                }
                else
                {
                    EXFAIL_OUT(ret);
                }
            }
        } /* '='==flag */
    } /* while */
    
out:
    
    if (NULL!=readbuf_buffered)
    {
        NDRX_SYSBUF_FREE(readbuf_buffered);
    }

    if (NULL!=readbuf)
    {
        NDRX_SYSBUF_FREE(readbuf);
    }

    if (NULL!=value)
    {
        NDRX_SYSBUF_FREE(value);
    }

    UBF_LOG(log_debug, "%s: return %d", __func__, ret);
    
    return ret;

}

/**
 * Print VIEW data to file pointer 
 * Note that only initialized are printed in case if 
 * @param cstruct ptr to the view struct (c)
 * @param view view name
 * @param outf file pointer to which to print
 * @param p_writef masking function if any (shared with UBF)
 * @param dataptr1 RFU data pointer
 * @param level level (front tabs at which to print)
 * @return EXSUCCEED/EXFAIL
 */
expublic int ndrx_Bvfprint (char *cstruct, char *view, FILE * outf,
          ndrx_plugin_tplogprintubf_hook_t p_writef, void *dataptr1, int level)
{
    int ret=EXSUCCEED;
    BFLDID bfldid;
    BFLDLEN  len;
    BFLDOCC occ;
    char *p;
    int fldtype;
    char *cnv_buf = NULL;
    char *tmp_buf = NULL;
    BFLDLEN cnv_len;
    char fmt_wdata[256];
    char fmt_ndata[256];
    int i;
    BVIEWFLD *vdata;
    Bvnext_state_t bprint_state;
    char *p_view = view;
    
    /* Indicators.. */
    short *C_count;
    short C_count_stor;
    unsigned short *L_length; /* will transfer as long */
    
    ndrx_typedview_t *v;
    ndrx_typedview_field_t *f;
    
    UBF_LOG(log_debug, "%s enter at level %d", __func__, level);
    
    memset(&bprint_state, 0, sizeof(bprint_state));
    
    for (i=0; i<level; i++)
    {
        fmt_wdata[i]='\t';
        fmt_ndata[i]='\t';
    }
    
    fmt_wdata[i]='%';
    fmt_ndata[i]='%';
    
    i++;
    fmt_wdata[i]='s';
    fmt_ndata[i]='s';
    
    i++;
    fmt_wdata[i]='\\';
    fmt_ndata[i]='\\';
            
    i++;
    fmt_wdata[i]='t';
    fmt_ndata[i]='t';
    
    i++;
    fmt_wdata[i]='%';
    fmt_ndata[i]='\\';
    
    i++;
    fmt_wdata[i]='s';
    fmt_ndata[i]='n';
    fmt_ndata[i+1]=EXEOS;
    
    i++;
    fmt_wdata[i]='\\';
    
    i++;
    fmt_wdata[i]='n';
    fmt_ndata[i+1]=EXEOS;
    
    bfldid = BFIRSTFLDID;

    DL_FOREACH(v->fields, f)
    {
        p_view=NULL;
        
        /* free up any buffers used: */
        if (NULL!=tmp_buf)
        {
            NDRX_FREE(tmp_buf);
            tmp_buf = NULL;
        }

        if (NULL!=cnv_buf)
        {
            NDRX_FREE(cnv_buf);
            cnv_buf = NULL;
        }
        
        if (f->flags & NDRX_VIEW_FLAG_ELEMCNT_IND_C)
        {
            C_count = (short *)(cstruct+f->count_fld_offset);
        }
        else
        {
            C_count_stor=f->count; 
            C_count = &C_count_stor;
        }
        
        /* extra check: */
        if (*C_count > f->count)
        {
            UBF_LOG(log_error, "Invalid count for field %s.%s in "
                    "view %hd, specified: %hd", v->vname, f->cname, 
                    f->count, *C_count);
            
            ndrx_Bset_error_fmt(BNOCNAME, "Invalid count for field %s.%s in "
                    "view %hd, specified: %hd", v->vname, f->cname, 
                    f->count, *C_count);
            EXFAIL_OUT(ret);
        }
        
        for (occ=0; occ<*C_count; occ++)
        {
            BFLDLEN dim_size = f->fldsize/f->count;
            p = cstruct+f->offset+occ*dim_size;
            
            /* get the carray length  */
            if (f->flags & NDRX_VIEW_FLAG_LEN_INDICATOR_L)
            {
                L_length = (unsigned short *)(cstruct+f->length_fld_offset+
                            occ*sizeof(unsigned short));
                len = (BFLDLEN)*L_length;
            }
            else
            {
                len=dim_size;
            }
            
            /* print the field... */
            if (BFLD_STRING==fldtype || BFLD_CARRAY==fldtype)
            {
                int temp_len;

                /* For strings we must count off trailing EOS */
                if (BFLD_STRING==fldtype)
                {
                    len=strlen(p);
                }

                temp_len = ndrx_get_nonprintable_char_tmpspace(p, len);

                if (temp_len!=len) /* for carray we need EOS at end! */
                {
                    UBF_LOG(log_debug, "Containing special characters -"
                                        " needs to temp buffer for prefixing");
                    tmp_buf=NDRX_MALLOC(temp_len+1); /* adding +1 for EOS */
                    if (NULL==tmp_buf)
                    {
                        ndrx_Bset_error_fmt(BMALLOC, "%s: Failed to allocate ",
                                __func__, temp_len+1);
                        EXFAIL_OUT(ret);
                    }
                    
                    /* build the printable string */
                    ndrx_build_printable_string(tmp_buf, temp_len+1, p, len);
                    
                    p = tmp_buf;
                }
                else if (BFLD_CARRAY==fldtype) /* we need EOS for carray... */
                {
                    tmp_buf=NDRX_MALLOC(temp_len+1); /* adding +1 for EOS */

                    memcpy(tmp_buf, p, temp_len);

                    if (NULL==tmp_buf)
                    {
                        ndrx_Bset_error_fmt(BMALLOC, "%s: Failed to allocate ", 
                                __func__, temp_len+1);
                        EXFAIL_OUT(ret);
                    }
                    tmp_buf[temp_len] = EXEOS;
                    p = tmp_buf;
                }
            }
            else
            {
                cnv_buf=ndrx_Btypcvt(&cnv_len, BFLD_STRING, p, fldtype, len);

                if (NULL==cnv_buf)
                {
                    /* we failed to convert - FAIL! No buffers should be allocated
                     * at the moment. */
                    break; /* <<< BREAK */
                }
                else
                {
                    p=cnv_buf;
                }

                len=cnv_len;
            }
            
        }
        
        /* value is kept in p */
        if (len>0)
        {
            if (NULL!=p_writef)
            {
                char *tmp;
                long tmp_len;
                int do_write = EXFALSE;
                
                NDRX_ASPRINTF(&tmp, &tmp_len, OUTPUT_FORMAT_WDATA);
                
                if (NULL==tmp)
                {
                    ndrx_Bset_error_fmt(BMALLOC, "%s: NDRX_ASPRINTF failed", 
                            __func__);
                    EXFAIL_OUT(ret);
                }
                
                tmp_len++;
                
                if (EXSUCCEED!=(ret=p_writef(&tmp, tmp_len, dataptr1, &do_write, 
                        outf, bfldid)))
                {
                    ndrx_Bset_error_fmt(BEINVAL, "%s: p_writef user function "
                            "failed with %d for [%s]", 
                            __func__, ret, tmp);
                    NDRX_FREE(tmp);
                    EXFAIL_OUT(ret);
                }
                
                if (do_write)
                {
                    fprintf(outf, "%s", tmp);
                }
                        
                NDRX_FREE(tmp);
            }
            else
            {
                fprintf(outf, OUTPUT_FORMAT_WDATA);
            }
            
        }
        else
        {
            if (NULL!=p_writef)
            {
                char *tmp;
                long tmp_len;
                int do_write = EXFALSE;
                
                NDRX_ASPRINTF(&tmp, &tmp_len, OUTPUT_FORMAT_NDATA);
                
                if (NULL==tmp)
                {
                    ndrx_Bset_error_fmt(BMALLOC, "%s: NDRX_ASPRINTF failed 2", 
                            __func__);
                    EXFAIL_OUT(ret);
                }
                
                tmp_len++;
                
                if (EXSUCCEED!=(ret=p_writef(&tmp, tmp_len, dataptr1, &do_write, outf,
                        bfldid)))
                {
                    ndrx_Bset_error_fmt(BEINVAL, "%s: p_writef user function "
                            "failed with %d for [%s] 2", 
                            __func__, ret, tmp);
                    NDRX_FREE(tmp);
                    EXFAIL_OUT(ret);
                }
                
                if (do_write)
                {
                    fprintf(outf, "%s", tmp);
                }
                
                NDRX_FREE(tmp);
            }
            else
            {
                fprintf(outf, OUTPUT_FORMAT_NDATA);
            }   
        }
        
        if (NULL!=outf && ferror(outf))
        {
            ndrx_Bset_error_fmt(BEUNIX, "Failed to write to file with error: [%s]",
                        strerror(errno));
            EXFAIL_OUT(ret);
        }
        
    }
    
out:
    /* Free up allocated resources */
    if (NULL!=tmp_buf)
    {
        NDRX_FREE(tmp_buf);
    }

    if (NULL!=cnv_buf)
    {
        NDRX_FREE(cnv_buf);
    }

    /* release the stuff... */

    if (0==level)
    {
        fflush(outf);
    }

    return ret;
}

/* vim: set ts=4 sw=4 et smartindent: */
