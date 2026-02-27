#include "sng_xml_writer.h"

#include "open-psarc/psarc_file.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <locale>
#include <sstream>
#include <utility>
#include <vector>

#include <pugixml.hpp>

namespace
{

std::string FormatFloat(float value)
{
    return std::format("{:.3f}", value);
}

std::string FormatPlainFloat(float value)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << value;
    return oss.str();
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
    vocals_node.append_attribute("count") = static_cast<int>(sng.vocals.size());

    for (const auto& vocal : sng.vocals)
    {
        auto node = vocals_node.append_child("vocal");
        node.append_attribute("time") = FormatFloat(vocal.time).c_str();
        node.append_attribute("note") = vocal.note;
        node.append_attribute("length") = FormatFloat(vocal.length).c_str();
        node.append_attribute("lyric") = vocal.lyric.c_str();
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
    node.append_attribute("represent") = props.represent;
    node.append_attribute("bonusArr") = props.bonus_arr;
    node.append_attribute("standardTuning") = props.standard_tuning;
    node.append_attribute("nonStandardChords") = props.non_standard_chords;
    node.append_attribute("barreChords") = props.barre_chords;
    node.append_attribute("powerChords") = props.power_chords;
    node.append_attribute("dropDPower") = props.drop_d_power;
    node.append_attribute("openChords") = props.open_chords;
    node.append_attribute("fingerPicking") = props.finger_picking;
    node.append_attribute("pickDirection") = props.pick_direction;
    node.append_attribute("doubleStops") = props.double_stops;
    node.append_attribute("palmMutes") = props.palm_mutes;
    node.append_attribute("harmonics") = props.harmonics;
    node.append_attribute("pinchHarmonics") = props.pinch_harmonics;
    node.append_attribute("hopo") = props.hopo;
    node.append_attribute("tremolo") = props.tremolo;
    node.append_attribute("slides") = props.slides;
    node.append_attribute("unpitchedSlides") = props.unpitched_slides;
    node.append_attribute("bends") = props.bends;
    node.append_attribute("tapping") = props.tapping;
    node.append_attribute("vibrato") = props.vibrato;
    node.append_attribute("fretHandMutes") = props.fret_hand_mutes;
    node.append_attribute("slapPop") = props.slap_pop;
    node.append_attribute("twoFingerPicking") = props.two_finger_picking;
    node.append_attribute("fifthsAndOctaves") = props.fifths_and_octaves;
    node.append_attribute("syncopation") = props.syncopation;
    node.append_attribute("bassPick") = props.bass_pick;
    node.append_attribute("sustain") = props.sustain;
    node.append_attribute("pathLead") = props.path_lead;
    node.append_attribute("pathRhythm") = props.path_rhythm;
    node.append_attribute("pathBass") = props.path_bass;
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
        bend_node.append_attribute("time") = FormatFloat(bend.time).c_str();
        if (std::abs(bend.step) > 0.000001f)
        {
            bend_node.append_attribute("step") = FormatFloat(bend.step).c_str();
        }
    }
}

