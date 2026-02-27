#include "sng_parser.h"

#include "open-psarc/psarc_file.h"

#include <cstring>
#include <format>

namespace
{

class BinaryReader
{
  public:
    explicit BinaryReader(std::span<const uint8_t> data) : m_data(data)
    {
    }

    void EnsureAvailable(size_t bytes) const
    {
        if (m_pos + bytes > m_data.size())
        {
            throw PsarcException(std::format(
                "SNG parse error: read past end at offset {} (need {} bytes, {} available)", m_pos,
                bytes, m_data.size() - m_pos));
        }
    }

    [[nodiscard]] float ReadFloat()
    {
        EnsureAvailable(4);
        float value = 0;
        std::memcpy(&value, m_data.data() + m_pos, 4);
        m_pos += 4;
        return value;
    }

    [[nodiscard]] double ReadDouble()
    {
        EnsureAvailable(8);
        double value = 0;
        std::memcpy(&value, m_data.data() + m_pos, 8);
        m_pos += 8;
        return value;
    }

    [[nodiscard]] int8_t ReadInt8()
    {
        EnsureAvailable(1);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto value = static_cast<int8_t>(m_data[m_pos]);
        m_pos += 1;
        return value;
    }

    [[nodiscard]] uint8_t ReadUInt8()
    {
        EnsureAvailable(1);
        uint8_t value = m_data[m_pos];
        m_pos += 1;
        return value;
    }

    [[nodiscard]] int16_t ReadInt16()
    {
        EnsureAvailable(2);
        uint16_t raw = static_cast<uint16_t>(m_data[m_pos] | (m_data[m_pos + 1] << 8));
        m_pos += 2;
        return static_cast<int16_t>(raw);
    }

    [[nodiscard]] uint16_t ReadUInt16()
    {
        EnsureAvailable(2);
        uint16_t value = static_cast<uint16_t>(m_data[m_pos] | (m_data[m_pos + 1] << 8));
        m_pos += 2;
        return value;
    }

    [[nodiscard]] int32_t ReadInt32()
    {
        EnsureAvailable(4);
        uint32_t raw = m_data[m_pos] | (m_data[m_pos + 1] << 8) | (m_data[m_pos + 2] << 16) |
                       (m_data[m_pos + 3] << 24);
        m_pos += 4;
        return static_cast<int32_t>(raw);
    }

    [[nodiscard]] uint32_t ReadUInt32()
    {
        EnsureAvailable(4);
        uint32_t value = m_data[m_pos] | (m_data[m_pos + 1] << 8) | (m_data[m_pos + 2] << 16) |
                         (m_data[m_pos + 3] << 24);
        m_pos += 4;
        return value;
    }

    [[nodiscard]] std::string ReadFixedString(size_t size)
    {
        EnsureAvailable(size);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const auto* start = reinterpret_cast<const char*>(m_data.data() + m_pos);
        m_pos += size;

        // Find null terminator
        size_t len = 0;
        while (len < size && start[len] != '\0')
        {
            ++len;
        }
        return {start, len};
    }

    void Skip(size_t bytes)
    {
        EnsureAvailable(bytes);
        m_pos += bytes;
    }

    [[nodiscard]] size_t Position() const
    {
        return m_pos;
    }

    [[nodiscard]] size_t Size() const
    {
        return m_data.size();
    }

