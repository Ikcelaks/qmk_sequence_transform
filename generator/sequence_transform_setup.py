from argparse import ArgumentParser
import json
from pathlib import Path


if __name__ == '__main__':
    parser = ArgumentParser()

    parser.add_argument(
        "-c", "--config-path", type=Path,
        help="config file", required=True
    )

    cli_args = parser.parse_args()

    THIS_FOLDER = Path(__file__).parent

    default_config_file = THIS_FOLDER / "sequence_transform_config_default.json"
    user_config_path = cli_args.config_path
    user_config_file = user_config_path / "config.json"
    user_rules_priority_file = user_config_path / "rules_priority"
    Path.mkdir(user_config_path, exist_ok=True)
    config = json.load(open(default_config_file, 'rt', encoding="utf-8"))

    if not user_config_file.is_file():
        with open(file=user_config_file, mode="a") as cf:
            json.dump(config, cf, ensure_ascii=False, indent=4)

    user_rules_files = []
    if not user_rules_priority_file.is_file():
        user_rules_files = [f'{user_config_path}/sample.rules']
        with open(file=user_rules_priority_file, mode="a") as rpf:
            rpf.write("sample.rules")
    else:
        with open(file=user_rules_priority_file) as rpf:
            user_rules_files = [ f'{user_config_path}/{rules_file}' for rules_file in rpf.readlines()]

    print(f'{user_config_file} {" ".join(user_rules_files)}')
