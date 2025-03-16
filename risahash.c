#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*This is a program to demonstrate the 'risahash' audio comparison algorithm - a very simple audio perceptual hash inspired by part of the phash algorithm for images. 
  Risahash itsself is almost no code at all - the bulk of the following program is the .wav file parser and associated bitdepth/mono conversion.*/

int load_wav(unsigned char *filename, unsigned long long *datalen);
unsigned long long risahash_getsliceval(signed char *data, unsigned int len);
unsigned long long risahash_gethash(signed char *buffer, unsigned long long len);

signed char *wavdata;

int main(int argc, char *argv[]){
  unsigned long long samples;
  if(argc<=1){
    printf("Risahash: An audio perceptual-hash utility.\nrisahash <input> [option]\n  Input may be - to read from stdin.\nTakes .wav file as input - if you want to hash other formats, convert to .wav first.\n  -power will return the mean squared amplitude. With no option, returns the hex-encoded risahash.\n");
    return(0);
  }
  if(load_wav(argv[1], &samples)){
    fprintf(stderr, "Error loading .wav input.");
    return(1);
  }
  if(argc==3){
    if(!strcmp("-power", argv[2])){
      unsigned int n;
      unsigned long long tot=0;
      for(n=0;n<samples;n++){
        signed int val=wavdata[n];
        tot+=val*val;
       }
      printf("%f\n", tot/(float)(samples*128*128));
      return(0);
    }
  }
  unsigned long long hash=risahash_gethash(wavdata, samples);
  printf("%llX\n", hash);
}

