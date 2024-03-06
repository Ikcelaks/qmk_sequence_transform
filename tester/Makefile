CC = gcc

ODIR = build

vpath %.c  ./ ../

OSFLAG :=
ifeq ($(OS),Windows_NT)
	OSFLAG += -D WIN32
endif

CFLAGS = -I. \
	-I$(QMKPATH)/platforms \
	-I$(QMKPATH)/quantum \
	-I$(QMKPATH)/quantum/sequencer \
	-I$(QMKPATH)/quantum/logging \
	-I$(QMKPATH)/quantum/keymap_extras \
	-I../ \
	-DST_TESTER \
	-DSEQUENCE_TRANSFORM_MISSED_RULES \
	-DNO_PRINT \
	-D_CONSOLE \
	$(OSFLAG)
	
 
DEPS = qmk_wrappers.h \
	../keybuffer.h \
	../key_stack.h \
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
	keybuffer.o \
	key_stack.o
	
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c $(DEPS)
	@echo Compiling $<
	@mkdir -p $(ODIR)
	@$(CC) -c -o $@ $< $(CFLAGS)

tester: $(OBJ)
	@$(CC) -o $@ $^ $(CFLAGS)
	@echo "Build successful!"
	
.PHONY: clean

clean:
	rm -f $(ODIR)/*.o