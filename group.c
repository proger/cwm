/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Andy Adamson <dros@monkey.org>
 * Copyright (c) 2004,2005 Marius Aamodt Eriksen <marius@monkey.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $OpenBSD: group.c,v 1.50 2011/05/11 13:53:51 okan Exp $
 */

#include <sys/param.h>
#include <sys/queue.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "calmwm.h"

static void		 group_add(struct group_ctx *, struct client_ctx *);
static void		 group_remove(struct client_ctx *);
static void		 group_hide(struct screen_ctx *, struct group_ctx *);
static void		 group_show(struct screen_ctx *, struct group_ctx *);
static void		 group_fix_hidden_state(struct group_ctx *);
static void		 group_setactive(struct screen_ctx *, long);
static void		 group_set_names(struct screen_ctx *);

const char *shortcut_to_name[] = {
	"nogroup", "one", "two", "three", "four", "five", "six",
	"seven", "eight", "nine"
};

static void
group_add(struct group_ctx *gc, struct client_ctx *cc)
{
	long	no;
	if (cc == NULL || gc == NULL)
		errx(1, "group_add: a ctx is NULL");

	no = gc->shortcut - 1;

	if (cc->group == gc)
		return;

	if (cc->group != NULL)
		TAILQ_REMOVE(&cc->group->clients, cc, group_entry);

	XChangeProperty(X_Dpy, cc->win, _NET_WM_DESKTOP, XA_CARDINAL,
	    32, PropModeReplace, (unsigned char *)&no, 1);

	TAILQ_INSERT_TAIL(&gc->clients, cc, group_entry);
	cc->group = gc;
}

static void
group_remove(struct client_ctx *cc)
{
	long	no = 0xffffffff;

	if (cc == NULL || cc->group == NULL)
		errx(1, "group_remove: a ctx is NULL");

	XChangeProperty(X_Dpy, cc->win, _NET_WM_DESKTOP, XA_CARDINAL,
	    32, PropModeReplace, (unsigned char *)&no, 1);

	TAILQ_REMOVE(&cc->group->clients, cc, group_entry);
	cc->group = NULL;
}

static void
group_hide(struct screen_ctx *sc, struct group_ctx *gc)
{
	struct client_ctx	*cc;

	screen_updatestackingorder(sc);

	gc->nhidden = 0;
	gc->highstack = 0;
	TAILQ_FOREACH(cc, &gc->clients, group_entry) {
		client_hide(cc);
		gc->nhidden++;
		if (cc->stackingorder > gc->highstack)
			gc->highstack = cc->stackingorder;
	}
	gc->hidden = 1;		/* XXX: equivalent to gc->nhidden > 0 */
}

static void
group_show(struct screen_ctx *sc, struct group_ctx *gc)
{
	struct client_ctx	*cc;
	Window			*winlist;
	u_int			 i;
	int			 lastempty = -1;

	gc->highstack = 0;
	TAILQ_FOREACH(cc, &gc->clients, group_entry) {
		if (cc->stackingorder > gc->highstack)
			gc->highstack = cc->stackingorder;
	}
	winlist = (Window *) xcalloc(sizeof(*winlist), (gc->highstack + 1));

	/*
	 * Invert the stacking order as XRestackWindows() expects them
	 * top-to-bottom.
	 */
	TAILQ_FOREACH(cc, &gc->clients, group_entry) {
		winlist[gc->highstack - cc->stackingorder] = cc->win;
		client_unhide(cc);
	}

	/* Un-sparseify */
	for (i = 0; i <= gc->highstack; i++) {
		if (!winlist[i] && lastempty == -1)
			lastempty = i;
		else if (winlist[i] && lastempty != -1) {
			winlist[lastempty] = winlist[i];
			if (++lastempty == i)
				lastempty = -1;
		}
	}

	XRestackWindows(X_Dpy, winlist, gc->nhidden);
	xfree(winlist);

	gc->hidden = 0;
	group_setactive(sc, gc->shortcut - 1);
}

