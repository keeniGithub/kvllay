CXX ?= g++
SRC ?= src/main.cpp
BUILD_DIR = build
TARGET = $(BUILD_DIR)/kvllay$(EXE)

CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I header -I include -I include/kvllay
LDFLAGS = 
LDLIBS = 

ifeq ($(OS),Windows_NT)
EXE = .exe
CXXFLAGS += -D _WIN32_WINNT=0x0A00
LDLIBS += -lws2_32
MKDIR = cmd /C if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
REMOVE = cmd /C if exist "$(BUILD_DIR)\kvllay.exe" del /Q "$(BUILD_DIR)\kvllay.exe"
RUN = $(TARGET)
else
EXE =
MKDIR = mkdir -p $(BUILD_DIR)
REMOVE = rm -f $(BUILD_DIR)/kvllay
RUN = ./$(TARGET)
LDLIBS += -pthread
endif

COMPILE = $(CXX) $(SRC) $(CXXFLAGS) -o $(TARGET) $(LDFLAGS) $(LDLIBS)

default:
	$(MKDIR)
	$(REMOVE)
	${COMPILE}
	${RUN}

compile:
	$(MKDIR)
	$(REMOVE)
	${COMPILE}

run:
	${RUN}

clean:
	$(REMOVE)
