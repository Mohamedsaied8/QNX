
#
#	Makefile for images project
#



# rule to build an ifs from a build file
%.ifs : %.build
	mkifs -v $< $@

IMAGES = training_image.ifs


all:  $(IMAGES)



clean:
	rm -f *.o $(BINS) $(IMAGES) *.sym
	cd solutions; make clean_solutions


 	