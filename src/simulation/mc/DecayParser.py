#!/usr/bin/python2
# coding: utf8

# generates Database for A2Channelmanager from textfile

from __future__ import print_function
import sys, os
import itertools

def init():
    if not len(sys.argv) == 3:
        print()
        print("Usage:    ",__file__, "<infile> <outfile>")
        print()
        print("   Parses cross-section-data from from <infile> and generates x-section-functions for the A2-Cocktail.")
        print("   Syntax for <infile>:")
        print("")
        print("     # comment")
        print("     Particle meson1a [meson1b] [meson1c]")
        print("     PhotonEnergy cross-section")
        print("        ⋮")
        print("     [Particle meson2a [meson2b] [meson2c]")
        print("     PhotonEnergy cross-section")
        print("        ⋮ ]")
        print("      ⋮")
        print()
        sys.exit(1)
    #cwd = os.path.dirname(__file__)
   
    decayfilename  = sys.argv[1]   #   default: decayfilename  = "14001Decays.dat"
    infile = os.path.abspath(decayfilename)
    
    headerfilename = sys.argv[2]   #   default: headerfilename = "chgen.h"
    outfile= os.path.abspath(headerfilename)

    identifier="Particle"
    functionName="LoadStdDecays"

    return ( identifier, infile, outfile, headerfilename,functionName)

#=== generate code to initalize database:
def genText(particle,numbers):
    rstring = ''.join(['      pdata.Energies = std::vector<double> {\n         ',
                       ', '.join(numbers[0]) + '\n      };\n',
                       '      pdata.Xsections = std::vector<double> {\n         ',
                       ', '.join(numbers[1]) + '\n      };\n',
                       '      XList["'+particle+'"] = pdata;\n\n'])
    return rstring
    
def main():

    identifier, infile, outfile, headerfilename, functionName = init()

    #load file and take away comments
    with open(infile) as cfile:
        lines = [ (''.join(itertools.takewhile(lambda x: x != '#',line))).split() for line in cfile.readlines() ]


    #remove all empty lines:
    lines = list(itertools.ifilter(lambda x: x != [],lines))

    # ----  sanaty checks:
    # should start with Particle description:
    if lines[0][0] != identifier:
        print("Error parsing file: no Particle description")


    headerfile = '\n'.join([
        '/**',
        ' * @file ' + headerfilename,
        ' *',
        ' *  !!! Do not change, autogenerated',
        ' *  !!! by DecayParser.',
        ' *',
        ' */',
        '',
        '#include "A2Channels.h"',
        '#include <vector>',
        '',
#        'using namespace ant::simulation::mc;',
        '',
        'ant::simulation::mc::XsecList ' + functionName + '()',
        '{',
        '      ant::simulation::mc::ParticleData pdata;',
        '      ant::simulation::mc::XsecList XList;',
        '',
        '' ] )

    for i in range(len(lines)):
        if lines[i][0] == "Particle":
            particle = ' '.join(lines[i][1:])
            numbers = list(itertools.takewhile(lambda x: x[0] != "Particle",lines[i+1:]))
            headerfile = headerfile + genText(particle, zip(*numbers)  )

    headerfile = headerfile + '      return XList;\n}'

    with open(outfile,"w") as ofile:
        print(headerfile,file=ofile)


if __name__ == '__main__':
    main()
