#
# Quake2 Makefile for Linux 2.0
#
# Nov '97 by Zoid <zoid@idsoftware.com>
#
# ELF only
#

# Here are your build options:
# (Note: not all options are available for all platforms).
BUILD_GLX=YES		# X11 GLX client
BUILD_X11=NO		# X11 software client
BUILD_EVDEV=NO		# build evdev mouse input support
#--------------------------------------------------------

# Check OS type.
OSTYPE := $(shell uname -s)

ifneq ($(OSTYPE),Linux)
ifneq ($(OSTYPE),FreeBSD)
$(error OS $(OSTYPE) is currently not supported)
endif
endif

# this nice line comes from the linux kernel makefile
ARCH := $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc/ -e s/sparc64/sparc/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/alpha/axp/)

ifneq ($(ARCH),i386)
ifneq ($(ARCH),axp)
ifneq ($(ARCH),ppc)
ifneq ($(ARCH),sparc)
$(error arch $(ARCH) is currently not supported)
endif
endif
endif
endif

CC=gcc

#BASE CFLAGS
BASE_CFLAGS=-Wall -pipe -Dstricmp=strcasecmp
ifneq ($(ARCH),i386)
 BASE_CFLAGS+=-DC_ONLY
endif

ifeq ($(strip $(BUILD_EVDEV)),YES)
 BASE_CFLAGS+=-DWITH_EVDEV
endif

DEBUG_CFLAGS=$(BASE_CFLAGS) -g

ifeq ($(ARCH),axp)
RELEASE_CFLAGS=$(BASE_CFLAGS) -ffast-math -funroll-loops \
	-fomit-frame-pointer -fexpensive-optimizations
endif
ifeq ($(ARCH),ppc)
RELEASE_CFLAGS=$(BASE_CFLAGS) -O2 -ffast-math -funroll-loops \
	-fomit-frame-pointer -fexpensive-optimizations
endif
ifeq ($(ARCH),sparc)
RELEASE_CFLAGS=$(BASE_CFLAGS) -ffast-math -funroll-loops \
	-fomit-frame-pointer -fexpensive-optimizations
endif
ifeq ($(ARCH),i386)
RELEASE_CFLAGS=$(BASE_CFLAGS) -O2 -fno-strict-aliasing -ffast-math -fexpensive-optimizations
endif

#BASE LDFLAGS
ifeq ($(OSTYPE),FreeBSD)
LDFLAGS=-lm
endif
ifeq ($(OSTYPE),Linux)
LDFLAGS=-lm -ldl
endif

#FOR X11 BUILDS
XCFLAGS=-I/usr/X11R6/include
XLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext -lXxf86dga -lXxf86vm
DO_CC=$(CC) $(CFLAGS) -o $@ -c $<
DO_AS=$(CC) $(CFLAGS) -DELF -x assembler-with-cpp -o $@ -c $<

#FOR GLX BUILDS
GLXCFLAGS=$(XCFLAGS)
GLXLDFLAGS=$(XLDFLAGS) -ljpeg -lpng
DO_GL_CC=$(CC) $(CFLAGS) -DGL_QUAKE -o $@ -c $<
DO_GL_AS=$(CC) $(CFLAGS) -DGL_QUAKE -DELF -x assembler-with-cpp -o $@ -c $<

#############################################################################
# SETUP AND BUILD
#############################################################################

VERSION=3.21

MOUNT_DIR=.

BUILD_DEBUG_DIR=debug$(ARCH)
BUILD_RELEASE_DIR=release$(ARCH)
CLIENT_DIR=$(MOUNT_DIR)/client
SERVER_DIR=$(MOUNT_DIR)/server
REF_SOFT_DIR=$(MOUNT_DIR)/ref_soft
REF_GL_DIR=$(MOUNT_DIR)/ref_gl
COMMON_DIR=$(MOUNT_DIR)/qcommon
LINUX_DIR=$(MOUNT_DIR)/linux
GAME_DIR=$(MOUNT_DIR)/game
UI_DIR=$(MOUNT_DIR)/ui

.PHONY : targets build_debug build_release clean clean-debug clean-release clean2

TARGETS=$(BUILDDIR)/aq2 

ifeq ($(strip $(BUILD_X11)),YES)
 TARGETS += $(BUILDDIR)/aq2sw
endif

all: build_debug build_release

