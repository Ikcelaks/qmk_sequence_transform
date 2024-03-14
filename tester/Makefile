ifeq ($(QMKPATH),)
  $(error Please set the QMKPATH env var to your qmk_firmware directory)
endif

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
	-I.. \
	-DST_TESTER \
	-DSEQUENCE_TRANSFORM_MISSED_RULES \
	-DSEQUENCE_TRANSFORM_ENABLE_FALLBACK_BUFFER \
	-DNO_PRINT \
	-D_CONSOLE \
	$(OSFLAG)

DEPS = tester.h \
	tester_utils.h \
	sim_output_buffer.h \
	../qmk_wrapper.h \
	../keybuffer.h \
	../key_stack.h \
	../trie.h \
	../cursor.h \
	../utils.h \
	../sequence_transform.h \
	../sequence_transform_data.h \
	../sequence_transform_test.h

_OBJ = tester.o \
	test_all_rules.o \
	test_ascii_string.o \
	test_perform.o \
	test_virtual_output.o \
	test_cursor.o \
	test_backspace.o \
	test_find_rule.o \
	qmk_wrapper.o \
	tester_utils.o \
	sim_output_buffer.o \
	keybuffer.o \
	key_stack.o \
	trie.o \
	cursor.o \
	utils.o \
	sequence_transform.o

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
