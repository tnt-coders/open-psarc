#include "sng_xml_writer.h"

#include "open-psarc/psarc_file.h"

#include <algorithm>
#include <format>
#include <vector>

#include <pugixml.hpp>

namespace
{

std::string FormatFloat(float value)
{
    return std::format("{:.3f}", value);
}

bool Has(uint32_t mask, sng::NoteMask flag)
{
    return (mask & static_cast<uint32_t>(flag)) != 0;
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

void WriteArrangementProperties(pugi::xml_node song, const SngManifestArrangementProperties& props)
{
    auto node = song.append_child("arrangementProperties");
    node.append_attribute("represent") = props.m_represent;
    node.append_attribute("bonusArr") = props.m_bonus_arr;
    node.append_attribute("standardTuning") = props.m_standard_tuning;
    node.append_attribute("nonStandardChords") = props.m_non_standard_chords;
    node.append_attribute("barreChords") = props.m_barre_chords;
    node.append_attribute("powerChords") = props.m_power_chords;
    node.append_attribute("dropDPower") = props.m_drop_d_power;
    node.append_attribute("openChords") = props.m_open_chords;
    node.append_attribute("fingerPicking") = props.m_finger_picking;
    node.append_attribute("pickDirection") = props.m_pick_direction;
    node.append_attribute("doubleStops") = props.m_double_stops;
    node.append_attribute("palmMutes") = props.m_palm_mutes;
    node.append_attribute("harmonics") = props.m_harmonics;
    node.append_attribute("pinchHarmonics") = props.m_pinch_harmonics;
    node.append_attribute("hopo") = props.m_hopo;
    node.append_attribute("tremolo") = props.m_tremolo;
    node.append_attribute("slides") = props.m_slides;
    node.append_attribute("unpitchedSlides") = props.m_unpitched_slides;
    node.append_attribute("bends") = props.m_bends;
    node.append_attribute("tapping") = props.m_tapping;
    node.append_attribute("vibrato") = props.m_vibrato;
    node.append_attribute("fretHandMutes") = props.m_fret_hand_mutes;
    node.append_attribute("slapPop") = props.m_slap_pop;
    node.append_attribute("twoFingerPicking") = props.m_two_finger_picking;
    node.append_attribute("fifthsAndOctaves") = props.m_fifths_and_octaves;
    node.append_attribute("syncopation") = props.m_syncopation;
    node.append_attribute("bassPick") = props.m_bass_pick;
    node.append_attribute("sustain") = props.m_sustain;
    node.append_attribute("pathLead") = props.m_path_lead;
    node.append_attribute("pathRhythm") = props.m_path_rhythm;
    node.append_attribute("pathBass") = props.m_path_bass;
}

void WriteBendValues(pugi::xml_node parent, const std::vector<sng::BendValue>& bends)
{
    if (bends.empty())
    {
        return;
    }

    auto bends_node = parent.append_child("bendValues");
    bends_node.append_attribute("count") = static_cast<int>(bends.size());
    for (const auto& bend : bends)
    {
        auto bend_node = bends_node.append_child("bendValue");
        bend_node.append_attribute("time") = FormatFloat(bend.m_time).c_str();
        bend_node.append_attribute("step") = FormatFloat(bend.m_step).c_str();
    }
}

void WriteNoteFlags(pugi::xml_node node, const sng::Note& note)
{
    if (Has(note.m_mask, sng::PARENT))
    {
        node.append_attribute("linkNext") = 1;
    }
    if (Has(note.m_mask, sng::ACCENT))
    {
        node.append_attribute("accent") = 1;
    }
    if (Has(note.m_mask, sng::BEND) && note.m_max_bend > 0.0f)
    {
        node.append_attribute("bend") = FormatFloat(note.m_max_bend).c_str();
    }
    if (Has(note.m_mask, sng::HAMMERON))
    {
        node.append_attribute("hammerOn") = 1;
    }
    if (Has(note.m_mask, sng::HARMONIC))
    {
        node.append_attribute("harmonic") = 1;
    }
    if (Has(note.m_mask, sng::HAMMERON) || Has(note.m_mask, sng::PULLOFF))
    {
        node.append_attribute("hopo") = 1;
    }
    if (Has(note.m_mask, sng::IGNORE))
    {
        node.append_attribute("ignore") = 1;
    }
    if (note.m_left_hand >= 0)
    {
        node.append_attribute("leftHand") = note.m_left_hand;
    }
    if (Has(note.m_mask, sng::MUTE))
    {
        node.append_attribute("mute") = 1;
    }
    if (Has(note.m_mask, sng::PALMMUTE))
    {
        node.append_attribute("palmMute") = 1;
    }
    if (Has(note.m_mask, sng::PLUCK))
    {
        node.append_attribute("pluck") = 1;
    }
    if (Has(note.m_mask, sng::PULLOFF))
    {
        node.append_attribute("pullOff") = 1;
    }
    if (Has(note.m_mask, sng::SLAP))
    {
        node.append_attribute("slap") = 1;
    }
    if (Has(note.m_mask, sng::SLIDE) && note.m_slide_to >= 0)
    {
        node.append_attribute("slideTo") = note.m_slide_to;
    }
    if (Has(note.m_mask, sng::TREMOLO))
    {
        node.append_attribute("tremolo") = 1;
    }
    if (Has(note.m_mask, sng::PINCHHARMONIC))
    {
        node.append_attribute("harmonicPinch") = 1;
    }
    if (note.m_pick_direction > 0)
    {
        node.append_attribute("pickDirection") = 1;
    }
    if (Has(note.m_mask, sng::RIGHTHAND))
    {
        node.append_attribute("rightHand") = 1;
    }
    if (Has(note.m_mask, sng::SLIDEUNPITCHEDTO) && note.m_slide_unpitch_to >= 0)
    {
        node.append_attribute("slideUnpitchTo") = note.m_slide_unpitch_to;
    }
    if (Has(note.m_mask, sng::TAP))
    {
        node.append_attribute("tap") = std::max<int>(0, note.m_tap);
    }
    if (Has(note.m_mask, sng::VIBRATO) && note.m_vibrato > 0)
    {
        node.append_attribute("vibrato") = note.m_vibrato;
    }
}

void WriteChordNoteFromTemplate(pugi::xml_node chord, const sng::SngData& sng,
                                const sng::Note& note, int string_idx)
{
    if (note.m_chord_id < 0 || static_cast<size_t>(note.m_chord_id) >= sng.m_chords.size())
    {
        return;
    }

    const auto& template_chord = sng.m_chords[note.m_chord_id];
    if (template_chord.m_frets[string_idx] < 0)
    {
        return;
    }

    auto cn = chord.append_child("chordNote");
    cn.append_attribute("time") = FormatFloat(note.m_time).c_str();
    cn.append_attribute("string") = string_idx;
    cn.append_attribute("fret") = template_chord.m_frets[string_idx];
    if (note.m_sustain > 0.0f)
    {
        cn.append_attribute("sustain") = FormatFloat(note.m_sustain).c_str();
    }
    if (template_chord.m_fingers[string_idx] >= 0)
    {
        cn.append_attribute("leftHand") = template_chord.m_fingers[string_idx];
    }

    if (note.m_chord_notes_id < 0 ||
        static_cast<size_t>(note.m_chord_notes_id) >= sng.m_chord_notes.size())
    {
        return;
    }

    const auto& cn_data = sng.m_chord_notes[note.m_chord_notes_id];
    if (Has(cn_data.m_mask[string_idx], sng::PARENT))
    {
        cn.append_attribute("linkNext") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::ACCENT))
    {
        cn.append_attribute("accent") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::HAMMERON))
    {
        cn.append_attribute("hammerOn") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::HARMONIC))
    {
        cn.append_attribute("harmonic") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::HAMMERON) ||
        Has(cn_data.m_mask[string_idx], sng::PULLOFF))
    {
        cn.append_attribute("hopo") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::IGNORE))
    {
        cn.append_attribute("ignore") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::MUTE))
    {
        cn.append_attribute("mute") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::PALMMUTE))
    {
        cn.append_attribute("palmMute") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::PLUCK))
    {
        cn.append_attribute("pluck") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::PULLOFF))
    {
        cn.append_attribute("pullOff") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::SLAP))
    {
        cn.append_attribute("slap") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::SLIDE) && cn_data.m_slide_to[string_idx] >= 0)
    {
        cn.append_attribute("slideTo") = cn_data.m_slide_to[string_idx];
    }
    if (Has(cn_data.m_mask[string_idx], sng::TREMOLO))
    {
        cn.append_attribute("tremolo") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::PINCHHARMONIC))
    {
        cn.append_attribute("harmonicPinch") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::RIGHTHAND))
    {
        cn.append_attribute("rightHand") = 1;
    }
    if (Has(cn_data.m_mask[string_idx], sng::SLIDEUNPITCHEDTO) &&
        cn_data.m_slide_unpitch_to[string_idx] >= 0)
    {
        cn.append_attribute("slideUnpitchTo") = cn_data.m_slide_unpitch_to[string_idx];
    }
    if (Has(cn_data.m_mask[string_idx], sng::VIBRATO) && cn_data.m_vibrato[string_idx] > 0)
    {
        cn.append_attribute("vibrato") = cn_data.m_vibrato[string_idx];
    }

    WriteBendValues(cn, cn_data.m_bend_data[string_idx].m_bend_values);
}

