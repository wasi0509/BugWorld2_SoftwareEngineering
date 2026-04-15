# vim:ts=8 sw=8 noet:
# 


.PHONY: all clean

BUGS :=	simple.bug stupid.bug sample.bug spinning.bug firefly.bug\
	beetle.bug ladybird.bug

warning:
	@echo Adjust paths before using this Makefile!

all:	$(BUGS)

clean:	    
	rm -f simple.bug stupid.bug spinning.bug beetle.bug

%.bug:	%.b ../beacon/bugc
	../beacon/bugc $< > $@

%.bug:	%.buggy ../src/asm
	../src/asm $< > $@

../beacon/bugc:
	make -C ../beacon

../src/asm:
	make -C ../src