build_debug:
	@-mkdir -p $(BUILD_DEBUG_DIR) \
		$(BUILD_DEBUG_DIR)/q2glx \
		$(BUILD_DEBUG_DIR)/q2swx \
		$(BUILD_DEBUG_DIR)/game
	$(MAKE) targets BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS) -DLINUX_VERSION='\"$(VERSION) Debug\"'"

build_release:
	@-mkdir -p $(BUILD_RELEASE_DIR) \
		$(BUILD_RELEASE_DIR)/q2glx \
		$(BUILD_RELEASE_DIR)/q2swx \
		$(BUILD_RELEASE_DIR)/game
	$(MAKE) targets BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(RELEASE_CFLAGS) -DLINUX_VERSION='\"$(VERSION)\"'"

targets: $(TARGETS)

#############################################################################
# CLIENT/SERVER
#############################################################################

QUAKE2GLX_OBJS = \
	$(BUILDDIR)/q2glx/cl_cin.o \
	$(BUILDDIR)/q2glx/cl_ents.o \
	$(BUILDDIR)/q2glx/cl_fx.o \
	$(BUILDDIR)/q2glx/cl_input.o \
	$(BUILDDIR)/q2glx/cl_inv.o \
	$(BUILDDIR)/q2glx/cl_main.o \
	$(BUILDDIR)/q2glx/cl_parse.o \
	$(BUILDDIR)/q2glx/cl_pred.o \
	$(BUILDDIR)/q2glx/cl_tent.o \
	$(BUILDDIR)/q2glx/cl_scrn.o \
	$(BUILDDIR)/q2glx/cl_view.o \
	$(BUILDDIR)/q2glx/cl_newfx.o \
	$(BUILDDIR)/q2glx/console.o \
	$(BUILDDIR)/q2glx/keys.o \
	$(BUILDDIR)/q2glx/snd_dma.o \
	$(BUILDDIR)/q2glx/snd_mem.o \
	$(BUILDDIR)/q2glx/snd_mix.o \
	$(BUILDDIR)/q2glx/m_flash.o \
	$(BUILDDIR)/q2glx/cl_loc.o \
	$(BUILDDIR)/q2glx/cl_draw.o \
	$(BUILDDIR)/q2glx/cl_demo.o \
	\
	$(BUILDDIR)/q2glx/cmd.o \
	$(BUILDDIR)/q2glx/cmodel.o \
	$(BUILDDIR)/q2glx/common.o \
	$(BUILDDIR)/q2glx/crc.o \
	$(BUILDDIR)/q2glx/cvar.o \
	$(BUILDDIR)/q2glx/files.o \
	$(BUILDDIR)/q2glx/md4.o \
	$(BUILDDIR)/q2glx/net_chan.o \
	$(BUILDDIR)/q2glx/pmove.o \
	$(BUILDDIR)/q2glx/q_shared.o \
	\
	$(BUILDDIR)/q2glx/sv_ccmds.o \
	$(BUILDDIR)/q2glx/sv_ents.o \
	$(BUILDDIR)/q2glx/sv_game.o \
	$(BUILDDIR)/q2glx/sv_init.o \
	$(BUILDDIR)/q2glx/sv_main.o \
	$(BUILDDIR)/q2glx/sv_send.o \
	$(BUILDDIR)/q2glx/sv_user.o \
	$(BUILDDIR)/q2glx/sv_world.o \
	\
	$(BUILDDIR)/q2glx/q_shlinux.o \
	$(BUILDDIR)/q2glx/vid_so.o \
	$(BUILDDIR)/q2glx/sys_linux.o \
	$(BUILDDIR)/q2glx/glob.o \
	$(BUILDDIR)/q2glx/net_udp.o \
	$(BUILDDIR)/q2glx/cd_linux.o \
	$(BUILDDIR)/q2glx/snd_linux.o \
	\
	$(BUILDDIR)/q2glx/qmenu.o \
	$(BUILDDIR)/q2glx/ui_addressbook.o \
	$(BUILDDIR)/q2glx/ui_atoms.o \
	$(BUILDDIR)/q2glx/ui_controls.o \
	$(BUILDDIR)/q2glx/ui_credits.o \
	$(BUILDDIR)/q2glx/ui_demos.o \
	$(BUILDDIR)/q2glx/ui_dmoptions.o \
	$(BUILDDIR)/q2glx/ui_download.o \
	$(BUILDDIR)/q2glx/ui_game.o \
	$(BUILDDIR)/q2glx/ui_joinserver.o \
	$(BUILDDIR)/q2glx/ui_keys.o \
	$(BUILDDIR)/q2glx/ui_loadsavegame.o \
	$(BUILDDIR)/q2glx/ui_main.o \
	$(BUILDDIR)/q2glx/ui_multiplayer.o \
	$(BUILDDIR)/q2glx/ui_newoptions.o \
	$(BUILDDIR)/q2glx/ui_playerconfig.o \
	$(BUILDDIR)/q2glx/ui_quit.o \
	$(BUILDDIR)/q2glx/ui_startserver.o \
	$(BUILDDIR)/q2glx/ui_video.o \
	\
	$(BUILDDIR)/q2glx/gl_draw.o \
	$(BUILDDIR)/q2glx/gl_image.o \
	$(BUILDDIR)/q2glx/gl_light.o \
	$(BUILDDIR)/q2glx/gl_mesh.o \
	$(BUILDDIR)/q2glx/gl_model.o \
	$(BUILDDIR)/q2glx/gl_rmain.o \
	$(BUILDDIR)/q2glx/gl_rmisc.o \
	$(BUILDDIR)/q2glx/gl_rsurf.o \
	$(BUILDDIR)/q2glx/gl_warp.o \
	$(BUILDDIR)/q2glx/gl_decal.o \
	$(BUILDDIR)/q2glx/gl_png.o \
	\
	$(BUILDDIR)/q2glx/qgl_linux.o \
	$(BUILDDIR)/q2glx/gl_glx.o \
	$(BUILDDIR)/q2glx/rw_linux.o
	
