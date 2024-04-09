CC			:= gcc
PYTHON		:= python3
ODIR		:= build
QMKPATH		?= /home/qmk/qmk_firmware
ST_GEN_PY	?= ../generator/sequence_transform_data.py
ST_DICT 	?= ../../sequence_transform_dict.txt
ST_CONFIG	?= ../../sequence_transform_config.json
ST_GEN_IN 	:= $(ST_CONFIG) $(ST_DICT) $(ST_GEN_PY)

LIB_DIR			:= ../
TESTER_DIR		:= ./
LIB_SOURCES		:= $(wildcard $(LIB_DIR)/*.c)
LIB_OBJECTS		:= $(LIB_SOURCES:$(LIB_DIR)/%.c=$(ODIR)/%.o)
TESTER_SOURCES	:= $(wildcard $(TESTER_DIR)/*.c)
TESTER_OBJECTS	:= $(TESTER_SOURCES:$(TESTER_DIR)/%.c=$(ODIR)/%.o) $(LIB_OBJECTS)
DEPENDS			:= $(TESTER_OBJECTS:%.o=%.d)

vpath %.c  $(LIB_DIR) $(TESTER_DIR)

OSFLAG :=
ifeq ($(OS),Windows_NT)
	OSFLAG += -D WIN32
endif

CFLAGS := -Werror -Wundef \
	-I. \
	-I$(QMKPATH)/platforms \
	-I$(QMKPATH)/quantum \
	-I$(QMKPATH)/quantum/sequencer \
	-I$(QMKPATH)/quantum/logging \
	-I$(QMKPATH)/quantum/keymap_extras \
	-I.. \
	-DST_TESTER \
	-DSEQUENCE_TRANSFORM_DEBUG=1 \
	-DSEQUENCE_TRANSFORM_ENHANCED_BACKSPACE=1 \
	-DSEQUENCE_TRANSFORM_RULE_SEARCH=0 \
	-DSEQUENCE_TRANSFORM_FALLBACK_BUFFER=1 \
	-D_CONSOLE \
	$(OSFLAG)

ST_GEN_OUT := ../sequence_transform_data.h \
	../sequence_transform_test.h

all: gen tester

$(ST_GEN_OUT): $(ST_GEN_IN)
	@echo Running generator
	$(PYTHON) $(ST_GEN_PY) -c $(ST_CONFIG)

gen: $(ST_GEN_OUT)

tester: $(TESTER_OBJECTS)
	@$(CC) -o $@ $^ $(CFLAGS)
	@echo "Build successful!"

-include $(DEPENDS)

$(ODIR)/%.o: %.c | $(ODIR)
	@echo Compiling $<
	@$(CC) -MMD -c -o $@ $< $(CFLAGS)

$(ODIR):
	@mkdir -p $@

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o $(ODIR)/*.d
