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

static-linux:
	$(MKDIR)
	$(CXX) $(SRC) -std=c++17 -Wall -Wextra -O2 -static -s -I header -I include -I include/kvllay -o $(BUILD_DIR)/kvllay-linux-x86_64 -pthread

static-windows:
	$(MKDIR)
	$(CXX) $(SRC) -std=c++17 -Wall -Wextra -O2 -static -static-libgcc -static-libstdc++ -s -D _WIN32_WINNT=0x0A00 -I header -I include -I include/kvllay -o $(BUILD_DIR)/kvllay-windows-x86_64.exe -lws2_32

run:
	${RUN}

test:
	python3 test_kvllay.py 6389

clean:
	$(REMOVE)

docker-build:
	docker build -t kvllay:latest .

docker-run:
	docker run -d --name kvllay -p 6379:6379 kvllay:latest

docker-compose-up:
	docker compose up -d

docker-compose-down:
	docker compose down

