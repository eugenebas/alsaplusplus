#ifndef ALSAPLUSPLUS_MIXER_HPP
#define ALSAPLUSPLUS_MIXER_HPP

#include <alsaplusplus/common.hpp>
#include <alsa/control.h>
#include <alsa/pcm.h>
#include <alsa/mixer.h>

#include <cmath>

#define MAP_VOL_RES (INT32_MAX / 100)
#define MAX_LINEAR_DB_SCALE	24

namespace AlsaPlusPlus
{
  class Mixer
  {
    public:
      Mixer(std::string hw_device, std::string volume_element_name);
      ~Mixer();

      static bool device_exists(std::string hw_device);
      static bool element_exists(std::string hw_device, std::string element_name);
      float inc_vol_pct(float pct, bool mapped = false, snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_MONO);
      float dec_vol_pct(float pct, bool mapped = false, snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_MONO);
      float set_vol_pct(float pct, bool mapped = false);
      float get_cur_vol_pct(bool mapped = false, snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_MONO);
      float mute();
      float unmute();
    private:
      int err;
      std::string device_name;
      snd_mixer_t* mixer_handle;
      snd_mixer_selem_id_t* simple_mixer_handle;
      std::string simple_elem_name;
      snd_mixer_elem_t* element_handle;
      long mute_vol;

      void trim_pct(float& pct);
      void set_vol_raw(long vol);
      long get_cur_vol_raw(snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_MONO);
      void get_vol_range(long* min_vol, long* max_vol);
      float get_normalized_volume(snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_MONO);
      int set_normalized_volume(float pct);
      static int get_mapped_volume_range(snd_mixer_elem_t *elem, long *pmin, long *pmax);
      float get_vol_mapped(snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_MONO);
      void set_vol_mapped(float pct);
  };
}

#endif
