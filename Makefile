
# --- Tools ---
TRIPLE = i686-pc-windows-msvc
CXX = clang-cl
LD = lld-link

# --- Dependencies ---
MINHOOK_DIR = ./MinHook_134
D3DX_INCLUDES ?= ./d3d9x_headers

# SDK paths are exported by the GitHub Actions workflow.
# GNU Make imports these environment variables automatically.

# --- Project ---
TARGET = UnitXP_SP3.dll

SRCS = \
    coffTimeDateStamp.cpp \
    distanceBetween.cpp \
    dllmain.cpp \
    editCamera.cpp \
    edit_CWorld_Intersect.cpp \
    FPScap.cpp \
    gameEvent.cpp \
    gameQuit.cpp \
    gameSocket.cpp \
    inSight.cpp \
    LuaDebug.cpp \
    modernNameplateDistance.cpp \
    notifyOS.cpp \
    performanceProfiling.cpp \
    polyfill.cpp \
    sceneBegin_sceneEnd.cpp \
    screenshot.cpp \
    stb_image_write.cpp \
    targeting.cpp \
    timer.cpp \
    utf8_to_utf16.cpp \
    Vanilla1121_functions.cpp \
    weather.cpp \
    worldText.cpp \
    $(MINHOOK_DIR)/src/buffer.c \
    $(MINHOOK_DIR)/src/hde/hde32.c \
    $(MINHOOK_DIR)/src/hook.c \
    $(MINHOOK_DIR)/src/trampoline.c

OBJS = $(patsubst %.cpp,%.obj,$(patsubst %.c,%.obj,$(SRCS)))

LIBS = user32.lib kernel32.lib gdi32.lib libcmt.lib

# --- Common compiler flags ---
COMMON_FLAGS = --target=$(TRIPLE) \
    /O1 /MT /W3 /nologo \
    /DWIN32_LEAN_AND_MEAN /D_X86_ /D_WIN32 \
    /Z7 /GS \
    -imsvc "$(MSVC_INCLUDE)" \
    -imsvc "$(SDK_UCRT_INC)" \
    -imsvc "$(SDK_UM_INC)" \
    -imsvc "$(SDK_SHARED_INC)" \
    -imsvc "$(MINHOOK_DIR)/include"

# --- C++ flags ---
CXXFLAGS = $(COMMON_FLAGS) \
    /EHsc /arch:SSE2 /fp:precise \
    /std:c++17 /Zc:__cplusplus \
    /DNULL=0 /Gy \
    -Wno-microsoft-cast \
    -Wno-unused-parameter \
    -Wno-unused-variable \
    -Wno-unused-function \
    -imsvc "$(D3DX_INCLUDES)"

# --- C flags ---
CFLAGS = $(COMMON_FLAGS)

# --- Linker flags ---
LDFLAGS = /NOLOGO /DLL /MACHINE:X86 \
    /OPT:REF /OPT:ICF /SAFESEH \
    /NXCOMPAT /FUNCTIONPADMIN \
    /DEBUG /PDB:"$(TARGET:.dll=.pdb)" /ignore:4099 \
    /LIBPATH:"$(MSVC_LIB)" \
    /LIBPATH:"$(SDK_UCRT_LIB)" \
    /LIBPATH:"$(SDK_UM_LIB)"

# --- Build rules ---
all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) /OUT:$@ $(OBJS) $(LIBS)

%.obj: %.cpp
	$(CXX) $(CXXFLAGS) /c $< /Fo$@

%.obj: %.c
	$(CXX) $(CFLAGS) /c $< /Fo$@

clean:
	rm -f $(OBJS) *.dll *.lib *.exp *.pdb

.PHONY: all clean
