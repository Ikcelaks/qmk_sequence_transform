
import subprocess


command = (
    'powershell.exe -Command ".\hid_listen.exe | Select-String -Pattern '
    '\\"st_rule,.+?,\\d+?,\\w+?,\\w+?\\"" >> '
    'rule_usage_log.csv"'
)

subprocess.run(command, shell=True)
