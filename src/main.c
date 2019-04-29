#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "files.h"
#include "fft.h"



#define MAX_BEATS 256
#define MAX_PEAKS 4096


#define PI 3.1415926535f
#define FFT_ORDER 9
#define WINDOW_SIZE (1<<FFT_ORDER)
#define N_HOPS 4
#define HOP_SIZE (WINDOW_SIZE/N_HOPS)

typedef struct
{
	int delta;
	int time_samples;
	float amplitude;
	int is_beat;
} PEAK_INFO;

float fast_downsample(float * data, int size, int factor)
{
	for(int i = 0; i < size/factor; i++)
	{
		data[i] = data[i*factor];
		for(int j = 1; j < factor; j++)
			data[i] += data[i*factor+j];
		
		data[i] *= 1.0f/(float)factor;
	}
}

WAV_DATA oss_transform(float * data, int size, float rate, float * output)
{
	
	double window_r[WINDOW_SIZE],window_i[WINDOW_SIZE];
	float energy[WINDOW_SIZE]={0},delta[WINDOW_SIZE]={0};
	float convolution_window[WINDOW_SIZE];
	
	for(int i=0; i<WINDOW_SIZE; i++)
		convolution_window[i] = (0.5f - cos(2.0f*PI*(float)i/(float)(WINDOW_SIZE-1)) / 2.0f);
	
	float last_sample = 0.0f;
	for(int w=0; w < size - WINDOW_SIZE; w+=WINDOW_SIZE)
	{
		for(int h=0; h < WINDOW_SIZE; h+=HOP_SIZE)
		{
			int t = h+w;
			for(int i=0; i<WINDOW_SIZE; i++)
			{
				window_r[i]=data[t+i]*convolution_window[i];
				window_i[i]=0.0f;
			}
		
			FFT(1,FFT_ORDER,window_r,window_i);
			
			float total_energy = 0.0f, total_delta = 0.0f;
			for(int i=1; i < WINDOW_SIZE; i++)
			{
				float last_energy = energy[i];
				total_energy += energy[i] = log(1.0f+1000.0f*sqrtf(window_r[i]*window_r[i] + window_i[i]*window_i[i]));
				if(energy[i]-last_energy < 0)
					total_delta += delta[i] = 0.0f;
				else
					total_delta += delta[i] = energy[i]-last_energy;
			}
			
			float out;
			
			out =  (total_energy) / (float) (2.0f*WINDOW_SIZE*N_HOPS);
			out *= (total_delta) / (float) (2.0f*WINDOW_SIZE*N_HOPS);
			
			float c = 0.99f;
			float lo = out;
			last_sample = lo = (1.0f-c)*lo+c*last_sample;
			
			for(int i=0; i < WINDOW_SIZE; i++)
				output[t+i] += (out-lo) * convolution_window[i];
		}
	}
	
	
	if(1)
	{
		// sonification
		for(int i = 0; i < size; i++)
			data[i] = output[i] * (output[i]<0.0f? 0.0f:((float)rand() / (float)RAND_MAX));

		save_wav("out/oss.wav",data,size);
	}
	
	fast_downsample(output,size,HOP_SIZE);
	
	WAV_DATA wav;
	wav.data = output;
	wav.size = size / HOP_SIZE;
	wav.rate = rate / HOP_SIZE;
	
	return wav;
}


int find_peaks(PEAK_INFO * peaks, float * oss, int size)
{
	int beat_idx = 0, n_peaks = 0;
	
	for(int i = 1; i < size && n_peaks < MAX_PEAKS; i++)
	if(i==1 || (oss[i-1] < 0.0f && oss[i] > 0.0f))
	{
		int arg_max=i,i0,i1;
		float max = oss[i];

		i0 = i;
		
		for(; i < size && oss[i]>0.0f; i++)
		if(max < oss[i]) 
		{
			arg_max = i;
			max = oss[i]; 
		}
		
		i1 = i;
		
		peaks[n_peaks].time_samples = arg_max;
		peaks[n_peaks].amplitude = max;
		
		n_peaks++;		
	}
	
	for(int i = 1; i < n_peaks; i++)
		peaks[i].delta = peaks[i].time_samples - peaks[i-1].time_samples;
	peaks[0].delta = 0;
	
	return n_peaks;
}


