#ifndef FILE_LOADER_INCLUDED
#define FILE_LOADER_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
//		RIFF chunk iterator
//


//#define LE_16(X) ((X)>>8)&0xFF,(X)&0xFF
//#define LE_32(X) ((X)>>24)&0xFF,((X)>>16)&0xFF,((X)>>8)&0xFF,(X)&0xFF
#define LE_16(X) (X)&0xFF,((X)>>8)&0xFF
#define LE_32(X) (X)&0xFF,((X)>>8)&0xFF,((X)>>16)&0xFF,((X)>>24)&0xFF

#define RIFF_ID(A,B,C,D) ( (A<<0) | (B<<8) | (C<<16) | (D<<24) )
#define SWAP16(X) ( (((X)&0x00FF)<<8) | (((X)&0xFF00)>>8) )
#define SWAP32(X) ( (SWAP16(X)<<16) | (SWAP16(X>>16)) )
#define MACHINE_ENDIANNESS ((endian_one == *(char*)&endian_one)? LITTLE_ENDIAN : BIG_ENDIAN)

static int endian_one = 1;

#ifndef LITTLE_ENDIAN
enum ENDIANNESS
{
	LITTLE_ENDIAN,
	BIG_ENDIAN,
};
#endif
typedef struct RIFF_CHUNK_t
{
	unsigned id,size,swap;
	long int start;
	struct RIFF_CHUNK_t * parent;
}RIFF_CHUNK;

void print_riff_info(RIFF_CHUNK * chunk)
{
	char str[5] = "xxxx";
	*(unsigned*)str = chunk->id;
	//printf("found chunk id: '%s' at position %d with size=%d\n",str,chunk->start,chunk->size);
}

RIFF_CHUNK make_riff_chunk(FILE*fp, unsigned endianness, RIFF_CHUNK * parent)
{
	RIFF_CHUNK chunk = {0};
	if(endianness != MACHINE_ENDIANNESS) chunk.swap = 1;
	
	if(parent)	chunk.start = parent->start;
	else		chunk.start = (unsigned) ftell(fp);
	chunk.parent = parent;
	
	return chunk;
}

unsigned get_riff_chunk(RIFF_CHUNK * chunk, FILE * fp)
{
	fseek(fp,chunk->start+chunk->size,SEEK_SET);
	int next=fgetc(fp);
	if(next==EOF) return 0;
	ungetc(next,fp);	
	
	chunk->id=0;
	fread(&chunk->id,4,1,fp);
	fread(&chunk->size,4,1,fp);
	chunk->start = (unsigned)ftell(fp);
	if(chunk->swap) chunk->size = SWAP32(chunk->size);
	
	if(chunk->parent)
	if(chunk->start >= chunk->parent->start + chunk->parent->size)
		return 0;
	
	print_riff_info(chunk);
	return chunk->id;
}

typedef struct 
{
	float * data;
	float *channels[2];
	int n_chan;
	int size;
	float rate;	
} WAV_DATA;


WAV_DATA load_wav(char * file)
{
	WAV_DATA wav = {0};
	FILE * fp = fopen(file,"rb");
	if(!fp) { printf("File not found: %s\n",file); return wav; }
	char header[44]={0}; fread(header,sizeof(header),1,fp);
	fseek(fp,SEEK_SET,0);
	
	enum {PCM=1, IEEE_F32=3};	
	
	wav.n_chan = *(unsigned short*)(header+22);
	wav.rate = *(unsigned int*)(header+24);
	int format = *(unsigned short*)(header+20);
	int bps = (*(unsigned short*)(header+32))/wav.n_chan;
	
	printf("format=%d bps=%d n_chan=%d rate=%d\n",format,bps,wav.n_chan,(int)wav.rate);
	
	for(RIFF_CHUNK chunk=make_riff_chunk(fp,LITTLE_ENDIAN,NULL); get_riff_chunk(&chunk,fp);)
	switch(chunk.id)
	{	
		case RIFF_ID('R','I','F','F'): chunk.size = 4; break;
		case RIFF_ID('d','a','t','a'):
		{
			wav.size = chunk.size/(bps*wav.n_chan);
			printf("size=%d n_chan=%d\n",(int)wav.size,wav.n_chan);
			
			float * data = (float *)malloc(wav.n_chan*wav.size*sizeof(float));
			
			wav.data = data;
			for(int i=0; i<wav.n_chan; i++)
				wav.channels[i] = data+wav.size*i;
			
			switch(format)
			{
				case IEEE_F32:
				for(int i=0; i < wav.size; i++)
				for(int c=0; c < wav.n_chan; c++)
					fread(&wav.channels[c][i],bps,1,fp);
				break;
				
				case PCM:
				for(int i=0; i < wav.size; i++)
				for(int c=0; c < wav.n_chan; c++)
				{
					int value=0;
					unsigned char byte=0;
					
					for(int b=0;b<bps;b++)
					{
						fread(&byte,1,1,fp);
						value |= ((unsigned int)byte)<<(8*b);
					}
					for(int b=bps;b<4;b++)
						if(byte&(1<<7))value |= 0xFF<<(8*b);
					
					wav.channels[c][i] = (float)value/(float)(1<<(8*bps-1));
				}
				break;
			}
			
			fclose(fp);
			return wav;
		}		
		break;
	}	
	
	fclose(fp);
	return wav;
}

void save_wav(char * file, float * data, int size)
{	
	FILE * fp = fopen(file,"wb+");
	if(!fp) return;
	
	size *= sizeof(float);
	char header[44] = 
	{
		'R','I','F','F', LE_32(size+44), 
		'W','A','V','E',
		'f','m','t',' ', LE_32(16),
			LE_16(3), 		// audio format
			LE_16(1), 		// n chan
			
			LE_32(44100), 	// samps p sec
			LE_32(4*44100), // bytes p sec
			LE_16(4), 		// block align
			LE_16(32), 		// bits p sample

		'd','a','t','a', LE_32(size) // size in bytes
	};
	
	fwrite(header,1,sizeof(header),fp);
	fwrite(data,1,size,fp);
	
	fclose(fp);
}

#endif