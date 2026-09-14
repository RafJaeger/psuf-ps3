#---------------------------------------------------------------------------------
# Clear implicit built-in rules
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment. export PSL1GHT=<path>")
endif

include $(PSL1GHT)/ppu_rules

TARGET      := ps3-fps-unlocker
BUILD       := build
SOURCES     := source
DATA        := data
INCLUDES    := include

TITLE       := PSUF
APPID       := FPSU00001
CONTENTID   := UP0001-$(APPID)_00-0000000000000000
PKGFILES    := release

CFLAGS      = -O2 -Wall -Wextra -mcpu=cell $(MACHDEP) $(INCLUDE) -std=gnu99 -fno-strict-aliasing
CXXFLAGS    = $(CFLAGS)
LDFLAGS     = $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS        := -lrsx -lgcm_sys -lio -lsysutil -laudio -lrt -llv2 -lnet -lsysmodule -lm
LIBDIRS     :=

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
export BUILDDIR := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES    := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES := $(CPPFILES:.cpp=.o) \
                 $(CFILES:.c=.o) \
                 $(sFILES:.s=.o) \
                 $(SFILES:.S=.o) \
                 $(BINFILES:.bin=.bin.o)

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(LIBPSL1GHT_INC)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
                   $(LIBPSL1GHT_LIB)

.PHONY: $(BUILD) clean pkg run

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).fake.self $(OUTPUT).pkg $(OUTPUT).gnpdrm.pkg

pkg: $(BUILD) $(OUTPUT).pkg

run: $(BUILD)
	@$(PS3LOADAPP) $(OUTPUT).self

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).self: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
$(OFILES): $(BINFILES)

-include $(DEPENDS)

endif