ifeq ($(ARCH),i386)
QUAKE2GLX_OBJS += \
	$(BUILDDIR)/q2glx/snd_mixa.o
endif


$(BUILDDIR)/aq2 : $(QUAKE2GLX_OBJS)
	$(CC) $(CFLAGS) -o $@ $(QUAKE2GLX_OBJS) $(LDFLAGS) $(GLXLDFLAGS)

$(BUILDDIR)/q2glx/%.o :    $(CLIENT_DIR)/%.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/%.o :    $(GAME_DIR)/%.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/%.o :    $(COMMON_DIR)/%.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/%.o :    $(SERVER_DIR)/%.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/%.o :    $(UI_DIR)/%.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/%.o :    $(REF_GL_DIR)/%.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/q_shlinux.o :  $(LINUX_DIR)/q_shlinux.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/vid_so.o :     $(LINUX_DIR)/vid_so.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/sys_linux.o :  $(LINUX_DIR)/sys_linux.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/glob.o :       $(LINUX_DIR)/glob.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/net_udp.o :    $(LINUX_DIR)/net_udp.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/cd_linux.o :   $(LINUX_DIR)/cd_linux.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/snd_linux.o :  $(LINUX_DIR)/snd_linux.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/snd_mixa.o :   $(LINUX_DIR)/snd_mixa.s
	$(DO_GL_AS)

$(BUILDDIR)/q2glx/rw_linux.o :   $(LINUX_DIR)/rw_linux.c
	$(DO_GL_CC) $(GLXCFLAGS)

$(BUILDDIR)/q2glx/qgl_linux.o :  $(LINUX_DIR)/qgl_linux.c
	$(DO_GL_CC)

$(BUILDDIR)/q2glx/gl_glx.o :     $(LINUX_DIR)/gl_glx.c
	$(DO_GL_CC) $(GLXCFLAGS)
	
	
#############################################################################
# REF_SOFT
#############################################################################

