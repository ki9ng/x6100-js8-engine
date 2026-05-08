// SPDX-License-Identifier: MIT
//
// Target-side fork of fate's util.cc. Drops the libsndfile-dependent
// readwav/writewav (the firmware path doesn't go through WAV files at all)
// and keeps everything else verbatim.
//
// All the math/DSP helpers below are bit-for-bit identical to upstream fate.
// If fate's util.cc is updated, this file should be re-synced and the wav
// helpers re-removed.

#include <sys/time.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <complex>
#include "util.h"

double
now()
{
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void
writetxt(std::vector<double> v, const char *filename)
{
  FILE *fp = fopen(filename, "w");
  if(fp == 0){
    fprintf(stderr, "could not write %s\n", filename);
    exit(1);
  }
  for(ulong i = 0; i < v.size(); i++){
    fprintf(fp, "%f\n", v[i]);
  }
  fclose(fp);
}

//
// Goertzel Algorithm for a Non-integer Frequency Index, Rick Lyons
// https://www.dsprelated.com/showarticle/495.php
//
std::complex<double>
goertzel(std::vector<double> v, int rate, int i0, int n, double hz)
{
  double bin_hz = rate / (double) n;
  double k = hz / bin_hz;

  double alpha = 2 * M_PI * k / n;
  double beta = 2 * M_PI * k * (n - 1.0) / n;

  double two_cos_alpha = 2 * cos(alpha);
  double a = cos(beta);
  double b = -sin(beta);
  double c = sin(alpha) * sin(beta) - cos(alpha)*cos(beta);
  double d = sin(2 * M_PI * k);

  double w1 = 0;
  double w2 = 0;

  for(int i = 0; i < n; i++){
    double w0 = v[i0+i] + two_cos_alpha * w1 - w2;
    w2 = w1;
    w1 = w0;
  }

  double re = w1*a + w2*c;
  double im = w1*b + w2*d;

  return std::complex<double>(re, im);
}

double
vmax(const std::vector<double> &v)
{
  double mx = 0;
  int got = 0;
  for(int i = 0; i < (int) v.size(); i++){
    if(got == 0 || v[i] > mx){
      got = 1;
      mx = v[i];
    }
  }
  return mx;
}

std::complex<double>
cvmax(const std::vector<std::complex<double>> &v)
{
  std::complex<double> mx(0, 0);
  int got = 0;
  for(int i = 0; i < (int) v.size(); i++){
    if(got == 0 || std::abs(v[i]) > std::abs(mx)){
      got = 1;
      mx = v[i];
    }
  }
  return mx;
}

std::vector<double>
vreal(const std::vector<std::complex<double>> &a)
{
  std::vector<double> b(a.size());
  for(int i = 0; i < (int) a.size(); i++){
    b[i] = a[i].real();
  }
  return b;
}

std::vector<double>
vimag(const std::vector<std::complex<double>> &a)
{
  std::vector<double> b(a.size());
  for(int i = 0; i < (int) a.size(); i++){
    b[i] = a[i].imag();
  }
  return b;
}

double
gfsk_point(double b, double t)
{
  double c = M_PI * sqrt(2.0 / log(2.0));
  double x = 0.5 * (erf(c * b * (t + 0.5)) - erf(c * b * (t - 0.5)));
  return x;
}

std::vector<double>
gfsk_window(int samples_per_symbol, double b)
{
  std::vector<double> v(3 * samples_per_symbol);
  double sum = 0;
  for(int i = 0; i < (int) v.size(); i++){
    double x = i / (double)samples_per_symbol;
    x -= 1.5;
    double y = gfsk_point(b, x);
    v[i] = y;
    sum += y;
  }

  for(int i = 0; i < (int) v.size(); i++){
    v[i] /= sum;
  }

  return v;
}

std::vector<std::complex<double>>
gfsk_c(const std::vector<int> &symbols,
     double hz0, double hz1,
     double spacing, int rate, int symsamples,
     double phase0,
     const std::vector<double> &gwin)
{
  assert((gwin.size() % 2) == 0);

  std::vector<double> hzv(symsamples * (symbols.size() + 2), 0.0);
  for(int bi = 0; bi < (int) symbols.size(); bi++){
    double base_hz = hz0 + (hz1 - hz0) * (bi / (double) symbols.size());
    double fr = base_hz + (symbols[bi] * spacing);
    int mid = symsamples*(bi+1) + symsamples/2;
    hzv[mid] = fr * symsamples / 2.0;
    hzv[mid-1] = fr * symsamples / 2.0;
  }

  for(int i = 0; i < symsamples; i++){
    hzv[i] = hzv[i+symsamples];
    hzv[symsamples*(symbols.size()+1) + i] = hzv[symsamples*symbols.size() + i];
  }

  int half = gwin.size() / 2;
  std::vector<double> o(hzv.size());
  for(int i = 0; i < (int) o.size(); i++){
    double sum = 0;
    for(int j = 0; j < (int) gwin.size(); j++){
      int k = i - half + j;
      if(k >= 0 && k < (int) hzv.size()){
        sum += hzv[k] * gwin[j];
      }
    }
    o[i] = sum;
  }

  std::vector<double> oo(symsamples * symbols.size());
  for(int i = 0; i < (int) oo.size(); i++){
    oo[i] = o[i + symsamples];
  }

  std::vector<std::complex<double>> v(symsamples * symbols.size());
  double theta = phase0;
  for(int i = 0; i < (int) v.size(); i++){
    v[i] = std::complex(cos(theta), sin(theta));
    double hz = oo[i];
    theta += 2 * M_PI / (rate / hz);
  }

  return v;
}

std::vector<double>
gfsk_r(const std::vector<int> &symbols,
       double hz0, double hz1,
       double spacing, int rate, int symsamples,
       double phase0,
       const std::vector<double> &gwin)
{
  assert((gwin.size() % 2) == 0);

  std::vector<double> hzv(symsamples * (symbols.size() + 2), 0.0);
  for(int bi = 0; bi < (int) symbols.size(); bi++){
    double base_hz = hz0 + (hz1 - hz0) * (bi / (double) symbols.size());
    double fr = base_hz + (symbols[bi] * spacing);
    int mid = symsamples*(bi+1) + symsamples/2;
    hzv[mid] = fr * symsamples / 2.0;
    hzv[mid-1] = fr * symsamples / 2.0;
  }

  for(int i = 0; i < symsamples; i++){
    hzv[i] = hzv[i+symsamples];
    hzv[symsamples*(symbols.size()+1) + i] = hzv[symsamples*symbols.size() + i];
  }

  int half = gwin.size() / 2;
  std::vector<double> o(hzv.size());
  for(int i = 0; i < (int) o.size(); i++){
    double sum = 0;
    for(int j = 0; j < (int) gwin.size(); j++){
      int k = i - half + j;
      if(k >= 0 && k < (int) hzv.size()){
        sum += hzv[k] * gwin[j];
      }
    }
    o[i] = sum;
  }

  std::vector<double> oo(symsamples * symbols.size());
  for(int i = 0; i < (int) oo.size(); i++){
    oo[i] = o[i + symsamples];
  }

  std::vector<double> v(symsamples * symbols.size());
  double theta = phase0;
  for(int i = 0; i < (int) v.size(); i++){
    v[i] = cos(theta);
    double hz = oo[i];
    theta += 2 * M_PI / (rate / hz);
  }

  return v;
}

double
cycle_second()
{
  double tt = now();
  double st = trunc(tt / 15.0) * 15;
  return tt - st;
}
