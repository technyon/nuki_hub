# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import os
import re
import sys

if len(sys.argv) < 2:
    print('Usage: python generate_slave_configs.py <output_directory>')
    sys.exit(1)
output_directory = sys.argv[1]

# Input Kconfig file
component_path = os.path.normpath(os.path.join(os.path.realpath(__file__),'..', '..'))
kconfig_file = os.path.join(component_path, f"idf_v{os.getenv('ESP_IDF_VERSION')}", 'Kconfig.slave_select.in')

# Output file prefix
output_prefix = 'sdkconfig.ci.'

# Regex pattern to match all available options for SLAVE_IDF_TARGET
pattern = r'^ *config SLAVE_IDF_TARGET_(\w+)'

# Read the Kconfig and generate specific sdkconfig.ci.{slave} for each option
with open(kconfig_file, 'r') as file:
    for line in file:
        match = re.match(pattern, line)
        if match:
            slave = match.group(1)
            output_file = os.path.join(output_directory, f'{output_prefix}{slave.lower()}')

            with open(output_file, 'w') as out_file:
                out_file.write(f'CONFIG_SLAVE_IDF_TARGET_{slave.upper()}=y\n')

            print(f'Generated: {output_file}')
