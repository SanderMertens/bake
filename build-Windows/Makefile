
!IFNDEF config
config = debug
!ENDIF

!IFNDEF verbose
SILENT = @
!ENDIF

!IF "$(config)" == "debug"
bake_config = debug
!ENDIF

!IF "$(config)" == "release"
bake_config = release
!ENDIF

PROJECTS = bake

.PHONY: all

all: $(PROJECTS)

bake:
!IF "$(bake_config)" != ""
	@ECHO "==== Building bake ($(bake_config)) ===="
	$(MAKE) /NOLOGO /F bake.mak config=$(bake_config) all
!ENDIF

clean:
	$(MAKE) /F bake.mak clean

help:
	@ECHO "Usage: nmake [config=name] [target]"
	@ECHO ""
	@ECHO "CONFIGURATIONS:"
	@ECHO "  debug"
	@ECHO "  release"
	@ECHO ""
	@ECHO "TARGETS:"
	@ECHO "   all (default)"
	@ECHO "   clean"
	@ECHO "   bake"
	@ECHO ""
	@ECHO "For more information, see https://github.com/premake/premake-core/wiki"

