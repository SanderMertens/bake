
LINK = link.exe

!IFNDEF config
config=debug
!ENDIF

!IFNDEF verbose
SILENT = @
!ENDIF

.PHONY: clean prebuild prelink

!IF "$(config)" == "debug"
TARGETDIR = ..
TARGET = $(TARGETDIR)\bake_util.dll
OBJDIR = ..\.bake_cache\debug
DEFINES = -DUT_IMPL -DDEBUG $(DEFINES)
INCLUDES = /I.. /I"$(BAKE_HOME)/include" $(INCLUDES)
ALL_CPPFLAGS = $(CPPFLAGS) /nologo $(DEFINES) $(INCLUDES) $(ALL_CPPFLAGS)
ALL_CFLAGS   = $(CFLAGS) $(ALL_CPPFLAGS) -D_XOPEN_SOURCE=600 $(ALL_CFLAGS)
ALL_CXXFLAGS = $(CXXFLAGS) $(ALL_CPPFLAGS) -D_XOPEN_SOURCE=600 $(ALL_CXXFLAGS)
ALL_RESFLAGS = $(RESFLAGS) $(DEFINES) $(INCLUDES) $(ALL_RESFLAGS)
LIBS = Ws2_32.lib Shlwapi.lib Kernel32.lib Dbghelp.lib Shell32.lib
ALL_LDFLAGS = $(LDFLAGS) $(LIBS)
LINKCMD = $(LINK) /DLL /OUT:$@ $(OBJECTS) $(ALL_LDFLAGS)

all: prebuild prelink $(TARGET)
	@:

!ENDIF

!IF "$(config)" == "release"
TARGETDIR = ..
TARGET = $(TARGETDIR)\bake_util.dll
OBJDIR = ..\.bake_cache\release
DEFINES = -DUT_IMPL $(DEFINES)
INCLUDES = /I.. /I"$(BAKE_HOME)/include" $(INCLUDES)
ALL_CPPFLAGS = $(CPPFLAGS) /nologo -DWIN32 -D__i386__ $(DEFINES) $(INCLUDES) $(ALL_CPPFLAGS)
ALL_CFLAGS   = $(CFLAGS) $(ALL_CPPFLAGS) -D_XOPEN_SOURCE=600 $(ALL_CFLAGS)
ALL_CXXFLAGS = $(CXXFLAGS) $(ALL_CPPFLAGS) -D_XOPEN_SOURCE=600 $(ALL_CXXFLAGS)
ALL_RESFLAGS = $(RESFLAGS) $(DEFINES) $(INCLUDES) $(ALL_RESFLAGS)
LIBS = Ws2_32.lib Shlwapi.lib Kernel32.lib Dbghelp.lib Shell32.lib
ALL_LDFLAGS = $(LDFLAGS) $(LIBS)
LINKCMD = $(LINK) /DLL /OUT:$@ $(OBJECTS) $(ALL_LDFLAGS)

all: prebuild prelink $(TARGET)
	@:

!ENDIF

CPP_SOURCE= ..\src\dl_win.c \
			..\src\env.c \
			..\src\expr.c \
			..\src\file.c \
			..\src\fs_win.c \
			..\src\fs_common.c \
			..\src\iter.c \
			..\src\jsw_rbtree.c \
			..\src\ll.c \
			..\src\load.c \
			..\src\log.c \
			..\src\memory.c \
			..\src\os.c \
			..\src\parson.c \
			..\src\path.c \
			..\src\proc_win.c \
			..\src\rb.c \
			..\src\strbuf.c \
			..\src\string.c \
			..\src\thread_win.c \
			..\src\time.c \
			..\src\util.c \
			..\src\version.c \

OBJECTS=$(CPP_SOURCE:.c=.obj)

$(TARGET): $(OBJECTS) $(TARGETDIR) $(OBJDIR)
	@echo Linking bake_util
	$(SILENT) $(LINKCMD)
	$(POSTBUILDCMDS)

$(TARGETDIR):
	@echo Creating $(TARGETDIR)
	$(SILENT) mkdir $(TARGETDIR)

$(OBJDIR):
	@echo Creating $(OBJDIR)
	$(SILENT) mkdir $(OBJDIR)

clean:
	@echo Cleaning bake_util
	$(SILENT) @if EXIST $(TARGET) del /Q $(TARGET)
	$(SILENT) @if EXIST $(OBJDIR) del /Q $(OBJDIR)
	$(SILENT) @if EXIST ..\src\*.obj del /Q ..\src\*.obj

prebuild:
	$(PREBUILDCMDS)

prelink:
	$(PRELINKCMDS)

$(OBJECTS): $(OBJDIR)

.c.obj:
	$(SILENT) $(CC) $(ALL_CFLAGS) /c $*.c  /Fo$@ 

