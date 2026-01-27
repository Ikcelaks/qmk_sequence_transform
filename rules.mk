ST_GEN_PY := $(MODULE_PATH_SEQUENCE_TRANSFORM)/generator/sequence_transform_data.py
ST_GEN_SETUP_PY := $(MODULE_PATH_SEQUENCE_TRANSFORM)/generator/sequence_transform_setup.py
ST_GEN_CONFIG_DIR := $(KEYMAP_PATH)/sequence_transform
ST_GEN_IN := $(KEYMAP_PATH)/sequence_transform_config.json $(KEYMAP_PATH)/sequence_transform_dict.txt

ST_GEN_OUT := $(INTERMEDIATE_OUTPUT)/src/st_gen_metadata.h $(INTERMEDIATE_OUTPUT)/src/sequence_transform_data.h

all: $(ST_GEN_OUT)

$(ST_GEN_OUT): $(ST_GEN_IN) $(ST_GEN_PY)
	@echo Running sequence_transform generator
	python3 $(ST_GEN_PY) --keymappath $(KEYMAP_PATH) --config $(KEYMAP_PATH)/sequence_transform_config.json --outputpath $(INTERMEDIATE_OUTPUT)/src/

LIB_SRC += utils.c
LIB_SRC += trie.c
LIB_SRC += keybuffer.c
LIB_SRC += cursor.c
LIB_SRC += key_stack.c
LIB_SRC += triecodes.c
LIB_SRC += predicates.c
LIB_SRC += st_debug.c
