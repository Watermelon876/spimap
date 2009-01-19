//=============================================================================
//  SPIDIR - tree search



#include "common.h"
#include "Matrix.h"
#include "phylogeny.h"
#include "likelihood.h"
#include "parsimony.h"
#include "search.h"
#include "mldist.h"


namespace spidir {



//=============================================================================
// Nearest Neighbor Interchange Topology Proposal

/*

    Proposes a new tree using Nearest Neighbor Interchange
       
       Branch for NNI is specified by giving its two incident nodes (node1 and 
       node2).  Change specifies which  subtree of node1 will be swapped with
       the uncle.  See figure below.

         node2
        /     \
      nodeb    node1
               /  \
         nodea     * 

*/
void proposeNni(Tree *tree, Node *nodea, Node *nodeb)
{
    Node *node1 = nodea->parent;
    Node *node2 = nodeb->parent;
    
    // assert that node1 and node2 are incident to the same branch
    assert(node1->parent == node2 ||
           node2->parent == node1);
    
    // find child indexes
    int a = (node1->children[0] == nodea) ? 0 : 1;
    assert(node1->children[a] == nodea);

    int b = (node2->children[0] == nodeb) ? 0 : 1;
    assert(node2->children[b] == nodeb);
    
    // swap parent pointers
    nodea->parent = node2;
    nodeb->parent = node1;
    
    // swap child pointers
    node2->children[b] = nodea;
    node1->children[a] = nodeb;
}


void proposeRandomNni(Tree *tree, Node **a, Node **b)
{
    // find edges for NNI
    int choice;
    do {
        choice = irand(tree->nnodes);
    } while (tree->nodes[choice]->isLeaf() || 
             tree->nodes[choice]->parent == NULL);
    
    Node *node1 = tree->nodes[choice];
    Node *node2 = tree->nodes[choice]->parent;
    *a = node1->children[irand(2)];
    *b = (node2->children[0] == node1) ? node2->children[1] :
                                         node2->children[0];
}


//=============================================================================
// Subtree pruning and regrafting (SPR)


/*
    a = subtree
    e = newpos
    
    BEFORE
            ....
        f         d
       /           \
      c             e
     / \           ...
    a   b
   ... ...

    AFTER

        f         d
       /           \
      b             c
     ...           / \
                  a   e
                 ... ...

    Requirements:
    1. a (subtree) is not root or children of root
    2. e (newpos) is not root, a, descendant of a, c (parent of a), or 
       b (sibling of a)
    3. tree is binary

*/
void proposeSpr(Tree *tree, Node *subtree, Node *newpos)
{
    Node *a = subtree;
    Node *e = newpos;
    
    Node *c = a->parent;
    Node *f = c->parent;
    const int bi = (c->children[0] == a) ? 1 : 0;
    Node *b = c->children[bi];
    const int ci = (f->children[0] == c) ? 0 : 1;
    Node *d = e->parent;
    const int ei = (d->children[0] == e) ? 0 : 1;

    d->children[ei] = c;
    c->children[bi] = e;
    f->children[ci] = b;
    b->parent = f;
    c->parent = d;
    e->parent = c;
}

/*
    What if e == f  (also equivalent to NNI) this is OK

    BEFORE
    
          d
         / \
        e  ...
       / \
      c  ...         
     / \           
    a   b
   ... ...

    AFTER
          d
         / \
        c
       / \
      a   e
     ... / \
        b  ...
       ...
       
  What if d == f  (also equivalent to NNI) this is OK
  
    BEFORE
          
        f
       / \
      c   e
     / \  ...
    a   b
   ... ...

    AFTER
          
        f
       / \
      b   c  
     ... / \ 
        a   e
       ... ...  
*/



/*
    Requirements:
    1. a (subtree) is not root or children of root
    2. e (newpos) is not root, a, descendant of a, c (parent of a), or 
       b (sibling of a)
    3. tree is binary
*/
void proposeRandomSpr(Tree *tree, Node **subtree, Node **newpos)
{
    assert(tree->nnodes >= 5);

    // find subtree (a) to cut off (any node that is not root or child of root)
    int choice;
    do {
        choice = irand(tree->nnodes);
    } while (tree->nodes[choice]->parent == NULL ||
             tree->nodes[choice]->parent->parent == NULL);
    Node *a = tree->nodes[choice];
    *subtree = a;
    
    // find sibling (b) of a
    Node *c = a->parent;
    const int bi = (c->children[0] == a) ? 1 : 0;
    Node *b = c->children[bi];
    
    // choose newpos (e)
    Node *e = NULL;
    do {
        choice = irand(tree->nnodes);
        e = tree->nodes[choice];
        
        // test if e is a valid choice
        if (e->parent == NULL || e == a || e == c || e == b)
            continue;
        
        // also test if e is a descendent of a
        bool under_a = false;
        for (Node *ptr = e->parent; ptr != NULL; ptr = ptr->parent) {
            if (ptr == a) {
                under_a = true;
                break;
            }
        }            
        
        if (under_a)
            continue;
        
        break;
    } while (true);
    *newpos = e;
}



//=============================================================================
// NNI Proposer

NniProposer::NniProposer(SpeciesTree *stree, int *gene2species,
                         int niter) :
    nodea(NULL),
    nodeb(NULL),
    nodec(NULL),
    noded(NULL),
    oldroot1(NULL),
    oldroot2(NULL),
    stree(stree),
    gene2species(gene2species),
    niter(niter),
    iter(0),
    correctTree(NULL),
    correctSeen(false)
{}
    
    
void NniProposer::propose(Tree *tree)
{
    const float rerootProb = 1.0;
    const float doubleNniProb = 0.0; // NO double NNI
    
    // increase iteration
    iter++;
    
    nodea = nodeb = nodec = noded = NULL;

    // propose new tree
    proposeRandomNni(tree, &nodea, &nodeb);
    proposeNni(tree, nodea, nodeb);

    if (frand() < doubleNniProb) {
        //printLog(LOG_MEDIUM, "search: double NNI\n");
        proposeRandomNni(tree, &nodec, &noded);
        proposeNni(tree, nodec, noded);
    }
    
    // reroot tree if stree is given
    if (frand() < rerootProb) {
        oldroot1 = tree->root->children[0];
        oldroot2 = tree->root->children[1];
        
        if (stree != NULL) {
            reconRoot(tree, stree, gene2species);
        }
    } else {
        oldroot1 = NULL;
        oldroot2 = NULL;
    }
    
    if (correctTree) {
        if (tree->sameTopology(correctTree))
            correctSeen = true;
    }

    //assert(tree->assertTree());
}

void NniProposer::revert(Tree *tree)
{
    // reject, undo topology change

    if (oldroot1) {
        if (oldroot1->parent == oldroot2)
            tree->reroot(oldroot1);
        else
            tree->reroot(oldroot2);
    }
    
    //printf("NNI %d %d %d %d\n", node1->name, node1->parent->name, 
    //       node2->name, node2->nchildren);
    if (nodec)
        proposeNni(tree, nodec, noded);
    if (nodea)
        proposeNni(tree, nodea, nodeb);
}


bool NniProposer::more()
{
    return iter < niter;
}



//=============================================================================
// SPR + NNI Proposer

SprNniProposer::SprNniProposer(SpeciesTree *stree, int *gene2species,
                               int niter, float sprRatio) :
    NniProposer(stree, gene2species, niter),
    sprRatio(sprRatio)
{
}
    
    
void SprNniProposer::propose(Tree *tree)
{
    const float noChange = 0.00;
    float choice = frand();
    
    if (correctTree) {
        if (tree->sameTopology(correctTree))
            correctSeen = true;
    }
    
    if (choice < noChange) {
        lastPropose = PROPOSE_NONE;
        iter++; // increase iteration
        
    } if (choice < noChange + (1.0 - noChange) * sprRatio) {
        lastPropose = PROPOSE_SPR;
        
        // increase iteration
        iter++;
        
        proposeRandomSpr(tree, &nodea, &nodeb);
        
        // remember sibling of nodea
        const Node *p = nodea->parent;
        nodec = (p->children[0] == nodea) ? p->children[1] : p->children[0];
        
        proposeSpr(tree, nodea, nodeb);
        
    } else {
        lastPropose = PROPOSE_NNI;
        NniProposer::propose(tree);
    }
}

void SprNniProposer::revert(Tree *tree)
{
    if (lastPropose == PROPOSE_NONE)
        return;
    else if (lastPropose == PROPOSE_SPR) {
        proposeSpr(tree, nodea, nodec);
    
    } else if (lastPropose == PROPOSE_NNI) {
        NniProposer::revert(tree);
    } else {
        assert(false);
    }
}

//=============================================================================
// Dup/Loss proposer

DupLossProposer::DupLossProposer(TopologyProposer *proposer, 
                                 SpeciesTree *stree, int *gene2species,
                                 float dupprob, float lossprob,
                                 int quickiter, int niter) :
    proposer(proposer),
    quickiter(quickiter),
    niter(niter),
    iter(0),
    correctTree(NULL),
    correctSeen(false),
    stree(stree),
    gene2species(gene2species),
    dupprob(dupprob),
    lossprob(lossprob),
    recon(0),
    events(0),
    oldtop(NULL)
{
}

void DupLossProposer::propose(Tree *tree)
{
    iter++;
    
    // record correct tree if we visit it
    if (correctTree) {
        if (tree->sameTopology(correctTree))
            correctSeen = true;
    }
    
    
    // do simple proposal if dup/loss probs are disabled
    if (dupprob < 0.0 || lossprob < 0.0 || quickiter <= 1) {
        proposer->propose(tree);
        return;
    }
    
    // save old topology
    oldtop = tree->copy();
    
    // do simple proposal
    if (frand() < .2) {
        proposer->propose(tree);
        return;
    }
    
    // recon tree to species tree
    recon.ensureSize(tree->nnodes);
    events.ensureSize(tree->nnodes);
    recon.setSize(tree->nnodes);
    events.setSize(tree->nnodes);
    reconcile(tree, stree, gene2species, recon);
    labelEvents(tree, recon, events);

    
    ExtendArray<Tree*> trees(0, quickiter);
    ExtendArray<float> logls(0, quickiter);
    
    float sum = -INFINITY;
    
    // make many subproposals
    for (int i=0; i<quickiter; i++) {
        proposer->propose(tree);
        
        reconcile(tree, stree, gene2species, recon);
        labelEvents(tree, recon, events);
    
        int addedNodes = addImpliedSpecNodes(tree, stree, recon, events);
        float logl = birthDeathTreeQuickPrior(tree, stree, recon, events, 
                                              dupprob, lossprob);
        sum = logadd(sum, logl);
        //printf("qiter %d %f\n", i, logl);
        removeImpliedSpecNodes(tree, addedNodes);
        recon.setSize(tree->nnodes);
        events.setSize(tree->nnodes);
        
        Tree *tree2 = tree->copy();
        
        // save tree and logl
        trees.append(tree2);
        logls.append(logl);
        
        //if (isLogLevel(LOG_MEDIUM)) {
        //    printLog(LOG_MEDIUM, "qiter %d %f\n", i, logl);
        //    displayTree(tree2, getLogFile());
        //}
        
        proposer->revert(tree);
    }    
    
    // propose one of the subproposals 
    float choice = frand();
    float partsum = -INFINITY;
    
    for (int i=0; i<quickiter; i++) {
        partsum = logadd(partsum, logls[i]);
    
        //printf("part %d %f (%f)\n", i, expf(partsum - sum), choice);
        
        if (choice < expf(partsum - sum)) {
            // propose tree i
            // trees[i];
            //printLog(LOG_MEDIUM, "choose %d\n", i);
            tree->setTopology(trees[i]);
            
            //displayTree(trees[i]);
            //displayTree(tree);
            
            break;
        }
        
        
    }

    
    // clean up subproposals
    for (int i=0; i<quickiter; i++)
        delete trees[i];
    
    //proposer->propose(tree);
}

void DupLossProposer::revert(Tree *tree)
{
    // do simple proposal if dup/loss probs are disabled
    if (dupprob < 0.0 || lossprob < 0.0 || quickiter <= 1) {
        proposer->revert(tree);
        return;
    }
    
    //printf("set oldtop\n");
    tree->setTopology(oldtop);
    delete oldtop;
}


//=============================================================================
// Fitting branch lengths

ParsimonyFitter::ParsimonyFitter(int nseqs, int seqlen, char **seqs) :
    nseqs(nseqs),
    seqlen(seqlen),
    seqs(seqs)
{}


float ParsimonyFitter::findLengths(Tree *tree)
{
    parsimony(tree, nseqs, seqs);
    return 0.0;
}



HkyFitter::HkyFitter(int nseqs, int seqlen, char **seqs, 
                     float *bgfreq, float tsvratio, int maxiter,
                     bool useLogl) :
    nseqs(nseqs),
    seqlen(seqlen),
    seqs(seqs),
    bgfreq(bgfreq),
    tsvratio(tsvratio),
    maxiter(maxiter),
    useLogl(useLogl)
{}

float HkyFitter::findLengths(Tree *tree)
{ 
    float logl = findMLBranchLengthsHky(tree, nseqs, seqs, bgfreq, 
                                        tsvratio, maxiter);
    if (useLogl)
        return logl;
    else
        return 0.0;
}



float SpidirSample::findLengths(Tree *tree)
{
    // reconcile gene tree to species tree
    int recon[tree->nnodes];
    int events[tree->nnodes];

    reconcile(tree, stree, gene2species, recon);
    labelEvents(tree, recon, events);
    
    generateBranchLengths(tree, stree, recon, events, params);
    return 0.0;
}


float HkySpidirSample::findLengths(Tree *tree)
{ 
    // reconcile gene tree to species tree
    int recon[tree->nnodes];
    int events[tree->nnodes];

    reconcile(tree, stree, gene2species, recon);
    labelEvents(tree, recon, events);
    
    float logl = -INFINITY;
    
    for (int i=0; i<maxiter; i++) {
        generateBranchLengths(tree, stree, recon, events, params);
        logl = logadd(logl, calcHkySeqProb(tree, nseqs, seqs, bgfreq, tsvratio));
    }
    logl -= log(maxiter);
    
    return logl;
}


BirthDeathFitter::BirthDeathFitter(int nseqs, int seqlen, char **seqs, 
                                   float *bgfreq, float tsvratio,
                                   SpeciesTree *stree, int *gene2species, 
                                   float birthRate, float deathRate) :
    nseqs(nseqs),
    seqlen(seqlen), 
    seqs(seqs), 
    bgfreq(bgfreq), 
    tsvratio(tsvratio),
    stree(stree),
    gene2species(gene2species),
    birthRate(birthRate),
    deathRate(deathRate)
{}


float BirthDeathFitter::findLengths(Tree *tree)
{
    // reconcile gene tree to species tree
    ExtendArray<int> recon(tree->nnodes);
    ExtendArray<int> events(tree->nnodes);
    reconcile(tree, stree, gene2species, recon);
    labelEvents(tree, recon, events);
    
    int addedNodes = addImpliedSpecNodes(tree, stree, recon, events);
    
    float qbdlogl = birthDeathTreeQuickPrior(tree, stree, recon, events, 
                                             birthRate, deathRate);  
    
    sampleDupTimes(tree, stree, recon, events, birthRate, deathRate);
    
    //displayTree(tree, stdout, 100);
    
    // TODO: I could add multiple samples here
    
    float bdlogl = birthDeathTreeQuickPrior(tree, stree, recon, events, 
                                            birthRate, deathRate);
    
    removeImpliedSpecNodes(tree, addedNodes);
    
    float logl = findMLBranchLengthsHky(tree, nseqs, seqs, bgfreq, 
                                        tsvratio, 10);
    
    //float logl = calcHkySeqProb(tree, nseqs, seqs, bgfreq, tsvratio) + bdlogl;
    
    printLog(LOG_MEDIUM, "search logl (total, bd, qbd): %f %f %f\n", 
             logl, bdlogl, qbdlogl);
    
    return logl;
}


//=============================================================================
// Likelihood function

SpidirBranchLikelihoodFunc::SpidirBranchLikelihoodFunc(
    int nnodes, SpeciesTree *stree, 
    SpidirParams *params, 
    int *gene2species,
    float predupprob, float dupprob, float lossprob, bool estGenerate, 
    bool onlyduploss, bool oldduploss) :
    
