#include "KnowledgeBase.h"
#include "BinaryData.h"

namespace mm
{
    const KnowledgeBase& KnowledgeBase::get()
    {
        static const KnowledgeBase instance;   // thread-safe initialisation
        return instance;
    }

    KnowledgeBase::KnowledgeBase()
    {
        const auto json = juce::String::fromUTF8 (BinaryData::knowledge_json, BinaryData::knowledge_jsonSize);
        const auto root = juce::JSON::parse (json);
        jassert (root.isObject());   // knowledge.json is malformed

        if (auto* arr = root["processes"].getArray())
            for (auto& p : *arr)
                processes_.push_back ({ p["id"].toString(), p["name"].toString() });

        if (auto* arr = root["instruments"].getArray())
        {
            for (auto& i : *arr)
            {
                Instrument inst { i["id"].toString(), i["name"].toString(), {} };
                if (auto* kw = i["keywords"].getArray())
                    for (auto& k : *kw)
                        inst.keywords.add (k.toString().toLowerCase());
                instruments_.push_back (inst);
            }
        }

        if (auto* arr = root["genres"].getArray())
        {
            for (auto& g : *arr)
            {
                Genre genre;
                genre.id = g["id"].toString();
                genre.name = g["name"].toString();
                genre.summary = g["summary"].toString();
                genre.bpm = g["bpm"].toString();
                genre.theoryText = g["theoryText"].toString();

                if (auto* els = g["elements"].getArray())
                    for (auto& e : *els)
                        genre.elements.add (e.toString());

                if (auto* steps = g["workflow"].getArray())
                    for (auto& st : *steps)
                        genre.workflow.push_back ({ st["phase"].toString(), st["title"].toString(), st["text"].toString() });

                if (auto* guidesObj = g["guides"].getDynamicObject())
                {
                    for (auto& entry : guidesObj->getProperties())
                    {
                        const auto& gv = entry.value;
                        Guide guide;

                        if (auto* range = gv["peakRange"].getArray(); range != nullptr && range->size() == 2)
                        {
                            guide.peakMinDb = (float) (double) range->getReference (0);
                            guide.peakMaxDb = (float) (double) range->getReference (1);
                        }

                        if (auto* zones = gv["zones"].getArray())
                        {
                            for (auto& z : *zones)
                            {
                                Zone zone;
                                zone.lowHz  = (float) (double) z["lo"];
                                zone.highHz = (float) (double) z["hi"];
                                zone.kind   = z["kind"].toString();
                                zone.label  = z["label"].toString();
                                zone.tip    = z["tip"].toString();

                                if (auto* t = z["target"].getArray(); t != nullptr && t->size() == 2)
                                {
                                    zone.targetMinDb = (float) (double) t->getReference (0);
                                    zone.targetMaxDb = (float) (double) t->getReference (1);
                                }
                                zone.tooMuch   = z["tooMuch"].toString();
                                zone.tooLittle = z["tooLittle"].toString();
                                guide.zones.push_back (zone);
                            }
                        }

                        if (auto* tipsObj = gv["tips"].getDynamicObject())
                            for (auto& t : tipsObj->getProperties())
                                guide.tips[t.name.toString()] = t.value.toString();

                        genre.guides[entry.name.toString()] = std::move (guide);
                    }
                }

                genres_.push_back (std::move (genre));
            }
        }
    }

    const Guide* KnowledgeBase::findGuide (int genreIndex, int instrumentIndex) const
    {
        if (! juce::isPositiveAndBelow (genreIndex, (int) genres_.size())
            || ! juce::isPositiveAndBelow (instrumentIndex, (int) instruments_.size()))
            return nullptr;

        const auto& guides = genres_[(size_t) genreIndex].guides;
        auto it = guides.find (instruments_[(size_t) instrumentIndex].id);
        return it != guides.end() ? &it->second : nullptr;
    }

    int KnowledgeBase::indexOfGenre (const juce::String& id) const
    {
        for (size_t i = 0; i < genres_.size(); ++i)
            if (genres_[i].id == id)
                return (int) i;
        return -1;
    }

    int KnowledgeBase::indexOfInstrument (const juce::String& id) const
    {
        for (size_t i = 0; i < instruments_.size(); ++i)
            if (instruments_[i].id == id)
                return (int) i;
        return -1;
    }

    int KnowledgeBase::guessInstrumentFromTrackName (const juce::String& trackName) const
    {
        const auto name = trackName.toLowerCase();
        const auto words = juce::StringArray::fromTokens (name, " _-.:/()[]0123456789", "");

        // Pick the instrument with the longest matching keyword, so "Vocal Chops" finds
        // the chops keyword rather than plain "vocal".
        int best = -1, bestLength = 0;
        for (size_t i = 0; i < instruments_.size(); ++i)
        {
            for (auto& kw : instruments_[i].keywords)
            {
                // Long keywords can match inside a name; short ones ("bd", "ep") must be a whole word.
                const bool match = kw.length() >= 3 ? name.contains (kw) : words.contains (kw);
                if (match && kw.length() > bestLength)
                {
                    best = (int) i;
                    bestLength = kw.length();
                }
            }
        }
        return best;
    }
}
