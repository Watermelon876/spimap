#ifndef SPIDIR_COMMON_H
#define SPIDIR_COMMON_H

/*=============================================================================

    SPIDIR - SPecies Informed DIstanced-based Reconstruction
    
    Matt Rasmussen
    Wed Jun 13 22:09:24 EDT 2007


=============================================================================*/

// headers c++ 
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <string>
#include <vector>
#include <string.h>

// spidir headers
#include "ExtendArray.h"

using namespace std;

namespace spidir {

// constants
#ifndef INFINITY
#   define INFINITY 1e1000
#endif



//=============================================================================
// Math

// indexing a matrix stored as a single array (row-major)
// m: number of columns
// i: row index
// j: column index
#define matind(m, i, j) ((m)*(i) + (j))


inline float frand(float max=1.0)
{ return rand() / float(RAND_MAX) * max; }

inline float frand(float min, float max)
{ return min + (rand() / float(RAND_MAX) * (max-min)); }

inline int irand(int max)
{ return int(rand() / float(RAND_MAX) * max); }

inline int irand(int min, int max)
{ return min + int(rand() / float(RAND_MAX) * (max - min)); }


// computes the log(normalPdf(x | u, s^2))
inline float normallog(float x, float u, float s)
{
    const float log_sqrt_2_pi = 0.91893853320467267;
    if (s == 0.0)
        return -INFINITY;
    else
        return - logf(s) - log_sqrt_2_pi - (x-u)*(x-u) / (2.0*s*s);
    
    //return - log(s) - log(sqrt(2.0*M_PI)) - (x-u)*(x-u) / (2.0*s*s);
    //return log(1.0/(s * sqrt(2.0*M_PI)) * exp(- (x-u)*(x-u) / (2.0 * s*s)));
}

extern "C" {

float poisson(int x, float lambda);
double gammln(double xx);
double gamm(double x);
float gammalog(float x, float a, float b);
float gammaPdf(float x, float a, float b);
float invgamma(float x, float a, float b);
float invgammaCdf(float x, float a, float b);
double quantInvgamma(double p, double a, double b);

// Derivative of Gamma distribution with respect to x
float gammaDerivX(float x, float a, float b);
    
// Derivative of Gamma distribution with respect to a
float gammaDerivA(float x, float a, float b);

// Derivative of Gamma distribution with respect to b
float gammaDerivB(float x, float a, float b);

// Derivative of Gamma distribution with respect to nu (its variance)
float gammaDerivV(float x, float v);

// Second Derivative of Gamma distribution with respect to nu (its variance)
double gammaDerivV2(float x, float v);

// PDF of a sum of n gamma variables
double gammaSumPdf(double y, int n, float *alpha, float *beta, 
		   float tol);

float normalvariate(float mu, float sigma);
float gammavariate(float alpha, float beta);

inline float invgammavariate(float alpha, float beta)
{ return 1.0 / gammavariate(alpha, beta); }

inline float expovariate(float lambda)
{ return -log(frand()) / lambda; }

} // extern "C"

template <class T>
double variance(T *vals, int size)
{
    double mean = 0.0;
    for (int i=0; i<size; i++)
        mean += vals[i];
    mean /= size;
    
    double tot = 0.0;
    for (int i=0; i<size; i++)
        tot += (vals[i] - mean) * (vals[i] - mean);
    
    return tot / size;
}

template <class T>
double stdev(T* vals, int size)
{
    return sqrt(variance(vals, size));
}



// Find a root of a function func(x) using the secant method
// x0 and x1 are initial estimates of the root
template <class Func>
float secantRoot(Func &f, float x0, float x1, int maxiter, 
                 float minx=.000001, float esp=.002)
{
    float f0 = f(x0);
    for (int i=0; i<maxiter; i++) {
        if (fabs((x1 - x0)*2.0 / (x0+x1)) < esp)
            return x0;
        float f1 = f(x1);
        float x2 = x1 - (x1 - x0) * f1 / (f1 - f0);
        
        x0 = x1;
        x1 = (x2 > minx) ? x2 : minx;
        f0 = f1;
    }

    return x1;
}


// Find a root of a function func(x) using the bisection method
// This is less efficient but is more robust than Newton's or Secant
// x0 and x1 are initial estimates of the root
template <class Func>
float bisectRoot(Func &f, float x0, float x1, const float err=.001)
{
    float f0 = f(x0);
    float f1 = f(x1);
    
    while (fabs(x1 - x0) > 2 * err) {
        //printf("in:  %f %f; %f %f\n", x0, x1, f0, f1);

        float x2 = (x0 + x1) / 2.0;
        float f2 = f(x2);
        
        if (f0 * f2 > 0) {
            x0 = x2;
            f0 = f2;
        } else {
            x1 = x2;
            f1 = f2;
        }
    }

    return (x0 + x1) / 2.0;
}


// computes log(a + b) given log(a) and log(b)
inline float logadd(float lna, float lnb)
{
    float diff = lna - lnb;
    if (diff < 40.0)
        return logf(expf(diff) + 1.0) + lnb;
    else
        return lna;
}

void invertPerm(int *perm, int *inv, int size);

template <class T>
void permute(T* array, int *perm, int size)
{
    ExtendArray<T> tmp(size);
    
    // transfer permutation to temp array
    for (int i=0; i<size; i++)
        tmp[i] = array[perm[i]];
    
    // copy permutation back to original array
    for (int i=0; i<size; i++)
        array[i] = tmp[i];
}

template <class T>
T ipow(T val, int expo)
{
    T result = 1.0;
    unsigned int e = expo;

    if ((int)e < 0) {
	e = -e;
	val = 1.0 / val;
    }

    while (true) {
	if (e & 1)
	    result *= val;
	if ((e >>= 1) == 0)
	    break;
	val *= val;
    }

    return result;
}


int choose(int n, int k);

double fchoose(int n, int k);


template <class T>
int findval(T *array, int size, const T &val)
{
    for (int i=0; i<size; i++)
        if (array[i] == val)
            return i;
    return -1;
}


//=============================================================================
// sorting

template <class KeyType, class ValueType>
struct RankSortCmp
{
    RankSortCmp(ValueType *values):
        values(values)
    {}
    
