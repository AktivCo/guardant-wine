
noinst_PROGRAMS = grdwine$(EXEEXT)
grdwine_SOURCES = grdwine.spec grdwine.c grdimpl.h grdimpl_linux.c

AM_CPPFLAGS = -D__WINESRC__ -I$(wineincs) -I$(wineincs)/wine/windows
CLEANFILES = grdwine.dll.so

grdwine$(EXEEXT):	grdwine.spec grdwine.o grdimpl_linux.o grdwine.dll grdwine.dll.so
			true

grdwine.dll.so:	grdwine.spec grdwine.o grdimpl_linux.o
		$(WINEGCC) -shared $^ -o $@ -lkernel32

grdwine.dll: grdwine.spec
		$(WINEGCC) -o $@ -Wb,--fake-module -shared $^ -mno-cygwin

install-am:	grdwine.dll.so grdwine.dll
		$(MKDIR_P) $(winepe) $(wineso)
		$(INSTALL_PROGRAM) grdwine.dll.so $(wineso)/grdwine.dll.so
		$(INSTALL_PROGRAM) grdwine.dll $(winepe)/grdwine.dll