void WriteNoteFlags(pugi::xml_node node, const sng::Note& note)
{
    if (Has(note.mask, sng::PARENT))
    {
        node.append_attribute("linkNext") = 1;
    }
    if (Has(note.mask, sng::ACCENT))
    {
        node.append_attribute("accent") = 1;
    }
    if (!note.bend_values.empty())
    {
        node.append_attribute("bend") = FormatPlainFloat(note.max_bend).c_str();
    }
    if (Has(note.mask, sng::HAMMERON))
    {
        node.append_attribute("hammerOn") = 1;
    }
    if (Has(note.mask, sng::HARMONIC))
    {
        node.append_attribute("harmonic") = 1;
    }
    if (Has(note.mask, sng::HAMMERON) || Has(note.mask, sng::PULLOFF))
    {
        node.append_attribute("hopo") = 1;
    }
    if (Has(note.mask, sng::IGNORE))
    {
        node.append_attribute("ignore") = 1;
    }
    if (note.left_hand >= 0)
    {
        node.append_attribute("leftHand") = note.left_hand;
    }
    if (Has(note.mask, sng::MUTE))
    {
        node.append_attribute("mute") = 1;
    }
    if (Has(note.mask, sng::PALMMUTE))
    {
        node.append_attribute("palmMute") = 1;
    }
    if (Has(note.mask, sng::PLUCK))
    {
        node.append_attribute("pluck") = 1;
    }
    if (Has(note.mask, sng::PULLOFF))
    {
        node.append_attribute("pullOff") = 1;
    }
    if (Has(note.mask, sng::SLAP))
    {
        node.append_attribute("slap") = 1;
    }
    if (Has(note.mask, sng::SLIDE) && note.slide_to >= 0)
    {
        node.append_attribute("slideTo") = note.slide_to;
    }
    if (Has(note.mask, sng::TREMOLO))
    {
        node.append_attribute("tremolo") = 1;
    }
    if (Has(note.mask, sng::PINCHHARMONIC))
    {
        node.append_attribute("harmonicPinch") = 1;
    }
    if (note.pick_direction > 0)
    {
        node.append_attribute("pickDirection") = 1;
    }
    if (Has(note.mask, sng::RIGHTHAND))
    {
        node.append_attribute("rightHand") = 1;
    }
    if (Has(note.mask, sng::SLIDEUNPITCHEDTO) && note.slide_unpitch_to >= 0)
    {
        node.append_attribute("slideUnpitchTo") = note.slide_unpitch_to;
    }
    if (Has(note.mask, sng::TAP))
    {
        node.append_attribute("tap") = std::max<int>(0, note.tap);
    }
    if (Has(note.mask, sng::VIBRATO) && note.vibrato > 0)
    {
        node.append_attribute("vibrato") = note.vibrato;
    }
}

