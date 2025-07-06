import re, json, argparse

parser = argparse.ArgumentParser()

parser.add_argument('version', type=str)
args = parser.parse_args()

with open('ota/old/manifest.json', 'r+') as json_file:
    data = json.load(json_file)
    data[str(int((float(args.version)*100)+0.1))] = args.version
    data2 = sorted(data.items(), reverse=True)
    sorted_dict = {}
    k = 6
    for key, value in data2:
        if k > 0:
            sorted_dict[key] = value
            k = k - 1
    json_file.seek(0)
    json.dump(sorted_dict, json_file, indent=4)
    json_file.truncate()