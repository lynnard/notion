/*
 * ion/ioncore/floatframe.c
 *
 * Copyright (c) Tuomo Valkonen 1999-2006. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#include <string.h>

#include <libtu/objp.h>
#include <libtu/minmax.h>

#include <ioncore/common.h>
#include <ioncore/frame.h>
#include <ioncore/framep.h>
#include <ioncore/frame-pointer.h>
#include <ioncore/frame-draw.h>
#include <ioncore/saveload.h>
#include <ioncore/names.h>
#include <ioncore/regbind.h>
#include <ioncore/resize.h>
#include <ioncore/sizehint.h>
#include <ioncore/extlconv.h>
#include <ioncore/strings.h>
#include <ioncore/bindmaps.h>

#include "floatframe.h"
#include "group-ws.h"


/*{{{ Destroy/create frame */


static bool floatframe_init(WFloatFrame *frame, WWindow *parent,
                            const WFitParams *fp)
{
    frame->bar_w=fp->g.w;
    frame->tab_min_w=0;
    frame->bar_max_width_q=1.0;
    
    if(!frame_init((WFrame*)frame, parent, fp, "frame-floating-groupws"))
        return FALSE;
    
    frame->frame.flags|=(FRAME_BAR_OUTSIDE|
                         FRAME_DEST_EMPTY|
                         FRAME_SZH_USEMINMAX|
                         FRAME_FWD_CWIN_RQGEOM);

    region_add_bindmap((WRegion*)frame, ioncore_floatframe_bindmap);
    
    return TRUE;
}


WFloatFrame *create_floatframe(WWindow *parent, const WFitParams *fp)
{
    CREATEOBJ_IMPL(WFloatFrame, floatframe, (p, parent, fp));
}


/*}}}*/
                          

/*{{{ Geometry */

#define BAR_H(FRAME) \
    ((FRAME)->frame.flags&FRAME_TAB_HIDE ? 0 : (FRAME)->frame.bar_h)

void floatframe_offsets(const WFloatFrame *frame, WRectangle *off)
{
    GrBorderWidths bdw=GR_BORDER_WIDTHS_INIT;
    uint bar_h=0;
    
    if(frame->frame.brush!=NULL)
        grbrush_get_border_widths(frame->frame.brush, &bdw);
    
    off->x=-bdw.left;
    off->y=-bdw.top;
    off->w=bdw.left+bdw.right;
    off->h=bdw.top+bdw.bottom;

    bar_h=BAR_H(frame);

    off->y-=bar_h;
    off->h+=bar_h;
}


void floatframe_border_geom(const WFloatFrame *frame, WRectangle *geom)
{
    geom->x=0;
    geom->y=BAR_H(frame);
    geom->w=REGION_GEOM(frame).w;
    geom->h=REGION_GEOM(frame).h-BAR_H(frame);
    geom->w=maxof(geom->w, 0);
    geom->h=maxof(geom->h, 0);

}


void floatframe_bar_geom(const WFloatFrame *frame, WRectangle *geom)
{
    geom->x=0;
    geom->y=0;
    geom->w=frame->bar_w;
    geom->h=BAR_H(frame);
}


void floatframe_managed_geom(const WFloatFrame *frame, WRectangle *geom)
{
    WRectangle off;
    *geom=REGION_GEOM(frame);
    floatframe_offsets(frame, &off);
    geom->x=-off.x;
    geom->y=-off.y;
    geom->w=maxof(geom->w-off.w, 0);
    geom->h=maxof(geom->h-off.h, 0);
}


#define floatframe_border_inner_geom floatframe_managed_geom


void floatframe_resize_hints(WFloatFrame *frame, XSizeHints *hints_ret)
{
    frame_resize_hints(&frame->frame, hints_ret);
    
    if(frame->frame.flags&FRAME_SHADED){
        hints_ret->min_height=frame->frame.bar_h;
        hints_ret->max_height=frame->frame.bar_h;
        hints_ret->base_height=frame->frame.bar_h;
        if(!(hints_ret->flags&PMaxSize)){
            hints_ret->max_width=INT_MAX;
            hints_ret->flags|=PMaxSize;
        }
    }
}


