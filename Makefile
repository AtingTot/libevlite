
# -----------------------------------------------------------

CC		= gcc
CXX		= g++
CFLAGS	= -Wall -Wformat=0 -Iinclude/ -Isrc/ -Itest/ -ggdb -fPIC -O2 -finline-limit=1000 -D__EVENT_VERSION__=\"$(REALNAME)\"
CXXFLAGS= -Wall -Wformat=0 -Iinclude/ -Isrc/ -Itest/ -ggdb -fPIC -O2 -finline-limit=1000 -D__EVENT_VERSION__=\"$(REALNAME)\"
LFLAGS	= -ggdb -lpthread 
SOFLAGS	= -shared -Wl,-soname,$(SONAME)

LIBNAME	= libevlite.so
SONAME	= $(LIBNAME).7
REALNAME= $(LIBNAME).7.3.0

PREFIX		= /usr/local
LIBPATH		= $(PREFIX)/lib
INCLUDEPATH	= $(PREFIX)/include

OS		= $(shell uname)

#
# 利用git tag发布软件版本
#
#APPNAME=`git describe | awk -F- '{print $$1}'`
#VERSION=`git describe | awk -F- '{print $$2}'`
#MAJORVER=`git describe | awk -F- '{print $$2}' | awk -F. '{print $$1}'`
#
#LIBNAME=$(APPNAME).so
#SONAME=$(APPNAME).so.$(MAJORVER)
#REALNAME=$(APPNAME).so.$(VERSION)
#

OBJS 	= utils.o timer.o event.o \
			threads.o \
			message.o channel.o session.o \
			iolayer.o

ifeq ($(OS),Linux)
#	LFLAGS += -lrt -L/usr/local/lib -ltcmalloc_minimal
	LFLAGS += -lrt
	OBJS += epoll.o
else
	OBJS += kqueue.o
endif

# Release, open it
# CFLAGS += DNDEBUG

# -----------------------------------------------------------

install : all
	rm -rf $(INCLUDEPATH)/evlite
	cp -a include $(INCLUDEPATH)/evlite
	rm -rf $(LIBPATH)/$(REALNAME); cp $(REALNAME) $(LIBPATH)
	rm -rf $(LIBPATH)/$(SONAME); ln -s $(REALNAME) $(LIBPATH)/$(SONAME)
	rm -rf $(LIBPATH)/$(LIBNAME); ln -s $(REALNAME) $(LIBPATH)/$(LIBNAME)

all : $(REALNAME)

$(REALNAME) : $(OBJS)
	$(CC) $(SOFLAGS) $(LFLAGS) $^ -o $@
	rm -rf $(SONAME); ln -s $@ $(SONAME)
	rm -rf $(LIBNAME); ln -s $@ $(LIBNAME)

test : test_events test_addtimer echoserver-lock echoserver iothreads_dispatcher

test_events : test_events.o $(OBJS)
	$(CC) $^ -o $@ $(LFLAGS)

test_addtimer : test_addtimer.o $(OBJS)
	$(CC) $^ -o $@ $(LFLAGS)

echoserver-lock : accept-lock-echoserver.o $(OBJS)
	$(CC) $^ -o $@ $(LFLAGS)

echoclient : io.o echoclient.o $(OBJS)
	$(CXX) $^ -o $@ $(LFLAGS)

echoserver : io.o echoserver.o $(OBJS)
	$(CXX) $^ -o $@ $(LFLAGS)

pingpong : pingpong.o $(OBJS)
	$(CC) $^ -o $@ $(LFLAGS)

echostress :
	$(CC) -I/usr/local/include -L/usr/local/lib -levent test/echostress.c -o $@

iothreads_dispatcher : test_iothreads.o $(OBJS)
	$(CC) $(LFLAGS) $^ -o $@

chatroom : chatroom_server chatroom_client

chatroom_server: io.o chatroom_server.o $(OBJS)
	$(CXX) $^ -o $@ $(LFLAGS)

chatroom_client: io.o chatroom_client.o $(OBJS)
	$(CXX) $^ -o $@ $(LFLAGS)

clean :

	rm -rf *.o
	rm -rf *.log
	rm -rf *.core
	rm -rf core.*
	rm -rf vgcore.*

	rm -rf $(SONAME)
	rm -rf $(LIBNAME)
	rm -rf $(REALNAME)

	rm -rf test_events event.fifo
	rm -rf test_addtimer echoclient echostress echoserver pingpong echoserver-lock iothreads_dispatcher
	rm -rf chatroom_client chatroom_server
	
# --------------------------------------------------------
#
# gmake的规则
#
%.o : %.c
	$(CC) $(CFLAGS) -c $^ -o $@

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

VPATH = src:include:test
	