  private:
    std::span<const uint8_t> m_data;
    size_t m_pos = 0;
};

sng::BendValue ReadBendValue(BinaryReader& reader)
{
    sng::BendValue bv;
    bv.m_time = reader.ReadFloat();
    bv.m_step = reader.ReadFloat();
    bv.m_unk1 = reader.ReadInt16();
    bv.m_unk2 = reader.ReadUInt8();
    bv.m_unk3 = reader.ReadUInt8();
    return bv;
}

// Section 1: BPM
std::vector<sng::Bpm> ReadBpms(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::Bpm> bpms(count);
    for (auto& bpm : bpms)
    {
        bpm.m_time = reader.ReadFloat();
        bpm.m_measure = reader.ReadInt16();
        bpm.m_beat = reader.ReadInt16();
        bpm.m_phrase_iteration = reader.ReadInt32();
        bpm.m_mask = reader.ReadInt32();
    }
    return bpms;
}

// Section 2: Phrases
std::vector<sng::Phrase> ReadPhrases(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::Phrase> phrases(count);
    for (auto& phrase : phrases)
    {
        phrase.m_solo = reader.ReadUInt8();
        phrase.m_disparity = reader.ReadUInt8();
        phrase.m_ignore = reader.ReadUInt8();
        phrase.m_padding = reader.ReadUInt8();
        phrase.m_max_difficulty = reader.ReadInt32();
        phrase.m_phrase_iteration_links = reader.ReadInt32();
        phrase.m_name = reader.ReadFixedString(32);
    }
    return phrases;
}

// Section 3: Chords
std::vector<sng::Chord> ReadChords(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::Chord> chords(count);
    for (auto& chord : chords)
    {
        chord.m_mask = reader.ReadUInt32();
        for (int i = 0; i < 6; ++i)
        {
            // Read as unsigned, map 0xFF to -1
            uint8_t raw = reader.ReadUInt8();
            chord.m_frets[i] = (raw == 0xFF) ? -1 : static_cast<int8_t>(raw);
        }
        for (int i = 0; i < 6; ++i)
        {
            uint8_t raw = reader.ReadUInt8();
            chord.m_fingers[i] = (raw == 0xFF) ? -1 : static_cast<int8_t>(raw);
        }
        for (int& note : chord.m_notes)
        {
            note = reader.ReadInt32();
        }
        chord.m_name = reader.ReadFixedString(32);
    }
    return chords;
}

// Section 4: ChordNotes
std::vector<sng::ChordNotes> ReadChordNotes(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::ChordNotes> chord_notes(count);
    for (auto& cn : chord_notes)
    {
        // NoteMask per string
        for (auto& mask : cn.m_mask)
        {
            mask = reader.ReadUInt32();
        }
        // BendData[6] - each has up to 32 BendValues + UsedCount
        for (auto& bd : cn.m_bend_data)
        {
            bd.m_bend_values.resize(32);
            for (auto& bv : bd.m_bend_values)
            {
                bv = ReadBendValue(reader);
            }
            bd.m_used_count = reader.ReadInt32();
            bd.m_bend_values.resize(bd.m_used_count);
        }
        for (int8_t& i : cn.m_slide_to)
        {
            i = reader.ReadInt8();
        }
        for (int8_t& i : cn.m_slide_unpitch_to)
        {
            i = reader.ReadInt8();
        }
        for (short& i : cn.m_vibrato)
        {
            i = reader.ReadInt16();
        }
    }
    return chord_notes;
}

// Section 5: Vocals
std::vector<sng::Vocal> ReadVocals(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::Vocal> vocals(count);
    for (auto& vocal : vocals)
    {
        vocal.m_time = reader.ReadFloat();
        vocal.m_note = reader.ReadInt32();
        vocal.m_length = reader.ReadFloat();
        vocal.m_lyric = reader.ReadFixedString(48);
    }
    return vocals;
}

// Section 6: SymbolsHeaders
std::vector<sng::SymbolsHeader> ReadSymbolsHeaders(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::SymbolsHeader> headers(count);
    for (auto& header : headers)
    {
        header.m_unk1 = reader.ReadInt32();
        header.m_unk2 = reader.ReadInt32();
        header.m_unk3 = reader.ReadInt32();
        header.m_unk4 = reader.ReadInt32();
        header.m_unk5 = reader.ReadInt32();
        header.m_unk6 = reader.ReadInt32();
        header.m_unk7 = reader.ReadInt32();
        header.m_unk8 = reader.ReadInt32();
    }
    return headers;
}

// Section 7: SymbolsTextures
std::vector<sng::SymbolsTexture> ReadSymbolsTextures(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::SymbolsTexture> textures(count);
    for (auto& texture : textures)
    {
        texture.m_font_name = reader.ReadFixedString(128);
        texture.m_font_path_length = reader.ReadInt32();
        texture.m_unk = reader.ReadInt32();
        texture.m_width = reader.ReadInt32();
        texture.m_height = reader.ReadInt32();
    }
    return textures;
}

// Section 8: SymbolDefinitions
std::vector<sng::SymbolDefinition> ReadSymbolDefinitions(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::SymbolDefinition> definitions(count);
    for (auto& def : definitions)
    {
        def.m_text = reader.ReadFixedString(12);
        for (float& val : def.m_rect_outer)
        {
            val = reader.ReadFloat();
        }
        for (float& val : def.m_rect_inner)
        {
            val = reader.ReadFloat();
        }
    }
    return definitions;
}

// Section 9: PhraseIterations
std::vector<sng::PhraseIteration> ReadPhraseIterations(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::PhraseIteration> iterations(count);
    for (auto& iter : iterations)
    {
        iter.m_phrase_id = reader.ReadInt32();
        iter.m_start_time = reader.ReadFloat();
        iter.m_next_phrase_time = reader.ReadFloat();
        for (int& diff : iter.m_difficulty)
        {
            diff = reader.ReadInt32();
        }
    }
    return iterations;
}

// Section 10: PhraseExtraInfos
std::vector<sng::PhraseExtraInfo> ReadPhraseExtraInfos(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::PhraseExtraInfo> infos(count);
    for (auto& info : infos)
    {
        info.m_phrase_id = reader.ReadInt32();
        info.m_difficulty = reader.ReadInt32();
        info.m_empty = reader.ReadInt32();
        info.m_level_jump = reader.ReadUInt8();
        info.m_redundant = reader.ReadInt16();
        info.m_padding = reader.ReadUInt8();
    }
    return infos;
}

// Section 11: NLinkedDifficulties
std::vector<sng::NLinkedDifficulty> ReadNLinkedDifficulties(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::NLinkedDifficulty> nlds(count);
    for (auto& nld : nlds)
    {
        nld.m_level_break = reader.ReadInt32();
        const auto phrase_count = reader.ReadInt32();
        nld.m_nld_phrases.resize(phrase_count);
        for (auto& phrase : nld.m_nld_phrases)
        {
            phrase = reader.ReadInt32();
        }
    }
    return nlds;
}

// Section 12: Actions
std::vector<sng::Action> ReadActions(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::Action> actions(count);
    for (auto& action : actions)
    {
        action.m_time = reader.ReadFloat();
        action.m_name = reader.ReadFixedString(256);
    }
    return actions;
}

// Section 13: Events
std::vector<sng::Event> ReadEvents(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::Event> events(count);
    for (auto& event : events)
    {
        event.m_time = reader.ReadFloat();
        event.m_name = reader.ReadFixedString(256);
    }
    return events;
}

// Section 14: Tones
std::vector<sng::Tone> ReadTones(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::Tone> tones(count);
    for (auto& tone : tones)
    {
        tone.m_time = reader.ReadFloat();
        tone.m_tone_id = reader.ReadInt32();
    }
    return tones;
}

// Section 15: DNAs
std::vector<sng::Dna> ReadDnas(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::Dna> dnas(count);
    for (auto& dna : dnas)
    {
        dna.m_time = reader.ReadFloat();
        dna.m_dna_id = reader.ReadInt32();
    }
    return dnas;
}

// Section 16: Sections
std::vector<sng::Section> ReadSections(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::Section> sections(count);
    for (auto& section : sections)
    {
        section.m_name = reader.ReadFixedString(32);
        section.m_number = reader.ReadInt32();
        section.m_start_time = reader.ReadFloat();
        section.m_end_time = reader.ReadFloat();
        section.m_start_phrase_iteration_index = reader.ReadInt32();
        section.m_end_phrase_iteration_index = reader.ReadInt32();
        for (unsigned char& byte : section.m_string_bytes)
        {
            byte = reader.ReadUInt8();
        }
    }
    return sections;
}

// Read a Note struct (used in Arrangements)
sng::Note ReadNote(BinaryReader& reader)
{
    sng::Note note;
    note.m_mask = reader.ReadUInt32();
    note.m_flags = reader.ReadUInt32();
    note.m_hash = reader.ReadUInt32();
    note.m_time = reader.ReadFloat();
    note.m_string = reader.ReadInt8();
    note.m_fret = reader.ReadInt8();
    note.m_anchor_fret = reader.ReadInt8();
    note.m_anchor_width = reader.ReadInt8();
    note.m_chord_id = reader.ReadInt32();
    note.m_chord_notes_id = reader.ReadInt32();
    note.m_phrase_id = reader.ReadInt32();
    note.m_phrase_iteration_id = reader.ReadInt32();
    note.m_fingerprint_id[0] = reader.ReadInt16();
    note.m_fingerprint_id[1] = reader.ReadInt16();
    note.m_next_iteration = reader.ReadInt16();
    note.m_prev_iteration = reader.ReadInt16();
    note.m_parent_prev_note = reader.ReadInt16();
    note.m_slide_to = reader.ReadInt8();
    note.m_slide_unpitch_to = reader.ReadInt8();
    note.m_left_hand = reader.ReadInt8();
    note.m_tap = reader.ReadInt8();
    note.m_pick_direction = reader.ReadInt8();
    note.m_slap = reader.ReadInt8();
    note.m_pluck = reader.ReadInt8();
    note.m_vibrato = reader.ReadInt16();
    note.m_sustain = reader.ReadFloat();
    note.m_max_bend = reader.ReadFloat();

    const auto bend_count = reader.ReadInt32();
    note.m_bend_values.resize(bend_count);
    for (auto& bv : note.m_bend_values)
    {
        bv = ReadBendValue(reader);
    }

    return note;
}

// Section 17: Arrangements
std::vector<sng::Arrangement> ReadArrangements(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::Arrangement> arrangements(count);
    for (int arr_idx = 0; arr_idx < count; ++arr_idx)
    {
        auto& arr = arrangements[arr_idx];
        arr.m_difficulty = reader.ReadInt32();

        // Anchors
        const auto anchor_count = reader.ReadInt32();
        arr.m_anchors.resize(anchor_count);
        for (auto& anchor : arr.m_anchors)
        {
            anchor.m_start_time = reader.ReadFloat();
            anchor.m_end_time = reader.ReadFloat();
            anchor.m_unk1 = reader.ReadFloat();
            anchor.m_unk2 = reader.ReadFloat();
            anchor.m_fret = reader.ReadInt32();
            anchor.m_width = reader.ReadInt32();
            anchor.m_phrase_iteration_index = reader.ReadInt32();
        }

        // Anchor Extensions
        const auto anchor_ext_count = reader.ReadInt32();
        arr.m_anchor_extensions.resize(anchor_ext_count);
        for (auto& ext : arr.m_anchor_extensions)
        {
            ext.m_beat_time = reader.ReadFloat();
            ext.m_fret_id = reader.ReadInt8();
            ext.m_unk2 = reader.ReadInt32();
            ext.m_unk3 = reader.ReadInt16();
            ext.m_unk4 = reader.ReadInt8();
        }

        // Fingerprints - handshape
        const auto hs_count = reader.ReadInt32();
        arr.m_fingerprints_handshape.resize(hs_count);
        for (auto& fp : arr.m_fingerprints_handshape)
        {
            fp.m_chord_id = reader.ReadInt32();
            fp.m_start_time = reader.ReadFloat();
            fp.m_end_time = reader.ReadFloat();
            fp.m_unk1 = reader.ReadFloat();
            fp.m_unk2 = reader.ReadFloat();
        }

        // Fingerprints - arpeggio
        const auto arp_count = reader.ReadInt32();
        arr.m_fingerprints_arpeggio.resize(arp_count);
        for (auto& fp : arr.m_fingerprints_arpeggio)
        {
            fp.m_chord_id = reader.ReadInt32();
            fp.m_start_time = reader.ReadFloat();
            fp.m_end_time = reader.ReadFloat();
            fp.m_unk1 = reader.ReadFloat();
            fp.m_unk2 = reader.ReadFloat();
        }

        // Notes
        const auto note_count = reader.ReadInt32();
        arr.m_notes.resize(note_count);
        for (auto& note : arr.m_notes)
        {
            note = ReadNote(reader);
        }

        // Per-arrangement metadata
        arr.m_phrase_count = reader.ReadInt32();
        arr.m_average_notes_per_iteration.resize(arr.m_phrase_count);
        for (auto& avg : arr.m_average_notes_per_iteration)
        {
            avg = reader.ReadFloat();
        }

        arr.m_phrase_iteration_count1 = reader.ReadInt32();
        arr.m_notes_in_iteration1.resize(arr.m_phrase_iteration_count1);
        for (auto& n : arr.m_notes_in_iteration1)
        {
            n = reader.ReadInt32();
        }

        arr.m_phrase_iteration_count2 = reader.ReadInt32();
        arr.m_notes_in_iteration2.resize(arr.m_phrase_iteration_count2);
        for (auto& n : arr.m_notes_in_iteration2)
        {
            n = reader.ReadInt32();
        }
    }
    return arrangements;
}

// Section 18: Metadata
sng::Metadata ReadMetadata(BinaryReader& reader)
{
    sng::Metadata meta;
    meta.m_max_score = reader.ReadDouble();
    meta.m_max_notes_and_chords = reader.ReadDouble();
    meta.m_max_notes_and_chords_real = reader.ReadDouble();
    meta.m_point_per_note = reader.ReadDouble();
    meta.m_first_beat_length = reader.ReadFloat();
    meta.m_start_time = reader.ReadFloat();
    meta.m_capo_fret_id = reader.ReadInt8();
    meta.m_last_conversion_date_time = reader.ReadFixedString(32);
    meta.m_part = reader.ReadInt16();
    meta.m_song_length = reader.ReadFloat();
    meta.m_string_count = reader.ReadInt32();
    meta.m_tuning.resize(meta.m_string_count);
    for (auto& t : meta.m_tuning)
    {
        t = reader.ReadInt16();
    }
    meta.m_first_note_time = reader.ReadFloat();
    meta.m_first_note_time2 = reader.ReadFloat();
    meta.m_max_difficulty = reader.ReadInt32();
    return meta;
}

} // namespace

