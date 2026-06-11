TARGET_NAME := main
RUNARGS ?=

# Compilers
CXX := g++
NVCC := nvcc

# Source extensions
CPP_EXT := cpp
CU_EXT := cu

# Directories
SRC_DIR := src
TMPDIR_BASE := tmp
DEBUGDIR_BASE := debug

# CUDA paths
CUDA_HOME := /usr/local/cuda-13.1

# CUDA architecture (adjust for your GPU)
# Common values: sm_52 (Maxwell), sm_61 (Pascal), sm_75 (Turing), sm_86 (Ampere), sm_89 (Ada)
CUDA_ARCH := sm_86

# Flags
CXXFLAGS := -Wall -std=c++20 -O2 -I$(CUDA_HOME)/include
NVCCFLAGS := -std=c++20 -O2 -arch=$(CUDA_ARCH)
LDFLAGS := -L$(CUDA_HOME)/lib64
LDLIBS := -lglfw -lGLEW -lGL -lcudart

# OS detection
ifeq ($(OS),Windows_NT)
	OS_SUFFIX := -win_nt
else
	OS_SUFFIX := -$(shell uname -r)
endif

# Generate dependency files
CXXFLAGS += -MMD -MP
NVCCFLAGS += -MMD -MP

MODE_DIR := release_build

# Directories for object files
TMPDIR := $(TMPDIR_BASE)/$(MODE_DIR)$(OS_SUFFIX)
DEBUGDIR := $(DEBUGDIR_BASE)/$(MODE_DIR)$(OS_SUFFIX)

# Target
ifeq ($(OS),Windows_NT)
	TARGET := $(DEBUGDIR)/$(TARGET_NAME).exe
else
	TARGET := $(DEBUGDIR)/$(TARGET_NAME)
endif

# Recursive wildcard
rwildcard = $(foreach d,$(wildcard $1/*),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# Cross-platform mkdir -p
ifeq ($(OS),Windows_NT)
	MKDIR_P = if not exist "$(1)" mkdir "$(1)"
else
	MKDIR_P = mkdir -p "$(1)"
endif

# Collect sources
CPP_SRCS := $(call rwildcard,$(SRC_DIR),*.$(CPP_EXT))
CU_SRCS := $(call rwildcard,$(SRC_DIR),*.$(CU_EXT))

# Object files
CPP_OBJS := $(patsubst $(SRC_DIR)/%.$(CPP_EXT),$(TMPDIR)/$(SRC_DIR)/%.o,$(CPP_SRCS))
CU_OBJS := $(patsubst $(SRC_DIR)/%.$(CU_EXT),$(TMPDIR)/$(SRC_DIR)/%.o,$(CU_SRCS))
ALL_OBJS := $(CPP_OBJS) $(CU_OBJS)

# Dependency files
CPP_DEPS := $(patsubst %.o,%.d,$(CPP_OBJS))
CU_DEPS := $(patsubst %.o,%.d,$(CU_OBJS))

.PHONY : all run clean info

all: $(TARGET)

run : $(TARGET)
	$(info RUN     $(TARGET) $(RUNARGS))
	@./$(TARGET) $(RUNARGS)

info:
	$(info C++ sources: $(CPP_SRCS))
	$(info CUDA sources: $(CU_SRCS))
	$(info Objects: $(ALL_OBJS))
	$(info Target: $(TARGET))

clean :
	$(if $(wildcard $(DEBUGDIR_BASE)), @rm -r $(DEBUGDIR_BASE))
	$(if $(wildcard $(TMPDIR_BASE)), @rm -r $(TMPDIR_BASE))

# Link using nvcc (handles CUDA runtime library)
$(TARGET) : $(ALL_OBJS)
	@$(call MKDIR_P,$(dir $@))
	$(info LD      $@)
	@$(NVCC) $(LDFLAGS) -o $@ $(ALL_OBJS) $(LDLIBS)

# Compile C++ files
$(TMPDIR)/$(SRC_DIR)/%.o : $(SRC_DIR)/%.$(CPP_EXT)
	@$(call MKDIR_P,$(dir $@))
	$(info CXX     $<)
	@$(CXX) -c $(CXXFLAGS) -o $@ $<

# Compile CUDA files
$(TMPDIR)/$(SRC_DIR)/%.o : $(SRC_DIR)/%.$(CU_EXT)
	@$(call MKDIR_P,$(dir $@))
	$(info NVCC    $<)
	@$(NVCC) -c $(NVCCFLAGS) -o $@ $<

# Include dependency files
-include $(CPP_DEPS)
-include $(CU_DEPS)