void WriteInstrumentalXml(const sng::SngData& sng, const std::filesystem::path& output_path,
                          const SngManifestMetadata* manifest)
{
    pugi::xml_document doc;
    auto decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "utf-8";

    auto song = doc.append_child("song");
    song.append_attribute("version") = "8";

    song.append_child("title").text().set(
        (manifest && manifest->m_title.has_value()) ? manifest->m_title->c_str() : "");
    song.append_child("arrangement")
        .text()
        .set((manifest && manifest->m_arrangement.has_value()) ? manifest->m_arrangement->c_str()
                                                               : "");
    song.append_child("part").text().set(sng.m_metadata.m_part);
    song.append_child("offset").text().set(FormatFloat(-sng.m_metadata.m_start_time).c_str());
    song.append_child("centOffset")
        .text()
        .set((manifest && manifest->m_cent_offset.has_value()) ? *manifest->m_cent_offset : 0.0f);
    song.append_child("songLength").text().set(FormatFloat(sng.m_metadata.m_song_length).c_str());
    song.append_child("songNameSort")
        .text()
        .set((manifest && manifest->m_song_name_sort.has_value())
                 ? manifest->m_song_name_sort->c_str()
                 : "");
    song.append_child("startBeat").text().set(FormatFloat(sng.m_metadata.m_start_time).c_str());

    float average_tempo = 120.0f;
    if (manifest)
    {
        average_tempo = manifest->m_average_tempo.value_or(0.0f);
    }
    song.append_child("averageTempo").text().set(FormatFloat(average_tempo).c_str());

    auto tuning = song.append_child("tuning");
    for (int i = 0; i < 6; ++i)
    {
        const int value =
            i < static_cast<int>(sng.m_metadata.m_tuning.size()) ? sng.m_metadata.m_tuning[i] : 0;
        tuning.append_attribute(std::format("string{}", i).c_str()) = value;
    }

    song.append_child("capo").text().set(std::max<int>(0, sng.m_metadata.m_capo_fret_id));
    song.append_child("artistName")
        .text()
        .set((manifest && manifest->m_artist_name.has_value()) ? manifest->m_artist_name->c_str()
                                                               : "");
    song.append_child("artistNameSort")
        .text()
        .set((manifest && manifest->m_artist_name_sort.has_value())
                 ? manifest->m_artist_name_sort->c_str()
                 : "");
    song.append_child("albumName")
        .text()
        .set((manifest && manifest->m_album_name.has_value()) ? manifest->m_album_name->c_str()
                                                              : "");
    song.append_child("albumNameSort")
        .text()
        .set((manifest && manifest->m_album_name_sort.has_value())
                 ? manifest->m_album_name_sort->c_str()
                 : "");
    song.append_child("albumYear")
        .text()
        .set((manifest && manifest->m_album_year.has_value()) ? *manifest->m_album_year : 0);
    song.append_child("crowdSpeed").text().set(1);

    WriteArrangementProperties(song, manifest ? manifest->m_arrangement_properties.value_or(
                                                    SngManifestArrangementProperties{})
                                              : SngManifestArrangementProperties{});
    song.append_child("lastConversionDateTime")
        .text()
        .set(sng.m_metadata.m_last_conversion_date_time.c_str());

    auto phrases = song.append_child("phrases");
    phrases.append_attribute("count") = static_cast<int>(sng.m_phrases.size());
    for (const auto& phrase : sng.m_phrases)
    {
        auto node = phrases.append_child("phrase");
        node.append_attribute("maxDifficulty") = phrase.m_max_difficulty;
        node.append_attribute("name") = phrase.m_name.c_str();
        if (phrase.m_disparity == 1)
        {
            node.append_attribute("disparity") = 1;
        }
        if (phrase.m_ignore == 1)
        {
            node.append_attribute("ignore") = 1;
        }
        if (phrase.m_solo == 1)
        {
            node.append_attribute("solo") = 1;
        }
    }

    auto phrase_iterations = song.append_child("phraseIterations");
    phrase_iterations.append_attribute("count") = static_cast<int>(sng.m_phrase_iterations.size());
    for (const auto& pi : sng.m_phrase_iterations)
    {
        auto node = phrase_iterations.append_child("phraseIteration");
        node.append_attribute("time") = FormatFloat(pi.m_start_time).c_str();
        node.append_attribute("phraseId") = pi.m_phrase_id;
        if (pi.m_difficulty[0] > 0 || pi.m_difficulty[1] > 0 || pi.m_difficulty[2] > 0)
        {
            auto hero_levels = node.append_child("heroLevels");
            hero_levels.append_attribute("count") = 3;
            for (int i = 0; i < 3; ++i)
            {
                auto hero = hero_levels.append_child("heroLevel");
                hero.append_attribute("hero") = i + 1;
                hero.append_attribute("difficulty") = pi.m_difficulty[i];
            }
        }
    }

    std::vector<const sng::NLinkedDifficulty*> nld_entries;
    for (const auto& nld : sng.m_nlinked_difficulties)
    {
        if (nld.m_level_break >= 0 && !nld.m_nld_phrases.empty())
        {
            nld_entries.push_back(&nld);
        }
    }

    auto linked_diffs = song.append_child("linkedDiffs");
    linked_diffs.append_attribute("count") = static_cast<int>(nld_entries.size());
    for (const auto* nld : nld_entries)
    {
        auto node = linked_diffs.append_child("linkedDiff");
        node.append_attribute("levelBreak") = nld->m_level_break;
        for (size_t i = 0; i < nld->m_nld_phrases.size(); ++i)
        {
            node.append_attribute(std::format("phraseId{}", i).c_str()) = nld->m_nld_phrases[i];
        }
    }

    auto phrase_props = song.append_child("phraseProperties");
    phrase_props.append_attribute("count") = static_cast<int>(sng.m_phrase_extra_infos.size());
    for (const auto& info : sng.m_phrase_extra_infos)
    {
        auto node = phrase_props.append_child("phraseProperty");
        node.append_attribute("phraseId") = info.m_phrase_id;
        node.append_attribute("redundant") = info.m_redundant;
        node.append_attribute("levelJump") = info.m_level_jump;
        node.append_attribute("empty") = info.m_empty;
        node.append_attribute("difficulty") = info.m_difficulty;
    }

    auto chord_templates = song.append_child("chordTemplates");
    chord_templates.append_attribute("count") = static_cast<int>(sng.m_chords.size());
    for (const auto& chord : sng.m_chords)
    {
        auto node = chord_templates.append_child("chordTemplate");
        node.append_attribute("chordName") = chord.m_name.c_str();
        std::string display_name = chord.m_name;
        if (chord.m_mask == 1)
        {
            display_name += "-arp";
        }
        else if (chord.m_mask == 2)
        {
            display_name += "-nop";
        }
        node.append_attribute("displayName") = display_name.c_str();
        for (int i = 0; i < 6; ++i)
        {
            node.append_attribute(std::format("finger{}", i).c_str()) = chord.m_fingers[i];
        }
        for (int i = 0; i < 6; ++i)
        {
            node.append_attribute(std::format("fret{}", i).c_str()) = chord.m_frets[i];
        }
    }

    auto ebeats = song.append_child("ebeats");
    ebeats.append_attribute("count") = static_cast<int>(sng.m_bpms.size());
    for (const auto& bpm : sng.m_bpms)
    {
        auto node = ebeats.append_child("ebeat");
        node.append_attribute("time") = FormatFloat(bpm.m_time).c_str();
        node.append_attribute("measure") = (bpm.m_mask & 0x01) ? bpm.m_measure : -1;
    }

    auto sections = song.append_child("sections");
    sections.append_attribute("count") = static_cast<int>(sng.m_sections.size());
    for (const auto& section : sng.m_sections)
    {
        auto node = sections.append_child("section");
        node.append_attribute("name") = section.m_name.c_str();
        node.append_attribute("number") = section.m_number;
        node.append_attribute("startTime") = FormatFloat(section.m_start_time).c_str();
    }

    auto events = song.append_child("events");
    events.append_attribute("count") = static_cast<int>(sng.m_events.size());
    for (const auto& event : sng.m_events)
    {
        auto node = events.append_child("event");
        node.append_attribute("time") = FormatFloat(event.m_time).c_str();
        node.append_attribute("code") = event.m_name.c_str();
    }

    if (manifest && manifest->m_tone_base.has_value() && !manifest->m_tone_base->empty())
    {
        song.append_child("tonebase").text().set(manifest->m_tone_base->c_str());
    }

    auto tones = song.append_child("tones");
    tones.append_attribute("count") = static_cast<int>(sng.m_tones.size());
    for (const auto& tone : sng.m_tones)
    {
        auto node = tones.append_child("tone");
        node.append_attribute("time") = FormatFloat(tone.m_time).c_str();
        node.append_attribute("id") = tone.m_tone_id;

        std::string tone_name = "N/A";
        if (manifest && tone.m_tone_id >= 0 && tone.m_tone_id < 4)
        {
            tone_name = manifest->m_tone_names[static_cast<size_t>(tone.m_tone_id)].value_or("");
        }
        node.append_attribute("name") = tone_name.c_str();
    }

    auto levels = song.append_child("levels");
    levels.append_attribute("count") = static_cast<int>(sng.m_arrangements.size());
    for (const auto& arr : sng.m_arrangements)
    {
        auto level = levels.append_child("level");
        level.append_attribute("difficulty") = arr.m_difficulty;

        std::vector<const sng::Note*> single_notes;
        std::vector<const sng::Note*> chords;
        for (const auto& note : arr.m_notes)
        {
            if (note.m_chord_id >= 0 && Has(note.m_mask, sng::CHORD))
            {
                chords.push_back(&note);
            }
            else
            {
                single_notes.push_back(&note);
            }
        }

        auto notes_node = level.append_child("notes");
        notes_node.append_attribute("count") = static_cast<int>(single_notes.size());
        for (const auto* note : single_notes)
        {
            auto node = notes_node.append_child("note");
            node.append_attribute("time") = FormatFloat(note->m_time).c_str();
            node.append_attribute("string") = note->m_string;
            node.append_attribute("fret") = note->m_fret;
            if (note->m_sustain > 0.0f)
            {
                node.append_attribute("sustain") = FormatFloat(note->m_sustain).c_str();
            }
            WriteNoteFlags(node, *note);
            WriteBendValues(node, note->m_bend_values);
        }

        auto chords_node = level.append_child("chords");
        chords_node.append_attribute("count") = static_cast<int>(chords.size());
        for (const auto* note : chords)
        {
            auto node = chords_node.append_child("chord");
            node.append_attribute("time") = FormatFloat(note->m_time).c_str();
            node.append_attribute("chordId") = note->m_chord_id;
            if (Has(note->m_mask, sng::PARENT))
            {
                node.append_attribute("linkNext") = 1;
            }
            if (Has(note->m_mask, sng::ACCENT))
            {
                node.append_attribute("accent") = 1;
            }
            if (Has(note->m_mask, sng::FRETHANDMUTE))
            {
                node.append_attribute("fretHandMute") = 1;
            }
            if (Has(note->m_mask, sng::HIGHDENSITY))
            {
                node.append_attribute("highDensity") = 1;
            }
            if (Has(note->m_mask, sng::IGNORE))
            {
                node.append_attribute("ignore") = 1;
            }
            if (Has(note->m_mask, sng::PALMMUTE))
            {
                node.append_attribute("palmMute") = 1;
            }
            if (Has(note->m_mask, sng::HAMMERON) || Has(note->m_mask, sng::PULLOFF))
            {
                node.append_attribute("hopo") = 1;
            }

            if (Has(note->m_mask, sng::CHORDPANEL))
            {
                for (int s = 0; s < 6; ++s)
                {
                    WriteChordNoteFromTemplate(node, sng, *note, s);
                }
            }
        }

        auto anchors = level.append_child("anchors");
        anchors.append_attribute("count") = static_cast<int>(arr.m_anchors.size());
        for (const auto& anchor : arr.m_anchors)
        {
            auto node = anchors.append_child("anchor");
            node.append_attribute("time") = FormatFloat(anchor.m_start_time).c_str();
            node.append_attribute("fret") = anchor.m_fret;
            node.append_attribute("width") =
                FormatFloat(static_cast<float>(anchor.m_width)).c_str();
        }

        struct HandShapeView
        {
            int32_t m_chord_id = 0;
            float m_start = 0;
            float m_end = 0;
        };

        std::vector<HandShapeView> handshapes;
        handshapes.reserve(arr.m_fingerprints_handshape.size() +
                           arr.m_fingerprints_arpeggio.size());
        for (const auto& hs : arr.m_fingerprints_handshape)
        {
            handshapes.push_back({hs.m_chord_id, hs.m_start_time, hs.m_end_time});
        }
        for (const auto& arp : arr.m_fingerprints_arpeggio)
        {
            handshapes.push_back({arp.m_chord_id, arp.m_start_time, arp.m_end_time});
        }
        std::sort(
            handshapes.begin(), handshapes.end(),
            [](const HandShapeView& a, const HandShapeView& b) { return a.m_start < b.m_start; });

        auto handshapes_node = level.append_child("handShapes");
        handshapes_node.append_attribute("count") = static_cast<int>(handshapes.size());
        for (const auto& hs : handshapes)
        {
            auto node = handshapes_node.append_child("handShape");
            node.append_attribute("chordId") = hs.m_chord_id;
            node.append_attribute("startTime") = FormatFloat(hs.m_start).c_str();
            node.append_attribute("endTime") = FormatFloat(hs.m_end).c_str();
        }
    }

    if (!doc.save_file(output_path.string().c_str(), "  ", pugi::format_default,
                       pugi::encoding_utf8))
    {
        throw PsarcException(std::format("Failed to write XML: {}", output_path.string()));
    }
}

} // namespace

void SngXmlWriter::Write(const sng::SngData& sng, const std::filesystem::path& output_path,
                         const SngManifestMetadata* manifest)
{
    if (!sng.m_vocals.empty())
    {
        WriteVocalXml(sng, output_path);
    }
    else
    {
        WriteInstrumentalXml(sng, output_path, manifest);
    }
}
