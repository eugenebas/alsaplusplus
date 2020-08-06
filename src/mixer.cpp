#include <alsaplusplus/mixer.hpp>

using namespace AlsaPlusPlus;

constexpr long MINIMAL_VOLUME = 0;
constexpr long MAXIMUM_VOLUME = 65535;

Mixer::Mixer(std::string hw_device, std::string volume_element_name) :
  err(0),
  device_name(hw_device),
  simple_elem_name(volume_element_name)
{
  if ((err = snd_mixer_open(&mixer_handle, 0)) < 0)
    handle_error_code(err, true, "Cannot open handle to mixer device.");

  if ((err = snd_mixer_attach(mixer_handle, device_name.c_str())) < 0)
    handle_error_code(err, true, "Cannot attach mixer to device.");

  if ((err = snd_mixer_selem_register(mixer_handle, NULL, NULL)) < 0)
    handle_error_code(err, true, "Cannot register simple mixer object.");

  if ((err = snd_mixer_load(mixer_handle)) < 0)
    handle_error_code(err, true, "Cannot load sound mixer.");

  snd_mixer_selem_id_alloca(&simple_mixer_handle);
  snd_mixer_selem_id_set_index(simple_mixer_handle, 0);
  snd_mixer_selem_id_set_name(simple_mixer_handle, simple_elem_name.c_str());
  element_handle = snd_mixer_find_selem(mixer_handle, simple_mixer_handle);

  if (element_handle == NULL)
  {
    std::ostringstream oss;
    oss << "Could not find simple mixer element named " << simple_elem_name << ".";
    handle_error_code(static_cast<int>(std::errc::argument_out_of_domain), true, oss.str());
  }
}

Mixer::~Mixer()
{
  snd_mixer_close(mixer_handle);
}

bool Mixer::device_exists(std::string hw_device)
{
  int err;
  snd_mixer_t* temp_handle;

  if ((err = snd_mixer_open(&temp_handle, 0)) < 0)
  {
    handle_error_code(err, false, "Cannot open handle to a mixer device.");
    return false;
  }

  if ((err = snd_mixer_attach(temp_handle, hw_device.c_str())) < 0)
  {
    snd_mixer_close(temp_handle);
    return false;
  }

  snd_mixer_close(temp_handle);
  return true;
}

bool Mixer::element_exists(std::string hw_device, std::string element_name)
{
  int err;
  snd_mixer_t* temp_handle;
  snd_mixer_selem_id_t* simple_temp_handle;

  if ((err = snd_mixer_open(&temp_handle, 0)) < 0)
  {
    handle_error_code(err, false, "Cannot open handle to a mixer device.");
    return false;
  }

  if ((err = snd_mixer_attach(temp_handle, hw_device.c_str())) < 0)
  {
    handle_error_code(err, true, "Cannot attach mixer to device.");
    snd_mixer_close(temp_handle);
    return false;
  }

  if ((err = snd_mixer_selem_register(temp_handle, NULL, NULL)) < 0)
  {
    handle_error_code(err, true, "Cannot register simple mixer object.");
    snd_mixer_close(temp_handle);
    return false;
  }

  if ((err = snd_mixer_load(temp_handle)) < 0)
  {
    handle_error_code(err, true, "Cannot load sound mixer.");
    snd_mixer_close(temp_handle);
    return false;
  }

  snd_mixer_selem_id_alloca(&simple_temp_handle);
  snd_mixer_selem_id_set_index(simple_temp_handle, 0);
  snd_mixer_selem_id_set_name(simple_temp_handle, element_name.c_str());
  snd_mixer_elem_t* elem_handle = snd_mixer_find_selem(temp_handle, simple_temp_handle);

  if (elem_handle == NULL)
  {
    snd_mixer_close(temp_handle);
    return false;
  }

  snd_mixer_close(temp_handle);
  return true;
}


float Mixer::dec_vol_pct(float pct, bool mapped, snd_mixer_selem_channel_id_t channel)
{
  trim_pct(pct);
  float cur_vol = get_cur_vol_pct(mapped, channel);
  return set_vol_pct(cur_vol - pct, mapped);
}

float Mixer::inc_vol_pct(float pct, bool mapped, snd_mixer_selem_channel_id_t channel)
{
  trim_pct(pct);
  float cur_vol = get_cur_vol_pct(mapped, channel);
  return set_vol_pct(cur_vol + pct, mapped);
}