    nnodes(nnodes),
    stree(stree),
    params(params),
    gene2species(gene2species),
    recon(nnodes),
    events(nnodes),
    predupprob(predupprob),
    dupprob(dupprob),
    lossprob(lossprob),
    estGenerate(estGenerate),
    onlyduploss(onlyduploss),
    oldduploss(oldduploss)
{}


float SpidirBranchLikelihoodFunc::likelihood(Tree *tree) {
    // reconcile tree to species tree
    reconcile(tree, stree, gene2species, recon);
    labelEvents(tree, recon, events);
    float generate;
    
    if (estGenerate)
        generate = -1;
    else
        generate = -99;
        
    return treelk(tree, stree,
                  recon, events, params,
                  generate, predupprob, dupprob, lossprob, onlyduploss,
                  oldduploss);
}



float SpidirBranchLikelihoodFunc::likelihood2(Tree *tree) {
    // reconcile tree to species tree
    reconcile(tree, stree, gene2species, recon);
    labelEvents(tree, recon, events);
    
    if (!oldduploss)
        return rareEventsLikelihood(tree, stree, recon, 
                                events, predupprob, dupprob, lossprob);
    else
        return rareEventsLikelihood_old(tree, stree, recon, 
                                        events, predupprob, dupprob);
}


float HkyBranchLikelihoodFunc::likelihood(Tree *tree)
{
    return calcHkySeqProb(tree, nseqs, seqs, bgfreq, tsvratio);
}


//=============================================================================


// propose initial tree by Neighbor Joining
Tree *getInitialTree(string *genes, int nseqs, int seqlen, char **seqs)
{
    int nnodes = nseqs * 2 - 1;

    ExtendArray<int> ptree(nnodes);
    ExtendArray<float> dists(nnodes);
    Matrix<float> distmat(nseqs, nseqs);

    calcDistMatrix(nseqs, seqlen, seqs, distmat.getMatrix());
    neighborjoin(nseqs, distmat.getMatrix(), ptree, dists);

    Tree *tree = new Tree(nnodes);
    ptree2tree(nnodes, ptree, tree);
    tree->setLeafNames(genes);

    return tree;
}


// propose initial tree and root by species
Tree *getInitialTree(string *genes, int nseqs, int seqlen, char **seqs,
                     SpeciesTree *stree, int *gene2species)
{
    Tree *tree = getInitialTree(genes, nseqs, seqlen, seqs);
    reconRoot(tree, stree, gene2species);

    return tree;
}



//=============================================================================
// MCMC search



void printSearchStatus(Tree *tree, SpeciesTree *stree, int *gene2species,
                       int *recon=NULL, int *events=NULL)
{
    if (stree) {
        bool cleanupRecon = false;
        if (!recon) {
            recon = new int [tree->nnodes];
            cleanupRecon = true;
        }

        bool cleanupEvents = false;    
        if (!events) {
            events = new int [tree->nnodes];
            cleanupEvents = true;
        }    

        //assert(tree->assertTree());
    
        reconcile(tree, stree, gene2species, recon);
        labelEvents(tree, recon, events);
        int losses = countLoss(tree, stree, recon);        
        
        // count dups
        int dups = 0;
        for (int i=0; i<tree->nnodes; i++)
            if (events[i] == EVENT_DUP)
                dups++;
        
        printLog(LOG_LOW, "search: dups = %d\n", dups);
        printLog(LOG_LOW, "search: loss = %d\n", losses);
        
        if (cleanupRecon)
            delete [] recon;
        if (cleanupEvents)
            delete [] events;        
    }                
    
    if (isLogLevel(LOG_LOW))
        displayTree(tree, getLogFile());
    
    if (isLogLevel(LOG_MEDIUM)) {
        printLog(LOG_MEDIUM, "tree: ");
        tree->writeNewick(getLogFile(), NULL, 0, true);
        printLog(LOG_MEDIUM, "\n\n");
    }
}


Tree *searchMCMC(Tree *initTree, 
                 string *genes, int nseqs, int seqlen, char **seqs,
                 SampleFunc *samples,
                 BranchLikelihoodFunc *lkfunc,
                 TopologyProposer *proposer,
                 BranchLengthFitter *fitter)
{
    Tree *toptree = NULL;
    float toplogl = -INFINITY, logl=-INFINITY, nextlogl;
    Tree *tree = initTree;
    
    
    // determine initial tree
    if (initTree == NULL)
        tree = getInitialTree(genes, nseqs, seqlen, seqs);
    
    // used for printSearchStatus
    ExtendArray<int> recon(tree->nnodes);
    ExtendArray<int> events(tree->nnodes);
    
    // init likelihood score
    parsimony(tree, nseqs, seqs); // get initial branch lengths
    logl = fitter->findLengths(tree);
    logl += lkfunc->likelihood(tree);
    toplogl = logl;
    toptree = tree->copy();
    
    int accept = 0;
    int reject = 0;
    
        
    // MCMC loop
    for (int i=0; proposer->more(); i++) {
        printLog(LOG_LOW, "search: iter %d\n", i);
    
        // propose new tree 
        proposer->propose(tree);
        //assert(tree->assertTree());
        
        float seqlk = fitter->findLengths(tree);
        float branchlk = lkfunc->likelihood(tree);
        nextlogl = seqlk + branchlk;       
        
        
        // acceptance rule
        if (nextlogl > logl ||
            nextlogl - logl > log(frand()))
        {
            // accept
            printLog(LOG_MEDIUM, "search: accept %f (%f)\n", nextlogl, logl);
            if (isLogLevel(LOG_MEDIUM))
                printSearchStatus(tree, lkfunc->getSpeciesTree(), 
                                 lkfunc->getGene2species(), recon, events);            
            
            accept++;
            logl = nextlogl;

            // keep track of toptree            
            if (logl > toplogl) {
                delete toptree;
                toptree = tree->copy();
                toplogl = logl;
            }
            
        } else {                
            // reject, undo topology change
            printLog(LOG_MEDIUM, "search: reject %f (%f)\n", nextlogl, logl);
            if (isLogLevel(LOG_MEDIUM))
                printSearchStatus(tree, lkfunc->getSpeciesTree(), 
                                  lkfunc->getGene2species(), recon, events);
            
            reject++;
            proposer->revert(tree);
        }
        
        // return a tree sample
        (*samples)(tree);
    }
    
    printLog(LOG_LOW, "accept rate: %f\n", accept / float(accept+reject));
    
    return toptree;
}



Tree *searchClimb(Tree *initTree, 
                  string *genes, int nseqs, int seqlen, char **seqs,
                  BranchLikelihoodFunc *lkfunc,
                  TopologyProposer *proposer,
                  BranchLengthFitter *fitter)
{
    Tree *toptree = NULL;
    float toplogl = -INFINITY, nextlogl;
    Tree *tree = initTree;
        
    
    // determine initial tree
    if (initTree == NULL)
        tree = getInitialTree(genes, nseqs, seqlen, seqs,
                              lkfunc->getSpeciesTree(), 
                              lkfunc->getGene2species());
    
    ExtendArray<int> recon(tree->nnodes);
    ExtendArray<int> events(tree->nnodes);
    
    // init likelihood score
    parsimony(tree, nseqs, seqs); // get initial branch lengths
    float seqlk = fitter->findLengths(tree);
    float branchlk = lkfunc->likelihood(tree);
    float logl2 = lkfunc->likelihood2(tree);
    toplogl = seqlk + branchlk;
    toptree = tree->copy();
    
    
    // log initial tree
    printLog(LOG_LOW, "search: initial %f\n", toplogl);
    printLog(LOG_LOW, "search: lnl   = %f\n", toplogl);
    printLog(LOG_LOW, "search: seqlk = %f\n", seqlk);
    printLog(LOG_LOW, "search: lenlk = %f\n", branchlk);          
    printLog(LOG_LOW, "search: lnl2  = %f\n", logl2);
                        
    printSearchStatus(tree, lkfunc->getSpeciesTree(), 
                      lkfunc->getGene2species(), recon, events);
    
    
    int accept = 0;
    int reject = 0;
    
    // search loop
    for (int i=0; proposer->more(); i++) {
        printLog(LOG_LOW, "search: iter %d\n", i);
    
        // propose new tree 
        proposer->propose(tree);
        //assert(tree->assertTree());
        
        // calculate likelihood
        seqlk = fitter->findLengths(tree);
        branchlk = lkfunc->likelihood(tree);
        nextlogl = seqlk + branchlk;
        //assert(tree->assertTree());
        
        // acceptance rule
        if (nextlogl > toplogl)
        {
            accept++;
            float logl2 = lkfunc->likelihood2(tree);
        
            printLog(LOG_LOW, "search: accept %f\n", nextlogl);
            printLog(LOG_LOW, "search: lnl   = %f\n", nextlogl);
            printLog(LOG_LOW, "search: seqlk = %f\n", seqlk);
            printLog(LOG_LOW, "search: lenlk = %f\n", branchlk);          
            printLog(LOG_LOW, "search: lnl2  = %f\n", logl2);
            
            // accept
            toplogl = nextlogl;
            
            printSearchStatus(tree, lkfunc->getSpeciesTree(), 
                  lkfunc->getGene2species(), recon, events);
            delete toptree;
            toptree = tree->copy();
        } else {           
            // reject, undo topology change
            reject++;
            
            float logl2 = lkfunc->likelihood2(tree);
            
            printLog(LOG_LOW, "search: reject %f\n", nextlogl);
            printLog(LOG_LOW, "search: lnl   = %f\n", nextlogl);
            printLog(LOG_LOW, "search: seqlk = %f\n", seqlk);
            printLog(LOG_LOW, "search: lenlk = %f\n", branchlk);          
            printLog(LOG_LOW, "search: lnl2  = %f\n", logl2);            
            
            if (isLogLevel(LOG_MEDIUM))
                printSearchStatus(tree, lkfunc->getSpeciesTree(), 
                      lkfunc->getGene2species(), recon, events);
            
            proposer->revert(tree);
        }
    }
    
    printLog(LOG_LOW, "accept rate: %f\n", accept / float(accept+reject));
    
    return toptree;
}

} // namespace spidir
