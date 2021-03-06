#ifndef __VARIANT_H
#define __VARIANT_H

#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <utility>
#include <stdlib.h>
#include <assert.h>
#include <stack>
#include <queue>
#include <set>
#include <sstream>
#include "split.h"
#include "join.h"
#include "tabixpp/tabix.hpp"
#include "smithwaterman/SmithWatermanGotoh.h"
#include "convert.h"
#include "multichoose/multichoose.h"

using namespace std;

namespace vcf {

class Variant;

enum VariantFieldType { FIELD_FLOAT = 0
                      , FIELD_INTEGER
                      , FIELD_BOOL
                      , FIELD_STRING
                      , FIELD_UNKNOWN
                      };

enum VariantFieldNumber { ALLELE_NUMBER = -2
                        , GENOTYPE_NUMBER = -1
                        };

const int INDEX_NONE = -1;
const int NULL_ALLELE = -1;

VariantFieldType typeStrToFieldType(string& typeStr);
ostream& operator<<(ostream& out, VariantFieldType type);


class VariantCallFile {

public:

    istream* file;
    Tabix* tabixFile;

    bool usingTabix;

    string header;
    string line; // the current line
    string fileformat;
    string fileDate;
    string source;
    string reference;
    string phasing;
    map<string, VariantFieldType> infoTypes;
    map<string, int> infoCounts;
    map<string, VariantFieldType> formatTypes;
    map<string, int> formatCounts;
    vector<string> sampleNames;
    bool parseSamples;
    bool _done;

    void updateSamples(vector<string>& newSampleNames);
    void addHeaderLine(string line);
    void removeInfoHeaderLine(string line);
    void removeGenoHeaderLine(string line);
    vector<string> infoIds(void);
    vector<string> formatIds(void);

    bool open(string filename) {
        vector<string> filenameParts = split(filename, ".");
        if (filenameParts.back() == "vcf") {
            return openFile(filename);
        } else if (filenameParts.back() == "gz" || filenameParts.back() == "bgz") {
            return openTabix(filename);
        }
    }

    bool openFile(string filename) {
        file = &_file;
        _file.open(filename.c_str(), ifstream::in);
        parsedHeader = parseHeader();
        return parsedHeader;
    }

    bool openTabix(string& filename) {
        usingTabix = true;
        tabixFile = new Tabix(filename);
        parsedHeader = parseHeader();
        return parsedHeader;
    }

    bool open(istream& stream) {
        file = &stream;
        parsedHeader = parseHeader();
        return parsedHeader;
    }

    bool open(ifstream& stream) {
        file = &stream;
        parsedHeader = parseHeader();
        return parsedHeader;
    }

    bool openForOutput(string& headerStr) {
        parsedHeader = parseHeader(headerStr);
        return parsedHeader;
    }

    VariantCallFile(void) : usingTabix(false), parseSamples(true) { }
    ~VariantCallFile(void) {
        if (usingTabix) {
            delete tabixFile;
        }
    }

    bool is_open(void) { return parsedHeader; }

    bool eof(void) { return _file.eof(); }

    bool done(void) { return _done; }

    bool parseHeader(string& headerStr);

    bool parseHeader(void);

    bool getNextVariant(Variant& var);

    bool setRegion(string region);
    bool setRegion(string seq, long int start, long int end = 0);

private:
    bool firstRecord;
    bool usingFile;
    ifstream _file;
    bool parsedHeader;

};

class VariantAllele {
    friend ostream& operator<<(ostream& out, VariantAllele& var);
    friend bool operator<(const VariantAllele& a, const VariantAllele& b);
public:
    string ref;
    string alt;
    string repr;
    unsigned long position;
    VariantAllele(string r, string a, unsigned long p)
        : ref(r), alt(a), position(p)
    {
        stringstream s;
        s << position << ":" << ref << "/" << alt;
        repr = s.str();
    }
};


class Variant {

    friend ostream& operator<<(ostream& out, Variant& var);

public:

    VariantCallFile* vcf;
    string sequenceName;
    unsigned long position;
    string id;
    string ref;
    vector<string> alt;      // a list of all the alternate alleles present at this locus
    vector<string> alleles;  // a list all alleles (ref + alt) at this locus
                             // the indicies are organized such that the genotype codes (0,1,2,.etc.)
                             // correspond to the correct offest into the allelese vector.
                             // that is, alleles[0] = ref, alleles[1] = first alternate allele, etc.
    map<string, int> altAlleleIndexes;  // reverse lookup for alleles
    map<string, vector<VariantAllele> > parsedAlternates(bool includePreviousBaseForIndels = false);

    map<string, string> extendedAlternates(long int newPosition, long int length);

    string originalLine; // the literal of the record, as read
    // TODO
    // the ordering of genotypes for the likelihoods is given by: F(j/k) = (k*(k+1)/2)+j
    // vector<pair<int, int> > genotypes;  // indexes into the alleles, ordered as per the spec
    string filter;
    double quality;
    VariantFieldType infoType(string& key);
    map<string, vector<string> > info;  // vector<string> allows for lists by Genotypes or Alternates
    map<string, bool> infoFlags;
    VariantFieldType formatType(string& key);
    vector<string> format;
    map<string, map<string, vector<string> > > samples;  // vector<string> allows for lists by Genotypes or Alternates
    vector<string> sampleNames;
    vector<string> outputSampleNames;

