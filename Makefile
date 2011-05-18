PROG=		cwm

SRCS=		calmwm.c screen.c xmalloc.c client.c menu.c \
		search.c util.c xutil.c conf.c xevents.c group.c \
		kbfunc.c mousefunc.c font.c parse.y

CPPFLAGS=	-I/usr/X11R7/include -I/usr/X11R7/include/freetype2 -I${.CURDIR}

NOGCCERROR=
CFLAGS=		-g -Wall

LDADD+=		-L/usr/X11R7/lib -lXft -lXrender -lX11 ${LIBXCB} -lXau -lXdmcp \
		-lfontconfig -lexpat -lfreetype -lz -lXinerama -lXrandr -lXext

MANDIR=		${X11BASE}/man/cat
MAN=		cwm.1 cwmrc.5

CLEANFILES=	cwm.cat1 cwmrc.cat5

.include <bsd.obj.mk>
.include <bsd.prog.mk>
.include <bsd.x11.mk>
