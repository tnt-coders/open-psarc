#include "sng_xml_writer.h"

#include "open-psarc/psarc_file.h"

#include <format>

#include <pugixml.hpp>

namespace
{

// Format float with 3 decimal places (matching RS toolkit conventions)
std::string FormatFloat(float value)
{
    return std::format("{:.3f}", value);
}

void WriteVocalXml(const sng::SngData& sng, const std::filesystem::path& output_path)
{
    pugi::xml_document doc;

    auto decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "utf-8";

    auto vocals_node = doc.append_child("vocals");
    vocals_node.append_attribute("count") = static_cast<int>(sng.m_vocals.size());

    for (const auto& vocal : sng.m_vocals)
    {
        auto node = vocals_node.append_child("vocal");
        node.append_attribute("time") = FormatFloat(vocal.m_time).c_str();
        node.append_attribute("note") = vocal.m_note;
        node.append_attribute("length") = FormatFloat(vocal.m_length).c_str();
        node.append_attribute("lyric") = vocal.m_lyric.c_str();
    }

    if (!doc.save_file(output_path.string().c_str(), "  ", pugi::format_default,
                       pugi::encoding_utf8))
    {
        throw PsarcException(std::format("Failed to write XML: {}", output_path.string()));
    }
}

void WriteInstrumentalXml(const sng::SngData& sng, const std::filesystem::path& output_path)
{
    pugi::xml_document doc;

    auto decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "utf-8";

    auto song = doc.append_child("song");

    // Metadata
    song.append_child("title");
    song.append_child("arrangement");
    song.append_child("part").text().set(sng.m_metadata.m_part);
    song.append_child("offset").text().set(FormatFloat(0.0f).c_str());
    song.append_child("songLength").text().set(FormatFloat(sng.m_metadata.m_song_length).c_str());
    song.append_child("startBeat").text().set(FormatFloat(sng.m_metadata.m_start_time).c_str());
    song.append_child("averageTempo")
        .text()
        .set(FormatFloat(sng.m_metadata.m_first_beat_length > 0
                             ? 60.0f / sng.m_metadata.m_first_beat_length
                             : 0.0f)
                 .c_str());

    auto tuning = song.append_child("tuning");
    for (int i = 0; i < static_cast<int>(sng.m_metadata.m_tuning.size()) && i < 6; ++i)
    {
        tuning.append_attribute(std::format("string{}", i).c_str()) = sng.m_metadata.m_tuning[i];
    }

    song.append_child("capo").text().set(sng.m_metadata.m_capo_fret_id);

    // Phrases
    auto phrases = song.append_child("phrases");
    phrases.append_attribute("count") = static_cast<int>(sng.m_phrases.size());
    for (const auto& phrase : sng.m_phrases)
    {
        auto node = phrases.append_child("phrase");
        node.append_attribute("name") = phrase.m_name.c_str();
        node.append_attribute("maxDifficulty") = phrase.m_max_difficulty;
        node.append_attribute("solo") = phrase.m_solo;
        node.append_attribute("disparity") = phrase.m_disparity;
        node.append_attribute("ignore") = phrase.m_ignore;
    }

    // PhraseIterations
    auto phrase_iterations = song.append_child("phraseIterations");
    phrase_iterations.append_attribute("count") = static_cast<int>(sng.m_phrase_iterations.size());
    for (const auto& pi : sng.m_phrase_iterations)
    {
        auto node = phrase_iterations.append_child("phraseIteration");
        node.append_attribute("time") = FormatFloat(pi.m_start_time).c_str();
        node.append_attribute("phraseId") = pi.m_phrase_id;
    }

    // LinkedDiffs
    auto linked_diffs = song.append_child("linkedDiffs");
    linked_diffs.append_attribute("count") = static_cast<int>(sng.m_nlinked_difficulties.size());
    for (const auto& nld : sng.m_nlinked_difficulties)
    {
        auto node = linked_diffs.append_child("linkedDiff");
        node.append_attribute("levelBreak") = nld.m_level_break;
        for (size_t i = 0; i < nld.m_nld_phrases.size(); ++i)
        {
            node.append_attribute(std::format("phraseId{}", i).c_str()) = nld.m_nld_phrases[i];
        }
    }

    // PhraseProperties
    auto phrase_props = song.append_child("phraseProperties");
    phrase_props.append_attribute("count") = static_cast<int>(sng.m_phrase_extra_infos.size());
    for (const auto& pei : sng.m_phrase_extra_infos)
    {
        auto node = phrase_props.append_child("phraseProperty");
        node.append_attribute("phraseId") = pei.m_phrase_id;
        node.append_attribute("difficulty") = pei.m_difficulty;
        node.append_attribute("empty") = pei.m_empty;
        node.append_attribute("levelJump") = pei.m_level_jump;
        node.append_attribute("redundant") = pei.m_redundant;
    }

    // ChordTemplates
    auto chord_templates = song.append_child("chordTemplates");
    chord_templates.append_attribute("count") = static_cast<int>(sng.m_chords.size());
    for (const auto& chord : sng.m_chords)
    {
        auto node = chord_templates.append_child("chordTemplate");
        node.append_attribute("chordName") = chord.m_name.c_str();
        for (int i = 0; i < 6; ++i)
        {
            node.append_attribute(std::format("fret{}", i).c_str()) =
                static_cast<int>(chord.m_frets[i]);
        }
        for (int i = 0; i < 6; ++i)
        {
            node.append_attribute(std::format("finger{}", i).c_str()) =
                static_cast<int>(chord.m_fingers[i]);
        }
    }

    // Ebeats (BPMs)
    auto ebeats = song.append_child("ebeats");
    ebeats.append_attribute("count") = static_cast<int>(sng.m_bpms.size());
    for (const auto& bpm : sng.m_bpms)
    {
        auto node = ebeats.append_child("ebeat");
        node.append_attribute("time") = FormatFloat(bpm.m_time).c_str();
        node.append_attribute("measure") = bpm.m_measure >= 0 ? bpm.m_measure : -1;
    }

    // Sections
    auto sections = song.append_child("sections");
    sections.append_attribute("count") = static_cast<int>(sng.m_sections.size());
    for (const auto& section : sng.m_sections)
    {
        auto node = sections.append_child("section");
        node.append_attribute("name") = section.m_name.c_str();
        node.append_attribute("number") = section.m_number;
        node.append_attribute("startTime") = FormatFloat(section.m_start_time).c_str();
    }

    // Events
    auto events = song.append_child("events");
    events.append_attribute("count") = static_cast<int>(sng.m_events.size());
    for (const auto& event : sng.m_events)
    {
        auto node = events.append_child("event");
        node.append_attribute("time") = FormatFloat(event.m_time).c_str();
        node.append_attribute("code") = event.m_name.c_str();
    }

    // Tones
    auto tones = song.append_child("tones");
    tones.append_attribute("count") = static_cast<int>(sng.m_tones.size());
    for (const auto& tone : sng.m_tones)
    {
        auto node = tones.append_child("tone");
        node.append_attribute("time") = FormatFloat(tone.m_time).c_str();
        node.append_attribute("id") = tone.m_tone_id;
    }

    // Levels (Arrangements)
    auto levels = song.append_child("levels");
    levels.append_attribute("count") = static_cast<int>(sng.m_arrangements.size());
    for (const auto& arr : sng.m_arrangements)
    {
        auto level = levels.append_child("level");
        level.append_attribute("difficulty") = arr.m_difficulty;

        // Separate notes into single notes and chords
        std::vector<const sng::Note*> single_notes;
        std::vector<const sng::Note*> chord_notes;
        for (const auto& note : arr.m_notes)
        {
            if (note.m_chord_id >= 0 && (note.m_mask & sng::CHORD))
            {
                chord_notes.push_back(&note);
            }
            else
            {
                single_notes.push_back(&note);
            }
        }

        // Notes
        auto notes_node = level.append_child("notes");
        notes_node.append_attribute("count") = static_cast<int>(single_notes.size());
        for (const auto* note : single_notes)
        {
            auto node = notes_node.append_child("note");
            node.append_attribute("time") = FormatFloat(note->m_time).c_str();
            node.append_attribute("string") = static_cast<int>(note->m_string);
            node.append_attribute("fret") = static_cast<int>(note->m_fret);
            node.append_attribute("sustain") = FormatFloat(note->m_sustain).c_str();
            node.append_attribute("bend") = FormatFloat(note->m_max_bend).c_str();
            node.append_attribute("hammerOn") = (note->m_mask & sng::HAMMERON) ? 1 : 0;
            node.append_attribute("pullOff") = (note->m_mask & sng::PULLOFF) ? 1 : 0;
            node.append_attribute("slideTo") = static_cast<int>(note->m_slide_to);
            node.append_attribute("slideUnpitchTo") = static_cast<int>(note->m_slide_unpitch_to);
            node.append_attribute("harmonic") = (note->m_mask & sng::HARMONIC) ? 1 : 0;
            node.append_attribute("palmMute") = (note->m_mask & sng::PALMMUTE) ? 1 : 0;
            node.append_attribute("tremolo") = (note->m_mask & sng::TREMOLO) ? 1 : 0;
            node.append_attribute("mute") = (note->m_mask & sng::MUTE) ? 1 : 0;
            node.append_attribute("accent") = (note->m_mask & sng::ACCENT) ? 1 : 0;
            node.append_attribute("tap") = static_cast<int>(note->m_tap);
            node.append_attribute("vibrato") = static_cast<int>(note->m_vibrato);

            // Bend values
            if (!note->m_bend_values.empty())
            {
                auto bends = node.append_child("bendValues");
                bends.append_attribute("count") = static_cast<int>(note->m_bend_values.size());
                for (const auto& bv : note->m_bend_values)
                {
                    auto bend_node = bends.append_child("bendValue");
                    bend_node.append_attribute("time") = FormatFloat(bv.m_time).c_str();
                    bend_node.append_attribute("step") = FormatFloat(bv.m_step).c_str();
                }
            }
        }

        // Chords
        auto chords_node = level.append_child("chords");
        chords_node.append_attribute("count") = static_cast<int>(chord_notes.size());
        for (const auto* note : chord_notes)
        {
            auto node = chords_node.append_child("chord");
            node.append_attribute("time") = FormatFloat(note->m_time).c_str();
            node.append_attribute("chordId") = note->m_chord_id;
            node.append_attribute("highDensity") = (note->m_mask & sng::HIGHDENSITY) ? 1 : 0;
            node.append_attribute("strum") = (note->m_mask & sng::STRUM) ? "down" : "up";

            // ChordNotes (per-string details for technique chords)
            if (note->m_chord_notes_id >= 0 &&
                static_cast<size_t>(note->m_chord_notes_id) < sng.m_chord_notes.size() &&
                (note->m_mask & sng::CHORDNOTES))
            {
                const auto& cn = sng.m_chord_notes[note->m_chord_notes_id];
                auto cn_node = node.append_child("chordNotes");
                for (int s = 0; s < 6; ++s)
                {
                    if (note->m_chord_id >= 0 &&
                        static_cast<size_t>(note->m_chord_id) < sng.m_chords.size() &&
                        sng.m_chords[note->m_chord_id].m_frets[s] >= 0)
                    {
                        auto cn_note = cn_node.append_child("chordNote");
                        cn_note.append_attribute("string") = s;
                        cn_note.append_attribute("fret") =
                            static_cast<int>(sng.m_chords[note->m_chord_id].m_frets[s]);
                        if (!cn.m_bend_data[s].m_bend_values.empty())
                        {
                            auto bends = cn_note.append_child("bendValues");
                            bends.append_attribute("count") =
                                static_cast<int>(cn.m_bend_data[s].m_bend_values.size());
                            for (const auto& bv : cn.m_bend_data[s].m_bend_values)
                            {
                                auto bv_node = bends.append_child("bendValue");
                                bv_node.append_attribute("time") = FormatFloat(bv.m_time).c_str();
                                bv_node.append_attribute("step") = FormatFloat(bv.m_step).c_str();
                            }
                        }
                        // Map 0xFF sentinel to -1 for slide attributes
                        int slide_to =
                            (cn.m_slide_to[s] == 0xFF) ? -1 : static_cast<int>(cn.m_slide_to[s]);
                        int slide_unpitch_to = (cn.m_slide_unpitch_to[s] == 0xFF)
                                                   ? -1
                                                   : static_cast<int>(cn.m_slide_unpitch_to[s]);
                        cn_note.append_attribute("slideTo") = slide_to;
                        cn_note.append_attribute("slideUnpitchTo") = slide_unpitch_to;
                    }
                }
            }
        }

        // Anchors
        auto anchors_node = level.append_child("anchors");
        anchors_node.append_attribute("count") = static_cast<int>(arr.m_anchors.size());
        for (const auto& anchor : arr.m_anchors)
        {
            auto node = anchors_node.append_child("anchor");
            node.append_attribute("time") = FormatFloat(anchor.m_start_time).c_str();
            node.append_attribute("fret") = anchor.m_fret;
            node.append_attribute("width") = anchor.m_width;
        }

        // HandShapes (regular fingerprints)
        auto handshapes_node = level.append_child("handShapes");
        handshapes_node.append_attribute("count") =
            static_cast<int>(arr.m_fingerprints_handshape.size());
        for (const auto& hs : arr.m_fingerprints_handshape)
        {
            auto node = handshapes_node.append_child("handShape");
            node.append_attribute("chordId") = hs.m_chord_id;
            node.append_attribute("startTime") = FormatFloat(hs.m_start_time).c_str();
            node.append_attribute("endTime") = FormatFloat(hs.m_end_time).c_str();
        }
    }

    if (!doc.save_file(output_path.string().c_str(), "  ", pugi::format_default,
                       pugi::encoding_utf8))
    {
        throw PsarcException(std::format("Failed to write XML: {}", output_path.string()));
    }
}

} // namespace

void SngXmlWriter::Write(const sng::SngData& sng, const std::filesystem::path& output_path)
{
    if (!sng.m_vocals.empty())
    {
        WriteVocalXml(sng, output_path);
    }
    else
    {
        WriteInstrumentalXml(sng, output_path);
    }
}