    //void addInfoInt(string& tag, int value);
    //void addInfoFloat(string& tag, double value);
    //void addInfoString(string& tag, string& value);

    void removeAlt(string& altallele);

    // metrics describing the number and type of the genotypes
    // present for this variant.
    unsigned int num_hom_ref;     // how many hom_ref gts are there?
    unsigned int num_het;         // how many het gts are there?
    unsigned int num_hom_alt;     // how many hom_alt gts are there?
    unsigned int num_unknown;     // how many unknown gts are there?
    unsigned int num_valid;       // how many valid (i.e., !unknown) gts are there?
    unsigned int num_alt_alleles; // how many alternate alleles are there?
    
    vector<string>  gts;    // vector of genotypes (AA AG, etc.) for each sample (in order)
    vector<int> gt_types;   // vector of genotypes (0, 1, 2, etc.) for each sample (in order)
    vector<int> gt_phases; // vector of booleans described the phase of each gt for each sample (in order)

public:

    Variant() { }

    Variant(VariantCallFile& v)
        : sampleNames(v.sampleNames)
        , outputSampleNames(v.sampleNames)
        , vcf(&v)
        , num_hom_ref(0)
        , num_het(0)
        , num_hom_alt(0)
        , num_unknown(0)
        , num_alt_alleles(0)
        , num_valid (0)
        , gts()
        , gt_types()
        , gt_phases()
    {
        gts.reserve(1000);
        gt_types.reserve(1000);
        gt_phases.reserve(1000);
    }

    string repr(void); // ARQ: needed for __repr__ support in pyvcflib

    void setVariantCallFile(VariantCallFile& v);
    void setVariantCallFile(VariantCallFile* v);

    void parse(string& line, bool parseSamples = true);
    void addFilter(string& tag);
    bool getValueBool(string& key, string& sample, int index = INDEX_NONE);
    double getValueFloat(string& key, string& sample, int index = INDEX_NONE);
    string getValueString(string& key, string& sample, int index = INDEX_NONE);
    bool getSampleValueBool(string& key, string& sample, int index = INDEX_NONE);
    double getSampleValueFloat(string& key, string& sample, int index = INDEX_NONE);
    string getSampleValueString(string& key, string& sample, int index = INDEX_NONE);
    bool getInfoValueBool(string& key, int index = INDEX_NONE);
    double getInfoValueFloat(string& key, int index = INDEX_NONE);
    string getInfoValueString(string& key, int index = INDEX_NONE);
    void printAlt(ostream& out);      // print a comma-sep list of alternate alleles to an ostream
    void printAlleles(ostream& out);  // print a comma-sep list of *all* alleles to an ostream
    int getAltAlleleIndex(string& allele);
    void updateAlleleIndexes(void);
    void addFormatField(string& key);
    void setOutputSampleNames(vector<string>& outputSamples);
    map<pair<int, int>, int> getGenotypeIndexesDiploid(void);
    int getNumSamples(void);
    int getNumValidGenotypes(void);
    // TODO
    //void setInfoField(string& key, string& val);
    
    // ARQ methods
    // return the alternate allele frequency. 
    // -1.0 if more than one alternate allele
    float getAAF(void);
    // return pi_hat (nucleotide diversity)
    // -1.0 if more than one alternate allele
    // Derived from Population Genetics: A Concise Guide,
    // 2nd ed., p.45, John Gillespie
    float getNucleotideDiversity(void);
    // tests for SNPs and SNP types
    bool isSNP(void);
    bool isTransition(void);
    // tests for INDELs and INDEL types
    bool isINDEL(void);
    bool isDeletion(void);
    
    
private:
    string lastFormat;
    void tabulateGenotype(string numeric_genotype);
};

// from BamTools
// RuleToken implementation

struct RuleToken {

    // enums
    enum RuleTokenType { OPERAND = 0
                       , NUMBER
                       , BOOLEAN_VARIABLE
                       , NUMERIC_VARIABLE
                       , STRING_VARIABLE
                       , AND_OPERATOR
                       , OR_OPERATOR
                       , ADD_OPERATOR
                       , SUBTRACT_OPERATOR
                       , MULTIPLY_OPERATOR
                       , DIVIDE_OPERATOR
                       , NOT_OPERATOR
                       , EQUAL_OPERATOR
                       , GREATER_THAN_OPERATOR
                       , LESS_THAN_OPERATOR
                       , LEFT_PARENTHESIS
                       , RIGHT_PARENTHESIS
                       };

    // constructor
    RuleToken(string token, map<string, VariantFieldType>& variables);
    RuleToken(void) 
        : type(BOOLEAN_VARIABLE)
        , state(false)
    { }

    // data members
    RuleTokenType type;
    string value;

    double number;
    string str;
    bool state;

    bool isVariable; // if this is a variable
    //bool isEvaluated; // when we evaluate variables

