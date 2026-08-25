/**
 * @file types/handles.h
 *
 * @brief Names of the types that cross module boundaries
 *
 * A header that only ever mentions one of these through a pointer
 * needs the name, not the definition, so it includes this instead of
 * the whole header the type is defined in.  That keeps a declaration
 * from dragging in every field, macro and function of a module it
 * merely refers to.
 *
 * A header that stores one of these by value, or reads a field of
 * one, does need the definition and includes the real header.
 *
 * Every name here is guarded on its own, with the same guard the
 * headers defining these types already use, so including both this
 * file and the real one in any order is fine.
 *
 * @note Unlike its neighbours in @c types/, this file defines
 *       nothing.  @c types/pair.h and @c types/direction.h are where
 *       their own types live, whereas every name here belongs to a
 *       module of its own and is merely named again, so a header can
 *       refer to it without the definition
 *
 * @ingroup types
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef TYPES_HANDLES_H
#define TYPES_HANDLES_H


/* Containers */
#ifndef LIST_TD_DECLARED
#define LIST_TD_DECLARED
/** Singly linked list; defined in @c adt/list.h */
typedef struct list_s list_td;
#endif

#ifndef CDLIST_TD_DECLARED
#define CDLIST_TD_DECLARED
/** Circular doubly linked list; defined in @c adt/cdlist.h */
typedef struct cdlist_s cdlist_td;
#endif

#ifndef OHTBL_TD_DECLARED
#define OHTBL_TD_DECLARED
/** Open-addressed hash table; defined in @c adt/ohtbl.h */
typedef struct ohtbl_s ohtbl_td;
#endif

/* Window manager domain */
#ifndef CLIENT_TD_DECLARED
#define CLIENT_TD_DECLARED
/** A managed window; defined in @c client.h */
typedef struct client_s client_td;
#endif

#ifndef DESKTOP_TD_DECLARED
#define DESKTOP_TD_DECLARED
/** One desktop of a surface; defined in @c desktop.h */
typedef struct desktop_s desktop_td;
#endif

#ifndef SURFACE_TD_DECLARED
#define SURFACE_TD_DECLARED
/** One screen and everything on it; defined in @c surface.h */
typedef struct surface_s surface_td;
#endif

#ifndef WM_TD_DECLARED
#define WM_TD_DECLARED
/** The window manager singleton; defined in @c wm.h */
typedef struct wm_s wm_td;
#endif

#ifndef CONFIG_TD_DECLARED
#define CONFIG_TD_DECLARED
/** The whole configuration; defined in @c config.h */
typedef struct config_s config_td;
#endif

#ifndef RULES_TD_DECLARED
#define RULES_TD_DECLARED
/** The rule set; defined in @c rules.h */
typedef struct rules_s rules_td;
#endif

#ifndef SESSION_TD_DECLARED
#define SESSION_TD_DECLARED
/** Session hooks and their children; defined in @c session.h */
typedef struct session_s session_td;
#endif


#endif  /* ! TYPES_HANDLES_H */