QUAKE2SWX_OBJS = \
	$(BUILDDIR)/q2swx/cl_cin.o \
	$(BUILDDIR)/q2swx/cl_ents.o \
	$(BUILDDIR)/q2swx/cl_fx.o \
	$(BUILDDIR)/q2swx/cl_input.o \
	$(BUILDDIR)/q2swx/cl_inv.o \
	$(BUILDDIR)/q2swx/cl_main.o \
	$(BUILDDIR)/q2swx/cl_parse.o \
	$(BUILDDIR)/q2swx/cl_pred.o \
	$(BUILDDIR)/q2swx/cl_tent.o \
	$(BUILDDIR)/q2swx/cl_scrn.o \
	$(BUILDDIR)/q2swx/cl_view.o \
	$(BUILDDIR)/q2swx/cl_newfx.o \
	$(BUILDDIR)/q2swx/console.o \
	$(BUILDDIR)/q2swx/keys.o \
	$(BUILDDIR)/q2swx/snd_dma.o \
	$(BUILDDIR)/q2swx/snd_mem.o \
	$(BUILDDIR)/q2swx/snd_mix.o \
	$(BUILDDIR)/q2swx/m_flash.o \
	$(BUILDDIR)/q2swx/cl_loc.o \
	$(BUILDDIR)/q2swx/cl_draw.o \
	$(BUILDDIR)/q2swx/cl_demo.o \
	\
	$(BUILDDIR)/q2swx/cmd.o \
	$(BUILDDIR)/q2swx/cmodel.o \
	$(BUILDDIR)/q2swx/common.o \
	$(BUILDDIR)/q2swx/crc.o \
	$(BUILDDIR)/q2swx/cvar.o \
	$(BUILDDIR)/q2swx/files.o \
	$(BUILDDIR)/q2swx/md4.o \
	$(BUILDDIR)/q2swx/net_chan.o \
	$(BUILDDIR)/q2swx/pmove.o \
	$(BUILDDIR)/q2swx/q_shared.o \
	\
	$(BUILDDIR)/q2swx/sv_ccmds.o \
	$(BUILDDIR)/q2swx/sv_ents.o \
	$(BUILDDIR)/q2swx/sv_game.o \
	$(BUILDDIR)/q2swx/sv_init.o \
	$(BUILDDIR)/q2swx/sv_main.o \
	$(BUILDDIR)/q2swx/sv_send.o \
	$(BUILDDIR)/q2swx/sv_user.o \
	$(BUILDDIR)/q2swx/sv_world.o \
	\
	$(BUILDDIR)/q2swx/q_shlinux.o \
	$(BUILDDIR)/q2swx/vid_so.o \
	$(BUILDDIR)/q2swx/sys_linux.o \
	$(BUILDDIR)/q2swx/glob.o \
	$(BUILDDIR)/q2swx/net_udp.o \
	$(BUILDDIR)/q2swx/cd_linux.o \
	$(BUILDDIR)/q2swx/snd_linux.o \
	\
	$(BUILDDIR)/q2swx/qmenu.o \
	$(BUILDDIR)/q2swx/ui_addressbook.o \
	$(BUILDDIR)/q2swx/ui_atoms.o \
	$(BUILDDIR)/q2swx/ui_controls.o \
	$(BUILDDIR)/q2swx/ui_credits.o \
	$(BUILDDIR)/q2swx/ui_demos.o \
	$(BUILDDIR)/q2swx/ui_dmoptions.o \
	$(BUILDDIR)/q2swx/ui_download.o \
	$(BUILDDIR)/q2swx/ui_game.o \
	$(BUILDDIR)/q2swx/ui_joinserver.o \
	$(BUILDDIR)/q2swx/ui_keys.o \
	$(BUILDDIR)/q2swx/ui_loadsavegame.o \
	$(BUILDDIR)/q2swx/ui_main.o \
	$(BUILDDIR)/q2swx/ui_multiplayer.o \
	$(BUILDDIR)/q2swx/ui_newoptions.o \
	$(BUILDDIR)/q2swx/ui_playerconfig.o \
	$(BUILDDIR)/q2swx/ui_quit.o \
	$(BUILDDIR)/q2swx/ui_startserver.o \
	$(BUILDDIR)/q2swx/ui_video.o \
	\
	$(BUILDDIR)/q2swx/r_aclip.o \
	$(BUILDDIR)/q2swx/r_alias.o \
	$(BUILDDIR)/q2swx/r_bsp.o \
	$(BUILDDIR)/q2swx/r_draw.o \
	$(BUILDDIR)/q2swx/r_edge.o \
	$(BUILDDIR)/q2swx/r_image.o \
	$(BUILDDIR)/q2swx/r_light.o \
	$(BUILDDIR)/q2swx/r_main.o \
	$(BUILDDIR)/q2swx/r_misc.o \
	$(BUILDDIR)/q2swx/r_model.o \
	$(BUILDDIR)/q2swx/r_part.o \
	$(BUILDDIR)/q2swx/r_poly.o \
	$(BUILDDIR)/q2swx/r_polyse.o \
	$(BUILDDIR)/q2swx/r_rast.o \
	$(BUILDDIR)/q2swx/r_scan.o \
	$(BUILDDIR)/q2swx/r_sprite.o \
	$(BUILDDIR)/q2swx/r_surf.o \
	\
	$(BUILDDIR)/q2swx/rw_x11.o \
	$(BUILDDIR)/q2swx/rw_linux.o