float Mixer::set_vol_pct(float pct, bool mapped)
{
  long min, max;

  trim_pct(pct);
  if(!mapped)
  {
      get_vol_range(&min, &max);
      set_vol_raw((long)((float)min + (pct * (max - min))));
  }
  else
  {
      set_vol_mapped(pct);
  }

  return get_cur_vol_pct(mapped);
}

float Mixer::get_cur_vol_pct(bool mapped, snd_mixer_selem_channel_id_t channel)
{
  if(!mapped)
  {
      long min, max, cur;
      get_vol_range(&min, &max);
      cur = get_cur_vol_raw(channel);
      return round((float)cur / (max - min) * 100.0) / 100.0;
  }
  else
  {
      return get_vol_mapped(channel);
  }
}

float Mixer::mute()
{
  mute_vol = get_cur_vol_raw();
  return set_vol_pct(0);
}

float Mixer::unmute()
{
  set_vol_raw(mute_vol);
  return get_cur_vol_pct();
}

void Mixer::trim_pct(float& pct)
{
  pct = (pct < 0) ? 0 : pct;
  pct = (pct > 1) ? 1 : pct;
}

void Mixer::set_vol_raw(long vol)
{
  err = snd_mixer_selem_set_playback_volume_all(element_handle, vol);

  if (err < 0)
    handle_error_code(err, false, "Cannot set volume to requested value.");
}

int Mixer::get_mapped_volume_range(snd_mixer_elem_t *elem, long *pmin, long *pmax)
{
    *pmin = 0;
    *pmax = MAP_VOL_RES;
    return 0;
}

float Mixer::get_vol_mapped(snd_mixer_selem_channel_id_t channel)
{
    return get_normalized_volume(channel);
}

float Mixer::get_normalized_volume(snd_mixer_selem_channel_id_t channel)
{
    long min, max, value;
    float normalized, min_norm;
    int err = snd_mixer_selem_get_playback_dB_range(element_handle, &min, &max);
    if (err < 0 || min >= max) {
        return get_cur_vol_pct();
    }

    err = snd_mixer_selem_get_playback_dB(element_handle, channel, &value);
    if (err < 0)
        handle_error_code(err, false, "Could not get volume for provided channel.");

    if (max - min <= MAX_LINEAR_DB_SCALE * 100) {
        return (value - min) / (double)(max - min);
    }

    normalized = exp10((value - max) / 6000.0);
    if (min != SND_CTL_TLV_DB_GAIN_MUTE) {
        min_norm = exp10((min - max) / 6000.0);
        normalized = (normalized - min_norm) / (1 - min_norm);
    }

    return normalized;
}

void Mixer::set_vol_mapped(float pct)
{
    long min, max;
    get_mapped_volume_range(element_handle, &min, &max);
    int err = set_normalized_volume(((float)min + (pct * (max - min))) / MAP_VOL_RES);
    if (err < 0)
        handle_error_code(err, false, "Cannot set volume to requested value.");
}

int Mixer::set_normalized_volume(float pct)
{
    long min, max, value;
    double min_norm;
    int err = snd_mixer_selem_get_playback_dB_range(element_handle, &min, &max);
    if (err < 0 || min >= max) {
      get_vol_range(&min, &max);
      set_vol_raw((long)((float)min + (pct * (max - min))));
      return 0;
    }

    if (max - min <= MAX_LINEAR_DB_SCALE * 100) {
        value = lrint(pct * (max - min)) + min;
        return snd_mixer_selem_set_playback_dB_all(element_handle, value, 0);
    }

    if (min != SND_CTL_TLV_DB_GAIN_MUTE) {
        min_norm = exp10((min - max) / 6000.0);
        pct = pct * (1 - min_norm) + min_norm;
    }

    value = lrint(6000.0 * log10(pct)) + max;
    err = snd_mixer_selem_set_playback_dB_all(element_handle, value, 0);
    return err;
}

long Mixer::get_cur_vol_raw(snd_mixer_selem_channel_id_t channel)
{
  long cur_vol;

  err = snd_mixer_selem_get_playback_volume(element_handle, channel, &cur_vol);

  if (err < 0)
    handle_error_code(err, false, "Could not get volume for provided channel.");

  return cur_vol;
}

void Mixer::get_vol_range(long* min_vol, long* max_vol)
{
  err = snd_mixer_selem_get_playback_volume_range(element_handle, min_vol, max_vol);

  if (err < 0)
    handle_error_code(err, false, "Cannot get min/max volume range.");
}