void WriteChordNoteFromTemplate(pugi::xml_node chord, const sng::SngData& sng,
                                const sng::Note& note, int string_idx)
{
    if (note.chord_id < 0 || static_cast<size_t>(note.chord_id) >= sng.chords.size())
    {
        return;
    }

    const auto& template_chord = sng.chords[note.chord_id];
    const auto sidx = static_cast<size_t>(string_idx);
    if (template_chord.frets.at(sidx) < 0)
    {
        return;
    }

    auto cn = chord.append_child("chordNote");
    cn.append_attribute("time") = FormatFloat(note.time).c_str();
    cn.append_attribute("string") = string_idx;
    cn.append_attribute("fret") = template_chord.frets.at(sidx);
    if (note.sustain > 0.0f)
    {
        cn.append_attribute("sustain") = FormatFloat(note.sustain).c_str();
    }
    const auto raw_finger = static_cast<uint8_t>(template_chord.fingers.at(sidx));
    const int left_hand = (raw_finger == 0xFF) ? -1 : static_cast<int>(raw_finger);

    if (note.chord_notes_id < 0 ||
        static_cast<size_t>(note.chord_notes_id) >= sng.chord_notes.size())
    {
        if (left_hand != -1)
        {
            cn.append_attribute("leftHand") = left_hand;
        }
        return;
    }

    const auto& cn_data = sng.chord_notes[note.chord_notes_id];
    if (Has(cn_data.mask.at(sidx), sng::PARENT))
    {
        cn.append_attribute("linkNext") = 1;
    }
    if (Has(cn_data.mask.at(sidx), sng::ACCENT))
    {
        cn.append_attribute("accent") = 1;
    }
    if (!cn_data.bend_data.at(sidx).bend_values.empty())
    {
        cn.append_attribute("bend") = "0";
    }
    if (Has(cn_data.mask.at(sidx), sng::HAMMERON))
    {
        cn.append_attribute("hammerOn") = 1;
    }
    if (Has(cn_data.mask.at(sidx), sng::HARMONIC))
    {
        cn.append_attribute("harmonic") = 1;
    }
    if (Has(cn_data.mask.at(sidx), sng::HAMMERON) || Has(cn_data.mask.at(sidx), sng::PULLOFF))
    {
        cn.append_attribute("hopo") = 1;
    }
    if (Has(cn_data.mask.at(sidx), sng::IGNORE))
    {
        cn.append_attribute("ignore") = 1;
    }
    if (left_hand != -1)
    {
        cn.append_attribute("leftHand") = left_hand;
    }
    if (Has(cn_data.mask.at(sidx), sng::MUTE))
    {
        cn.append_attribute("mute") = 1;
    }
    if (Has(cn_data.mask.at(sidx), sng::PALMMUTE))
    {
        cn.append_attribute("palmMute") = 1;
    }
    if (Has(cn_data.mask.at(sidx), sng::PLUCK))
    {
        cn.append_attribute("pluck") = 1;
    }
    if (Has(cn_data.mask.at(sidx), sng::PULLOFF))
    {
        cn.append_attribute("pullOff") = 1;
    }
    if (Has(cn_data.mask.at(sidx), sng::SLAP))
    {
        cn.append_attribute("slap") = 1;
    }
    if (Has(cn_data.mask.at(sidx), sng::SLIDE) && cn_data.slide_to.at(sidx) >= 0)
    {
        cn.append_attribute("slideTo") = cn_data.slide_to.at(sidx);
    }
    if (Has(cn_data.mask.at(sidx), sng::TREMOLO))
    {
        cn.append_attribute("tremolo") = 1;
    }
    if (Has(cn_data.mask.at(sidx), sng::PINCHHARMONIC))
    {
        cn.append_attribute("harmonicPinch") = 1;
    }
    if (Has(cn_data.mask.at(sidx), sng::RIGHTHAND))
    {
        cn.append_attribute("rightHand") = 1;
    }
    if (Has(cn_data.mask.at(sidx), sng::SLIDEUNPITCHEDTO) && cn_data.slide_unpitch_to.at(sidx) >= 0)
    {
        cn.append_attribute("slideUnpitchTo") = cn_data.slide_unpitch_to.at(sidx);
    }
    if (Has(cn_data.mask.at(sidx), sng::VIBRATO) && cn_data.vibrato.at(sidx) > 0)
    {
        cn.append_attribute("vibrato") = cn_data.vibrato.at(sidx);
    }

    WriteBendValues(cn, cn_data.bend_data.at(sidx).bend_values);
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
        (manifest && manifest->title.has_value()) ? manifest->title->c_str() : "");
    song.append_child("arrangement")
        .text()
        .set((manifest && manifest->arrangement.has_value()) ? manifest->arrangement->c_str() : "");
    song.append_child("part").text().set(sng.metadata.part);
    song.append_child("offset").text().set(FormatFloat(-sng.metadata.start_time).c_str());
    song.append_child("centOffset")
        .text()
        .set((manifest && manifest->cent_offset.has_value()) ? *manifest->cent_offset : 0.0f);
    song.append_child("songLength").text().set(FormatFloat(sng.metadata.song_length).c_str());
    song.append_child("songNameSort")
        .text()
        .set((manifest && manifest->song_name_sort.has_value()) ? manifest->song_name_sort->c_str()
                                                                : "");
    song.append_child("startBeat").text().set(FormatFloat(sng.metadata.start_time).c_str());

    float average_tempo = 120.0f;
    if (manifest)
    {
        average_tempo = manifest->average_tempo.value_or(0.0f);
    }
    song.append_child("averageTempo").text().set(FormatFloat(average_tempo).c_str());

    auto tuning = song.append_child("tuning");
    for (int i = 0; i < 6; ++i)
    {
        const int value = std::cmp_less(i, sng.metadata.tuning.size()) ? sng.metadata.tuning[i] : 0;
        tuning.append_attribute(std::format("string{}", i).c_str()) = value;
    }

    song.append_child("capo").text().set(std::max<int>(0, sng.metadata.capo_fret_id));
    song.append_child("artistName")
        .text()
        .set((manifest && manifest->artist_name.has_value()) ? manifest->artist_name->c_str() : "");
    song.append_child("artistNameSort")
        .text()
        .set((manifest && manifest->artist_name_sort.has_value())
                 ? manifest->artist_name_sort->c_str()
                 : "");
    song.append_child("albumName")
        .text()
        .set((manifest && manifest->album_name.has_value()) ? manifest->album_name->c_str() : "");
    song.append_child("albumNameSort")
        .text()
        .set((manifest && manifest->album_name_sort.has_value())
                 ? manifest->album_name_sort->c_str()
                 : "");
    song.append_child("albumYear")
        .text()
        .set((manifest && manifest->album_year.has_value()) ? *manifest->album_year : 0);
    song.append_child("crowdSpeed").text().set(1);

    WriteArrangementProperties(song, manifest ? manifest->arrangement_properties.value_or(
                                                    SngManifestArrangementProperties{})
                                              : SngManifestArrangementProperties{});
    song.append_child("lastConversionDateTime")
        .text()
        .set(sng.metadata.last_conversion_date_time.c_str());

    auto phrases = song.append_child("phrases");
    phrases.append_attribute("count") = static_cast<int>(sng.phrases.size());
    for (const auto& phrase : sng.phrases)
    {
        auto node = phrases.append_child("phrase");
        node.append_attribute("maxDifficulty") = phrase.max_difficulty;
        node.append_attribute("name") = phrase.name.c_str();
        if (phrase.disparity == 1)
        {
            node.append_attribute("disparity") = 1;
        }
        if (phrase.ignore == 1)
        {
            node.append_attribute("ignore") = 1;
        }
        if (phrase.solo == 1)
        {
            node.append_attribute("solo") = 1;
        }
    }

    auto phrase_iterations = song.append_child("phraseIterations");
    phrase_iterations.append_attribute("count") = static_cast<int>(sng.phrase_iterations.size());
    for (const auto& pi : sng.phrase_iterations)
    {
        auto node = phrase_iterations.append_child("phraseIteration");
        node.append_attribute("time") = FormatFloat(pi.start_time).c_str();
        node.append_attribute("phraseId") = pi.phrase_id;
        if (pi.difficulty[0] > 0 || pi.difficulty[1] > 0 || pi.difficulty[2] > 0)
        {
            auto hero_levels = node.append_child("heroLevels");
            hero_levels.append_attribute("count") = 3;
            for (size_t i = 0; i < 3; ++i)
            {
                auto hero = hero_levels.append_child("heroLevel");
                hero.append_attribute("hero") = static_cast<int>(i) + 1;
                hero.append_attribute("difficulty") = pi.difficulty.at(i);
            }
        }
    }

    auto nld_node = song.append_child("newLinkedDiffs");
    nld_node.append_attribute("count") = static_cast<int>(sng.nlinked_difficulties.size());
    for (const auto& nld : sng.nlinked_difficulties)
    {
        auto node = nld_node.append_child("newLinkedDiff");
        node.append_attribute("levelBreak") = nld.level_break;
        node.append_attribute("ratio") = "1.000";
        node.append_attribute("phraseCount") = static_cast<int>(nld.nld_phrases.size());
        for (const auto phrase_id : nld.nld_phrases)
        {
            auto phrase = node.append_child("nld_phrase");
            phrase.append_attribute("id") = phrase_id;
        }
    }

    auto phrase_props = song.append_child("phraseProperties");
    phrase_props.append_attribute("count") = static_cast<int>(sng.phrase_extra_infos.size());
    for (const auto& info : sng.phrase_extra_infos)
    {
        auto node = phrase_props.append_child("phraseProperty");
        node.append_attribute("phraseId") = info.phrase_id;
        node.append_attribute("redundant") = info.redundant;
        node.append_attribute("levelJump") = info.level_jump;
        node.append_attribute("empty") = info.empty;
        node.append_attribute("difficulty") = info.difficulty;
    }

    auto chord_templates = song.append_child("chordTemplates");
    chord_templates.append_attribute("count") = static_cast<int>(sng.chords.size());
    for (const auto& chord : sng.chords)
    {
        auto node = chord_templates.append_child("chordTemplate");
        node.append_attribute("chordName") = chord.name.c_str();
        std::string display_name = chord.name;
        if (chord.mask == 1)
        {
            display_name += "-arp";
        }
        else if (chord.mask == 2)
        {
            display_name += "-nop";
        }
        node.append_attribute("displayName") = display_name.c_str();
        for (size_t i = 0; i < 6; ++i)
        {
            if (chord.fingers.at(i) != -1)
            {
                node.append_attribute(std::format("finger{}", i).c_str()) = chord.fingers.at(i);
            }
        }
        for (size_t i = 0; i < 6; ++i)
        {
            if (chord.frets.at(i) != -1)
            {
                node.append_attribute(std::format("fret{}", i).c_str()) = chord.frets.at(i);
            }
        }
    }

    auto ebeats = song.append_child("ebeats");
    ebeats.append_attribute("count") = static_cast<int>(sng.bpms.size());
    for (const auto& bpm : sng.bpms)
    {
        auto node = ebeats.append_child("ebeat");
        node.append_attribute("time") = FormatFloat(bpm.time).c_str();
        if ((bpm.mask & 0x01) != 0)
        {
            node.append_attribute("measure") = bpm.measure;
        }
    }

    if (manifest && manifest->tone_base.has_value() && !manifest->tone_base->empty())
    {
        song.append_child("tonebase").text().set(manifest->tone_base->c_str());
    }
    if (manifest)
    {
        static constexpr std::array<const char*, 4> g_k_tone_name_tags = {"tonea", "toneb", "tonec",
                                                                          "toned"};
        for (size_t i = 0; i < g_k_tone_name_tags.size(); ++i)
        {
            const auto& tone_name = manifest->tone_names.at(i);
            if (tone_name.has_value() && !tone_name->empty())
            {
                song.append_child(g_k_tone_name_tags.at(i)).text().set(tone_name->c_str());
            }
        }
    }

    auto tones = song.append_child("tones");
    tones.append_attribute("count") = static_cast<int>(sng.tones.size());
    for (const auto& tone : sng.tones)
    {
        auto node = tones.append_child("tone");
        node.append_attribute("time") = FormatFloat(tone.time).c_str();
        node.append_attribute("id") = tone.tone_id;

        std::string tone_name = "N/A";
        if (manifest && tone.tone_id >= 0 && tone.tone_id < 4)
        {
            tone_name = manifest->tone_names.at(static_cast<size_t>(tone.tone_id)).value_or("");
        }
        node.append_attribute("name") = tone_name.c_str();
    }

    auto sections = song.append_child("sections");
    sections.append_attribute("count") = static_cast<int>(sng.sections.size());
    for (const auto& section : sng.sections)
    {
        auto node = sections.append_child("section");
        node.append_attribute("name") = section.name.c_str();
        node.append_attribute("number") = section.number;
        node.append_attribute("startTime") = FormatFloat(section.start_time).c_str();
    }

    auto events = song.append_child("events");
    events.append_attribute("count") = static_cast<int>(sng.events.size());
    for (const auto& event : sng.events)
    {
        auto node = events.append_child("event");
        node.append_attribute("time") = FormatFloat(event.time).c_str();
        node.append_attribute("code") = event.name.c_str();
    }

    auto transcription_track = song.append_child("transcriptionTrack");
    transcription_track.append_attribute("difficulty") = -1;
    auto tt_notes = transcription_track.append_child("notes");
    tt_notes.append_attribute("count") = 0;
    auto tt_chords = transcription_track.append_child("chords");
    tt_chords.append_attribute("count") = 0;
    auto tt_anchors = transcription_track.append_child("anchors");
    tt_anchors.append_attribute("count") = 0;
    auto tt_hand_shapes = transcription_track.append_child("handShapes");
    tt_hand_shapes.append_attribute("count") = 0;

    auto levels = song.append_child("levels");
    levels.append_attribute("count") = static_cast<int>(sng.arrangements.size());
    for (const auto& arr : sng.arrangements)
    {
        auto level = levels.append_child("level");
        level.append_attribute("difficulty") = arr.difficulty;

        std::vector<const sng::Note*> single_notes;
        std::vector<const sng::Note*> chords;
        for (const auto& note : arr.notes)
        {
            if (note.chord_id >= 0 && Has(note.mask, sng::CHORD))
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
            node.append_attribute("time") = FormatFloat(note->time).c_str();
            node.append_attribute("string") = note->string;
            node.append_attribute("fret") = note->fret;
            if (note->sustain > 0.0f)
            {
                node.append_attribute("sustain") = FormatFloat(note->sustain).c_str();
            }
            WriteNoteFlags(node, *note);
            WriteBendValues(node, note->bend_values);
        }

        auto chords_node = level.append_child("chords");
        chords_node.append_attribute("count") = static_cast<int>(chords.size());
        for (const auto* note : chords)
        {
            auto node = chords_node.append_child("chord");
            node.append_attribute("time") = FormatFloat(note->time).c_str();
            node.append_attribute("chordId") = note->chord_id;
            if (Has(note->mask, sng::PARENT))
            {
                node.append_attribute("linkNext") = 1;
            }
            if (Has(note->mask, sng::ACCENT))
            {
                node.append_attribute("accent") = 1;
            }
            if (Has(note->mask, sng::FRETHANDMUTE))
            {
                node.append_attribute("fretHandMute") = 1;
            }
            if (Has(note->mask, sng::HIGHDENSITY))
            {
                node.append_attribute("highDensity") = 1;
            }
            if (Has(note->mask, sng::IGNORE))
            {
                node.append_attribute("ignore") = 1;
            }
            if (Has(note->mask, sng::PALMMUTE))
            {
                node.append_attribute("palmMute") = 1;
            }
            if (Has(note->mask, sng::HAMMERON) || Has(note->mask, sng::PULLOFF))
            {
                node.append_attribute("hopo") = 1;
            }

            if (Has(note->mask, sng::CHORDPANEL))
            {
                for (int s = 0; s < 6; ++s)
                {
                    WriteChordNoteFromTemplate(node, sng, *note, s);
                }
            }
        }

        auto anchors = level.append_child("anchors");
        anchors.append_attribute("count") = static_cast<int>(arr.anchors.size());
        for (const auto& anchor : arr.anchors)
        {
            auto node = anchors.append_child("anchor");
            node.append_attribute("time") = FormatFloat(anchor.start_time).c_str();
            node.append_attribute("fret") = anchor.fret;
            node.append_attribute("width") = FormatFloat(static_cast<float>(anchor.width)).c_str();
        }

        struct HandShapeView
        {
            int32_t chord_id = 0;
            float start = 0;
            float end = 0;
        };

        std::vector<HandShapeView> handshapes;
        handshapes.reserve(arr.fingerprints_handshape.size() + arr.fingerprints_arpeggio.size());
        for (const auto& hs : arr.fingerprints_handshape)
        {
            handshapes.push_back(
                {.chord_id = hs.chord_id, .start = hs.start_time, .end = hs.end_time});
        }
        for (const auto& arp : arr.fingerprints_arpeggio)
        {
            handshapes.push_back(
                {.chord_id = arp.chord_id, .start = arp.start_time, .end = arp.end_time});
        }
        std::ranges::sort(handshapes, [](const HandShapeView& a, const HandShapeView& b) {
            return a.start < b.start;
        });

        auto handshapes_node = level.append_child("handShapes");
        handshapes_node.append_attribute("count") = static_cast<int>(handshapes.size());
        for (const auto& hs : handshapes)
        {
            auto node = handshapes_node.append_child("handShape");
            node.append_attribute("chordId") = hs.chord_id;
            node.append_attribute("startTime") = FormatFloat(hs.start).c_str();
            node.append_attribute("endTime") = FormatFloat(hs.end).c_str();
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
    if (!sng.vocals.empty())
    {
        WriteVocalXml(sng, output_path);
    }
    else
    {
        WriteInstrumentalXml(sng, output_path, manifest);
    }
}
