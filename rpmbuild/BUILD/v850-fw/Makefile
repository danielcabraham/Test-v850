#This is Makefile for libvirgindownload

FILES:=$(wildcard *.cpp)
OBJECTS:=$(patsubst %.cpp,%.o,$(FILES))
OBJECTS:=$(filter-out Main.o v850_dnl_unittest.o,$(OBJECTS))
CXXFLAGS+=-m32 -fPIC -DLINUX -Wall 
LINT=/etc/lint4ngi/flint.out /etc/lint4ngi/base.lnt
LINTFLAGS= -u -v /etc/lint4ngi/Standard.lnt /etc/lint4ngi/ngigcclinux.lnt
LINTOUTPUT=lint.messages

APP=virgindownloadtestapp
LIBNAME=virgindownload

MAJOR=1
MINOR=11

DESTDIR=/
CONFDIR=$(DESTDIR)/etc/v850/
CONFDIR_DNL=$(DESTDIR)/var/firmware/
LIBDIR=$(DESTDIR)/lib/v850/
SMLIN=ln -sf

ifeq ($(FLW),1)
	CXXFLAGS+=-DFLW
endif

all: flashwriter

GLOBAL_H=Flash850.h globals.h

Main.o: Main.cpp $(GLOBAL_H)

$(OBJECTS):%.o: %.cpp %.h $(GLOBAL_H)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	-rm -rf *.o flashwriter $(UNITTEST) *.so* rpm $(LINTOUTPUT) virgindownloadtestapp

cleanobjects:
	-rm -rf *.o
	
flashwriter: Main.o $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lrt
	
virgindownloadtestapp: Main.o slink
	$(CXX) $(CXXFLAGS) -o $(APP) Main.o  -l$(LIBNAME) -L.

lint:
	$(LINT)  $(LINTFLAGS)  serial.cpp MausBus.cpp Timing.cpp cpld_gpio_swdnl.cpp MAUSProc.cpp parser.cpp v850_dnl_if.cpp flash.cpp > $(LINTOUTPUT)

sharedlib: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -shared -Wl,-soname,lib$(LIBNAME).so.$(MAJOR) -o lib$(LIBNAME).so.$(MAJOR).$(MINOR) $(OBJECTS) -lrt

install: sharedlib
	@echo "installation is called from make file"
	install -m 666 flash.cfg $(CONFDIR)
	#install -m 666 master.dnl $(CONFDIR_DNL)
	cp lib$(LIBNAME).so.$(MAJOR).$(MINOR) $(LIBDIR)
	ldconfig -v -n $(LIBDIR)/
	cd $(LIBDIR) && $(SMLIN) lib$(LIBNAME).so.$(MAJOR) lib$(LIBNAME).so

installHeader: v850_dnl_if.h
	install -m 644 v850_dnl_if.h $(DESTDIR)/usr/include

installApp: flashwriter
	install -m 655 flashwriter $(DESTDIR)/bin/v850/

ldconf: sharedlib
	ldconfig -v -n . $(LIBDIR)

uninstall:
	rm -rf $(LIBDIR)/lib$(LIBNAME)*
	rm -rf $(CONFDIR)/flash.cfg
	rm -rf $(DESTDIR)/usr/include/v850_dnl_if.h

rpms:
	make -C _rpmbuild

utest:
	make -C _utest