void
group_init(struct screen_ctx *sc)
{
	int	 i;
	long	 viewports[2] = {0, 0};
	long	 ndesks = CALMWM_NGROUPS, zero = 0;

	TAILQ_INIT(&sc->groupq);
	sc->group_hideall = 0;
	/* see if any group names have already been set and update the property
	 * with ours if they'll have changed.
	 */
	group_update_names(sc);

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		TAILQ_INIT(&sc->groups[i].clients);
		sc->groups[i].hidden = 0;
		sc->groups[i].shortcut = i + 1;
		TAILQ_INSERT_TAIL(&sc->groupq, &sc->groups[i], entry);
	}

	/* we don't support large desktops, so this is always (0, 0) */
	XChangeProperty(X_Dpy, sc->rootwin, _NET_DESKTOP_VIEWPORT,
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)viewports, 2);
	XChangeProperty(X_Dpy, sc->rootwin, _NET_NUMBER_OF_DESKTOPS,
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&ndesks, 1);
	/*
	 * we don't use virtual roots, so make sure it's not there from a 
	 * previous wm.
	 */
	XDeleteProperty(X_Dpy, sc->rootwin, _NET_VIRTUAL_ROOTS);
	/*
	 * We don't really have a ``showing desktop'' mode, so this is zero
	 * always. XXX Note that when we hide all groups, or when all groups
	 * are hidden we could technically set this later on.
	 */
	XChangeProperty(X_Dpy, sc->rootwin, _NET_SHOWING_DESKTOP,
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&zero, 1);
	group_setactive(sc, 0);
}

void
group_make_autogroup(struct conf *conf, char *val, int no)
{
	struct autogroupwin	*aw;
	char			*p;

	aw = xcalloc(1, sizeof(*aw));

	if ((p = strchr(val, ',')) == NULL) {
		aw->name = NULL;
		aw->class = xstrdup(val);
	} else {
		*(p++) = '\0';
		aw->name = xstrdup(val);
		aw->class = xstrdup(p);
	}
	aw->num = no;

	TAILQ_INSERT_TAIL(&conf->autogroupq, aw, entry);
}

static void
group_setactive(struct screen_ctx *sc, long idx)
{
	sc->group_active = &sc->groups[idx];
	XChangeProperty(X_Dpy, sc->rootwin, _NET_CURRENT_DESKTOP,
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&idx, 1);
}

void
group_movetogroup(struct client_ctx *cc, int idx)
{
	struct screen_ctx	*sc = cc->sc;

	if (idx < 0 || idx >= CALMWM_NGROUPS)
		err(1, "group_movetogroup: index out of range (%d)", idx);

	if(sc->group_active != &sc->groups[idx])
		client_hide(cc);
	group_add(&sc->groups[idx], cc);
}

/*
 * Colouring for groups upon add/remove.
 */
void
group_sticky_toggle_enter(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct group_ctx	*gc;

	gc = sc->group_active;

	if (gc == cc->group) {
		group_remove(cc);
		cc->highlight = CLIENT_HIGHLIGHT_UNGROUP;
	} else {
		group_add(gc, cc);
		cc->highlight = CLIENT_HIGHLIGHT_GROUP;
	}

	client_draw_border(cc);
}

void
group_sticky_toggle_exit(struct client_ctx *cc)
{
	cc->highlight = 0;
	client_draw_border(cc);
}

/*
 * if group_hidetoggle would produce no effect, toggle the group's hidden state
 */
static void
group_fix_hidden_state(struct group_ctx *gc)
{
	struct client_ctx	*cc;
	int			 same = 0;

	TAILQ_FOREACH(cc, &gc->clients, group_entry) {
		if (gc->hidden == ((cc->flags & CLIENT_HIDDEN) ? 1 : 0))
			same++;
	}

	if (same == 0)
		gc->hidden = !gc->hidden;
}

