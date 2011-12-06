#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

unsigned int *km_mapping_p; // positive coverage
unsigned int *km_mapping_n; // negative coverage
unsigned int *km_mapping_t; // temp coverage counter
unsigned int km_mapping_size = 0; // size
unsigned int km_mapping_pending = 0; // counter of pending output (we output data to /tmp/km.out every 65536 cycle
unsigned int km_mapping_t_pending = 0; // any counter pending in t

#define KM_MAX_LINEFREQ (4096)

void km_mapping_init(unsigned int lnsiz)
{
	km_mapping_p = (unsigned int*)calloc(lnsiz, sizeof(unsigned int));
	km_mapping_n = (unsigned int*)calloc(lnsiz, sizeof(unsigned int));
	km_mapping_t = (unsigned int*)calloc(lnsiz, sizeof(unsigned int));
	km_mapping_t_pending = 0;
	km_mapping_pending = 0;
	km_mapping_size = lnsiz;
}

void km_mapping_out()
{
	int i;
	FILE* out = fopen("/tmp/km.0.out", "w+");
	fprintf(out, "%u\n", km_mapping_size);
	for (i = 0; i < km_mapping_size; i++)
		fprintf(out, "%u %u %u\n", km_mapping_p[i], km_mapping_n[i], km_mapping_t[i]);
	fsync(fileno(out));
	fclose(out);
	rename("/tmp/km.0.out", "/tmp/km.out");
}

void km_mapping_for(unsigned int lnno, int flag)
{
	if (lnno >= km_mapping_size)
		return;
	switch (flag)
	{
		case 0:
			if (km_mapping_t[lnno] < KM_MAX_LINEFREQ)
			{
				++km_mapping_t[lnno];
				++km_mapping_t_pending;
			}
			break;
		case 1:
			if (km_mapping_t_pending > 0)
			{
				int i;
				for (i = 0; i < km_mapping_size; i++)
				{
					km_mapping_p[i] += km_mapping_t[i];
					km_mapping_t[i] = 0;
					if (km_mapping_p[i] > KM_MAX_LINEFREQ)
						km_mapping_p[i] = KM_MAX_LINEFREQ;
				}
				km_mapping_t_pending = 0;
			}
			if (km_mapping_p[lnno] < KM_MAX_LINEFREQ)
				++km_mapping_p[lnno];
			break;
		case -1:
			if (km_mapping_t_pending > 0)
			{
				int i;
				for (i = 0; i < km_mapping_size; i++)
				{
					km_mapping_n[i] += km_mapping_t[i];
					km_mapping_t[i] = 0;
					if (km_mapping_n[i] > KM_MAX_LINEFREQ)
						km_mapping_n[i] = KM_MAX_LINEFREQ;
				}
				km_mapping_t_pending = 0;
			}
			if (km_mapping_n[lnno] < KM_MAX_LINEFREQ)
				++km_mapping_n[lnno];
			break;
	}
	if ((++km_mapping_pending) >= 0x20000)
	{
		km_mapping_out();
		km_mapping_pending = 0;
	}
}

void km_mapping_close()
{
	km_mapping_out();
	free(km_mapping_p);
	free(km_mapping_n);
	free(km_mapping_t);
}
