#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sng
{

// NoteMask technique flags
enum class NoteMask : uint32_t
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
    CHORDPANEL = 0x80000000,
};

using enum NoteMask;

struct BendValue
{
    float time = 0;
    float step = 0;
    int16_t unk1 = 0;
    uint8_t unk2 = 0;
    uint8_t unk3 = 0;
};

// Section 1: BPM
struct Bpm
{
    float time = 0;
    int16_t measure = 0;
    int16_t beat = 0;
    int32_t phrase_iteration = 0;
    int32_t mask = 0;
};

// Section 2: Phrase
struct Phrase
{
    uint8_t solo = 0;
    uint8_t disparity = 0;
    uint8_t ignore = 0;
    uint8_t padding = 0;
    int32_t max_difficulty = 0;
    int32_t phrase_iteration_links = 0;
    std::string name;
};

// Section 3: Chord
struct Chord
{
    uint32_t mask = 0;
    std::array<int8_t, 6> frets{};
    std::array<int8_t, 6> fingers{};
    std::array<int32_t, 6> notes{};
    std::string name;
};

// Section 4: ChordNotes - BendData per string
struct BendData
{
    std::vector<BendValue> bend_values;
    int32_t used_count = 0;
};

struct ChordNotes
{
    std::array<uint32_t, 6> mask{};
    std::array<BendData, 6> bend_data{};
    std::array<int8_t, 6> slide_to{};
    std::array<int8_t, 6> slide_unpitch_to{};
    std::array<int16_t, 6> vibrato{};
};

// Section 5: Vocal
struct Vocal
{
    float time = 0;
    int32_t note = 0;
    float length = 0;
    std::string lyric;
};

// Section 6: SymbolsHeader
struct SymbolsHeader
{
    int32_t unk1 = 0;
    int32_t unk2 = 0;
    int32_t unk3 = 0;
    int32_t unk4 = 0;
    int32_t unk5 = 0;
    int32_t unk6 = 0;
    int32_t unk7 = 0;
    int32_t unk8 = 0;
};

// Section 7: SymbolsTexture
struct SymbolsTexture
{
    std::string font_name;
    int32_t font_path_length = 0;
    int32_t unk = 0;
    int32_t width = 0;
    int32_t height = 0;
};

// Section 8: SymbolDefinition
struct SymbolDefinition
{
    std::string text;
    std::array<float, 4> rect_outer{};
    std::array<float, 4> rect_inner{};
};

// Section 9: PhraseIteration
struct PhraseIteration
{
    int32_t phrase_id = 0;
    float start_time = 0;
    float next_phrase_time = 0;
    std::array<int32_t, 3> difficulty{};
};

// Section 10: PhraseExtraInfo
struct PhraseExtraInfo
{
    int32_t phrase_id = 0;
    int32_t difficulty = 0;
    int32_t empty = 0;
    uint8_t level_jump = 0;
    int16_t redundant = 0;
    uint8_t padding = 0;
};

// Section 11: NLinkedDifficulty
struct NLinkedDifficulty
{
    int32_t level_break = 0;
    std::vector<int32_t> nld_phrases;
};

// Section 12: Action
struct Action
{
    float time = 0;
    std::string name;
};

// Section 13: Event
struct Event
{
    float time = 0;
    std::string name;
};

// Section 14: Tone
struct Tone
{
    float time = 0;
    int32_t tone_id = 0;
};

// Section 15: DNA
struct Dna
{
    float time = 0;
    int32_t dna_id = 0;
};

// Section 16: Section (song sections)
struct Section
{
    std::string name;
    int32_t number = 0;
    float start_time = 0;
    float end_time = 0;
    int32_t start_phrase_iteration_index = 0;
    int32_t end_phrase_iteration_index = 0;
    std::array<uint8_t, 36> string_bytes{};
};

// Section 17: Arrangement sub-structs
struct Anchor
{
    float start_time = 0;
    float end_time = 0;
    float unk1 = 0;
    float unk2 = 0;
    int32_t fret = 0;
    int32_t width = 0;
    int32_t phrase_iteration_index = 0;
};

struct AnchorExtension
{
    float beat_time = 0;
    int8_t fret_id = 0;
    int32_t unk2 = 0;
    int16_t unk3 = 0;
    int8_t unk4 = 0;
};

struct Fingerprint
{
    int32_t chord_id = 0;
    float start_time = 0;
    float end_time = 0;
    float unk1 = 0;
    float unk2 = 0;
};

struct Note
{
    uint32_t mask = 0;
    uint32_t flags = 0;
    uint32_t hash = 0;
    float time = 0;
    int8_t string = 0;
    int8_t fret = 0;
    int8_t anchor_fret = 0;
    int8_t anchor_width = 0;
    int32_t chord_id = 0;
    int32_t chord_notes_id = 0;
    int32_t phrase_id = 0;
    int32_t phrase_iteration_id = 0;
    std::array<int16_t, 2> fingerprint_id{};
    int16_t next_iteration = 0;
    int16_t prev_iteration = 0;
    int16_t parent_prev_note = 0;
    int8_t slide_to = 0;
    int8_t slide_unpitch_to = 0;
    int8_t left_hand = 0;
    int8_t tap = 0;
    int8_t pick_direction = 0;
    int8_t slap = 0;
    int8_t pluck = 0;
    int16_t vibrato = 0;
    float sustain = 0;
    float max_bend = 0;
    std::vector<BendValue> bend_values;
};

struct Arrangement
{
    int32_t difficulty = 0;
    std::vector<Anchor> anchors;
    std::vector<AnchorExtension> anchor_extensions;
    std::vector<Fingerprint> fingerprints_arpeggio;
    std::vector<Fingerprint> fingerprints_handshape;
    std::vector<Note> notes;

    int32_t phrase_count = 0;
    std::vector<float> average_notes_per_iteration;
    int32_t phrase_iteration_count1 = 0;
    std::vector<int32_t> notes_in_iteration1;
    int32_t phrase_iteration_count2 = 0;
    std::vector<int32_t> notes_in_iteration2;
};

// Section 18: Metadata
struct Metadata
{
    double max_score = 0;
    double max_notes_and_chords = 0;
    double max_notes_and_chords_real = 0;
    double point_per_note = 0;
    float first_beat_length = 0;
    float start_time = 0;
    int8_t capo_fret_id = 0;
    std::string last_conversion_date_time;
    int16_t part = 0;
    float song_length = 0;
    int32_t string_count = 0;
    std::vector<int16_t> tuning;
    float first_note_time = 0;
    float first_note_time2 = 0;
    int32_t max_difficulty = 0;
};

// Top-level container for all parsed SNG data
struct SngData
{
    std::vector<Bpm> bpms;
    std::vector<Phrase> phrases;
    std::vector<Chord> chords;
    std::vector<ChordNotes> chord_notes;
    std::vector<Vocal> vocals;
    std::vector<SymbolsHeader> symbols_headers;
    std::vector<SymbolsTexture> symbols_textures;
    std::vector<SymbolDefinition> symbol_definitions;
    std::vector<PhraseIteration> phrase_iterations;
    std::vector<PhraseExtraInfo> phrase_extra_infos;
    std::vector<NLinkedDifficulty> nlinked_difficulties;
    std::vector<Action> actions;
    std::vector<Event> events;
    std::vector<Tone> tones;
    std::vector<Dna> dnas;
    std::vector<Section> sections;
    std::vector<Arrangement> arrangements;
    Metadata metadata;
};

} // namespace sng