void
group_hidetoggle(struct screen_ctx *sc, int idx)
{
	struct group_ctx	*gc;

	if (idx < 0 || idx >= CALMWM_NGROUPS)
		err(1, "group_hidetoggle: index out of range (%d)", idx);

	gc = &sc->groups[idx];
	group_fix_hidden_state(gc);

	if (gc->hidden)
		group_show(sc, gc);
	else {
		group_hide(sc, gc);
		/* XXX wtf? */
		if (TAILQ_EMPTY(&gc->clients))
			group_setactive(sc, idx);
	}
}

void
group_only(struct screen_ctx *sc, int idx)
{
	int	 i;

	if (idx < 0 || idx >= CALMWM_NGROUPS)
		err(1, "group_only: index out of range (%d)", idx);

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		if (i == idx)
			group_show(sc, &sc->groups[i]);
		else
			group_hide(sc, &sc->groups[i]);
	}
}

/*
 * Cycle through active groups.  If none exist, then just stay put.
 */
void
group_cycle(struct screen_ctx *sc, int reverse)
{
	struct group_ctx	*gc, *showgroup = NULL;

	assert(sc->group_active != NULL);

	gc = sc->group_active;
	for (;;) {
		gc = reverse ? TAILQ_PREV(gc, group_ctx_q, entry) :
		    TAILQ_NEXT(gc, entry);
		if (gc == NULL)
			gc = reverse ? TAILQ_LAST(&sc->groupq, group_ctx_q) :
			    TAILQ_FIRST(&sc->groupq);
		if (gc == sc->group_active)
			break;

		if (!TAILQ_EMPTY(&gc->clients) && showgroup == NULL)
			showgroup = gc;
		else if (!gc->hidden)
			group_hide(sc, gc);
	}

	if (showgroup == NULL)
		return;

	group_hide(sc, sc->group_active);

	if (showgroup->hidden)
		group_show(sc, showgroup);
	else
		group_setactive(sc, showgroup->shortcut - 1);
}

/* called when a client is deleted */
void
group_client_delete(struct client_ctx *cc)
{
	if (cc->group == NULL)
		return;

	TAILQ_REMOVE(&cc->group->clients, cc, group_entry);
	cc->group = NULL; /* he he */
}

void
group_menu(XButtonEvent *e)
{
	struct screen_ctx	*sc;
	struct group_ctx	*gc;
	struct menu		*mi;
	struct menu_q		 menuq;
	int			 i;

	sc = screen_fromroot(e->root);
	TAILQ_INIT(&menuq);

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		gc = &sc->groups[i];

		if (TAILQ_EMPTY(&gc->clients))
			continue;

		mi = xcalloc(1, sizeof(*mi));
		if (gc->hidden)
			snprintf(mi->text, sizeof(mi->text), "%d: [%s]",
			    gc->shortcut, sc->group_names[i]);
		else
			snprintf(mi->text, sizeof(mi->text), "%d: %s",
			    gc->shortcut, sc->group_names[i]);
		mi->ctx = gc;
		TAILQ_INSERT_TAIL(&menuq, mi, entry);
	}

	if (TAILQ_EMPTY(&menuq))
		return;

	mi = menu_filter(sc, &menuq, NULL, NULL, 0, NULL, NULL);

	if (mi == NULL || mi->ctx == NULL)
		goto cleanup;

	gc = (struct group_ctx *)mi->ctx;

	(gc->hidden) ? group_show(sc, gc) : group_hide(sc, gc);

cleanup:
	while ((mi = TAILQ_FIRST(&menuq)) != NULL) {
		TAILQ_REMOVE(&menuq, mi, entry);
		xfree(mi);
	}
}

void
group_alltoggle(struct screen_ctx *sc)
{
	int	 i;

	for (i = 0; i < CALMWM_NGROUPS; i++) {
		if (sc->group_hideall)
			group_show(sc, &sc->groups[i]);
		else
			group_hide(sc, &sc->groups[i]);
	}

	sc->group_hideall = (!sc->group_hideall);
}

