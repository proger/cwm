PROG=		cwm

SRCS=		calmwm.c screen.c xmalloc.c client.c menu.c \
		search.c util.c xutil.c conf.c xevents.c group.c \
		kbfunc.c mousefunc.c font.c parse.y strlcpy.c strlcat.c fgetln.c

CPPFLAGS=	-I/usr/include -I/usr/include/freetype2 -I${.CURDIR}

NOGCCERROR=
CFLAGS=		-g -Wall

LDADD+=		-L/usr/X11R7/lib -lXft -lXrender -lX11 ${LIBXCB} -lXau -lXdmcp \
		-lfontconfig -lexpat -lfreetype -lz -lXinerama -lXrandr -lXext

MANDIR=		${X11BASE}/man/cat
MAN=		cwm.1 cwmrc.5

CLEANFILES=	cwm.cat1 cwmrc.cat5

#.include <bsd.obj.mk>
.include <prog.mk>
#.include <bsd.x11.mk>
