#!/usr/bin/python

"""
Convert config file to JSON 

The JSON contains array of single fields. There is no hierarchy.
Fields are being grouped in GUI with "category".
A group is shown if a minimum of one field is visible.

Hints
  capabilities is a dictionary 
    field:[true | false]
    optional comma separated multiple values

"""

import os
import sys
import getopt
import configparser
from io import StringIO
from pyparsing import alphas, alphanums, Word, Literal, Combine, Group, Forward, ZeroOrMore, delimitedList

__author__ = "Thomas Hooge"
__copyright__ = "Copyleft 2025, all rights reversed"
__version__ = "0.1"
__email__ = "thomas@hoogi.de"
__status__ = "Development"

infile = None
outfile = ""
force = False # overwrite outfile

# Variables for condition parsing
fieldname = Combine(Word(alphas, exact=1) + Word(alphanums, max=15))
fieldvalue = Word(alphanums, max=16)
equals = Literal("=")
in_op = Literal("IN")
and_op = Literal("AND")
or_op = Literal("OR")
comparison = Group(fieldname + equals + fieldvalue) \
           | Group(fieldname + in_op + delimitedList(fieldvalue))
expr = Forward()
expr <<= comparison + ZeroOrMore((and_op | or_op) + comparison)

def parse_condition(condition):
    try:
        result = expr.parseString(condition, parseAll=True)
    except Exception as e:
        return ""
    out = StringIO()
    andlist = []
    for token in result:
        # list: field = value or field IN value [, value ...]
        # str: AND, OR
        # combine ANDs and output reaching OR
        if type(token) == str:
            if token == "OR":
                andstr = ",\n".join(andlist)
                out.write(f'\t\t{{ {andstr} }},\n')
                andlist = []
        else:
            if token[1] == '=':
                andlist.append(f'"{token[0]}": "{token[2]}"')
            elif token[1] == 'IN':
                n = len(token) - 2
                if n == 1:
                    # no list, write single value
                    andlist.append(f'"{token[0]}": "{token[2]}"') 
                else:
                    # write list
                    inlist = '", "'.join(token[2:])
                    andlist.append(f'"{token[0]}": [ "{inlist}" ]\n')
                
    if len(andlist) > 0:
        out.write("\t\t{{ {} }}".format(", ".join(andlist)))
    return out.getvalue()

def create_flist():
    flist = []
    for field in config.sections():
        properties = [f'\t"name": "{field}"']
        for prop, val in config.items(field):
            if prop in ["label", "type", "default", "description", "category", "check"]:
                properties.append(f'\t"{prop}": "{val}"')
            elif prop == "capabilities":
                # multiple values possible
                capas = []
                for capa in val.split(','):
                    k, v = capa.split(':')
                    capas.append(f'"{k.strip()}":"{v.strip()}"')
                capalist = ','.join(capas)
                properties.append(f'\t"{prop}": {{{capalist}}}')
            elif prop in ("min", "max"):
                properties.append(f'\t"{prop}": {val}')
            elif prop == "list":
                entries = '", "'.join([x.strip() for x in val.split(',')])
                properties.append(f'\t"list": ["{entries}"]')
            elif prop == "dict":
                d = {}
                for l in val.splitlines():
                    if len(l) < 3:
                        continue
                    k, v = l.split(':')
                    d[k.strip()] = v.strip()
                lines = []
                for k,v in d.items():
                    lines.append(f'\t\t{{"l":"{v}","v":"{k}"}}')
                entries = ",\n".join(lines)
                properties.append(f'\t"list": [\n{entries}\n\t]')
            elif prop == "condition":
                jsoncond = parse_condition(val)
                properties.append(f'\t"{prop}": [\n{jsoncond}\n\t]\n')
            else:
                pass # ignore unknown stuff
        fieldprops = ",\n".join(properties)
        flist.append(f'{{\n{fieldprops}\n}}')
    return flist

def usage():
    print("{} v{}".format(os.path.basename(__file__), __version__))
    print(__copyright__)
    print()
    print("Command line options")
    print(" -c --config     config file name to use")
    print(" -j --json       json file name to generate")
    print(" -f              force overwrite of existing json file")
    print(" -h              show this help")
    print()

if __name__ == '__main__': 
    try:
        options, remainder = getopt.getopt(sys.argv[1:], 'c:j:fh', ['config=', 'json='])
    except getopt.GetoptError as e:
        print(e)
        sys.exit(1)
    filename = None
    for opt, arg in options:
        if opt in ('-c', '--config'):
            infile = arg
        elif opt in ('-j', '--json'):
            outfile = arg
        elif opt == '-h':
            usage()
            sys.exit(0)
        elif opt == '-f':
            force = True
    if not infile:
        print("Error: config filename missing")
        sys.exit(1)
    if not os.path.isfile(infile):
        print(f"Error: configuration file '{filename} not found'")
        sys.exit(1)
    if os.path.isfile(outfile) and not force:
        print(f"Error: json file '{outfile}' already exists")
        sys.exit(1)

    config = configparser.ConfigParser()
    ret = config.read(infile)
    if len(ret) == 0:
        print(f"ERROR: Config file '{infile}' not found")
        sys.exit(1)

    flist = create_flist()
    out = "[\n{}\n]\n".format(",\n".join(flist))
    if not outfile:
        # print to console
        print(out)
    else:
        # write to file
        with open(outfile, "w") as fh:
            fh.write(out)
