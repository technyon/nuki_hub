#!/usr/bin/env python3
import binascii
import sys
import argparse

parser = argparse.ArgumentParser(description='Convert binary file to C-style array initializer.')
parser.add_argument("filename", help="the file to be converted")
parser.add_argument("-O", "--output", help="write output to a file")
parser.add_argument("-l", "--linebreak", type=int, help="add linebreak after every N element")
parser.add_argument("-L", "--linebreak-string", default="\n", help="use what to break link, defaults to \"\\n\"")
parser.add_argument("-S", "--separator-string", default=", ", help="use what to separate elements, defaults to \", \"")
parser.add_argument("-H", "--element-prefix", default="0x", help="string to be added to the head of element, defaults to \"0x\"")
parser.add_argument("-T", "--element-suffix", default="", help="string to be added to the tail of element, defaults to none")
parser.add_argument("-U", "--force-uppercase", action='store_true', help="force uppercase HEX representation")
parser.add_argument("-n", "--newline", action='store_true', help="add a newline on file end")
args = parser.parse_args()

def make_sublist_group(lst: list, grp: int) -> list:
    """
    Group list elements into sublists.

    make_sublist_group([1, 2, 3, 4, 5, 6, 7], 3) = [[1, 2, 3], [4, 5, 6], 7]
    """
    return [lst[i:i+grp] for i in range(0, len(lst), grp)]

def do_convension(content: bytes, to_uppercase: bool=False) -> str:
    hexstr = binascii.hexlify(content).decode("UTF-8")
    if to_uppercase:
        hexstr = hexstr.upper()
    array = [args.element_prefix + hexstr[i:i + 2] + args.element_suffix for i in range(0, len(hexstr), 2)]
    if args.linebreak:
        array = make_sublist_group(array, args.linebreak)
    else:
        array = [array,]
    
    return args.linebreak_string.join([args.separator_string.join(e) + args.separator_string for e in array])

if __name__ == "__main__":
    with open(args.filename, 'rb') as f:
        file_content = f.read()
    ret = do_convension(file_content, to_uppercase=args.force_uppercase)
    if args.newline:
        ret = ret + args.linebreak_string
    if args.output:
        with open(args.output, 'w') as f:
            f.write(ret)
    else:
        print(ret)