int find_beats(PEAK_INFO * beats, PEAK_INFO * peaks, int n_peaks, PEAK_INFO * ground, int n_ground)
{
	if(n_ground < 2) return 0;
	
	int n_beats = 0;
	int peak_idx = 0, ground_idx = 0;	

	while(peak_idx < n_peaks && ground_idx < n_ground && n_beats < MAX_BEATS)
	{
		int start_ground = ground_idx;
		int start_beat = n_beats;
		for(;;)
		{
			beats[n_beats++] = ground[ground_idx++];

			if(ground_idx >= n_ground || n_beats >= MAX_BEATS) break;
			
			if(ground_idx-1!=start_ground)
			if(ground[ground_idx].delta > ground[ground_idx-1].delta*2 ) break;		
		}
		
		while(peaks[peak_idx].time_samples < beats[n_beats-1].time_samples )
		{
			peak_idx++;
		}
		
		n_beats--;
		PEAK_INFO last_beat = beats[n_beats];
		int spb=last_beat.delta, est_spb=last_beat.delta;
		
		for(int i = peak_idx; i < n_peaks && n_beats < MAX_BEATS; i++)
		{		
			if(ground_idx != n_ground)
			if(last_beat.time_samples + spb/2 >= ground[ground_idx].time_samples)
				break;

			int delta = peaks[i].time_samples - last_beat.time_samples;
			
			int err = delta - est_spb;
			//float err_w = 8.0f*err*(1.0f-peaks[i].amplitude);
			int err_next = (peaks[i+1].time_samples - last_beat.time_samples) - est_spb;
			
			int tolerance = spb*0.15f;
			
			//printf("%f err%d errn%d tol%d espb%d\n",peaks[i].time_samples/(44100.0f/(float)HOP_SIZE), err, err_next, tolerance, est_spb);
			
			if( //peaks[i].is_beat ||
				
				err >= 0 && err <= tolerance
				|| fabsf(err) <= fabsf(err_next) && fabsf(err) <= tolerance
				// || fabsf(err_w) <= spb*0.5f
			
			)
			{
				int fill = est_spb/spb;
				
				for(int j = 0; j < fill; j++)
				{
					beats[n_beats] = last_beat;
					beats[n_beats].time_samples += j*spb;
					beats[n_beats].is_beat = j==0? 1:0;
					
					//beats[n_beats].delta = delta/fill;
					n_beats++;
				}
				
				last_beat = peaks[i];
				
				if(est_spb==spb) // only update spb when confident
				if(n_beats-start_beat > 4) // first might be invalid
				{
					int sum = 0;
					for(int k = 0; k < 4; k++)
						sum += beats[n_beats-k-1].time_samples - beats[n_beats-k-2].time_samples;
					
					spb = sum/4;
				}
				
				est_spb = spb;
			}
			else if (err > tolerance || err_next > tolerance) 
			{
				if(est_spb/spb >= 4) break;
				est_spb *= 2;
			}
		}
		
		if(last_beat.time_samples + spb/2 < ground[ground_idx].time_samples)
		if(n_beats < MAX_BEATS)
		{
			beats[n_beats] = last_beat;
			n_beats++;
		}
	}
	
	while(ground_idx < n_ground && n_beats < MAX_BEATS)
		beats[n_beats++] = ground[ground_idx++];			
	
	return n_beats;
	
}

int main(int argc, char * argv[])
{
	
	// speed up stdout by buffering
	//static char buffer[4096] ;
    //setvbuf( stdout, buffer, _IOFBF, sizeof(buffer) );
	
	enum MODES
	{
		USAGE,
		PEAKS,
		BEATS,
		TEMPO,
	};
	
	
	
	int mode;
	
	if(argc < 2) mode = USAGE;
	else if(strcmp(argv[1],"PEAKS")==0) mode = PEAKS;
	else if(strcmp(argv[1],"BEATS")==0) mode = BEATS;
	else if(strcmp(argv[1],"TEMPO")==0) mode = TEMPO;
	else if(strcmp(argv[1],"USAGE")==0) mode = USAGE;
	
	switch(mode)
	{
		case PEAKS: if(argc < 3) mode = USAGE; break;
		case BEATS: case TEMPO: if(argc < 4) mode = USAGE; break;
	}
	
	
	WAV_DATA wav,oss;
	if(argc>=3)
	{
		wav = load_wav(argv[2]);
		float * oss_buffer = malloc(sizeof(float)*wav.size); memset(oss_buffer,0,sizeof(float)*wav.size);
		oss = oss_transform(wav.data,wav.size,wav.rate,oss_buffer);
	}
	
	
	if(argc<3)
	{
		printf(
		"\nUSAGE:\n"
		"\tmain [USAGE|PEAKS|TEMPO|BEATS] FILE_PATH B_0 B_1 ... B_n\n"
		"\tFILE_PATH the full or relative path of a mono 44.1khz F32 wav file.\n"
		"\tPEAKS: produce a list of peak events.\n"
		"\tBEATS: produce a list of predicted beats.\n"
		"\tTEMPO: produce a list of tempos from predicted beats.\n"
		"\tB_0...B_n known beats provided in seconds.\n");
		return 0;
	}

	PEAK_INFO ground[MAX_PEAKS] = {0};
	int n_ground=0;
	
	for(int i = 0; i < argc-3 && i < MAX_BEATS; i++)
	{
		ground[i].time_samples = atof(argv[i+3]) * oss.rate;
		
		if(i==0)
			ground[i].delta = 0;
		else 
			ground[i].delta = ground[i].time_samples - ground[i-1].time_samples;
		
		n_ground++;
	}
	
	FILE * fp = fopen("out/info.txt","wb+");
	switch(mode)
	{
		case PEAKS:
		{
			PEAK_INFO peaks[MAX_PEAKS]={0};
			int n_peaks = find_peaks(peaks,oss.data,oss.size);
			
			
			for(int i = 0; i < n_peaks; i++)
				fprintf(fp,"%f\n", peaks[i].time_samples / oss.rate);
		}
		break;
		case BEATS:
		{
			PEAK_INFO peaks[MAX_PEAKS]={0}, beats[MAX_BEATS]={0};
			int n_peaks = find_peaks(peaks,oss.data,oss.size);
			int n_beats = find_beats(beats,peaks,n_peaks,ground,n_ground);
			for(int i = 0; i < n_beats; i++)
				fprintf(fp,"%f\n", beats[i].time_samples / oss.rate);	
		}
		break;
		case TEMPO:
		{
			PEAK_INFO peaks[MAX_PEAKS]={0}, beats[MAX_BEATS]={0};
			int n_peaks = find_peaks(peaks,oss.data,oss.size);
			int n_beats = find_beats(beats,peaks,n_peaks,ground,n_ground);
			for(int i = 0; i < n_beats-1; i++)
				fprintf(fp,"%f\n", 60.0f*(beats[i+1].time_samples - beats[i].time_samples)/oss.rate);
		}
		break;
	}
	fclose(fp);
	//fflush(stdout);
	
	return 0;
}