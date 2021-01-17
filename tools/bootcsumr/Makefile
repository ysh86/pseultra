TARGET_EXEC ?= bootcsumr

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src

SRCS := $(shell find -L $(SRC_DIRS) -name *.cpp -or -name *.c -or -name *.s)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find -L $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS ?= $(INC_FLAGS) -MMD -MP

CFLAGS += -Wall -Werror --std=c99 -O3 -msse2
CXXFLAGS += -Wall -Werror --std=c++11 -O3 -msse2


# libs

# OpenCL
CPPFLAGS += -I$(HOME)/SDKs/OpenCL-Headers -I$(HOME)/SDKs/OpenCL-CLHPP/include -DCL_SILENCE_DEPRECATION
ifdef WSL_DISTRO_NAME
	TARGET_EXEC := $(TARGET_EXEC).exe
	AS  := x86_64-w64-mingw32-as
	CC  := x86_64-w64-mingw32-gcc-posix
	CXX := x86_64-w64-mingw32-g++-posix
	#CPPFLAGS += -I$(HOME)/SDKs/mingw-std-threads
	#CXXFLAGS += -O0
	LDFLAGS += /mnt/c/Windows/System32/OpenCL.DLL -static -lstdc++ -lgcc -lwinpthread
else
	LDFLAGS += -framework opencl
endif

# OpenCV, glm
###CPPFLAGS += `pkg-config --cflags opencv glm`
###LDFLAGS += `pkg-config --libs opencv`

# ipp
###CPPFLAGS += -I$(HOME)/SDKs/ipp/include
###LDFLAGS += -L$(HOME)/SDKs/ipp/lib -lippcore -lippcc -lippi -lipps


$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# assembly
$(BUILD_DIR)/%.s.o: %.s
	$(MKDIR_P) $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# c++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
