# The MIT License (MIT)
#
# Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
# Copyright (c) 2020 Jérémie Galarneau <jeremie.galarneau@efficios.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

BARECTF ?= barectf
RM = rm -rf
MKDIR = mkdir

PLATFORM_DIR = platform/
CFLAGS = -O2 -fno-strict-aliasing -Wall -pedantic -I$(PLATFORM_DIR) -I.
CXXFLAGS = -std=c++17 -O2 -Wall -pedantic -I$(PLATFORM_DIR) -I.

TRACE_DIR = trace
TARGET = win32-fs-simple
OBJS = $(TARGET).o barectf.o barectf-platform-win32-fs.o

.PHONY: all

all: $(TARGET)

$(TRACE_DIR):
	$(MKDIR) $(TRACE_DIR)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^

$(TRACE_DIR)/metadata barectf-bitfield.h barectf.h barectf.c: config.yaml $(TRACE_DIR)
	$(BARECTF) $< -m $(TRACE_DIR)

barectf.o: barectf.c
	$(CC) $(CFLAGS) -ansi -c $<

barectf-platform-win32-fs.o: $(PLATFORM_DIR)/barectf-platform-win32-fs.cpp barectf.h
	$(CXX) $(CXXFLAGS) -c $<

$(TARGET).o: $(TARGET).cpp barectf.h barectf-bitfield.h
	$(CXX) $(CXXFLAGS) --std=c++17 -c $<

clean:
	$(RM) $(TARGET) $(OBJS) $(TRACE_DIR)
	$(RM) barectf.h barectf-bitfield.h barectf.c
