
#
#	Makefile for images project
#



# rule to build an ifs from a build file
%.ifs : %.build
	mkifs -v $< $@

IMAGES = training_image_sol.ifs


all:  $(IMAGES)


clean_solutions:
	rm -f *.o $(BINS) $(IMAGES) *.sym


 	