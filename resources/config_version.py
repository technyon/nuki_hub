import re, json, argparse
from urllib.request import urlopen

parser = argparse.ArgumentParser()

parser.add_argument('ota_type', type=str)
args = parser.parse_args()

regex = r"\#define NUKI_HUB_VERSION \"(.*)\""
regex2 = r"\#define NUKI_HUB_VERSION_INT \"(.*)\""
version = "unknown"

with open('src/Config.h', 'r') as file:
    file_content = file.read()
    matches = re.finditer(regex, file_content, re.MULTILINE)

    for matchNum, match in enumerate(matches, start=1):
        for groupNum in range(0, len(match.groups())):
            groupNum = groupNum + 1
            version = match.group(groupNum)

data = ""
number = 1

response = urlopen("https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/manifest.json")
data = json.loads(response.read().decode("utf-8"))
if ("number" not in data[args.ota_type]):
    number = 1
elif (data[args.ota_type]['version'] == version):
    number = data[args.ota_type]['number'] + 1
else:
    number = 1

version_int = int((float(version)*100)+0.1)
content_new = ""

with open ('src/Config.h', 'r' ) as readfile:
    file_content = readfile.read()
    content_new = re.sub(regex, "#define NUKI_HUB_VERSION \"" + str(version) + "-" + args.ota_type + str(number) + "\"", file_content, flags = re.M)
    content_new = re.sub(regex2, "#define NUKI_HUB_VERSION_INT (uint32_t)\"" + str(version_int) + "\"", content_new, flags = re.M)

with open('src/Config.h', 'w') as writefile:
    writefile.write(content_new)
