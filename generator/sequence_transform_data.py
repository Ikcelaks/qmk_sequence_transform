# Copyright 2021 Google LLC
# Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
# Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
# Copyright 2024 QKekos <q.kekos.q@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Python program to make sequence_transform_data.h.
This program reads from a prepared dictionary file and generates a
C source file "sequence_transform_data.h"
with a serialized trie embedded as an array.

Each line of the dict file defines "sequence -> transformation" pair.
Blank lines or lines starting with comment string are ignored.

Examples:
  :ex@        -> example
  :ob@        -> obvious
  :d@         -> develop
  :d@r        -> developer
"""

import re
import textwrap
import json
from typing import Any, Dict, Iterator, List, Tuple, Callable
from datetime import date, datetime
from string import digits
from pathlib import Path
from argparse import ArgumentParser


ST_GENERATOR_VERSION = "SEQUENCE_TRANSFORM_GENERATOR_VERSION_3_2"

GPL2_HEADER_C_LIKE = f'''\
// Copyright {date.today().year} QMK
// SPDX-License-Identifier: GPL-2.0-or-later
'''

GENERATED_HEADER_C_LIKE = f'''\
// This was file generated on {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}.
// Do not edit this file directly!
'''

TRIECODE_SEQUENCE_TOKEN_0 = 0x80
TRIECODE_SEQUENCE_METACHAR_0 = 0xA0
TRIECODE_TRANSFORM_SEQUENCE_REF_0 = 0x80
TRIE_MATCH_BIT = 0x80
TRIE_BRANCH_BIT = 0x40
TRIE_MULTI_BRANCH_BIT = 0x20
OUTPUT_FUNC_1 = 1
OUTPUT_FUNC_COUNT_MAX = 7
max_backspaces = 0

class bcolors:
    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    PURPLE = "\033[95m"
    CYAN = "\033[96m"

    ENDC = "\033[0m"
    BOLD = "\033[1m"
    UNDERLINE = "\033[4m"


###############################################################################
def color_currying(color: str) -> Callable:
    def inner(*text: Any) -> str:
        return f"{color}{' '.join(map(str, text))}{bcolors.ENDC}"

    return inner


###############################################################################
red = color_currying(bcolors.RED)
cyan = color_currying(bcolors.CYAN)
yellow = color_currying(bcolors.YELLOW)


###############################################################################
def err(*text: Any) -> str:
    return red("Error:", *text)


###############################################################################
def map_range(start: int, symbols: str) -> dict[str, int]:
    return {symbol: start + i for i, symbol in enumerate(symbols)}


###############################################################################
def map_ascii(symbols: str) -> dict[str, int]:
    return {symbol: ord(symbol) for symbol in symbols}


###############################################################################
def generate_sequence_symbol_map(seq_tokens, wordbreak_symbol) -> Dict[str, int]:
    return {
        **{chr(c): c for c in range(32, 126)},
        SPACE_SYMBOL: ord(" "),
        **map_range(TRIECODE_SEQUENCE_TOKEN_0, seq_tokens),
        **map_range(TRIECODE_SEQUENCE_METACHAR_0, SEQ_METACHAR_SYMBOLS),
    }


###############################################################################
def quiet_print(*args, **kwargs):
    if IS_QUIET:
        return

    print(*args, **kwargs)


###############################################################################
def generate_transform_symbol_map() -> Dict[str, int]:
    return {
        **{chr(c): c for c in range(32, 126)},
        SPACE_SYMBOL: ord(" "),
        **map_range(TRIECODE_TRANSFORM_SEQUENCE_REF_0, TRANSFORM_SEQUENCE_REFERENCE_SYMBOLS),
    }


###############################################################################
def generate_output_func_symbol_map(output_func_symbols) -> Dict[str, int]:
    if len(output_func_symbols) > OUTPUT_FUNC_COUNT_MAX:
        output_symbols_count_str = str(len(output_func_symbols))

        raise SystemExit(
            f'{err()} More than {OUTPUT_FUNC_COUNT_MAX} ({cyan(output_symbols_count_str)})'
            f' output_func_symbols were listed {output_func_symbols}'
        )

    return dict([
        (symbol, OUTPUT_FUNC_1 + i)
        for i, symbol in enumerate(output_func_symbols)
    ])


###############################################################################
def parse_file(
    file_name: str, symbol_map: Dict[str, int],
    separator: str, comment: str
) -> List[Tuple[str, str]]:
    """Parses sequence dictionary file.
    Each line of the file defines one "sequence -> transformation" pair.
    Blank lines or lines starting with the comment string are ignored.
    The function validates that sequences only have characters a-z.
    Overlapping sequences are matched to the longest valid match.
    """

    file_lines = parse_file_lines(file_name, separator, comment)
    sequence_set = set()
    duplicated_rules = []
    rules = []

    for line_number, sequence, transform in file_lines:
        if sequence in sequence_set:
            duplicated_rules.append(
                f'{err("line", line_number)}: '
                f'Duplicate sequence: "{cyan(sequence)}"'
            )

        # Check that `sequence` is valid.
        if not all([(c in symbol_map) for c in sequence[:-1]]):
            raise SystemExit(
                f'{err("line", line_number)}: '
                f'sequence "{cyan(sequence)}" has invalid characters'
            )

        if len(sequence) > 127:
            raise SystemExit(
                f'{err("line", line_number)}:'
                f'Sequence exceeds 127 chars: "{cyan(sequence)}"'
            )

        if IMPLICIT_TRANSFORM_LEADING_WORDBREAK and sequence.startswith(WORDBREAK_SYMBOL):
            transform = WORDBREAK_SYMBOL + transform

        rules.append((sequence, transform))
        sequence_set.add(sequence)

    if duplicated_rules:
        raise SystemExit("\n".join(duplicated_rules))

    return rules

###############################################################################
def add_default_rules(
    trie: Dict[str, tuple[str, dict]]
):
    for seq_token in SEQ_TOKEN_SYMBOLS:
        if 'MATCH' not in trie['TOKEN'].setdefault(seq_token, {'TOKEN': {}, 'CHAIN': [], 'OFFSET': 0}):
            quiet_print("Adding missing default rule for {seq_token}\n")
            trie['TOKEN'][seq_token]['MATCH'] = {
                    'SEQUENCE': seq_token,
                    'TRANSFORM': "",
                    'ACTION': {
                        'BACKSPACES': 0,
                        'FUNC': 0,
                        'COMPLETION': ""
                    },
                    'OFFSET': 0
                }

###############################################################################
def make_sequence_trie(
    seq_list: List[Tuple[str, str]],
    output_func_symbol_map: Dict[str, int]
) -> Tuple[Dict[str, tuple[str, dict]], set[str], set[str, List[str]]]:
    """Makes a trie from the sequences, writing in reverse."""
    def seq_len(seq_trans):
        return len(seq_trans[0])

    seq_list.sort(key=seq_len)
    trie = {'TOKEN': {}, 'CHAIN': [], 'OFFSET': 0}
    rules = []
    completions = set()
    completions.add("")
    missing_intermediate_rules = {}
    missing_prefix_rules = {}
    is_suffix_of = {}

    for seq, trans in seq_list:
        is_suffix_of[seq] = set()
        for cand_seq, cand_trans in seq_list:
            if cand_seq.endswith(seq) and cand_trans.endswith(trans):
                is_suffix_of[seq].add(cand_seq)

    for sequence, transform in seq_list:
        node = trie

        if len(transform) > 0 and transform[-1] in output_func_symbol_map:
            output_func = output_func_symbol_map[transform[-1]]
            transform = transform[:len(transform)-1]

        else:
            output_func = 0

        for sub_seq, sub_match in reversed(rules):
            if sequence.startswith(sub_seq):
                suffix = sequence[len(sub_seq):]
                prematch_output = sub_match['TRANSFORM'] + suffix
                i = 0
                while (
                    i < min(len(prematch_output) - 1, len(transform)) and
                    prematch_output[i] == transform[i]
                ):
                    i += 1

                backspaces = len(prematch_output) - 1 - i
                completion = transform[i:].replace(WORDBREAK_SYMBOL, ' ')
                match = {
                    'SEQUENCE': sequence,
                    'TRANSFORM': transform,
                    'ACTION': {
                        'BACKSPACES': backspaces,
                        'FUNC': output_func,
                        'COMPLETION': completion
                    },
                    'OFFSET': 0
                }

                for letter in suffix[i::-1]:
                    node = node['TOKEN'].setdefault(letter, {'TOKEN': {}, 'CHAIN': [], 'OFFSET': 0})

                chains = node['CHAIN']
                chains.append({
                    'SUB_RULE': sub_match,
                    'MATCH': match,
                    'OFFSET': 0
                })

                rules.append((sequence, match))
                completions.add(completion)

                # Check for missing prefix rules. These rules are not always desird, but they triggered
                # under the automatically under the old system and the user should be aware that they now
                # need to be specified explicitly
                for cand_sub_seq in is_suffix_of[sub_seq]:
                    prefix = cand_sub_seq[:cand_sub_seq.find(sub_seq)]
                    cand_seq = prefix + sequence
                    cand_trans = prefix + transform
                    if (prefix + sequence) not in [s for s, t in seq_list]:
                        missing_prefix_rules.setdefault(cand_sub_seq, []).append(f"{cand_seq} {SEP_STR} {cand_trans} ({sequence} {SEP_STR} {transform})")

                break
            elif sub_seq in sequence and not sequence.endswith(sub_seq):
                # This rule's sequence is a substring that is neither a prefix nor a suffix
                # of the current sequence, nor any of the current rule's sub-rules and will cause a problem.
                missing_prefix = sequence[:sequence.find(sub_seq)]
                missing_intermediate_rules.setdefault(missing_prefix + sub_seq, []).append(f"{sequence} {SEP_STR} {transform}")

        else:
            i = 0
            while (
                i < min(len(sequence) - 1, len(transform)) and
                sequence[i] == transform[i]
            ):
                i +=1

            backspaces = len(sequence) - 1 - i
            completion = transform[i:].replace(WORDBREAK_SYMBOL, ' ')
            match = {
                'SEQUENCE': sequence,
                'TRANSFORM': transform,
                'ACTION': {
                    'BACKSPACES': backspaces,
                    'FUNC': output_func,
                    'COMPLETION': completion
                },
                'OFFSET': 0
            }

            for letter in sequence[::-1]:
                node = node['TOKEN'].setdefault(letter, {'TOKEN': {}, 'CHAIN': [], 'OFFSET': 0})

            node['MATCH'] = match

            rules.append((sequence, match))
            completions.add(completion)

    add_default_rules(trie)

    return trie, completions, missing_intermediate_rules, missing_prefix_rules


###############################################################################
def generate_matches(pattern) -> list[tuple[str, str]]:
    valid_tokens = f"[\w{SEQ_TOKEN_SYMBOLS}{WORDBREAK_SYMBOL}]"

    square_brackets_group = re.findall(fr"\[({valid_tokens}+)]", pattern)
    if square_brackets_group:
        match = square_brackets_group[0]

        return generate_matches(pattern.replace(
            f"[{match}]",
            f"({'|'.join(match)})"
        ))

    patterns = []
    groups = re.findall(fr"\((?:{valid_tokens}\|?)+\)\??", pattern)

    if not groups:
        return [("", pattern)]

    match = groups[0]
    elements = re.findall(f"{valid_tokens}+", match)

    patterns.extend([
        (element, pattern.replace(match, element))
        for element in elements
    ])

    if "?" in match:
        patterns.append(("", pattern.replace(match, "")))

    return patterns


###############################################################################
def parse_tokens(tokens: List[str], parse_regex: bool) -> Iterator[Tuple[int, str, str]]:
    full_sequence, transform = tokens

    if parse_regex:
        for token, sequence in generate_matches(full_sequence):
            yield sequence, transform.replace(r"\1", token)
    else:
        yield full_sequence, transform


###############################################################################
def parse_file_lines(
    file_name: str, separator: str, comment: str
) -> Iterator[Tuple[int, str, str]]:
    """Parses lines read from `file_name` into sequence-transform pairs."""
    with open(file_name, 'rt', encoding="utf-8") as file:
        lines = file.readlines()

    regex_start = f"{COMMENT_STR}REGEX_START"
    regex_end = f"{COMMENT_STR}REGEX_END"
    in_regex_zone = False

    for line_number, line in enumerate(lines, 1):
        line = line.strip()

        if not line:
            continue

        if line == regex_start:
            in_regex_zone = True
            continue

        if line == regex_end:
            in_regex_zone = False
            continue

        if not line.startswith(comment):
            # Parse syntax "sequence -> transformation".
            # Using strip to ignore indenting.
            tokens = [token.strip() for token in line.split(separator, 1)]

            if len(tokens) != 2 or not tokens[0]:
                raise SystemExit(
                    f'{err(line_number)}: Invalid syntax: "{red(line)}"'
                )

            for sequence, transform in parse_tokens(tokens, in_regex_zone):
                yield line_number, sequence, transform


###############################################################################
def serialize_outputs(
    outputs: set[str]
) -> Tuple[List[int], Dict[str, int], int]:
    quiet_print(sorted(outputs, key=len, reverse=True))
    completions_str = ''
    completions_map = {}
    completions_offset = 0
    max_completion_len = 0

    output_list = sorted(outputs)

    for output in sorted(output_list, key=len, reverse=True):
        max_completion_len = max(max_completion_len, len(output))
        i = completions_str.find(output)

        if i == -1:
            quiet_print(f'{output} added at {completions_offset}')
            completions_map[output] = completions_offset
            completions_str += output
            completions_offset += len(output)

        else:
            quiet_print(f'{output} found at {i}')
            completions_map[output] = i

    quiet_print(completions_str)

    return (
        [TRANFORM_SYMBOL_MAP[c] for c in completions_str],
        completions_map,
        max_completion_len
    )


###############################################################################
def serialize_sequence_trie(
    symbol_map: Dict[str, int], trie: Dict[str, Any],
    completions_map: Dict[str, int]
) -> List[int]:
    """Serializes trie in a form readable by the C code.

    Returns:
    List of 16bit ints in the range 0-64k.
    """
    table = []

    def build_chain_match(sub_rule, match):
        return {
            'SUB_RULE': sub_rule,
            'DATA': build_match(match)
        }

    def build_match(match, more_branching = False):
        global max_backspaces

        action = match['ACTION']

        backspaces = action['BACKSPACES']
        max_backspaces = max(max_backspaces, backspaces)
        func = action['FUNC']
        completion = action['COMPLETION']
        completion_index = completions_map[completion]

        # 1 bit reserved for marking if trie branching continues
        code = 0x80 if more_branching else 0

        # 2 bits (6..5) are used for special function
        assert 0 <= func < 4
        code += func << 5

        # 5 bits (4..0) are used for backspaces
        assert 0 <= backspaces < 32
        code += backspaces

        # 8 bits (bits 7..0) are used for completion_len
        completion_len = len(completion)
        assert 0 <= completion_len < 256
        completion_len

        completion_index_byte1, completion_index_byte2 = divmod(completion_index, 0x100)

        # First output word stores coded info
        # Second stores completion data offset index
        return [code, completion_len, completion_index_byte1, completion_index_byte2]

    # Traverse trie in depth first order.
    def traverse(trie_node):

        node_header_data = []
        chain_matches = trie_node['CHAIN']
        chain_match_count = len(chain_matches)
        has_match = 'MATCH' in trie_node
        token_count = len(trie_node['TOKEN'])

        if has_match or chain_match_count > 0:
            node_type = TRIE_MATCH_BIT + \
                            (TRIE_BRANCH_BIT if token_count > 0 else 0) + \
                            (0x20 if has_match else 0x00)
            node_header_data = [node_type]

        if chain_match_count > 0:
            if chain_match_count > 0x0f:
                if chain_match_count > 0x0fff:
                    raise SystemExit(
                        f'{err()} Impressive. More than 4095 rules chained at once'
                    )
                count_code_byte1, count_code_byte2 = divmod(chain_match_count, 0x100)
                node_header_data = [node_header_data[0] | 0x10 | count_code_byte1, count_code_byte2]
            else:
                node_header_data[0] = node_header_data[0] | chain_match_count

        entry = {'node': trie_node}

        if len(node_header_data) > 0:
            entry['node_header_data'] = node_header_data

        if has_match:
            # Node has at lest one match, and or chained match
            # serialize the match node
            entry['match_data'] = build_match(trie_node['MATCH'], token_count > 0)
            entry['match_node'] = trie_node['MATCH']

        chain_data = []
        for cmatch in chain_matches:
            chain_data.append((
                build_chain_match(cmatch['SUB_RULE'], cmatch['MATCH']),
                cmatch['MATCH']
            ))
        if len(chain_data) > 0:
            entry['chain_data'] = chain_data

        if token_count == 1:  # Handle trie node with a single child.
            c, trie_node = next(iter(trie_node['TOKEN'].items()))
            entry['str'] = c

            # It's common for a trie to have long chains of single-child nodes.

            # We find the whole chain so that
            # we can serialize it more efficiently.
            # print(f"Singe-chain: Char {c}\n{json.dumps(trie_node, indent=4)}")
            while len(trie_node['TOKEN']) == 1 and len(trie_node['CHAIN']) == 0 and 'MATCH' not in trie_node:
                c, trie_node = next(iter(trie_node['TOKEN'].items()))
                # print(f"Singe-chain: Char {c}\n{json.dumps(trie_node, indent=4)}")
                entry['str'] += c

            table.append(entry)
            entry['links'] = [traverse(trie_node)]

        elif token_count > 0:  # Handle trie node with multiple children.
            entry['chars'] = ''.join(sorted(trie_node['TOKEN'].keys(), key=lambda k : symbol_map[k]))

            table.append(entry)
            # print(f"branch node: {json.dumps(entry, indent=4)}")
            entry['links'] = [traverse(trie_node['TOKEN'][c]) for c in entry['chars']]

        else:
            table.append(entry)

        # quiet_print(f'{err(0)} Data "{cyan(entry["data"])}"')
        return entry

    traverse(trie)
    # quiet_print(f'{err(0)} Data "{cyan(table)}"')

    def serialize(node: Dict[str, Any]) -> List[int]:
        # print(f"{json.dumps(node, indent=4)}")
        data = []
        # data = node['data']
        # quiet_print(f'{err(0)} Serialize Data "{cyan(data)}"')
        if 'node_header_data' in node:
            data = data + node['node_header_data']

        if 'match_data' in node:
            data = data + node['match_data']

        if 'chain_data' in node:
            for cmatch, _ in node['chain_data']:
                data = data + encode_link(cmatch['SUB_RULE']) + \
                    cmatch['DATA']

        if 'str' in node:  # Handle a chain table entry.
            return data + [1] + [symbol_map[c] for c in node['str']] + [0]

        if 'chars' in node:  # Handle a branch table entry.
            code = TRIE_BRANCH_BIT
            if any([(symbol_map[c] & TRIECODE_SEQUENCE_METACHAR_0) == TRIECODE_SEQUENCE_METACHAR_0 for c in node['chars']]):
                code = code | TRIE_MULTI_BRANCH_BIT
            links = [code]

            for c, link in zip(node['chars'], node['links']):
                links += [symbol_map[c]] + encode_link(link['node'])

            return data + links + [0]

        return data

    uint16_offset = 0

    # To encode links, first compute byte offset of each entry.
    for table_entry in table:
        table_entry['node']['OFFSET'] = uint16_offset
        temp_uint16_offset = uint16_offset + len(table_entry.get('node_header_data', []))
        if 'match_data' in table_entry:
            table_entry['match_node']['OFFSET'] = temp_uint16_offset
            temp_uint16_offset += len(table_entry['match_data'])
        if 'chain_data' in table_entry:
            # print(f"offset chain_data {table_entry['chain_data']}")
            for cmatch, cnode in table_entry['chain_data']:
                cnode['OFFSET'] = temp_uint16_offset + 2
                temp_uint16_offset += 2 + len(cmatch['DATA'])

        uint16_offset += len(serialize(table_entry))

        assert 0 <= uint16_offset <= 0xffff

    # Serialize final table.
    trie_data = [b for node in table for b in serialize(node)]

    return trie_data


###############################################################################
def encode_link(link: Dict[str, Any]) -> List[int]:
    """Encodes a node link as two bytes."""
    # print(f"{json.dumps(link, indent=4)}")
    uint16_offset = link['OFFSET']

    if not (0 <= uint16_offset <= 0xffff):
        raise SystemExit(
            f'{err()} The transforming table is too large, '
            f'a node link exceeds 64KB limit. '
            f'Try reducing the transforming dict to fewer entries.'
        )

    offset_byte1, offset_byte2 = divmod(uint16_offset, 0x100)

    return [offset_byte1, offset_byte2]


###############################################################################
def sequence_len(node: Tuple[str, str]) -> int:
    return len(node[0])


###############################################################################
def transform_len(node: Tuple[str, str]) -> int:
    return len(node[1])


###############################################################################
def byte_to_hex(b: int) -> str:
    return f'0x{b:02X}'


###############################################################################
def uint16_to_hex(b: int) -> str:
    return f'0x{b:04X}'


###############################################################################
def create_triecode_array_c_string(
    symbol_map: Dict[str, int],
    string: str
) -> str:
    """ returns a string filled with C definition of triecode array
        ex: "(uint8_t[4]){ 0x1234, 0x1234, 0x1234, 0},"
    """
    ints = [symbol_map[c] for c in string] + [0]
    int_str = ', '.join(map(byte_to_hex, ints))
    c_str = f'    (uint8_t[{len(ints)}]){{ {int_str} }},'
    return c_str


###############################################################################
def generate_sequence_transform_data(data_header_file, test_header_file):
    symbol_map = generate_sequence_symbol_map(SEQ_TOKEN_SYMBOLS, WORDBREAK_SYMBOL)
    output_func_symbol_map = generate_output_func_symbol_map(OUTPUT_FUNC_SYMBOLS)

    seq_tranform_list = parse_file(RULES_FILE, symbol_map, SEP_STR, COMMENT_STR)
    trie, outputs, missing_intermediate_rules, missing_prefix_rules = make_sequence_trie(seq_tranform_list, output_func_symbol_map)

    for missing_rule, affected_rules in missing_intermediate_rules.items():
        print(f"Consider adding a rule for this sequence: {cyan(missing_rule)}\n  To fix these rules")
        for rule in affected_rules:
            print(f"    {cyan(missing_rule)}{yellow(rule[len(missing_rule):])}")

    for cand_seq, missing_rules in missing_prefix_rules.items():
        print(f"Missing potential rules starting with {cand_seq}")
        for rule in missing_rules:
            print(f"    {rule}")

    s_outputs = serialize_outputs(outputs)
    completions_data, completions_map, max_completion_len = s_outputs

    trie_data = serialize_sequence_trie(symbol_map, trie, completions_map)
    quiet_print(json.dumps(trie, indent=4))

    assert all(0 <= b <= 0xffff for b in trie_data)
    assert all(0 <= b <= 0xff for b in completions_data)

    min_sequence = min(seq_tranform_list, key=sequence_len)[0]
    max_sequence = max(seq_tranform_list, key=sequence_len)[0]
    max_transform = max(seq_tranform_list, key=transform_len)[1]

    # Build the sequence_transform_data.h file.
    transforms = []
    test_rule_c_sequences = []
    test_rule_c_transforms = []

    for sequence, transform in seq_tranform_list:
        # Don't add rules with transformation functions to test header for now
        if transform[-1] not in output_func_symbol_map:
            c_sequence = create_triecode_array_c_string(symbol_map, sequence)
            c_transform = create_triecode_array_c_string(symbol_map | TRANFORM_SYMBOL_MAP, transform)
            test_rule_c_sequences.append(c_sequence)
            test_rule_c_transforms.append(c_transform)
        transform = transform.replace("\\", "\\ [escape]")
        sequence = f"{sequence:<{len(max_sequence)}}"
        transforms.append(f'//    {sequence} -> {transform}')

    header_lines = [
        GPL2_HEADER_C_LIKE,
        GENERATED_HEADER_C_LIKE,
        '#pragma once',
    ]

    tranforms_lines = [
        f'// Sequence Transformation dictionary with longest match semantics',
        f'// Dictionary ({len(seq_tranform_list)} entries):',
        *transforms,
    ]

    # token symbols stored as utf8 strings
    seq_sym_array_str = ", ".join(map(lambda c: f'"{c}"', SEQ_TOKEN_SYMBOLS))
    seq_metachar_array_str = ", ".join(map(lambda c: f'"{c}"', SEQ_METACHAR_SYMBOLS))
    trans_seq_ref_array_str = ", ".join(map(lambda c: f'"{c}"', TRANSFORM_SEQUENCE_REFERENCE_SYMBOLS))
    st_seq_tokens = f'static const char *st_seq_tokens[] = {{ {seq_sym_array_str} }};'
    st_seq_metachars = f'static const char *st_seq_metachars[] = {{ {seq_metachar_array_str} }};'
    st_trans_seq_ref_tokens = f'static const char *st_trans_seq_ref_tokens[] = {{ {trans_seq_ref_array_str} }};'
    st_space_token = f'static const char *st_space_token = "{SPACE_SYMBOL}";'
    # ascii versions
    seq_token_char_array_str = ", ".join(map(lambda c: f"'{c}'", SEQ_TOKEN_ASCII_CHARS))
    seq_metachar_char_array_str = ", ".join(map(lambda c: f"'{c}'", SEQ_METACHAR_ASCII_CHARS))
    st_seq_token_ascii_chars = f'static const char st_seq_token_ascii_chars[] = {{ {seq_token_char_array_str} }};'
    st_seq_metachar_ascii_chars = f'static const char st_seq_metachar_ascii_chars[] = {{ {seq_metachar_char_array_str} }};'
    # st_wordbreak_ascii = f"static const char st_wordbreak_ascii = '{WORDBREAK_ASCII}';"

    trie_stats_lines = [
        f'#define {ST_GENERATOR_VERSION}',
        '',
        f'#define TRIECODE_SEQUENCE_TOKEN_0 {uint16_to_hex(TRIECODE_SEQUENCE_TOKEN_0)}',
        f'#define TRIECODE_SEQUENCE_METACHAR_0 {uint16_to_hex(TRIECODE_SEQUENCE_METACHAR_0)}',
        f'#define TRIECODE_SEQUENCE_REF_TOKEN_0 {uint16_to_hex(TRIECODE_TRANSFORM_SEQUENCE_REF_0)}',
        f'#define SEQUENCE_MIN_LENGTH {len(min_sequence)} // "{min_sequence}"',
        f'#define SEQUENCE_MAX_LENGTH {len(max_sequence)} // "{max_sequence}"',
        f'#define TRANSFORM_MAX_LENGTH {len(max_transform)} // "{max_transform}"',
        f'#define COMPLETION_MAX_LENGTH {max_completion_len}',
        f'#define MAX_BACKSPACES {max_backspaces}',
        f'#define SEQUENCE_TRIE_SIZE {len(trie_data)}',
        f'#define COMPLETIONS_SIZE {len(completions_data)}',
        f'#define SEQUENCE_TOKEN_COUNT {len(SEQ_TOKEN_SYMBOLS)}',
        f'#define SEQUENCE_METACHAR_COUNT {len(SEQ_METACHAR_SYMBOLS)}',
        f'#define SEQUENCE_REF_TOKEN_COUNT {len(TRANSFORM_SEQUENCE_REFERENCE_SYMBOLS)}',
        '',
        st_seq_token_ascii_chars,
        st_seq_metachar_ascii_chars,
        # st_wordbreak_ascii,
        # qmk build checks for unused vars,
        # so we must use an ifdef here
        '#ifdef ST_TESTER',
        st_seq_tokens,
        st_seq_metachars,
        st_trans_seq_ref_tokens,
        st_space_token,
        '#endif'
    ]

    trie_data_lines = [
        'static const uint8_t '
        'sequence_transform_trie[SEQUENCE_TRIE_SIZE] PROGMEM = {',

        textwrap.fill(
            '    %s' % (', '.join(map(byte_to_hex, trie_data))),
            width=100, subsequent_indent='    '
        ),
        '};\n',

        'static const uint8_t '
        'sequence_transform_completions_data[COMPLETIONS_SIZE] PROGMEM = {',

        textwrap.fill(
            '    %s' % (', '.join(map(byte_to_hex, completions_data))),
            width=100, subsequent_indent='    '
        ),
        '};\n',
    ]

    # Write data header file
    sequence_transform_data_h_lines = [
        *header_lines,
        '',
        *trie_stats_lines,
        '',
        *tranforms_lines,
        '',
        *trie_data_lines,
    ]
    with open(data_header_file, "w", encoding="utf-8") as file:
        file.write("\n".join(sequence_transform_data_h_lines))

    # Write test header file
    sequence_transform_test_h_lines = [
        *header_lines,
        '',
        'static const uint8_t *st_test_sequences[] = {',
        *test_rule_c_sequences,
        '    0',
        '};',
        '',
        'static const uint8_t *st_test_transforms[] = {',
        *test_rule_c_transforms,
        '    0',
        '};'
    ]
    with open(test_header_file, "w", encoding="utf-8") as file:
        file.write("\n".join(sequence_transform_test_h_lines))


###############################################################################
if __name__ == '__main__':
    parser = ArgumentParser()

    parser.add_argument(
        "-c", "--config", type=str,
        help="config file path", default="../../sequence_transform_config.json"
    )

    parser.add_argument("-d", "--debug", action="store_true", default=False)
    cli_args = parser.parse_args()

    THIS_FOLDER = Path(__file__).parent

    data_header_file = THIS_FOLDER / "../sequence_transform_data.h"
    test_header_file = THIS_FOLDER / "../sequence_transform_test.h"
    config_file = THIS_FOLDER / cli_args.config
    config = json.load(open(config_file, 'rt', encoding="utf-8"))

    try:
        SEQ_TOKEN_SYMBOLS = list(config['sequence_token_symbols'].keys())
        SPACE_SYMBOL = config['space_symbol']
        WORDBREAK_SYMBOL = list(config['wordbreak_symbol'].keys())[0]
        DIGIT_SYMBOL = list(config['digit_symbol'].keys())[0]
        ALPHA_SYMBOL = list(config['alpha_symbol'].keys())[0]
        UPPER_ALPHA_SYMBOL = list(config['upper_alpha_symbol'].keys())[0]
        PUNCT_SYMBOL = list(config['punct_symbol'].keys())[0]
        NONTERMINATING_PUNCT_SYMBOL = list(config['nonterminating_punct_symbol'].keys())[0]
        TERMINATING_PUNCT_SYMBOL = list(config['terminating_punct_symbol'].keys())[0]
        ANY_SYMBOL = list(config['any_symbol'].keys())[0]
        OUTPUT_FUNC_SYMBOLS = config['output_funcs'].values()
        TRANSFORM_SEQUENCE_REFERENCE_SYMBOLS = config['transform_sequence_reference_symbols']
        COMMENT_STR = config['comment_str']
        SEP_STR = config['separator_str']
        RULES_FILE = THIS_FOLDER / "../../" / config['rules_file_name']
    except KeyError as e:
        raise SystemExit(f"Incorrect config! {cyan(*e.args)} key is missing.")

    IMPLICIT_TRANSFORM_LEADING_WORDBREAK = config.get('implicit_transform_leading_wordbreak', False)
    SEQ_TOKEN_ASCII_CHARS = list(config['sequence_token_symbols'].values())
    WORDBREAK_ASCII = config['wordbreak_symbol'][WORDBREAK_SYMBOL]
    DIGIT_ASCII = config['digit_symbol'][DIGIT_SYMBOL]
    ALPHA_ASCII = config['alpha_symbol'][ALPHA_SYMBOL]
    UPPER_ALPHA_ASCII = config['upper_alpha_symbol'][UPPER_ALPHA_SYMBOL]
    PUNCT_ASCII = config['punct_symbol'][PUNCT_SYMBOL]
    NONTERMINATING_PUNCT_ASCII = config['nonterminating_punct_symbol'][NONTERMINATING_PUNCT_SYMBOL]
    TERMINATING_PUNCT_ASCII = config['terminating_punct_symbol'][TERMINATING_PUNCT_SYMBOL]
    ANY_ASCII = config['any_symbol'][ANY_SYMBOL]
    SEQ_METACHAR_SYMBOLS = [UPPER_ALPHA_SYMBOL, ALPHA_SYMBOL, DIGIT_SYMBOL, TERMINATING_PUNCT_SYMBOL, NONTERMINATING_PUNCT_SYMBOL, PUNCT_SYMBOL, WORDBREAK_SYMBOL, ANY_SYMBOL]
    SEQ_METACHAR_ASCII_CHARS = [UPPER_ALPHA_ASCII, ALPHA_ASCII, DIGIT_ASCII, TERMINATING_PUNCT_ASCII, NONTERMINATING_PUNCT_ASCII, PUNCT_ASCII, WORDBREAK_ASCII, ANY_ASCII]
    TRANFORM_SYMBOL_MAP = generate_transform_symbol_map()

    IS_QUIET = not cli_args.debug
    generate_sequence_transform_data(data_header_file, test_header_file)
