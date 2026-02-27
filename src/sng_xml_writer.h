#pragma once

#include "sng_types.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>

struct SngManifestArrangementProperties
{
    int represent = 0;
    int bonus_arr = 0;
    int standard_tuning = 0;
    int non_standard_chords = 0;
    int barre_chords = 0;
    int power_chords = 0;
    int drop_d_power = 0;
    int open_chords = 0;
    int finger_picking = 0;
    int pick_direction = 0;
    int double_stops = 0;
    int palm_mutes = 0;
    int harmonics = 0;
    int pinch_harmonics = 0;
    int hopo = 0;
    int tremolo = 0;
    int slides = 0;
    int unpitched_slides = 0;
    int bends = 0;
    int tapping = 0;
    int vibrato = 0;
    int fret_hand_mutes = 0;
    int slap_pop = 0;
    int two_finger_picking = 0;
    int fifths_and_octaves = 0;
    int syncopation = 0;
    int bass_pick = 0;
    int sustain = 0;
    int path_lead = 0;
    int path_rhythm = 0;
    int path_bass = 0;
};

struct SngManifestMetadata
{
    std::optional<std::string> title;
    std::optional<std::string> arrangement;
    std::optional<float> cent_offset;
    std::optional<std::string> song_name_sort;
    std::optional<float> average_tempo;
    std::optional<std::string> artist_name;
    std::optional<std::string> artist_name_sort;
    std::optional<std::string> album_name;
    std::optional<std::string> album_name_sort;
    std::optional<int> album_year;
    std::optional<SngManifestArrangementProperties> arrangement_properties;
    std::optional<std::string> tone_base;
    std::array<std::optional<std::string>, 4> tone_names;
};

class SngXmlWriter
{
  public:
    static void Write(const sng::SngData& sng, const std::filesystem::path& output_path,
                      const SngManifestMetadata* manifest = nullptr);
};
