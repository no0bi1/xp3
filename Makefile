
# --- Tools ---
TRIPLE   = i686-pc-windows-msvc
CXX      = clang-cl
LD       = lld-link

# --- Dependencies ---
XWIN_ROOT       = ./xwinSDK
MINHOOK_DIR     = ./MinHook_134
D3DX_INCLUDES   = ./d3d9x_headers

# --- Project Files ---
TARGET   = UnitXP_SP3.dll

SRCS     = \
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

# --- Common Compiler Flags ---
COMMON_FLAGS = --target=$(TRIPLE) \
               /O1 /MT /W3 /nologo \
               /DWIN32_LEAN_AND_MEAN /D_X86_ /D_WIN32 \
               /Z7 /GS \
               -imsvc "$(XWIN_ROOT)/crt/include" \
               -imsvc "$(XWIN_ROOT)/sdk/include/ucrt" \
               -imsvc "$(XWIN_ROOT)/sdk/include/um" \
               -imsvc "$(XWIN_ROOT)/sdk/include/shared" \
               -imsvc "$(MINHOOK_DIR)/include"

# --- C++ Compiler Flags ---
CXXFLAGS = $(COMMON_FLAGS) \
           /EHsc \
           /arch:SSE2 /fp:precise \
           /std:c++17 /Zc:__cplusplus \
           /DNULL=0 \
           /Gy \
           -Wno-microsoft-cast \
           -Wno-unused-parameter \
           -Wno-unused-variable \
           -Wno-unused-function \
           -imsvc "$(D3DX_INCLUDES)"

# --- C Compiler Flags ---
CFLAGS = $(COMMON_FLAGS)

# --- Linker Flags ---
LDFLAGS = /NOLOGO /DLL /MACHINE:X86 \
          /OPT:REF /OPT:ICF /SAFESEH \
          /NXCOMPAT /FUNCTIONPADMIN \
          /DEBUG /PDB:"$(TARGET:.dll=.pdb)" /ignore:4099 \
          /LIBPATH:"$(XWIN_ROOT)/crt/lib/x86" \
          /LIBPATH:"$(XWIN_ROOT)/sdk/lib/ucrt/x86" \
          /LIBPATH:"$(XWIN_ROOT)/sdk/lib/um/x86"

# --- Build Rules ---
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
