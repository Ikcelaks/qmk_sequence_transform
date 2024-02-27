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
This program reads from a prepared dictionary file and generates a C source file
"sequence_transform_data.h" with a serialized trie embedded as an array. Run this
program and pass it as the first argument like:
$ qmk generate-autocorrect-data autocorrect_dict.txt
Each line of the dict file defines one typo and its correction with the syntax
"typo -> correction". Blank lines or lines starting with '#' are ignored.
Example:
  :thier        -> their
  fitler        -> filter
  lenght        -> length
  ouput         -> output
  widht         -> width
For full documentation, see QMK Docs
"""
import os.path
import sys
import textwrap
import json
from typing import Any, Dict, Iterator, List, Tuple
from string import digits
from pathlib import Path
from argparse import ArgumentParser

parser = ArgumentParser()

parser.add_argument(
    "-c", "--config", type=str,
    help="config file path", default="../../sequence_transform_config.json"
)

parser.add_argument("-q", "--quiet", action="store_true")
args = parser.parse_args()

THIS_FOLDER = Path(__file__).parent

KC_A = 0x04
KC_SPC = 0x2c
KC_MINUS = 0x2D
KC_SEMICOLON = 0x33
KC_1 = 0x1E
MOD_LSFT = 0x0200
KC_MAGIC_0 = 0x0100
qmk_digits = digits[1:] + digits[0]


S = lambda code: MOD_LSFT | code


def generate_range(start: int, chars: str) -> list[tuple[str, int]]:
    return [(char, start + i) for i, char in enumerate(chars)]


def generate_context_char_map(magic_chars, wordbreak_char) -> Dict[str, int]:
    return dict([
        *generate_range(KC_SEMICOLON, ";'`,./"),
        *generate_range(S(KC_SEMICOLON), ":\"~<>?"),
        *generate_range(KC_MINUS, "-=[]\\"),
        *generate_range(S(KC_MINUS), "_+\{\}|"),
        *generate_range(KC_1, qmk_digits),
        *generate_range(S(KC_1), "!@#$%^&*()"),
        *generate_range(KC_MAGIC_0, magic_chars),
        (wordbreak_char, KC_SPC),  # "Word break" character.
    ] + [(chr(c), c + KC_A - ord('a')) for c in range(ord('a'), ord('z') + 1)])


OUTPUT_FUNC_1 = 1
OUTPUT_FUNC_COUNT_MAX = 7


def quiet_print(*args, **kwargs):
    if config["quiet"]:
        return

    print(*args, **kwargs)


def generate_output_func_char_map(output_func_chars) -> Dict[str, int]:
    if len(output_func_chars) > OUTPUT_FUNC_COUNT_MAX:
        print('{fg_red}Error:{fg_reset} More than %d ({fg_cyan}%d{fg_reset}) output_func_chars were listed %s', OUTPUT_FUNC_COUNT_MAX, len(output_func_chars), output_func_chars)
        sys.exit(1)
    return dict([(char, OUTPUT_FUNC_1 + i) for i, char in enumerate(output_func_chars)])


def parse_file(file_name: str, char_map: Dict[str, int], separator: str, comment: str) -> List[Tuple[str, str]]:
    """Parses autocorrections dictionary file.
  Each line of the file defines one typo and its correction with the syntax
  "typo -> correction". Blank lines or lines starting with '#' are ignored. The
  function validates that typos only have characters a-z. Overlapping typos are
  matched to the longest valid match.
  Args:
    file_name: String, path of the autocorrections dictionary.
  Returns:
    List of (typo, correction) tuples.
  """

    rules = []
    context_set = set()
    for line_number, context, correction in parse_file_lines(file_name, separator, comment):
        if context in context_set:
            print('{fg_red}Error:%d:{fg_reset} Ignoring duplicate sequence: "{fg_cyan}%s{fg_reset}"', line_number, context)
            continue

        # Check that `context` is valid.
        if not  all([(c in char_map) for c in context[:-1]]):
            print('{fg_red}Error:%d:{fg_reset} sequence "{fg_cyan}%s{fg_reset}" has invalid characters', line_number, context)
            sys.exit(1)
        if len(context) > 127:
            print('{fg_red}Error:%d:{fg_reset} Sequence exceeds 127 chars: "{fg_cyan}%s{fg_reset}"', line_number, context)
            sys.exit(1)

        rules.append((context, correction))
        context_set.add(context)

    return rules


def make_trie(magicons: List[Tuple[str, str]], output_func_char_map: Dict[str, int]) -> Dict[str, Any]:
    """Makes a trie from the the typos, writing in reverse.
  Args:
    autocorrections: List of (typo, correction) tuples.
  Returns:
    Dict of dict, representing the trie.
  """
    trie = {}
    for context, correction in magicons:
        node = trie
        if correction[-1] in output_func_char_map:
            output_func = output_func_char_map[correction[-1]]
            target = correction[:len(correction)-1]
        else:
            output_func = 0
            target = correction
        for letter in context[::-1]:
            node = node.setdefault(letter, {})
        node['MATCH'] = (context, {'TARGET': target, 'RESULT': {'BACKSPACES': -1, 'FUNC': output_func, 'OUTPUT': ""}})

    return trie


def complete_trie(trie: Dict[str, Any], wordbreak_char: str):
    outputs = set()

    def complete_node(context, completion):
        nonlocal outputs
        back_context = []
        expanded_context = []
        for c in context[:-1]:
            back_context.append(c)
            expanded_context.append(c)
            match = get_trie_result(back_context)
            if not match:
                match = get_trie_result(expanded_context)
            if match:
                match_backspaces = match['RESULT']['BACKSPACES']
                match_output = match['RESULT']['OUTPUT']
                del(expanded_context[-(match_backspaces+1):])
                expanded_context.extend(match_output)
                # quiet_print(c, expanded_context)
        if expanded_context and expanded_context[0] == wordbreak_char:
            del(expanded_context[0])
        target = completion['TARGET']
        i = 0  # Make the autocorrection data for this entry and serialize it.
        while i < min(len(expanded_context), len(target)) and expanded_context[i] == target[i]:
            i += 1
        backspaces = len(expanded_context) - i
        output = target[i:].replace(wordbreak_char, " ")
        outputs.add(output)
        completion['RESULT']['BACKSPACES'] = backspaces
        completion['RESULT']['OUTPUT'] = output

    def get_trie_result(buffer: List[str]):
        longest_match = {}
        trienode = trie
        for c in reversed(buffer):
            if c in trienode:
                # quiet_print(c)
                trienode = trienode[c]
                if 'MATCH' in trienode:
                    context, completion = trienode['MATCH']
                    if completion['RESULT']['BACKSPACES'] == -1:
                        complete_node(context, completion)
                    longest_match = completion
            else:
                break
        return longest_match

    def traverse_trienode(trinode: Dict[str, Any]):
        for key, value in trinode.items():
            if key == 'MATCH':
                context, completion = value
                if completion['RESULT']['BACKSPACES'] == -1:
                    complete_node(context, completion)
            else:
                traverse_trienode(value)

    traverse_trienode(trie)
    return outputs


def parse_file_lines(file_name: str, separator: str, comment: str) -> Iterator[Tuple[int, str, str]]:
    """Parses lines read from `file_name` into context-correction pairs."""

    line_number = 0
    with open(file_name, 'rt', encoding="utf-8") as file:
        for line in file:
            line_number += 1
            line = line.strip()
            if line and line.find(comment) != 0:
                # Parse syntax "typo -> correction", using strip to ignore indenting.
                tokens = [token.strip() for token in line.split(separator, 1)]
                if len(tokens) != 2 or not tokens[0]:
                    print(f'Error:{line_number}: Invalid syntax: "{line}"')
                    sys.exit(1)

                context, correction = tokens

                yield line_number, context, correction


def serialize_outputs(outputs: set[str]) -> Tuple[List[int], Dict[str, int]]:
    quiet_print(sorted(outputs, key=len, reverse=True))
    completions_str = ''
    completions_map = {}
    completions_offset = 0
    for output in sorted(outputs, key=len, reverse=True):
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
    return (list(bytes(completions_str, 'ascii')), completions_map)


def serialize_trie(char_map: Dict[str, int], wordbreak_char: str, trie: Dict[str, Any], completions_map: Dict[str, int]) -> List[int]:
    """Serializes trie in a form readable by the C code.
  Args:
    autocorrections: List of (typo, correction) tuples.
    trie: Dict of dicts.
  Returns:
    List of ints in the range 0-255.
  """
    table = []

    # Traverse trie in depth first order.
    def traverse(trie_node):
        if 'MATCH' in trie_node:  # Handle a MATCH trie node.
            typo, completion = trie_node['MATCH']
            typo = typo.strip(wordbreak_char)
            backspaces = completion['RESULT']['BACKSPACES']
            func = completion['RESULT']['FUNC']
            output = completion['RESULT']['OUTPUT']
            output_index = completions_map[output]
            # 2 bits (16,15) are used for node type
            code = 0x8000 + (0x4000 if len(trie_node) > 1 else 0)
            # 3 bits (14..12) are used for special function
            assert 0 <= func < 8
            code += func << 11
            # 4 bits (11..8) are used for backspaces
            assert 0 <= backspaces < 16
            code += backspaces << 7
            # 7 bits (bits 7..1) are used for completion_len
            clen = len(output)
            assert 0 <= clen < 128
            code += clen
            # First output word stores coded info, second stores completion data offset index
            data = [code, output_index]
            # quiet_print('{fg_red}Error:%d:{fg_reset} Data "{fg_cyan}%s{fg_reset}"', 0, data)
            del trie_node['MATCH']
        else:
            data = []

        if len(trie_node) == 0:
            entry = {'data': data, 'links': [], 'uint16_offset': 0}
            table.append(entry)
        elif len(trie_node) == 1:  # Handle trie node with a single child.
            c, trie_node = next(iter(trie_node.items()))
            entry = {'data': data, 'chars': c, 'uint16_offset': 0}

            # It's common for a trie to have long chains of single-child nodes. We
            # find the whole chain so that we can serialize it more efficiently.
            while len(trie_node) == 1 and 'MATCH' not in trie_node:
                c, trie_node = next(iter(trie_node.items()))
                entry['chars'] += c

            table.append(entry)
            entry['links'] = [traverse(trie_node)]
        else:  # Handle trie node with multiple children.
            entry = {'data': data, 'chars': ''.join(sorted(trie_node.keys())), 'uint16_offset': 0}
            table.append(entry)
            entry['links'] = [traverse(trie_node[c]) for c in entry['chars']]

        # quiet_print('{fg_red}Error:%d:{fg_reset} Data "{fg_cyan}%s{fg_reset}"', 0, entry['data'])
        return entry

    traverse(trie)
    # quiet_print('{fg_red}Error:%d:{fg_reset} Data "{fg_cyan}%s{fg_reset}"', 0, table)

    def serialize(e: Dict[str, Any]) -> List[int]:
        data = e['data']
        # quiet_print('{fg_red}Error:%d:{fg_reset} Serialize Data "{fg_cyan}%s{fg_reset}"', 0, data)
        if not e['links']:  # Handle a leaf table entry.
            return data
        elif len(e['links']) == 1:  # Handle a chain table entry.
            return data + [char_map[c] for c in e['chars']] + [0]  # + encode_link(e['links'][0]))
        else:  # Handle a branch table entry.
            links = []
            for c, link in zip(e['chars'], e['links']):
                links += [char_map[c] | (0 if links else 0x4000)] + encode_link(link)
            return data + links + [0]

    uint16_offset = 0
    for e in table:  # To encode links, first compute byte offset of each entry.
        e['uint16_offset'] = uint16_offset
        uint16_offset += len(serialize(e))
        assert 0 <= uint16_offset <= 0xffff
    # Serialize final table.
    return [b for e in table for b in serialize(e)]


def encode_link(link: Dict[str, Any]) -> List[int]:
    """Encodes a node link as two bytes."""
    uint16_offset = link['uint16_offset']
    if not (0 <= uint16_offset <= 0xffff):
        quiet_print('{fg_red}Error:{fg_reset} The autocorrection table is too large, a node link exceeds 64KB limit. Try reducing the autocorrection dict to fewer entries.')
        sys.exit(1)
    return [uint16_offset]


def typo_len(e: Tuple[str, str]) -> int:
    return len(e[0])


def byte_to_hex(b: int) -> str:
    return f'0x{b:02X}'


def uint16_to_hex(b: int) -> str:
    return f'0x{b:04X}'


def generate_sequence_transform_data():
    magic_chars = config['magic_chars']
    output_func_chars = config['output_func_chars']
    wordbreak_char = config['wordbreak_char']
    comment_str = config['comment_str']
    sep_str = config['separator_str']
    out_file = THIS_FOLDER / "../sequence_transform_data.h"

    char_map = generate_context_char_map(magic_chars, wordbreak_char)
    output_func_char_map = generate_output_func_char_map(output_func_chars)

    autocorrections = parse_file(THIS_FOLDER / "../../" / config['rules_file_name'], char_map, sep_str, comment_str)
    trie = make_trie(autocorrections, output_func_char_map)
    outputs = complete_trie(trie, wordbreak_char)
    quiet_print(json.dumps(trie, indent=4))
    completions_data, completions_map = serialize_outputs(outputs)
    trie_data = serialize_trie(char_map, wordbreak_char, trie, completions_map)

    # current_keyboard = cli.args.keyboard or cli.config.user.keyboard or cli.config.generate_sequence_transform_data.keyboard
    # current_keymap = cli.args.keymap or cli.config.user.keymap or cli.config.generate_sequence_transform_data.keymap

    # if current_keyboard and current_keymap:
    #     cli.args.output = locate_keymap(current_keyboard, current_keymap).parent / 'sequence_transform_data.h'

    assert all(0 <= b <= 0xffff for b in trie_data)
    assert all(0 <= b <= 0xff for b in completions_data)

    min_typo = min(autocorrections, key=typo_len)[0]
    max_typo = max(autocorrections, key=typo_len)[0]

    # Build the sequence_transform_data.h file.
    # sequence_transform_data_h_lines = [GPL2_HEADER_C_LIKE, GENERATED_HEADER_C_LIKE, '#pragma once', '']
    sequence_transform_data_h_lines = ['#pragma once', '']

    sequence_transform_data_h_lines.append(f'// Autocorrection dictionary with longest match semantics:')
    sequence_transform_data_h_lines.append(f'// Autocorrection dictionary ({len(autocorrections)} entries):')
    for typo, correction in autocorrections:
        correction_escape_backslash = correction.replace("\\", "\\ [escape]")
        sequence_transform_data_h_lines.append(f'//   {typo:<{len(max_typo)}} -> {correction_escape_backslash}')

    sequence_transform_data_h_lines.extend([
        ''
        f'#define SPECIAL_KEY_TRIECODE_0 {uint16_to_hex(KC_MAGIC_0)}',
        f'#define SEQUENCE_MIN_LENGTH {len(min_typo)} // "{min_typo}"',
        f'#define SEQUENCE_MAX_LENGTH {len(max_typo)} // "{max_typo}"',
        f'#define DICTIONARY_SIZE {len(trie_data)}',
        f'#define COMPLETIONS_SIZE {len(completions_data)}',
        f'#define SEQUENCE_TRANSFORM_COUNT {len(magic_chars)}',
        '',
        'static const uint16_t sequence_transform_data[DICTIONARY_SIZE] PROGMEM = {',
        textwrap.fill('    %s' % (', '.join(map(uint16_to_hex, trie_data))), width=135, subsequent_indent='    '),
        '};',
        '',
        'static const uint8_t sequence_transform_completions_data[COMPLETIONS_SIZE] PROGMEM = {',
        textwrap.fill('    %s' % (', '.join(map(byte_to_hex, completions_data))), width=100, subsequent_indent='    '),
        '};',
    ])

    if os.path.exists(out_file):
        with open(out_file, "r", encoding="utf-8") as file:
            if file.read() == "\n".join(sequence_transform_data_h_lines):
                return

    # Show the results
    with open(out_file, "w", encoding="utf-8") as file:
        file.write("\n".join(sequence_transform_data_h_lines))

    # Show the results
    # dump_lines(cli.args.output, sequence_transform_data_h_lines, cli.args.quiet)


if __name__ == '__main__':
    config = json.load(open(THIS_FOLDER / args.config, 'rt', encoding="utf-8"))

    if args.quiet:
        config["quiet"] = True

    generate_sequence_transform_data()
