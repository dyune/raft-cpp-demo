CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -pthread -Iinclude
DEBUG_FLAGS = -O0 -g3
RELEASE_FLAGS = -O2 -DNDEBUG
SANITIZE_FLAGS = -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
TSAN_FLAGS = -O1 -g -fno-omit-frame-pointer -fsanitize=thread

SRC = src/kv_store.cpp src/transport.cpp src/node.cpp src/cluster.cpp
APP = build/raft_kv
RELEASE_APP = build/raft_kv_release
TEST = build/tests
ASAN_TEST = build/tests_asan
TSAN_TEST = build/tests_tsan
# Each flag family has a distinct output because Make does not track flag changes.

.PHONY: all debug release test asan tsan docs docs-build clean
all: debug

debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(APP)

release: $(RELEASE_APP)

$(APP): $(SRC) src/main.cpp | build
	$(CXX) $(CXXFLAGS) $^ -o $@

$(RELEASE_APP): $(SRC) src/main.cpp | build
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $^ -o $@

$(TEST): $(SRC) tests/tests.cpp | build
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $^ -o $@

test: $(TEST)
	./$(TEST)

$(ASAN_TEST): $(SRC) tests/tests.cpp | build
	$(CXX) $(CXXFLAGS) $(SANITIZE_FLAGS) $^ -o $@

asan: $(ASAN_TEST)
	./$(ASAN_TEST)

$(TSAN_TEST): $(SRC) tests/tests.cpp | build
	$(CXX) $(CXXFLAGS) $(TSAN_FLAGS) $^ -o $@

tsan: $(TSAN_TEST)
	./$(TSAN_TEST)

docs:
	npm --prefix website run dev

docs-build:
	npm --prefix website run build

build:
	mkdir -p build

clean:
	rm -rf build
