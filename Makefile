EXECUTABLE        = nesalizer
BUILD_DIR         = build
ifeq ($(origin CXX), default)
  CXX             = g++
endif
ifeq ($(origin CC), default)
  CC              = gcc
endif
CONF              = release
BACKTRACE_SUPPORT = 0
q = @
is_clang := $(if $(findstring clang,$(shell "$(CXX)" -v 2>&1)),1,0)
cpp_sources = audio apu blip_buf common controller cpu input main md5   \
  mapper mapper_0 mapper_1 mapper_2 mapper_3 mapper_4 mapper_5 mapper_7 \
  mapper_9 mapper_10 mapper_11 mapper_13 mapper_28 mapper_71 mapper_232 \
  ppu rom save_states sdl_backend timing
c_sources = tables
cpp_objects = $(addprefix $(BUILD_DIR)/,$(cpp_sources:=.o))
c_objects   = $(addprefix $(BUILD_DIR)/,$(c_sources:=.o))
objects     = $(c_objects) $(cpp_objects)
deps        = $(addprefix $(BUILD_DIR)/,$(c_sources:=.d) $(cpp_sources:=.d))
compile_flags := -I$(MARVELL_ROOTFS)/usr/include/SDL2 -DHAVE_OPENGLES2
LDLIBS :=  -lSDL2 -lSDL2_image -lrt -lm -lEGL -lGLESv2 -Wl,--gc-sections

# Steamlink Specific Stuff 
optimizations = -marm -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -march=armv7-a 

# Other Stuff
optimizations += -Ofast -ffast-math -funsafe-math-optimizations -pthread -ffinite-math-only -fno-exceptions -fno-math-errno -falign-functions=1 -falign-loops=1 -falign-jumps=1 -fno-unwind-tables -fno-asynchronous-unwind-tables
optimizations += -funsafe-loop-optimizations -ftree-loop-if-convert -ffunction-sections -fno-ident -fdata-sections -munaligned-access -fno-align-jumps -fmerge-all-constants -fno-stack-protector -fomit-frame-pointer -fno-builtin
warnings       =  -Wdouble-promotion -Wdisabled-optimization -Wmissing-format-attribute -Wno-switch -Wredundant-decls -ftree-loop-distribution 

ifeq ($(filter debug release release-debug,$(CONF)),)
    $(error unknown configuration "$(CONF)")
else ifneq ($(MAKECMDGOALS),clean)
    ifndef MAKE_RESTARTS
        $(info Using configuration "$(CONF)")
    endif
endif
ifneq ($(findstring debug,$(CONF)),)
    compile_flags += -ggdb
endif
ifneq ($(findstring release,$(CONF)),)
    compile_flags += $(optimizations) -DOPTIMIZING -fno-rtti
    link_flags    += $(optimizations) # -fuse-linker-plugin not on arm
endif
ifeq ($(BACKTRACE_SUPPORT),1)
    link_flags += -Wl,-export-dynamic
endif
compile_flags += $(warnings) -D_FILE_OFFSET_BITS=64
$(BUILD_DIR)/$(EXECUTABLE): $(objects)
	@echo Linking $@
	$(q)$(CXX) $(link_flags) $^ $(LDLIBS) -o $@
$(cpp_objects): $(BUILD_DIR)/%.o: src/%.cpp
	@echo Compiling $<
	$(q)$(CXX) -c -Iinclude $(compile_flags) $< -o $@
$(c_objects): $(BUILD_DIR)/%.o: src/%.c
	@echo Compiling $<
	$(q)$(CC) -c -Iinclude -std=c11 $(compile_flags) $< -o $@
$(deps): $(BUILD_DIR)/%.d: src/%.cpp
	@set -e; rm -f $@;                                                 \
	  $(CXX) -MM -Iinclude $(shell sdl2-config --cflags) $< > $@.$$$$; \
	  sed 's,\($*\)\.o[ :]*,$(BUILD_DIR)/\1.o $@ : ,g' < $@.$$$$ > $@; \
	  rm -f $@.$$$$
ifneq ($(MAKECMDGOALS),clean)
    -include $(deps)
endif
$(BUILD_DIR): ; $(q)mkdir $(BUILD_DIR)
$(objects) $(deps): | $(BUILD_DIR)
.PHONY: clean
clean: ; $(q)-rm -rf $(BUILD_DIR)