/*}}}*/


/*{{{ Drawing routines and such */


static void floatframe_brushes_updated(WFloatFrame *frame)
{
    /* Get new bar width limits */

    frame->tab_min_w=100;
    frame->bar_max_width_q=0.95;

    if(frame->frame.brush==NULL)
        return;
    
    if(grbrush_get_extra(frame->frame.brush, "floatframe_tab_min_w",
                         'i', &(frame->tab_min_w))){
        if(frame->tab_min_w<=0)
            frame->tab_min_w=1;
    }

    if(grbrush_get_extra(frame->frame.brush, "floatframe_bar_max_w_q", 
                         'd', &(frame->bar_max_width_q))){
        if(frame->bar_max_width_q<=0.0 || frame->bar_max_width_q>1.0)
            frame->bar_max_width_q=1.0;
    }
}


static void floatframe_set_shape(WFloatFrame *frame)
{
    WRectangle gs[2];
    int n=0;
    
    if(frame->frame.brush!=NULL){
        if(!(frame->frame.flags&FRAME_TAB_HIDE)){
            floatframe_bar_geom(frame, gs+n);
            n++;
        }
        floatframe_border_geom(frame, gs+n);
        n++;
    
        grbrush_set_window_shape(frame->frame.brush, 
                                 TRUE, n, gs);
    }
}


#define CF_TAB_MAX_TEXT_X_OFF 10


static int init_title(WFloatFrame *frame, int i)
{
    int textw;
    
    if(frame->frame.titles[i].text!=NULL){
        free(frame->frame.titles[i].text);
        frame->frame.titles[i].text=NULL;
    }
    
    textw=frame_nth_tab_iw((WFrame*)frame, i);
    frame->frame.titles[i].iw=textw;
    return textw;
}


static void floatframe_recalc_bar(WFloatFrame *frame)
{
    int bar_w=0, textw=0, tmaxw=frame->tab_min_w, tmp=0;
    WLListIterTmp itmp;
    WRegion *sub;
    const char *p;
    GrBorderWidths bdw;
    char *title;
    uint bdtotal;
    int i, m;
    
    if(frame->frame.bar_brush==NULL)
        return;
    
    m=FRAME_MCOUNT(&frame->frame);
    
    if(m>0){
        grbrush_get_border_widths(frame->frame.bar_brush, &bdw);
        bdtotal=((m-1)*(bdw.tb_ileft+bdw.tb_iright)
                 +bdw.right+bdw.left);

        FRAME_MX_FOR_ALL(sub, &(frame->frame), itmp){
            p=region_name(sub);
            if(p==NULL)
                continue;
            
            textw=grbrush_get_text_width(frame->frame.bar_brush,
                                         p, strlen(p));
            if(textw>tmaxw)
                tmaxw=textw;
        }

        bar_w=frame->bar_max_width_q*REGION_GEOM(frame).w;
        if(bar_w<frame->tab_min_w && 
           REGION_GEOM(frame).w>frame->tab_min_w)
            bar_w=frame->tab_min_w;
        
        tmp=bar_w-bdtotal-m*tmaxw;
        
        if(tmp>0){
            /* No label truncation needed, good. See how much can be padded. */
            tmp/=m*2;
            if(tmp>CF_TAB_MAX_TEXT_X_OFF)
                tmp=CF_TAB_MAX_TEXT_X_OFF;
            bar_w=(tmaxw+tmp*2)*m+bdtotal;
        }else{
            /* Some labels must be truncated */
        }
    }else{
        bar_w=frame->tab_min_w;
        if(bar_w>frame->bar_max_width_q*REGION_GEOM(frame).w)
            bar_w=frame->bar_max_width_q*REGION_GEOM(frame).w;
    }

    if(frame->bar_w!=bar_w){
        frame->bar_w=bar_w;
        floatframe_set_shape(frame);
    }

    if(m==0 || frame->frame.titles==NULL)
        return;
    
    i=0;
    FRAME_MX_FOR_ALL(sub, &(frame->frame), itmp){
        textw=init_title(frame, i);
        if(textw>0){
            if(frame->frame.flags&FRAME_SHOW_NUMBERS){
                char *s=NULL;
                libtu_asprintf(&s, "%d", i+1);
                if(s!=NULL){
                    title=grbrush_make_label(frame->frame.bar_brush, s, textw);
                    free(s);
                }else{
                    title=NULL;
                }
            }else{
                title=region_make_label(sub, textw, frame->frame.bar_brush);
            }
            frame->frame.titles[i].text=title;
        }
        i++;
    }
}


