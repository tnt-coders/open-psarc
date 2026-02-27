#pragma once

#include "sng_types.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>

struct SngManifestArrangementProperties
{
    int m_represent = 0;
    int m_bonus_arr = 0;
    int m_standard_tuning = 0;
    int m_non_standard_chords = 0;
    int m_barre_chords = 0;
    int m_power_chords = 0;
    int m_drop_d_power = 0;
    int m_open_chords = 0;
    int m_finger_picking = 0;
    int m_pick_direction = 0;
    int m_double_stops = 0;
    int m_palm_mutes = 0;
    int m_harmonics = 0;
    int m_pinch_harmonics = 0;
    int m_hopo = 0;
    int m_tremolo = 0;
    int m_slides = 0;
    int m_unpitched_slides = 0;
    int m_bends = 0;
    int m_tapping = 0;
    int m_vibrato = 0;
    int m_fret_hand_mutes = 0;
    int m_slap_pop = 0;
    int m_two_finger_picking = 0;
    int m_fifths_and_octaves = 0;
    int m_syncopation = 0;
    int m_bass_pick = 0;
    int m_sustain = 0;
    int m_path_lead = 0;
    int m_path_rhythm = 0;
    int m_path_bass = 0;
};

struct SngManifestMetadata
{
    std::optional<std::string> m_title;
    std::optional<std::string> m_arrangement;
    std::optional<float> m_cent_offset;
    std::optional<std::string> m_song_name_sort;
    std::optional<float> m_average_tempo;
    std::optional<std::string> m_artist_name;
    std::optional<std::string> m_artist_name_sort;
    std::optional<std::string> m_album_name;
    std::optional<std::string> m_album_name_sort;
    std::optional<int> m_album_year;
    std::optional<SngManifestArrangementProperties> m_arrangement_properties;
    std::optional<std::string> m_tone_base;
    std::array<std::optional<std::string>, 4> m_tone_names;
};

class SngXmlWriter
{
  public:
    static void Write(const sng::SngData& sng, const std::filesystem::path& output_path,
                      const SngManifestMetadata* manifest = nullptr);
};
