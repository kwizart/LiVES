// noise.c
// Weed plugin
// (c) G. Finch (salsaman) 2005
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-plugin.h"

///////////////////////////////////////////////////////////////////

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]={131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional

/////////////////////////////////////////////////////////////

typedef struct {
  uint32_t fastrand_val;
} static_data;

static inline uint32_t fastrand(static_data *sdata)
{
#define rand_a 1073741789L
#define rand_c 32749L

  return ((sdata->fastrand_val*=rand_a) + rand_c);
}

int noise_init(weed_plant_t *inst) {
  static_data *sdata=weed_malloc(sizeof(sdata));
  if(sdata == NULL ) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->fastrand_val=0; // TODO - seed with random seed
  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}


int noise_deinit(weed_plant_t *inst) {
  struct _sdata *sdata;
  int error;

  sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  if (sdata != NULL) {
    weed_free(sdata);
  }

  return WEED_NO_ERROR;
}


int noise_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channel,"width",&error)*3;
  int height=weed_get_int_value(in_channel,"height",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  unsigned char *end=src+height*irowstride;
  register int j;
  static_data *sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  sdata->fastrand_val=timestamp&0x0000FFFF;


  for (;src<end;src+=irowstride) {
    for (j=0;j<width;j++) {
      dst[j]=src[j]+((fastrand(sdata)&0xF8000000)>>27)-16;
    }
    dst+=orowstride;
  }
  return WEED_NO_ERROR;
}


weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]={WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};
    
    weed_plant_t *filter_class=weed_filter_class_init("noise","salsaman",1,WEED_FILTER_HINT_IS_POINT_EFFECT,&noise_init,&noise_process,&noise_deinit,in_chantmpls,out_chantmpls,NULL,NULL);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);
    
    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}
