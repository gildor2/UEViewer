# Makefile for VisualC/win32 target
# This file was automatically generated from "UnLoader.project": do not edit

#------------------------------------------------------------------------------
#	Compiler definitions
#------------------------------------------------------------------------------

CPP  = cl.exe -nologo -c -D WIN32 -D _WINDOWS
LINK = link.exe -nologo -filealign:512 -incremental:no
AR   = link.exe -lib -nologo

#------------------------------------------------------------------------------
#	symbolic targets
#------------------------------------------------------------------------------

ALL : MAIN
MAIN : UnLoader.exe

#------------------------------------------------------------------------------
#	"UnLoader.exe" target
#------------------------------------------------------------------------------

MAIN = \
	obj/UnCore.obj \
	obj/UnLoader.obj \
	obj/GlWindow.obj \
	obj/Math3D.obj

MAIN_DIRS = \
	obj

UnLoader.exe : $(MAIN_DIRS) $(MAIN)
	echo Creating executable "UnLoader.exe" ...
	$(LINK) -out:"UnLoader.exe" -libpath:"libs" $(MAIN) -subsystem:console

#------------------------------------------------------------------------------
#	compiling source files
#------------------------------------------------------------------------------

OPT_MAIN = -O1 -EHsc -I libs/include

DEPENDS = \
	Core.h \
	GlWindow.h \
	Math3D.h

obj/GlWindow.obj : GlWindow.cpp $(DEPENDS)
	$(CPP) -MD $(OPT_MAIN) -Fo"obj/GlWindow.obj" GlWindow.cpp

DEPENDS = \
	Core.h \
	GlWindow.h \
	Math3D.h \
	UnCore.h \
	UnMesh.h

obj/UnLoader.obj : UnLoader.cpp $(DEPENDS)
	$(CPP) -MD $(OPT_MAIN) -Fo"obj/UnLoader.obj" UnLoader.cpp

DEPENDS = \
	Core.h \
	Math3D.h

obj/Math3D.obj : Math3D.cpp $(DEPENDS)
	$(CPP) -MD $(OPT_MAIN) -Fo"obj/Math3D.obj" Math3D.cpp

DEPENDS = \
	Core.h \
	Math3D.h \
	UnCore.h

obj/UnCore.obj : UnCore.cpp $(DEPENDS)
	$(CPP) -MD $(OPT_MAIN) -Fo"obj/UnCore.obj" UnCore.cpp

#------------------------------------------------------------------------------
#	creating output directories
#------------------------------------------------------------------------------

obj:
	if not exist "obj" mkdir "obj"

