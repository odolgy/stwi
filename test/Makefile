#######################################
# Configuration
#######################################
# Application name
TARGET = test
# C includes
C_INCLUDES = \
-I../src \
-IUnity/src \
-I./ \
# Separate C source files
C_SOURCE_SEP = \
./main.c \
# C source folders that will be scanned recursively
C_SOURCE_DIRS = \
../src/ \
Unity/src \
# Output path
BUILD_DIR = build
# Replacement for '../' in target path
PARENT_DIR_SUBST = ^^
# C defines
C_DEFS =
# Debug flags
DEBUG = -g3
# Optimization flags
OPT = -O0
# Extra C flags
CFLAGS_EXTRA = -Wall -Werror
# Linker flags
LDFLAGS = 
# Executables prefix
PREFIX = /usr/bin/
# Echo output
VERBOSE = 0
# Compiler flag for generating .d file ('M' is general, 'MM' is GCC special)
DEPS_OPT = MM

#######################################
# Automated section
#######################################
CC = $(PREFIX)gcc
SZ = $(PREFIX)size

# Convert a source file to a build file
define bld_from_src
$(addprefix $(BUILD_DIR)/, \
$(subst ./,, \
$(subst ../,$(PARENT_DIR_SUBST)/,$(1))))
endef

# Convert a build file to a source file
define bld_to_src
$(subst $(PARENT_DIR_SUBST)/,../,$(1))
endef

C_SOURCES = $(C_SOURCE_SEP)
C_SOURCES += $(foreach dir,$(C_SOURCE_DIRS),$(shell find $(dir) -name "*.c"))
OBJECTS = $(call bld_from_src,$(C_SOURCES:.c=.o))
OBJECT_DIRS = $(sort $(dir $(OBJECTS)))
DEPS = $(OBJECTS:.o=.d)
CFLAGS = $(C_DEFS) $(C_INCLUDES) $(OPT) $(DEBUG) $(CFLAGS_EXTRA)

ifeq ($(VERBOSE),0)
NO_ECHO = @
else
NO_ECHO =
endif

.PHONY: all clean

#######################################
# Build project (default action)
#######################################
all: $(BUILD_DIR)/$(TARGET)

.SECONDEXPANSION:
$(BUILD_DIR)/%.o: $$(call bld_to_src,%.c) Makefile | $(OBJECT_DIRS)
	@echo Compiling $<
	$(NO_ECHO)$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.d: $$(call bld_to_src,%.c) Makefile | $(OBJECT_DIRS)
	$(NO_ECHO)echo '$(@:.d=.o): \' > $@ && $(CC) -$(DEPS_OPT) $(CFLAGS) $< | sed 's/[^ ]* //' >> $@

$(BUILD_DIR)/$(TARGET): $(OBJECTS) Makefile
	@echo Linking $(TARGET)
	$(NO_ECHO)$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(OBJECT_DIRS):
	$(NO_ECHO) mkdir -p $@

sinclude $(DEPS)

#######################################
# Clean up
#######################################
clean:
	-rm -rf $(BUILD_DIR)
