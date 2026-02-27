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
        auto raw = static_cast<uint16_t>(m_data[m_pos] | (m_data[m_pos + 1] << 8));
        m_pos += 2;
        return static_cast<int16_t>(raw);
    }

    [[nodiscard]] uint16_t ReadUInt16()
    {
        EnsureAvailable(2);
        auto value = static_cast<uint16_t>(m_data[m_pos] | (m_data[m_pos + 1] << 8));
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
    bv.time = reader.ReadFloat();
    bv.step = reader.ReadFloat();
    bv.unk1 = reader.ReadInt16();
    bv.unk2 = reader.ReadUInt8();
    bv.unk3 = reader.ReadUInt8();
    return bv;
}

// Section 1: BPM
std::vector<sng::Bpm> ReadBpms(BinaryReader& reader)
{
    const auto count = reader.ReadInt32();
    std::vector<sng::Bpm> bpms(count);
    for (auto& bpm : bpms)
    {
        bpm.time = reader.ReadFloat();
        bpm.measure = reader.ReadInt16();
        bpm.beat = reader.ReadInt16();
        bpm.phrase_iteration = reader.ReadInt32();
        bpm.mask = reader.ReadInt32();
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
        phrase.solo = reader.ReadUInt8();
        phrase.disparity = reader.ReadUInt8();
        phrase.ignore = reader.ReadUInt8();
        phrase.padding = reader.ReadUInt8();
        phrase.max_difficulty = reader.ReadInt32();
        phrase.phrase_iteration_links = reader.ReadInt32();
        phrase.name = reader.ReadFixedString(32);
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
        chord.mask = reader.ReadUInt32();
        for (signed char& fret : chord.frets)
        {
            // Read as unsigned, map 0xFF to -1
            uint8_t raw = reader.ReadUInt8();
            fret = (raw == 0xFF) ? int8_t{-1} : static_cast<int8_t>(raw);
        }
        for (signed char& finger : chord.fingers)
        {
            uint8_t raw = reader.ReadUInt8();
            finger = (raw == 0xFF) ? int8_t{-1} : static_cast<int8_t>(raw);
        }
        for (int& note : chord.notes)
        {
            note = reader.ReadInt32();
        }
        chord.name = reader.ReadFixedString(32);
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
        for (auto& mask : cn.mask)
        {
            mask = reader.ReadUInt32();
        }
        // BendData[6] - each has up to 32 BendValues + UsedCount
        for (auto& bd : cn.bend_data)
        {
            bd.bend_values.resize(32);
            for (auto& bv : bd.bend_values)
            {
                bv = ReadBendValue(reader);
            }
            bd.used_count = reader.ReadInt32();
            bd.bend_values.resize(bd.used_count);
        }
        for (int8_t& i : cn.slide_to)
        {
            i = reader.ReadInt8();
        }
        for (int8_t& i : cn.slide_unpitch_to)
        {
            i = reader.ReadInt8();
        }
        for (short& i : cn.vibrato)
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
        vocal.time = reader.ReadFloat();
        vocal.note = reader.ReadInt32();
        vocal.length = reader.ReadFloat();
        vocal.lyric = reader.ReadFixedString(48);
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
        header.unk1 = reader.ReadInt32();
        header.unk2 = reader.ReadInt32();
        header.unk3 = reader.ReadInt32();
        header.unk4 = reader.ReadInt32();
        header.unk5 = reader.ReadInt32();
        header.unk6 = reader.ReadInt32();
        header.unk7 = reader.ReadInt32();
        header.unk8 = reader.ReadInt32();
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
        texture.font_name = reader.ReadFixedString(128);
        texture.font_path_length = reader.ReadInt32();
        texture.unk = reader.ReadInt32();
        texture.width = reader.ReadInt32();
        texture.height = reader.ReadInt32();
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
        def.text = reader.ReadFixedString(12);
        for (float& val : def.rect_outer)
        {
            val = reader.ReadFloat();
        }
        for (float& val : def.rect_inner)
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
        iter.phrase_id = reader.ReadInt32();
        iter.start_time = reader.ReadFloat();
        iter.next_phrase_time = reader.ReadFloat();
        for (int& diff : iter.difficulty)
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
        info.phrase_id = reader.ReadInt32();
        info.difficulty = reader.ReadInt32();
        info.empty = reader.ReadInt32();
        info.level_jump = reader.ReadUInt8();
        info.redundant = reader.ReadInt16();
        info.padding = reader.ReadUInt8();
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
        nld.level_break = reader.ReadInt32();
        const auto phrase_count = reader.ReadInt32();
        nld.nld_phrases.resize(phrase_count);
        for (auto& phrase : nld.nld_phrases)
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
        action.time = reader.ReadFloat();
        action.name = reader.ReadFixedString(256);
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
        event.time = reader.ReadFloat();
        event.name = reader.ReadFixedString(256);
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
        tone.time = reader.ReadFloat();
        tone.tone_id = reader.ReadInt32();
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
        dna.time = reader.ReadFloat();
        dna.dna_id = reader.ReadInt32();
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
        section.name = reader.ReadFixedString(32);
        section.number = reader.ReadInt32();
        section.start_time = reader.ReadFloat();
        section.end_time = reader.ReadFloat();
        section.start_phrase_iteration_index = reader.ReadInt32();
        section.end_phrase_iteration_index = reader.ReadInt32();
        for (unsigned char& byte : section.string_bytes)
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
    note.mask = reader.ReadUInt32();
    note.flags = reader.ReadUInt32();
    note.hash = reader.ReadUInt32();
    note.time = reader.ReadFloat();
    note.string = reader.ReadInt8();
    note.fret = reader.ReadInt8();
    note.anchor_fret = reader.ReadInt8();
    note.anchor_width = reader.ReadInt8();
    note.chord_id = reader.ReadInt32();
    note.chord_notes_id = reader.ReadInt32();
    note.phrase_id = reader.ReadInt32();
    note.phrase_iteration_id = reader.ReadInt32();
    note.fingerprint_id[0] = reader.ReadInt16();
    note.fingerprint_id[1] = reader.ReadInt16();
    note.next_iteration = reader.ReadInt16();
    note.prev_iteration = reader.ReadInt16();
    note.parent_prev_note = reader.ReadInt16();
    note.slide_to = reader.ReadInt8();
    note.slide_unpitch_to = reader.ReadInt8();
    note.left_hand = reader.ReadInt8();
    note.tap = reader.ReadInt8();
    note.pick_direction = reader.ReadInt8();
    note.slap = reader.ReadInt8();
    note.pluck = reader.ReadInt8();
    note.vibrato = reader.ReadInt16();
    note.sustain = reader.ReadFloat();
    note.max_bend = reader.ReadFloat();

    const auto bend_count = reader.ReadInt32();
    note.bend_values.resize(bend_count);
    for (auto& bv : note.bend_values)
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
        arr.difficulty = reader.ReadInt32();

        // Anchors
        const auto anchor_count = reader.ReadInt32();
        arr.anchors.resize(anchor_count);
        for (auto& anchor : arr.anchors)
        {
            anchor.start_time = reader.ReadFloat();
            anchor.end_time = reader.ReadFloat();
            anchor.unk1 = reader.ReadFloat();
            anchor.unk2 = reader.ReadFloat();
            anchor.fret = reader.ReadInt32();
            anchor.width = reader.ReadInt32();
            anchor.phrase_iteration_index = reader.ReadInt32();
        }

        // Anchor Extensions
        const auto anchor_ext_count = reader.ReadInt32();
        arr.anchor_extensions.resize(anchor_ext_count);
        for (auto& ext : arr.anchor_extensions)
        {
            ext.beat_time = reader.ReadFloat();
            ext.fret_id = reader.ReadInt8();
            ext.unk2 = reader.ReadInt32();
            ext.unk3 = reader.ReadInt16();
            ext.unk4 = reader.ReadInt8();
        }

        // Fingerprints - handshape
        const auto hs_count = reader.ReadInt32();
        arr.fingerprints_handshape.resize(hs_count);
        for (auto& fp : arr.fingerprints_handshape)
        {
            fp.chord_id = reader.ReadInt32();
            fp.start_time = reader.ReadFloat();
            fp.end_time = reader.ReadFloat();
            fp.unk1 = reader.ReadFloat();
            fp.unk2 = reader.ReadFloat();
        }

        // Fingerprints - arpeggio
        const auto arp_count = reader.ReadInt32();
        arr.fingerprints_arpeggio.resize(arp_count);
        for (auto& fp : arr.fingerprints_arpeggio)
        {
            fp.chord_id = reader.ReadInt32();
            fp.start_time = reader.ReadFloat();
            fp.end_time = reader.ReadFloat();
            fp.unk1 = reader.ReadFloat();
            fp.unk2 = reader.ReadFloat();
        }

        // Notes
        const auto note_count = reader.ReadInt32();
        arr.notes.resize(note_count);
        for (auto& note : arr.notes)
        {
            note = ReadNote(reader);
        }

        // Per-arrangement metadata
        arr.phrase_count = reader.ReadInt32();
        arr.average_notes_per_iteration.resize(arr.phrase_count);
        for (auto& avg : arr.average_notes_per_iteration)
        {
            avg = reader.ReadFloat();
        }

        arr.phrase_iteration_count1 = reader.ReadInt32();
        arr.notes_in_iteration1.resize(arr.phrase_iteration_count1);
        for (auto& n : arr.notes_in_iteration1)
        {
            n = reader.ReadInt32();
        }

        arr.phrase_iteration_count2 = reader.ReadInt32();
        arr.notes_in_iteration2.resize(arr.phrase_iteration_count2);
        for (auto& n : arr.notes_in_iteration2)
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
    meta.max_score = reader.ReadDouble();
    meta.max_notes_and_chords = reader.ReadDouble();
    meta.max_notes_and_chords_real = reader.ReadDouble();
    meta.point_per_note = reader.ReadDouble();
    meta.first_beat_length = reader.ReadFloat();
    meta.start_time = reader.ReadFloat();
    meta.capo_fret_id = reader.ReadInt8();
    meta.last_conversion_date_time = reader.ReadFixedString(32);
    meta.part = reader.ReadInt16();
    meta.song_length = reader.ReadFloat();
    meta.string_count = reader.ReadInt32();
    meta.tuning.resize(meta.string_count);
    for (auto& t : meta.tuning)
    {
        t = reader.ReadInt16();
    }
    meta.first_note_time = reader.ReadFloat();
    meta.first_note_time2 = reader.ReadFloat();
    meta.max_difficulty = reader.ReadInt32();
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

    sng.bpms = ReadBpms(reader);
    sng.phrases = ReadPhrases(reader);
    sng.chords = ReadChords(reader);
    sng.chord_notes = ReadChordNotes(reader);
    sng.vocals = ReadVocals(reader);
    if (!sng.vocals.empty())
    {
        sng.symbols_headers = ReadSymbolsHeaders(reader);
        sng.symbols_textures = ReadSymbolsTextures(reader);
        sng.symbol_definitions = ReadSymbolDefinitions(reader);
    }
    sng.phrase_iterations = ReadPhraseIterations(reader);
    sng.phrase_extra_infos = ReadPhraseExtraInfos(reader);
    sng.nlinked_difficulties = ReadNLinkedDifficulties(reader);
    sng.actions = ReadActions(reader);
    sng.events = ReadEvents(reader);
    sng.tones = ReadTones(reader);
    sng.dnas = ReadDnas(reader);
    sng.sections = ReadSections(reader);
    sng.arrangements = ReadArrangements(reader);
    sng.metadata = ReadMetadata(reader);

    if (reader.Position() != reader.Size())
    {
        throw PsarcException(
            std::format("SNG parse error: {} bytes remaining after parsing (expected exact match)",
                        reader.Size() - reader.Position()));
    }

    return sng;
}
