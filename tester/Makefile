CC			:= gcc
PYTHON		:= python3
ODIR		:= build
QMKPATH		?= /home/qmk/qmk_firmware
ST_GEN_PY	?= ../generator/sequence_transform_data.py
ST_DICT 	?= ../../sequence_transform_dict.txt
ST_CONFIG	?= ../../sequence_transform_config.json
ST_GEN_IN 	:= $(ST_CONFIG) $(ST_DICT)

vpath %.c  ./:../

OSFLAG :=
ifeq ($(OS),Windows_NT)
	OSFLAG += -D WIN32
endif

CFLAGS := -Werror \
	-I. \
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

ST_GEN_OUT := ../sequence_transform_data.h \
	../sequence_transform_test.h

DEPS := tester.h \
	tester_utils.h \
	sim_output_buffer.h \
	../qmk_wrapper.h \
	../keybuffer.h \
	../key_stack.h \
	../trie.h \
	../cursor.h \
	../utils.h \
	../sequence_transform.h \
	$(ST_GEN_OUT)

_OBJ := tester.o \
	test_all_rules.o \
	test_ascii_string.o \
	test_perform.o \
	test_virtual_output.o \
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

all: gen tester

$(ST_GEN_OUT): $(ST_GEN_IN)
	@echo Running generator
	$(PYTHON) $(ST_GEN_PY) -c $(ST_CONFIG)

gen: $(ST_GEN_OUT) 

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
