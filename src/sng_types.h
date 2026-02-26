#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sng
{

// NoteMask technique flags
enum NoteMask : uint32_t
{
    CHORD = 0x00000002,
    OPEN = 0x00000004,
    FRETHANDMUTE = 0x00000008,
    TREMOLO = 0x00000010,
    HARMONIC = 0x00000020,
    PALMMUTE = 0x00000040,
    SLAP = 0x00000080,
    PLUCK = 0x00000100,
    HAMMERON = 0x00000200,
    PULLOFF = 0x00000400,
    SLIDE = 0x00000800,
    BEND = 0x00001000,
    SUSTAIN = 0x00002000,
    TAP = 0x00004000,
    PINCHHARMONIC = 0x00008000,
    VIBRATO = 0x00010000,
    MUTE = 0x00020000,
    IGNORE = 0x00040000,
    LEFTHAND = 0x00080000,
    RIGHTHAND = 0x00100000,
    HIGHDENSITY = 0x00200000,
    SLIDEUNPITCHEDTO = 0x00400000,
    SINGLE = 0x00800000,
    CHORDNOTES = 0x01000000,
    DOUBLESTOP = 0x02000000,
    ACCENT = 0x04000000,
    PARENT = 0x08000000,
    CHILD = 0x10000000,
    ARPEGGIO = 0x20000000,
    STRUM = 0x80000000,
};

struct BendValue
{
    float m_time = 0;
    float m_step = 0;
    int16_t m_unk1 = 0;
    uint8_t m_unk2 = 0;
    uint8_t m_unk3 = 0;
};

// Section 1: BPM
struct Bpm
{
    float m_time = 0;
    int16_t m_measure = 0;
    int16_t m_beat = 0;
    int32_t m_phrase_iteration = 0;
    int32_t m_mask = 0;
};

// Section 2: Phrase
struct Phrase
{
    uint8_t m_solo = 0;
    uint8_t m_disparity = 0;
    uint8_t m_ignore = 0;
    uint8_t m_padding = 0;
    int32_t m_max_difficulty = 0;
    int32_t m_phrase_iteration_links = 0;
    std::string m_name;
};

// Section 3: Chord
struct Chord
{
    uint32_t m_mask = 0;
    int8_t m_frets[6] = {};
    int8_t m_fingers[6] = {};
    int32_t m_notes[6] = {};
    std::string m_name;
};

// Section 4: ChordNotes - BendData per string
struct BendData
{
    std::vector<BendValue> m_bend_values;
    int32_t m_used_count = 0;
};

struct ChordNotes
{
    uint32_t m_mask[6] = {};
    BendData m_bend_data[6] = {};
    uint8_t m_slide_to[6] = {};
    uint8_t m_slide_unpitch_to[6] = {};
    int16_t m_vibrato[6] = {};
};

// Section 5: Vocal
struct Vocal
{
    float m_time = 0;
    int32_t m_note = 0;
    float m_length = 0;
    std::string m_lyric;
};

// Section 6: SymbolsHeader
struct SymbolsHeader
{
    int32_t m_unk1 = 0;
    int32_t m_unk2 = 0;
    int32_t m_unk3 = 0;
    int32_t m_unk4 = 0;
    int32_t m_unk5 = 0;
    int32_t m_unk6 = 0;
    int32_t m_unk7 = 0;
    int32_t m_unk8 = 0;
};

// Section 7: SymbolsTexture
struct SymbolsTexture
{
    std::string m_font_name;
    int32_t m_font_path_length = 0;
    int32_t m_unk = 0;
    int32_t m_width = 0;
    int32_t m_height = 0;
};

// Section 8: SymbolDefinition
struct SymbolDefinition
{
    std::string m_text;
    float m_rect_outer[4] = {};
    float m_rect_inner[4] = {};
};

// Section 9: PhraseIteration
struct PhraseIteration
{
    int32_t m_phrase_id = 0;
    float m_start_time = 0;
    float m_next_phrase_time = 0;
    int32_t m_difficulty[3] = {};
};

// Section 10: PhraseExtraInfo
struct PhraseExtraInfo
{
    int32_t m_phrase_id = 0;
    int32_t m_difficulty = 0;
    int32_t m_empty = 0;
    uint8_t m_level_jump = 0;
    int16_t m_redundant = 0;
    uint8_t m_padding = 0;
};

// Section 11: NLinkedDifficulty
struct NLinkedDifficulty
{
    int32_t m_level_break = 0;
    std::vector<int32_t> m_nld_phrases;
};

// Section 12: Action
struct Action
{
    float m_time = 0;
    std::string m_name;
};

// Section 13: Event
struct Event
{
    float m_time = 0;
    std::string m_name;
};

// Section 14: Tone
struct Tone
{
    float m_time = 0;
    int32_t m_tone_id = 0;
};

// Section 15: DNA
struct Dna
{
    float m_time = 0;
    int32_t m_dna_id = 0;
};

// Section 16: Section (song sections)
struct Section
{
    std::string m_name;
    int32_t m_number = 0;
    float m_start_time = 0;
    float m_end_time = 0;
    int32_t m_start_phrase_iteration_index = 0;
    int32_t m_end_phrase_iteration_index = 0;
    uint8_t m_string_bytes[36] = {};
};

// Section 17: Arrangement sub-structs
struct Anchor
{
    float m_start_time = 0;
    float m_end_time = 0;
    float m_unk1 = 0;
    float m_unk2 = 0;
    int32_t m_fret = 0;
    int32_t m_width = 0;
    int32_t m_phrase_iteration_index = 0;
};

struct AnchorExtension
{
    float m_beat_time = 0;
    uint8_t m_fret_id = 0;
    int32_t m_unk2 = 0;
    int16_t m_unk3 = 0;
    uint8_t m_unk4 = 0;
};

struct Fingerprint
{
    int32_t m_chord_id = 0;
    float m_start_time = 0;
    float m_end_time = 0;
    float m_unk1 = 0;
    float m_unk2 = 0;
};

struct Note
{
    uint32_t m_mask = 0;
    uint32_t m_flags = 0;
    uint32_t m_hash = 0;
    float m_time = 0;
    int8_t m_string = 0;
    int8_t m_fret = 0;
    int8_t m_anchor_fret = 0;
    int8_t m_anchor_width = 0;
    int32_t m_chord_id = 0;
    int32_t m_chord_notes_id = 0;
    int32_t m_phrase_id = 0;
    int32_t m_phrase_iteration_id = 0;
    int16_t m_fingerprint_id[2] = {};
    int16_t m_next_iteration = 0;
    int16_t m_prev_iteration = 0;
    int16_t m_parent_prev_note = 0;
    uint8_t m_slide_to = 0;
    uint8_t m_slide_unpitch_to = 0;
    uint8_t m_left_hand = 0;
    uint8_t m_tap = 0;
    uint8_t m_pick_direction = 0;
    uint8_t m_slap = 0;
    uint8_t m_pluck = 0;
    int16_t m_vibrato = 0;
    float m_sustain = 0;
    float m_max_bend = 0;
    std::vector<BendValue> m_bend_values;
};

struct Arrangement
{
    int32_t m_difficulty = 0;
    std::vector<Anchor> m_anchors;
    std::vector<AnchorExtension> m_anchor_extensions;
    std::vector<Fingerprint> m_fingerprints_arpeggio;
    std::vector<Fingerprint> m_fingerprints_handshape;
    std::vector<Note> m_notes;

    int32_t m_phrase_count = 0;
    std::vector<float> m_average_notes_per_iteration;
    int32_t m_phrase_iteration_count1 = 0;
    std::vector<int32_t> m_notes_in_iteration1;
    int32_t m_phrase_iteration_count2 = 0;
    std::vector<int32_t> m_notes_in_iteration2;
};

// Section 18: Metadata
struct Metadata
{
    double m_max_score = 0;
    double m_max_notes_and_chords = 0;
    double m_max_notes_and_chords_real = 0;
    double m_point_per_note = 0;
    float m_first_beat_length = 0;
    float m_start_time = 0;
    uint8_t m_capo_fret_id = 0;
    std::string m_last_conversion_date_time;
    int16_t m_part = 0;
    float m_song_length = 0;
    int32_t m_string_count = 0;
    std::vector<int16_t> m_tuning;
    float m_unk1 = 0;
    float m_unk2 = 0;
    int32_t m_max_difficulty = 0;
};

// Top-level container for all parsed SNG data
struct SngData
{
    std::vector<Bpm> m_bpms;
    std::vector<Phrase> m_phrases;
    std::vector<Chord> m_chords;
    std::vector<ChordNotes> m_chord_notes;
    std::vector<Vocal> m_vocals;
    std::vector<SymbolsHeader> m_symbols_headers;
    std::vector<SymbolsTexture> m_symbols_textures;
    std::vector<SymbolDefinition> m_symbol_definitions;
    std::vector<PhraseIteration> m_phrase_iterations;
    std::vector<PhraseExtraInfo> m_phrase_extra_infos;
    std::vector<NLinkedDifficulty> m_nlinked_difficulties;
    std::vector<Action> m_actions;
    std::vector<Event> m_events;
    std::vector<Tone> m_tones;
    std::vector<Dna> m_dnas;
    std::vector<Section> m_sections;
    std::vector<Arrangement> m_arrangements;
    Metadata m_metadata;
};

} // namespace sng