int load_wav(unsigned char *filename, unsigned long long *datalen){
  //This untested code of dubious reliability loads a wave file into the global variable wavdata, having first allocated it.
  //The wave is converted to a single-channel, 8-bit signed waveform: A char array.
  unsigned char *scratch=malloc(5);
  unsigned int chunksize;
  unsigned int n;
  FILE *wavfile;
  if(strcmp(filename, "-"))
    wavfile=fopen(filename, "rb");
  else
    wavfile=stdin;
  if(!wavfile)
    return(1);
  fread(scratch, 1, 4, wavfile);
  fread(&chunksize, 1, 4, wavfile);
  fread(scratch, 1, 4, wavfile);
  unsigned short channels; //WAVE supports 2^16 channels. But this program only supports 2.
  unsigned int samplerate;
  unsigned short bitdepth;
  unsigned int byterate;
  fread(scratch, 1, 4, wavfile);
  fread(&chunksize, 1, 4, wavfile);
  unsigned short audioformat;
  fread(&audioformat, 1, 2, wavfile);
  fread(&channels, 1, 2, wavfile);
  fread(&samplerate, 1, 4, wavfile);
  fread(&byterate, 1, 4, wavfile);
  unsigned short blockalign;
  fread(&blockalign, 1, 2, wavfile);
  fread(&bitdepth, 1, 2, wavfile);
  chunksize=chunksize-16;
  if(chunksize)
    fseek(wavfile, chunksize, SEEK_CUR);
  fread(scratch, 1, 4, wavfile);
  while(memcmp(scratch, "data", 4)){
    fread(&chunksize, 1, 4, wavfile);
//    printf("Unusual chunk encountered. %c%c%c%c %u\n", scratch[0],scratch[1],scratch[2],scratch[3],chunksize);
    fseek(wavfile, chunksize, SEEK_CUR);
    fread(scratch, 1, 4, wavfile);
  }
  unsigned int subchunksize2;
  fread(&subchunksize2, 1, 4, wavfile);
  unsigned int samples=subchunksize2/channels;
  if(bitdepth==16)
    samples=samples/2;
  if(bitdepth==24)
    samples=samples/3;
//  fprintf(stderr, "B/C/S/N: %u/%u/%u/%u %us\n", bitdepth, channels, samplerate, samples, samples/samplerate);
  datalen[0]=samples;
  wavdata=malloc(samples);
  if(bitdepth==8 && channels==1){
    fread(wavdata, 1, samples, wavfile);
    for(n=0;n<samples;n++) //unsigned to signed conversion.
      wavdata[n]-=128;
    return(0);
  }
  if(bitdepth==8 && channels==2){
    char onesample,twosample;
    for(n=0;n<samples;n++){
      fread(&onesample, 1, 1, wavfile);
      fread(&twosample, 1, 2, wavfile);
      onesample-=128;twosample-=128;
      wavdata[n]=((int)onesample+(int)twosample)/2;
    }
    return(0);
  }
  if(bitdepth==16 && channels==1){
    short onesample;
    for(n=0;n<samples;n++){
      fread(&onesample, 1, 2, wavfile);
      wavdata[n]=onesample/0x100;
    }
    return(0);
  }
  if(bitdepth==16 && channels==2){
    short onesample, twosample;
    for(n=0;n<samples;n++){
      fread(&onesample, 1, 2, wavfile);
      fread(&twosample, 1, 2, wavfile);
/*    if(abs(twosample)>abs(onesample)) //Alternate stereo-to-mono conversion. In theory, better for handling out-of-phase components.
        wavdata[n]=twosample/0x100;     //In my benchmarks, matching accuracy was no better than the conventional means.
      else
        wavdata[n]=onesample/0x100; */
      wavdata[n]=((int)onesample+(int)twosample)/0x200;
    }
    return(0);
  }
  if(bitdepth==24 && channels==2){
    short onesample, twosample;
    unsigned char scratch;
    for(n=0;n<samples;n++){
      fread(&scratch, 1, 1, wavfile); //Discard the least significant byte. This may look wrong, but remember: Little-endian data.
      fread(&onesample, 1, 2, wavfile);
      fread(&scratch, 1, 1, wavfile); //As above.
      fread(&twosample, 1, 2, wavfile);
      wavdata[n]=((int)onesample+(int)twosample)/0x200;
    }
    return(0);
  }
  if(bitdepth==24 && channels==1){
    short onesample;
    unsigned char scratch;
    for(n=0;n<samples;n++){
      fread(&scratch, 1, 1, wavfile);
      fread(&onesample, 1, 2, wavfile);
      wavdata[n]=onesample/0x100;
    }
    return(0);
  }
  fprintf(stderr, "No handler for .wav type: c%u b%u\n", channels, bitdepth);
  return(1);

}

unsigned long long risahash_gethash(signed char *buffer, unsigned long long len){
  unsigned int slicelen=len/66;
  unsigned long long hash=0;
  unsigned int n;
  unsigned long long last=risahash_getsliceval(wavdata, slicelen);
  for(n=1;n<65;n++){
    unsigned long long cur=risahash_getsliceval(wavdata+(n*slicelen), slicelen);
//    printf("%d %llu\n", n, cur);
    if(cur>last)
      hash=hash|0x01;
    hash=hash<<1;
    last=cur;
  }


  return(hash);

}

unsigned long long risahash_getsliceval(signed char *data, unsigned int len){
  //This needs to return a nice statistical value about the slice.
  //It also needs to be independent of sample rate - note that if the same audio is processed from two files of different sample rate,
  //  this function will get the same amount of time per slice in each, but different numbers of samples.
  //The simplist statistic is simply the sum of absolute or squared values, normalised for number of samples.
  //In testing I found squared gives better results than absolute, though the difference is tiny.
  unsigned long long tot=0; //Maximum added per sample is 128^2=16384. That means a 32-bit would risk overflow after 262144 samples - or two minutes of a 96KHz file!
                              //Going to have to use 64-bit math here to avoid that possibility.
  unsigned int n;
  for(n=0;n<len;n++){
    signed int val=data[n];
    tot+=(val*val);
  }
  if(len>1024)
    len=len/256; //Reduces rounding errors on small files. Other than this, makes no difference on output at all.
  return(tot/len);
}
