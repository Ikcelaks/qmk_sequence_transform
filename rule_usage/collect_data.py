
import csv
from collections import Counter
from dataclasses import dataclass


FILE_PATH = './rule_usage_log.csv'


@dataclass
class Rule:
    _: str
    sequence: str
    _backspace_count: str
    trigger_key: str
    completion: str
    entry_count: int
    collection: "RuleCollection" = None

    @property
    def backspace_count(self) -> int:
        return int(self._backspace_count)

    def __repr__(self) -> str:
        completion = f"{self.sequence[:-self.backspace_count]}{self.completion}"
        sequence = f"{self.sequence}{self.trigger_key}"

        count_offset = self.collection.max_completion_len - len(completion) + 1
        formatted_completion = f"{completion}{' ':>{count_offset}}"

        sequence_offset = self.collection.max_sequence_len + 1
        formatted_sequence = f"{sequence:<{sequence_offset}}"

        return (
            f"{formatted_sequence} -> {formatted_completion}"
            f"    : {self.entry_count}"
        )


@dataclass
class RuleCollection:
    rules: list[Rule]

    def __post_init__(self):
        for rule in self.rules:
            rule.collection = self

    @property
    def max_sequence_len(self) -> int:
        return max(map(len, [rule.sequence for rule in self.rules]))

    @property
    def max_completion_len(self) -> int:
        return max(map(len, [
            rule.sequence[:-rule.backspace_count] + rule.completion
            for rule in self.rules
        ]))


def main():
    rules_data = read_csv_file(FILE_PATH)
    rules = Counter(rules_data)

    rules_collection = RuleCollection([
        Rule(*rule + (rules[rule], )) for rule in rules
    ])

    for rule in rules_collection.rules:
        print(rule)

    input()


def read_csv_file(file_path):
    with open(file_path, newline='') as csvfile:
        reader = csv.reader(csvfile, delimiter=',', quotechar='"')
        return tuple(tuple(row) for row in reader if row)


if __name__ == "__main__":
    main()
