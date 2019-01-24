
LINK = link.exe

!IFNDEF config
config=debug
!ENDIF

!IFNDEF verbose
SILENT = @
!ENDIF

!IFNDEF BAKE_HOME
BAKE_HOME = $(USERPROFILE)\bake
!ENDIF

.PHONY: clean prebuild prelink

!IF "$(config)" == "debug"
TARGETDIR = ..
TARGET = $(TARGETDIR)\bake_lang_cpp.dll
OBJDIR = ..\.bake_cache\debug
DEFINES = /DDEBUG /DUT_IMPL $(DEFINES)
INCLUDES = /I.. /I"$(BAKE_HOME)\include" $(INCLUDES)
FORCE_INCLUDE =
ALL_CPPFLAGS = $(CPPFLAGS) /nologo /Od /MP $(DEFINES) $(INCLUDES) $(ALL_CPPFLAGS)
ALL_CFLAGS   = $(CFLAGS) $(ALL_CPPFLAGS) $(ALL_CFLAGS)
ALL_CXXFLAGS = $(CXXFLAGS) $(ALL_CPPFLAGS) -D_XOPEN_SOURCE=600 $(ALL_CXXFLAGS)
ALL_RESFLAGS = $(RESFLAGS) $(DEFINES) $(INCLUDES) $(ALL_RESFLAGS)
LIBS = bake_util.lib $(LIBS) 
ALL_LDFLAGS = $(LDFLAGS) /LIBPATH:..\..\..\..\util $(LIBS)
LINKCMD = $(LINK) /NOLOGO /DLL /OUT:$@ $(OBJECTS) $(ALL_LDFLAGS)
all: prebuild prelink $(TARGET)
	@:

!ENDIF

!IF "$(config)" == "release"
TARGETDIR = ..
TARGET = $(TARGETDIR)\bake_lang_cpp.dll
OBJDIR = ..\.bake_cache\release
DEFINES = /DUT_IMPL $(DEFINES)
INCLUDES = /I.. /I"$(BAKE_HOME)\include" $(INCLUDES)
FORCE_INCLUDE =
ALL_CPPFLAGS = $(CPPFLAGS) /nologo /Ox /GL /MP $(DEFINES) $(INCLUDES) $(ALL_CPPFLAGS)
ALL_CFLAGS   = $(CFLAGS) $(ALL_CPPFLAGS) $(ALL_CFLAGS)
ALL_CXXFLAGS = $(CXXFLAGS) $(ALL_CPPFLAGS) -D_XOPEN_SOURCE=600 $(ALL_CXXFLAGS)
ALL_RESFLAGS = $(RESFLAGS) $(DEFINES) $(INCLUDES) $(ALL_RESFLAGS)
LIBS = bake_util.lib $(LIBS) 
ALL_LDFLAGS = $(LDFLAGS) /LIBPATH:..\..\..\..\util $(LIBS)
LINKCMD = $(LINK) /NOLOGO /DLL /OUT:$@ $(OBJECTS) $(ALL_LDFLAGS)
all: prebuild prelink $(TARGET)
	@:

!ENDIF

CPP_SOURCE= ..\src\main.c \

OBJECTS=$(CPP_SOURCE:.c=.obj)


$(TARGET): $(OBJECTS) $(TARGETDIR)
	@echo Linking bake_lang_cpp
	$(SILENT) $(LINKCMD)
	$(POSTBUILDCMDS)

$(TARGETDIR):
	@echo Creating $(TARGETDIR)
	$(SILENT) mkdir $(TARGETDIR)

$(OBJDIR):
	@echo Creating $(OBJDIR)
	$(SILENT) mkdir $(OBJDIR)

clean:
	@echo Cleaning bake_lang_cpp
	$(SILENT) @if EXIST $(TARGET) del /Q $(TARGET)
	$(SILENT) @if EXIST $(OBJDIR) del /Q $(OBJDIR)
	$(SILENT) @if EXIST ..\src\*.obj del /Q ..\src\*.obj

prebuild:
	$(PREBUILDCMDS)

prelink:
	$(PRELINKCMDS)

$(OBJECTS): $(OBJDIR)

.c.obj:
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) /c $*.c  /Fo$@ 