sng::SngData SngParser::Parse(std::span<const uint8_t> data)
{
    if (data.empty())
    {
        throw PsarcException("SNG data is empty");
    }

    BinaryReader reader(data);
    sng::SngData sng;

    sng.m_bpms = ReadBpms(reader);
    sng.m_phrases = ReadPhrases(reader);
    sng.m_chords = ReadChords(reader);
    sng.m_chord_notes = ReadChordNotes(reader);
    sng.m_vocals = ReadVocals(reader);
    if (!sng.m_vocals.empty())
    {
        sng.m_symbols_headers = ReadSymbolsHeaders(reader);
        sng.m_symbols_textures = ReadSymbolsTextures(reader);
        sng.m_symbol_definitions = ReadSymbolDefinitions(reader);
    }
    sng.m_phrase_iterations = ReadPhraseIterations(reader);
    sng.m_phrase_extra_infos = ReadPhraseExtraInfos(reader);
    sng.m_nlinked_difficulties = ReadNLinkedDifficulties(reader);
    sng.m_actions = ReadActions(reader);
    sng.m_events = ReadEvents(reader);
    sng.m_tones = ReadTones(reader);
    sng.m_dnas = ReadDnas(reader);
    sng.m_sections = ReadSections(reader);
    sng.m_arrangements = ReadArrangements(reader);
    sng.m_metadata = ReadMetadata(reader);

    if (reader.Position() != reader.Size())
    {
        throw PsarcException(
            std::format("SNG parse error: {} bytes remaining after parsing (expected exact match)",
                        reader.Size() - reader.Position()));
    }

    return sng;
}
