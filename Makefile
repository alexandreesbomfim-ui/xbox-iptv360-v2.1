# IPTV360 - build com libxenon (devkitxenon). Requer DEVKITXENON no ambiente.
# Usa .RECIPEPREFIX = '>' para nao depender de TAB (evita erro ao copiar/colar).
.SUFFIXES:
ifeq ($(strip $(DEVKITXENON)),)
$(error Defina DEVKITXENON, ex.: export DEVKITXENON=/usr/local/xenon)
endif
include $(DEVKITXENON)/rules

.RECIPEPREFIX := >

TARGET   := iptv360
BUILD    := build
SOURCES  := source

CFLAGS   = -g -O2 -Wall $(MACHDEP) $(INCLUDE)
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
LDFLAGS  = -g $(MACHDEP) -Wl,--gc-sections -Wl,-Map,$(notdir $@).map
LIBS     ?= -lxenon -lm -lfat

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))

export LD := $(CXX)
export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o)
export INCLUDE := -I$(CURDIR)/$(SOURCES) -I$(CURDIR)/$(BUILD) -I$(LIBXENON_INC)
export LIBPATHS := -L$(LIBXENON_LIB)

.PHONY: all $(BUILD) clean
all: $(BUILD)

$(BUILD):
> @[ -d $@ ] || mkdir -p $@
> @$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile $(OUTPUT).elf32

clean:
> @echo clean ...
> @rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).elf32

else

DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).elf32: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)

endif