    bool operator()(KeyType i, KeyType j)
    { return values[i] < values[j]; }
    
    ValueType *values;
};

template <class KeyType, class ValueType>
void ranksort(KeyType *keys, ValueType *values, int size)
{
    RankSortCmp<KeyType, ValueType> cmp(values);
    sort(keys, keys + size, cmp);
}



//=============================================================================
// input/output


class BufferedReader
{
public:
    BufferedReader(FILE *stream=NULL, bool autoclose=true) :
        m_stream(stream),
        m_line(0, 10000),
        m_autoclose(autoclose)
    {}
    
    virtual ~BufferedReader()
    {
        if (m_autoclose && m_stream)
            fclose(m_stream);
    }
    
    
    bool open(const char *filename, const char *mode, 
              const char *errmsg="cannot read file '%s'\n")
    {
        m_stream = fopen(filename, mode);
        
        if (!m_stream) {
            fprintf(stderr, errmsg, filename);        
            return false;
        }
        return true;
    }
    
    
    char *readLine()
    {
        while (!feof(m_stream)) {
            int pos = m_line.size();
            char *ret = fgets(&(m_line.get()[pos]), 
                              m_line.get_capacity()-m_line.size(), m_stream);
            int readsize = strlen(&(m_line.get()[pos]));
            
            if (ret == NULL)
                return NULL;
            
            if (m_line.size() + readsize < m_line.get_capacity() - 1)
                return m_line.get();

            assert(m_line.increaseCapacity());
            m_line.setSize(m_line.size() + readsize);
        }
        
        return NULL;
    }
    
    
    void close()
    {
        if (m_stream)
            fclose(m_stream);
        m_stream = NULL;
    }
    
    
protected:
    FILE *m_stream;
    ExtendArray<char> m_line;
    bool m_autoclose;
};


bool inChars(char c, const char *chars);
bool chomp(char *str);
vector<string> split(const char *str, const char *delim, bool multiDelim = true);
string trim(const char *word);


// logging
enum {
    LOG_QUIET=0,
    LOG_LOW=1,
    LOG_MEDIUM=2,
    LOG_HIGH=3
};

void printLog(int level, const char *fmt, ...);
bool openLogFile(const char *filename);
void openLogFile(FILE *stream);
void closeLogFile();
FILE *getLogFile();
void setLogLevel(int level);
bool isLogLevel(int level);

void printError(const char *fmt, ...);


void printIntArray(int *array, int size);
void printFloatArray(float *array, int size);



} // namespace spidir

#endif // SPIDIR_COMMON_H
