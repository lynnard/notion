/*
 * ion/ioncore/frame.h
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#ifndef ION_IONCORE_FRAME_H
#define ION_IONCORE_FRAME_H

#include "common.h"
#include "window.h"
#include "attach.h"
#include "mplex.h"
#include "gr.h"
#include "extl.h"

INTROBJ(WFrame);

#define WFRAME_TAB_HIDE    0x0004
#define WFRAME_SAVED_VERT  0x0008
#define WFRAME_SAVED_HORIZ 0x0010
#define WFRAME_SHADED	  0x0020
#define WFRAME_SETSHADED	  0x0040
#define WFRAME_BAR_OUTSIDE 0x0080

DECLOBJ(WFrame){
	WMPlex mplex;
	
	int flags;
	int saved_w, saved_h;
	int saved_x, saved_y;
	
	int tab_dragged_idx;
	
	GrBrush *brush;
	GrBrush *bar_brush;
	GrTransparency tr_mode;
	int bar_h;
	GrTextElem *titles;
	int titles_n;
};


/* Create/destroy */
extern WFrame *create_frame(WWindow *parent, const WRectangle *geom);
extern bool frame_init(WFrame *frame, WWindow *parent,
						  const WRectangle *geom);
extern void frame_deinit(WFrame *frame);

/* Resize and reparent */
extern bool frame_reparent(WFrame *frame, WWindow *parent,
							  const WRectangle *geom);
extern void frame_fit(WFrame *frame, const WRectangle *geom);
extern void frame_resize_hints(WFrame *frame, XSizeHints *hints_ret,
								  uint *relw_ret, uint *relh_ret);

/* Focus */
extern void frame_activated(WFrame *frame);
extern void frame_inactivated(WFrame *frame);

/* Tabs */
extern int frame_nth_tab_w(const WFrame *frame, int n);
extern int frame_nth_tab_iw(const WFrame *frame, int n);
extern int frame_nth_tab_x(const WFrame *frame, int n);
extern int frame_tab_at_x(const WFrame *frame, int x);
extern void frame_toggle_tab(WFrame *frame);
extern void frame_update_attr_nth(WFrame *frame, int i);

/* Misc */
extern void frame_toggle_sub_tag(WFrame *frame);
extern bool frame_save_to_file(WFrame *frame, FILE *file, int lvl);
extern void frame_load_saved_geom(WFrame* frame, ExtlTab tab);
extern void frame_do_load(WFrame *frame, ExtlTab tab);

#endif /* ION_IONCORE_FRAME_H */