ifeq ($(ARCH),i386)
QUAKE2SWX_OBJS += \
	$(BUILDDIR)/q2swx/snd_mixa.o \
	$(BUILDDIR)/q2swx/r_aclipa.o \
	$(BUILDDIR)/q2swx/r_draw16.o \
	$(BUILDDIR)/q2swx/r_drawa.o \
	$(BUILDDIR)/q2swx/r_edgea.o \
	$(BUILDDIR)/q2swx/r_scana.o \
	$(BUILDDIR)/q2swx/r_spr8.o \
	$(BUILDDIR)/q2swx/r_surf8.o \
	$(BUILDDIR)/q2swx/math.o \
	$(BUILDDIR)/q2swx/d_polysa.o \
	$(BUILDDIR)/q2swx/r_varsa.o \
	$(BUILDDIR)/q2swx/sys_dosa.o
endif


$(BUILDDIR)/aq2sw :  $(QUAKE2SWX_OBJS)
	$(CC) $(CFLAGS) -o $@  $(QUAKE2SWX_OBJS) $(LDFLAGS) $(XLDFLAGS)


$(BUILDDIR)/q2swx/%.o :    $(CLIENT_DIR)/%.c
	$(DO_CC)

$(BUILDDIR)/q2swx/%.o :    $(GAME_DIR)/%.c
	$(DO_CC)

$(BUILDDIR)/q2swx/%.o :    $(COMMON_DIR)/%.c
	$(DO_CC)

$(BUILDDIR)/q2swx/%.o :    $(SERVER_DIR)/%.c
	$(DO_CC)

$(BUILDDIR)/q2swx/%.o :    $(UI_DIR)/%.c
	$(DO_CC)

$(BUILDDIR)/q2swx/%.o :    $(REF_SOFT_DIR)/%.c
	$(DO_CC)

$(BUILDDIR)/q2swx/q_shlinux.o :  $(LINUX_DIR)/q_shlinux.c
	$(DO_CC)

$(BUILDDIR)/q2swx/vid_so.o :     $(LINUX_DIR)/vid_so.c
	$(DO_CC)

$(BUILDDIR)/q2swx/sys_linux.o :  $(LINUX_DIR)/sys_linux.c
	$(DO_CC)

$(BUILDDIR)/q2swx/glob.o :       $(LINUX_DIR)/glob.c
	$(DO_CC)

$(BUILDDIR)/q2swx/net_udp.o :    $(LINUX_DIR)/net_udp.c
	$(DO_CC)

$(BUILDDIR)/q2swx/cd_linux.o :   $(LINUX_DIR)/cd_linux.c
	$(DO_CC)

$(BUILDDIR)/q2swx/snd_linux.o :  $(LINUX_DIR)/snd_linux.c
	$(DO_CC)

$(BUILDDIR)/q2swx/rw_linux.o :   $(LINUX_DIR)/rw_linux.c
	$(DO_CC) $(XCFLAGS)

$(BUILDDIR)/q2swx/rw_x11.o :     $(LINUX_DIR)/rw_x11.c
	$(DO_CC) $(XCFLAGS)
	
$(BUILDDIR)/q2swx/%.o :          $(LINUX_DIR)/%.s
	$(DO_AS)


#############################################################################
# MISC
#############################################################################

clean: clean-debug clean-release

clean-debug:
	$(MAKE) clean2 BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)"

clean-release:
	$(MAKE) clean2 BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(DEBUG_CFLAGS)"

clean2:
	rm -f \
	$(QUAKE2GLX_OBJS) \
	$(QUAKE2SWX_OBJS)

distclean:
	@-rm -rf $(BUILD_DEBUG_DIR) $(BUILD_RELEASE_DIR)
	@-rm -f `find . \( -not -type d \) -and \
		\( -name '*~' \) -type f -print`