static void floatframe_size_changed(WFloatFrame *frame, bool wchg, bool hchg)
{
    int bar_w=frame->bar_w;
    
    if(wchg)
        frame_recalc_bar((WFrame*)frame);
    if(hchg || (wchg && bar_w==frame->bar_w))
        floatframe_set_shape(frame);
}


/*}}}*/


/*{{{ Actions */


bool floatframe_set_sticky(WFloatFrame *frame, int sp)
{
    warn(TR("Temporarily unimplemented"));
    return FALSE;
}


/*EXTL_DOC
 * Set \var{frame} stickyness accoding to \var{how} (set/unset/toggle).
 * The resulting state is returned. This function only works across frames
 * on  \type{WGroupWS} that have the same \type{WMPlex} parent.
 * (Temporarily unimplemented)
 */
EXTL_EXPORT_AS(WFloatFrame, set_sticky)
bool floatframe_set_sticky_extl(WFloatFrame *frame, const char *how)
{
    return floatframe_set_sticky(frame, libtu_string_to_setparam(how));
}


/*EXTL_DOC
 * Is \var{frame} sticky? (Temporarily unimplemented)
 */
EXTL_SAFE
EXTL_EXPORT_MEMBER
bool floatframe_is_sticky(WFloatFrame *frame)
{
    warn(TR("Temporarily unimplemented"));
    return FALSE;
}


/*}}}*/


/*{{{ Save/load */


static ExtlTab floatframe_get_configuration(WFloatFrame *frame)
{
    return frame_get_configuration(&(frame->frame));
}


WRegion *floatframe_load(WWindow *par, const WFitParams *fp, ExtlTab tab)
{
    WFloatFrame *frame=create_floatframe(par, fp);
    
    if(frame==NULL)
        return NULL;
    
    frame_do_load((WFrame*)frame, tab);
    
    if(FRAME_MCOUNT(&frame->frame)==0){
        /* Nothing to manage, destroy */
        destroy_obj((Obj*)frame);
        frame=NULL;
    }
    
    return (WRegion*)frame;
}


/*}}}*/


/*{{{ Dynfuntab and class implementation */


static DynFunTab floatframe_dynfuntab[]={
    {mplex_size_changed, floatframe_size_changed},
    {frame_recalc_bar, floatframe_recalc_bar},

    {mplex_managed_geom, floatframe_managed_geom},
    {frame_bar_geom, floatframe_bar_geom},
    {frame_border_inner_geom, floatframe_border_inner_geom},
    {frame_border_geom, floatframe_border_geom},
    
    {(DynFun*)region_get_configuration,
     (DynFun*)floatframe_get_configuration},

    {frame_brushes_updated, floatframe_brushes_updated},
    
    {region_size_hints, floatframe_resize_hints},
    
    END_DYNFUNTAB
};


EXTL_EXPORT
IMPLCLASS(WFloatFrame, WFrame, NULL, floatframe_dynfuntab);

        
/*}}}*/