void
group_autogroup(struct client_ctx *cc)
{
	struct screen_ctx	*sc = cc->sc;
	struct autogroupwin	*aw;
	struct group_ctx	*gc;
	int			 no = -1;
	long			*grpno;

	if (cc->app_class == NULL || cc->app_name == NULL)
		return;

	if (xu_getprop(cc->win, _NET_WM_DESKTOP, XA_CARDINAL,
	    1, (unsigned char **)&grpno) > 0) {
		if (*grpno == 0xffffffff)
			no = 0;
		else if (*grpno > CALMWM_NGROUPS || *grpno < 0)
			no = CALMWM_NGROUPS - 1;
		else
			no = *grpno + 1;
		XFree(grpno);
	} else {
		TAILQ_FOREACH(aw, &Conf.autogroupq, entry) {
			if (strcmp(aw->class, cc->app_class) == 0 &&
			    (aw->name == NULL ||
			    strcmp(aw->name, cc->app_name) == 0)) {
				no = aw->num;
				break;
			}
		}
	}

	/* no group please */
	if (no == 0)
		return;

	TAILQ_FOREACH(gc, &sc->groupq, entry) {
		if (gc->shortcut == no) {
			group_add(gc, cc);
			return;
		}
	}

	if (Conf.flags & CONF_STICKY_GROUPS)
		group_add(sc->group_active, cc);
}

void
group_update_names(struct screen_ctx *sc)
{
	char		**strings, *p;
	unsigned char	*prop_ret;
	Atom		 type_ret;
	int		 format_ret, i = 0, nstrings = 0, n, setnames = 0;
	unsigned long	 bytes_after, num_ret;
	
	if (XGetWindowProperty(X_Dpy, sc->rootwin, _NET_DESKTOP_NAMES, 0,
	    0xffffff, False, UTF8_STRING, &type_ret, &format_ret,
	    &num_ret, &bytes_after, &prop_ret) == Success &&
	    prop_ret != NULL && format_ret == 8) {
		/* failure, just set defaults */
		prop_ret[num_ret - 1] = '\0'; /* paranoia */
		while (i < num_ret) {
			if (prop_ret[i++] == '\0')
				nstrings++;
		}
	}

	strings = xmalloc((nstrings < CALMWM_NGROUPS ? CALMWM_NGROUPS :
	    nstrings) * sizeof(*strings));

	i = n = 0;
	p = prop_ret;
	while (n < nstrings) {
		strings[n++] = xstrdup(p);
		p += strlen(p) + 1;
	}
	/*
	 * make sure we always set our defaults if nothing is there to
	 * replace them.
	 */
	if (n < CALMWM_NGROUPS) {
		setnames = 1;
		i = 1;
		while (n < CALMWM_NGROUPS)
			strings[n++] = xstrdup(shortcut_to_name[i++]);
	}

	if (prop_ret != NULL)
		XFree(prop_ret);
	if (sc->group_nonames != 0)
		free(sc->group_names);

	sc->group_names = strings;
	sc->group_nonames = n;
	if (setnames)
		group_set_names(sc);
}

static void
group_set_names(struct screen_ctx *sc)
{
	unsigned char	*p, *q;
	size_t		 len = 0, tlen, slen;
	int		 i;

	for (i = 0; i < sc->group_nonames; i++)
		len += strlen(sc->group_names[i]) + 1;
	q = p = xcalloc(len, sizeof(*p));

	tlen = len;
	for (i = 0; i < sc->group_nonames; i++) {
		slen = strlen(sc->group_names[i]) + 1;
		strlcpy(q, sc->group_names[i], tlen);
		tlen -= slen;
		q += slen;
	}
		
	XChangeProperty(X_Dpy, sc->rootwin, _NET_DESKTOP_NAMES,
	    UTF8_STRING, 8, PropModeReplace, p, len);
}
