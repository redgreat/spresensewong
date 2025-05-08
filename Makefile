include $(APPDIR)/Make.defs

# 应用信息
PROGNAME  = $(CONFIG_SPRESENSEWONG_PROGNAME)
PRIORITY  = $(CONFIG_SPRESENSEWONG_PRIORITY)
STACKSIZE = $(CONFIG_SPRESENSEWONG_STACKSIZE)
MODULE    = $(CONFIG_SPRESENSEWONG)

# 源文件
CSRCS = 
CXXSRCS = src/spresense_main.cxx \
          src/mp3_player/display.cpp \
          src/mp3_player/player.cpp \
          src/mp3_player/file_system.cpp \
          src/mp3_player/ui_screens.cpp \
          src/gnss_odometer/gnss_data.cpp \
          src/gnss_odometer/gnss_screens.cpp \
          src/main_menu.cpp

# 头文件搜索路径
CFLAGS += -I$(APPDIR)/include
CFLAGS += -I$(APPDIR)/spresensewong/src/include
CXXFLAGS += -I$(APPDIR)/include
CXXFLAGS += -I$(APPDIR)/spresensewong/src/include

include $(APPDIR)/Application.mk
