// Copyright © 2015, Battelle National Biodefense Institute (BNBI);
// all rights reserved. Authored by: Brian Ondov, Todd Treangen,
// Sergey Koren, and Adam Phillippy
//
// See the LICENSE.txt file included with this software for license information.

#include "CommandSketch.h"
#include "Sketch.h"
#include <iostream>

using namespace::std;

CommandSketch::CommandSketch()
: Command()
{
    name = "sketch";
    summary = "Create sketches (reduced representations for fast operations).";
    description = "Create a sketch file, which is a reduced representation of a sequence or set of sequences (based on min-hashes) that can be used for fast distance estimations. Input can be fasta or fastq files (gzipped or not), and \"-\" can be given to read from standard input. Input files can also be files of file names (see -l). For output, one sketch file will be generated, but it can have multiple sketches within it, divided by sequences or files (see -i). By default, the output file name will be the first input file with a '.msh' extension, or 'stdin.msh' if standard input is used (see -o).";
    argumentString = "fast(a|q)[.gz] ...";
    
    useOption("help");
    addOption("list", Option(Option::Boolean, "l", "Input", "List input. Each file contains a list of sequence files, one per line.", ""));
    addOption("prefix", Option(Option::File, "o", "Output", "Output prefix (first input file used if unspecified). The suffix '.msh' will be appended.", ""));
    useOption("kmer");
    //useOption("windowed");
    //useOption("window");
    useOption("sketchSize");
    //useOption("verbose");
    //useOption("silent");
    useOption("individual");
    useOption("warning");
    useOption("reads");
    useOption("minCov");
    useOption("targetCov");
    //useOption("illumina");
    //useOption("pacbio");
    //useOption("nanopore");
    useOption("noncanonical");
    useOption("threads");
}

int CommandSketch::run() const
{
    if ( arguments.size() == 0 || options.at("help").active )
    {
        print();
        return 0;
    }
    
    int verbosity = 1;//options.at("silent").active ? 0 : options.at("verbose").active ? 2 : 1;
    bool list = options.at("list").active;
    
    Sketch::Parameters parameters;
    
    parameters.kmerSize = options.at("kmer").getArgumentAsNumber();
    parameters.minHashesPerWindow = options.at("sketchSize").getArgumentAsNumber();
    parameters.concatenated = ! options.at("individual").active;
    parameters.noncanonical = options.at("noncanonical").active;
    parameters.reads = options.at("reads").active;
    parameters.minCov = options.at("minCov").getArgumentAsNumber();
    parameters.targetCov = options.at("targetCov").getArgumentAsNumber();
    parameters.windowed = false;//options.at("windowed").active;
    parameters.windowSize = 0;//options.at("window").getArgumentAsNumber();
    parameters.warning = options.at("warning").getArgumentAsNumber();
    parameters.parallelism = options.at("threads").getArgumentAsNumber();
    
    if ( options.at("minCov").active )
    {
        parameters.reads = true;
    }
    
    if ( parameters.reads && ! parameters.concatenated )
    {
        cerr << "ERROR: The option " << options.at("individual").identifier << " cannot be used with " << options.at("unique").identifier << "." << endl;
        return 1;
    }
    
    if ( parameters.concatenated && parameters.windowed )
    {
        cerr << "ERROR: " << options.at("concat").identifier << " and " << options.at("windowed").identifier << " are incompatible." << endl;
        return 1;
    }
    
    for ( int i = 0; i < arguments.size(); i++ )
    {
        if ( false && hasSuffix(arguments[i], suffixSketch) )
        {
            cerr << "ERROR: " << arguments[i] << " looks like it is already sketched.\n";
            exit(1);
        }
    }
    
    Sketch sketch;
    
    uint64_t lengthThreshold = (parameters.warning * pow(parameters.protein ? 20 : 4, parameters.kmerSize)) / (1. - parameters.warning);
    uint64_t lengthMax;
    double randomChance;
    int kMin;
    string lengthMaxName;
    int warningCount = 0;
    
    vector<string> files;
    
    for ( int i = 0; i < arguments.size(); i++ )
    {
        if ( list )
        {
            splitFile(arguments[i], files);
        }
        else
        {
            files.push_back(arguments[i]);
        }
    }
    
    sketch.initFromFiles(files, parameters, verbosity);
    
	for ( int i = 0; i < sketch.getReferenceCount(); i++ )
	{
		uint64_t length = sketch.getReference(i).length;
		
		if ( length > lengthThreshold )
		{
			if ( warningCount == 0 || length > lengthMax )
			{
				lengthMax = length;
				lengthMaxName = sketch.getReference(i).name;
				randomChance = sketch.getRandomKmerChance(i);
				kMin = sketch.getMinKmerSize(i);
			}
			
			warningCount++;
		}
	}
	
    string prefix;
    
    if ( options.at("prefix").argument.length() > 0 )
    {
        prefix = options.at("prefix").argument;
    }
    else
    {
        if ( arguments[0] == "-" )
        {
            prefix = "stdin";
        }
        else
        {
            prefix = arguments[0];
        }
    }
    
    string suffix = parameters.windowed ? suffixSketchWindowed : suffixSketch;
    
    if ( ! hasSuffix(prefix, suffix) )
    {
        prefix += suffix;
    }
    
    cerr << "Writing to " << prefix << "..." << endl;
    
    sketch.writeToCapnp(prefix.c_str());
    
    if ( warningCount > 0 && ! parameters.reads )
    {
    	sketch.warnKmerSize(lengthMax, lengthMaxName, randomChance, kMin, warningCount);
    }
    
    return 0;
}
