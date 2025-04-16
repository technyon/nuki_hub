import re

regex = r"\#define NUKI_HUB_VERSION \"(.*)\""
regex2 = r"\#define NUKI_HUB_VERSION_INT \(uint32_t\)(.*)"
version = "unknown"
version_int = "unknown"
content_new = ""

with open('src/Config.h', 'r') as file:
    file_content = file.read()
    matches = re.finditer(regex, file_content, re.MULTILINE)

    for matchNum, match in enumerate(matches, start=1):
        for groupNum in range(0, len(match.groups())):
            groupNum = groupNum + 1
            version = match.group(groupNum)
            version_int = int((float(version)*100)+0.1) + 1
            version = float(version_int / 100)

    content_new = re.sub(regex, "#define NUKI_HUB_VERSION \"" + str('{:.2f}'.format(version)) + "\"", file_content, flags = re.M)
    content_new = re.sub(regex2, "#define NUKI_HUB_VERSION_INT (uint32_t)" + str(version_int), content_new, flags = re.M)

with open('src/Config.h', 'w') as writefile:
    writefile.write(content_new)
