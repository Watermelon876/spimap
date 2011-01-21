
#include "common.h"
#include "spidir.h"
#include "logging.h"
#include "phylogeny.h"

namespace spidir {


const int dna2int [256] = 
{
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 9
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 19
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 29
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 39
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 49
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 59
    -1, -1, -1, -1, -1,  0, -1,  1, -1, -1,   // 69
    -1,  2, -1, -1, -1, -1, -1, -1, -1, -1,   // 79
    -1, -1, -1, -1,  3, -1, -1, -1, -1, -1,   // 89
    -1, -1, -1, -1, -1, -1, -1,  0, -1,  1,   // 99
    -1, -1, -1,  2, -1, -1, -1, -1, -1, -1,   // 109
    -1, -1, -1, -1, -1, -1,  3, -1, -1, -1,   // 119
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 129
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 139
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 149
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 159
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 169
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 179
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 189
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 199
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 209
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 219
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 229
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 239
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 249
    -1, -1, -1, -1, -1, -1                    // 255
};

const char *int2dna = "ACGT";

int dnatype[] = { 
    DNA_PURINE,     // A
    DNA_PRYMIDINE,  // C
    DNA_PURINE,     // G
    DNA_PRYMIDINE   // T
};    


// compute background frequencies
void computeBgfreq(int nseqs, char **seqs, float *bgfreq)
{
    // initialize with pseudo counts
    for (int i=0; i<4; i++)
        bgfreq[i] = 1.0;    

    int count = 4;

    for (int i=0; i<nseqs; i++) {
        for (char *c=seqs[i]; *c; c++) {
            int d = dna2int[(int) *c];
            if (d != -1) {
                count++;
                bgfreq[d] += 1.0;
            }
        }
    }

    // normalize
    for (int i=0; i<4; i++)
        bgfreq[i] /= count;
}


//=============================================================================
// Distance Matrix

// calculate the pairwise distances between sequences
// NOTE: simple version implemented first
void calcDistMatrix(int nseqs, int seqlen, char **seqs, float **distmat)
{
    for (int i=0; i<nseqs; i++) {
        distmat[i][i] = 0.0;
    
        for (int j=i+1; j<nseqs; j++) {
            float changes = 0.0;
            int len = 0;
            
            for (int k=0; k<seqlen; k++) {
                if (seqs[i][k] != '-' && seqs[j][k] != '-') {
                    len++;                
                    if (seqs[i][k] != seqs[j][k]) {
                        changes += 1;
                    }
                }
            }
            
            if (len > 0) {
                distmat[i][j] = changes / len;
                distmat[j][i] = changes / len;
            } else {
                distmat[i][j] = 1.0;
                distmat[j][i] = 1.0;
            }
        }
    }
}





bool writeDistMatrix(const char *filename, int ngenes, float **dists, 
                     string *names)
{
    FILE *stream = NULL;
    
    if ((stream = fopen(filename, "w")) == NULL) {
        fprintf(stderr, "cannot open '%s'\n", filename);
        return false;
    }
    
    // print number of genes
    fprintf(stream, "%d\n", ngenes);
    
    for (int i=0; i<ngenes; i++) {
        fprintf(stream, "%s ", names[i].c_str());
        
        for (int j=0; j<ngenes; j++) {
            fprintf(stream, "%f ", dists[i][j]);
        }
        fprintf(stream, "\n");
    }
    
    fclose(stream);
    return true;
}





//=============================================================================
// Spidir Parameters

SpidirParams *readSpidirParams(const char* filename)
{
    FILE *infile = NULL;
    
    if ((infile = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "cannot read file '%s'\n", filename);
        return NULL;
    }
    
    const int MAX_NAME = 51;
    float param1, param2;
    float alpha = -1, beta = -1;
    ExtendArray<float> mu(0, 40);
    ExtendArray<float> sigma(0, 40);
    ExtendArray<string> names(0, 40);
    
    char name[MAX_NAME];
    
    while (!feof(infile)) {
        int ntokens = fscanf(infile, "%50s\t%f\t%f", name, &param1, &param2);
        if (ntokens <= 0)
            break;
        if (ntokens != 3) {
            return NULL;
        }
        
        if (!strcmp(name, "baserate")) {
            alpha = param1;
            beta = param2;
        } else {
            names.append(name);
            mu.append(param1);
            sigma.append(param2);
        }
    }
    fclose(infile);    
    
    return new SpidirParams(names.size(), names, mu, sigma, alpha, beta);
}




// UNDER CONSTRUCTION
bool SpidirParams::order(SpeciesTree *stree)
{
    if (stree->nnodes != nsnodes) {
        printError("wrong number of parameters: %d %d\n", stree->nnodes, nsnodes);
        return false;
    }
    
    ExtendArray<Node*> nodeorder(0, stree->nnodes);
    getTreePreOrder(stree, &nodeorder);
        

    // make interior node names
    ExtendArray<int> inodes(stree->nnodes);
    
    int inodename = 1;
    for (int i=0; i<stree->nnodes; i++) {
        Node *node = nodeorder[i];
        if (node->isLeaf()) {
            inodes[node->name] = -1;
        } else {
            inodes[node->name] = inodename++;
        }
    }
    
    
    // loop through preordered nodes to construct permutation
    ExtendArray<int> invperm(0, stree->nnodes);
        
    for (int j=0; j<nsnodes; j++) {
        assert(invperm.size() == j);    
        for (int i=0; i<stree->nnodes; i++) {
            if (stree->nodes[i]->isLeaf()) {
                // if leaf, check if names match
                if (names[j] == stree->nodes[i]->longname) {
                    invperm.append(i);
                    break;
                }
            } else {
                if (atoi(names[j].c_str()) == inodes[i]) {
                    invperm.append(i);
                    break;
                }
            }
        }
    }
    

    
    ExtendArray<int> perm(stree->nnodes);
    
    // apply permutation
    invertPerm(invperm, perm, nsnodes);    
      
    permute(names, perm, nsnodes);
    permute(sp_alpha, perm, nsnodes);
    permute(sp_beta, perm, nsnodes);
    
    return true;
}


} // namespace spidir
