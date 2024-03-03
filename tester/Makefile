CC = gcc

ODIR = obj

vpath %.c  ./ ../

CFLAGS = -I. \
	-I$(QMKPATH)/platforms \
	-I$(QMKPATH)/quantum \
	-I$(QMKPATH)/quantum/sequencer \
	-I$(QMKPATH)/quantum/logging \
	-I$(QMKPATH)/quantum/keymap_extras \
	-I../ \
	-DST_TESTER \
	-DNO_PRINT \
	-D_CONSOLE
 
DEPS = qmk_wrappers.h \
	../keybuffer.h \
	../sequence_transform.h \
	../sequence_transform_data.h \
	../sequence_transform_test.h \
	../trie.h \
	../utils.h
		
_OBJ = tester.o \
	qmk_wrappers.o \
	utils.o \
	trie.o \
	sequence_transform.o \
	keybuffer.o
	
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

tester: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
	
.PHONY: clean

clean:
	rm -f $(ODIR)/*.o