    RuleToken apply(RuleToken& other);

};

inline int priority(const RuleToken& token) {
    switch ( token.type ) {
        case ( RuleToken::MULTIPLY_OPERATOR )     : return 8;
        case ( RuleToken::DIVIDE_OPERATOR )       : return 8;
        case ( RuleToken::ADD_OPERATOR )          : return 7;
        case ( RuleToken::SUBTRACT_OPERATOR )     : return 7;
        case ( RuleToken::NOT_OPERATOR )          : return 6;
        case ( RuleToken::EQUAL_OPERATOR )        : return 5;
        case ( RuleToken::GREATER_THAN_OPERATOR ) : return 5;
        case ( RuleToken::LESS_THAN_OPERATOR )    : return 5;
        case ( RuleToken::AND_OPERATOR )          : return 4;
        case ( RuleToken::OR_OPERATOR )           : return 3;
        case ( RuleToken::LEFT_PARENTHESIS )      : return 0;
        case ( RuleToken::RIGHT_PARENTHESIS )     : return 0;
        default: cerr << "invalid token type" << endl; exit(1);
    }
}

inline bool isRightAssociative(const RuleToken& token) {
    return (token.type == RuleToken::NOT_OPERATOR ||
            token.type == RuleToken::LEFT_PARENTHESIS);
}

inline bool isLeftAssociative(const RuleToken& token) {
    return !isRightAssociative(token);
}

inline bool isLeftParenthesis(const RuleToken& token) {
    return ( token.type == RuleToken::LEFT_PARENTHESIS );
}

inline bool isRightParenthesis(const RuleToken& token) {
    return ( token.type == RuleToken::RIGHT_PARENTHESIS );
}

inline bool isOperand(const RuleToken& token) {
    return ( token.type == RuleToken::OPERAND || 
             token.type == RuleToken::NUMBER ||
             token.type == RuleToken::NUMERIC_VARIABLE ||
             token.type == RuleToken::STRING_VARIABLE ||
             token.type == RuleToken::BOOLEAN_VARIABLE
           );
}

inline bool isOperator(const RuleToken& token) {
    return ( token.type == RuleToken::AND_OPERATOR ||
             token.type == RuleToken::OR_OPERATOR  ||
             token.type == RuleToken::NOT_OPERATOR ||
             token.type == RuleToken::EQUAL_OPERATOR ||
             token.type == RuleToken::GREATER_THAN_OPERATOR ||
             token.type == RuleToken::LESS_THAN_OPERATOR ||
             token.type == RuleToken::MULTIPLY_OPERATOR ||
             token.type == RuleToken::DIVIDE_OPERATOR ||
             token.type == RuleToken::ADD_OPERATOR ||
             token.type == RuleToken::SUBTRACT_OPERATOR
             );
}

inline bool isOperatorChar(const char& c) {
    return (c == '!' ||
            c == '&' ||
            c == '|' ||
            c == '=' ||
            c == '>' ||
            c == '<' ||
            c == '*' ||
            c == '/' ||
            c == '+' ||
            c == '-');
}

inline bool isParanChar(const char& c) {
    return (c == '(' || c == ')');
}

inline bool isNumeric(const RuleToken& token) {
    return token.type == RuleToken::NUMERIC_VARIABLE;
}

inline bool isString(const RuleToken& token) {
    return token.type == RuleToken::STRING_VARIABLE;
}

inline bool isBoolean(const RuleToken& token) {
    return token.type == RuleToken::BOOLEAN_VARIABLE;
}

inline bool isVariable(const RuleToken& token) {
    return isNumeric(token) || isString(token) || isBoolean(token);
}

void tokenizeFilterSpec(string& filterspec, stack<RuleToken>& tokens, map<string, VariantFieldType>& variables);


class VariantFilter {

public:

    enum VariantFilterType { SAMPLE = 0,
                             RECORD };

    string spec;
    queue<RuleToken> tokens; // tokens, infix notation
    queue<RuleToken> rules;  // tokens, prefix notation
    VariantFilterType type;
    VariantFilter(string filterspec, VariantFilterType filtertype, map<string, VariantFieldType>& variables);
    bool passes(Variant& var, string& sample); // all alts pass
    bool passes(Variant& var, string& sample, string& allele);
    void removeFilteredGenotypes(Variant& var);

};


// genotype manipulation

// TODO
//map<string, int> decomposeGenotype(string& genotype);

map<int, int> decomposeGenotype(string& genotype);

string genotypeToString(map<int, int>& genotype);

bool isHet(map<int, int>& genotype);

bool isHom(map<int, int>& genotype);

bool hasNonRef(map<int, int>& genotype);

bool isHomRef(map<int, int>& genotype);

bool isHomNonRef(map<int, int>& genotype);

bool isNull(map<int, int>& genotype);

int ploidy(map<int, int>& genotype);

// return a number describing the count of alternate alleles
// for a given genotype. 0 = homref, 1 = het, 
//                       2 = homnonref, -1 m= unknown
int genotypeValue(string & genotype);

string unionInfoHeaderLines(string& s1, string& s2);

vector<string>& unique(vector<string>& strings);

} // end namespace VCF

#endif
