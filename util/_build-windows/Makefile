
!IFNDEF config
config=debug
!ENDIF

!IFNDEF verbose
SILENT = @
!ENDIF

!IF "$(config)" == "debug"
bake_util_config = debug
!ENDIF

!IF "$(config)" == "release"
bake_util_config = release
!ENDIF

PROJECTS = bake_util

.PHONY: all

all: $(PROJECTS)

bake_util:
!IF "" != "$(bake_util_config)"
	@echo "==== Building bake_util ($(bake_util_config)) ===="
	@$(MAKE) /NOLOGO /f bake_util.mak config=$(bake_util_config) all
!ENDIF

clean:
	@$(MAKE) /NOLOGO /f bake_util.mak clean

help:
	@echo "Usage: nmake [config=name] [target]"
	@echo ""
	@echo "CONFIGURATIONS:"
	@echo "  debug"
	@echo "  release"
	@echo ""
	@echo "TARGETS:"
	@echo "   all (default)"
	@echo "   clean"
	@echo "   bake_util"
	@echo ""
	@echo "For more information, see https://github.com/premake/premake-core/wiki"
