#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Banco.h"
#include "BancoClasicos.h"
#include <set>

namespace { juce::String U (const char* s) { return juce::String::fromUTF8 (s); } }

B7Processor::B7Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "Barullo7FM", createLayout())
{
    volumeParam     = apvts.getRawParameterValue ("volume");
    bendParam       = apvts.getRawParameterValue ("bend");
    wheelRangeParam = apvts.getRawParameterValue ("wheelRange");
    wheelPitchParam = apvts.getRawParameterValue ("wheelPitch");
    wheelAmpParam   = apvts.getRawParameterValue ("wheelAmp");
    wheelEgParam    = apvts.getRawParameterValue ("wheelEg");

    for (int c = 0; c < b7::NUM_CARTRIDGES; ++c)
    {
        Cart cart;
        cart.name = b7::CARTRIDGES[c].name;
        cart.title = U (b7::CARTRIDGES[c].title);
        cart.descKind = b7::CARTRIDGES[c].descKind;
        for (int k = 0; k < 32; ++k) cart.voices[(size_t) k] = b7::unpackVmem (b7::CARTRIDGES[c].vmem + k * 128);
        cart.hash = b7::bankHash (cart.voices.data());
        carts.push_back (cart);
    }
    internal = carts[0].voices;   // a memoria interna empeza cos sons clásicos
    for (auto& p : fileCart) p = b7::initPatch();
    source = 0; voiceNum = 0;
    edit = carts[0].voices[0];

    // a carpeta de cartuchos lémbrase nun ficheiro de axustes do usuario
    juce::PropertiesFile::Options o;
    o.applicationName = "Barullo 7FM";
    o.folderName = "Escoita";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    settings = std::make_unique<juce::PropertiesFile> (o);
    const auto saved = settings->getValue ("cartFolder");
    if (saved.isNotEmpty()) { cartFolder = juce::File (saved); rescanCartFolder(); }

    publish (true, true);
    startTimerHz (30);
}

// ================================================================ carpeta de cartuchos
void B7Processor::setCartFolder (const juce::File& f)
{
    cartFolder = f;
    if (settings) { settings->setValue ("cartFolder", f.getFullPathName()); settings->saveIfNeeded(); }
    rescanCartFolder();
}

void B7Processor::rescanCartFolder()
{
    const uint64_t current = (source >= 0 && source < numCarts()) ? carts[(size_t) source].hash : 0;
    carts.resize ((size_t) b7::NUM_CARTRIDGES);
    if (! cartFolder.isDirectory()) { changes.fetch_add (1); return; }

    std::vector<Cart> known ((size_t) b7::NUM_KNOWN_BANKS), others;
    std::vector<bool> haveKnown ((size_t) b7::NUM_KNOWN_BANKS, false);
    std::set<uint64_t> seen;
    for (const auto& c : carts) seen.insert (c.hash);
    int files = 0;
    for (const auto& entry : juce::RangedDirectoryIterator (cartFolder, true, "*.syx;*.SYX;*.Syx", juce::File::findFiles))
    {
        if (++files > 512) break;
        const auto f = entry.getFile();
        if (f.getSize() <= 0 || f.getSize() > 1024 * 1024) continue;
        juce::MemoryBlock mb;
        if (! f.loadFileAsData (mb)) continue;
        const auto* bytes = static_cast<const uint8_t*> (mb.getData());
        const auto voices = b7::parseSysex (std::vector<uint8_t> (bytes, bytes + mb.getSize()));
        const int nBanks = (int) voices.size() / 32;
        for (int b = 0; b < nBanks; ++b)
        {
            Cart cart;
            for (int k = 0; k < 32; ++k) cart.voices[(size_t) k] = voices[(size_t) (b * 32 + k)];
            cart.hash = b7::bankHash (cart.voices.data());
            if (! seen.insert (cart.hash).second) continue;   // repetido
            int kb = -1;
            for (int i = 0; i < b7::NUM_KNOWN_BANKS; ++i) if (b7::KNOWN_BANKS[i].hash == cart.hash) { kb = i; break; }
            if (kb >= 0)
            {
                cart.name = b7::KNOWN_BANKS[kb].name;
                cart.title = U (b7::KNOWN_BANKS[kb].title);
                known[(size_t) kb] = cart; haveKnown[(size_t) kb] = true;
            }
            else
            {
                cart.name = f.getFileNameWithoutExtension().toUpperCase().substring (0, 10) + (nBanks > 1 ? "/" + juce::String (b + 1) : juce::String());
                cart.title = U ("Ficheiro: ") + f.getFileName();
                others.push_back (cart);
            }
        }
    }
    for (int i = 0; i < b7::NUM_KNOWN_BANKS; ++i) if (haveKnown[(size_t) i]) carts.push_back (known[(size_t) i]);
    std::sort (others.begin(), others.end(), [] (const Cart& a, const Cart& b) { return a.name.compareNatural (b.name) < 0; });
    for (auto& c : others) carts.push_back (c);

    // se o cartucho escollido segue aí, mantense
    if (source >= 0) source = current ? juce::jmax (0, sourceForHash (current)) : 0;
    changes.fetch_add (1);
}

int B7Processor::sourceForHash (uint64_t h) const
{
    for (int i = 0; i < numCarts(); ++i) if (carts[(size_t) i].hash == h) return i;
    return -1;
}

B7Processor::~B7Processor() { stopTimer(); }

juce::AudioProcessorValueTreeState::ParameterLayout B7Processor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume",
        NormalisableRange<float> (-30.0f, 6.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "bend", 1 }, "Alcance do bend", 0, 12, 2,
        AudioParameterIntAttributes().withLabel ("st")));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "wheelRange", 1 }, U ("Roda de modulación: alcance"), 0, 99, 99));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "wheelPitch", 1 }, U ("Roda de modulación: afinación"), true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "wheelAmp", 1 }, U ("Roda de modulación: amplitude"), false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "wheelEg", 1 }, U ("Roda de modulación: envolvente"), false));
    return l;
}

// ================================================================ memoria
std::array<b7::Patch, 32>& B7Processor::bankOf (int src)
{
    if (src == INTERNAL) return internal;
    if (src == FILE_CART) return fileCart;
    return carts[(size_t) juce::jlimit (0, numCarts() - 1, src)].voices;
}
const std::array<b7::Patch, 32>& B7Processor::bankOf (int src) const { return const_cast<B7Processor*> (this)->bankOf (src); }

const b7::Patch& B7Processor::voiceAt (int src, int i) const { return bankOf (src)[(size_t) juce::jlimit (0, 31, i)]; }

juce::String B7Processor::sourceName (int src) const
{
    if (src == INTERNAL) return "INTERNA";
    if (src == FILE_CART) return fileCartLoaded ? fileCartName : juce::String ("FICHEIRO");
    return carts[(size_t) juce::jlimit (0, numCarts() - 1, src)].name;
}

juce::String B7Processor::sourceTitle (int src) const
{
    if (src == INTERNAL) return U ("Memoria interna: os teus sons gardados");
    if (src == FILE_CART) return fileCartLoaded ? U ("Cartucho aberto dun ficheiro") : U ("Abre un .syx para velo aquí");
    return carts[(size_t) juce::jlimit (0, numCarts() - 1, src)].title;
}

juce::String B7Processor::voiceDesc (int src, int i) const
{
    if (src < 0 || src >= numCarts()) return {};
    i = juce::jlimit (0, 31, i);
    switch (carts[(size_t) src].descKind)
    {
        case 1: return U (b7::CLASICOS_DESC[i]);
        case 2: return U (b7::FACTORY[i].desc);
        default: return {};
    }
}

const juce::String B7Processor::getProgramName (int index)
{
    return juce::String (index + 1).paddedLeft ('0', 2) + " " + juce::String (b7::patchName (voiceAt (source, index)));
}

void B7Processor::publish (bool bank, bool reset)
{
    {
        const juce::SpinLock::ScopedLockType lk (sharedLock);
        if (bank) { shared.bank = bankOf (source); shared.bankDirty = true; }
        shared.patch = edit;
        if (reset) shared.patchReset = true; else shared.patchEdited = true;
    }
    changes.fetch_add (1);
}

void B7Processor::pullShared()
{
    const juce::SpinLock::ScopedTryLockType lk (sharedLock);
    if (! lk.isLocked()) return;   // a interface está a escribir: cóllese no seguinte bloque
    if (shared.bankDirty) { audioBank = shared.bank; shared.bankDirty = false; }
    if (shared.patchReset)
    {
        synth.setPatch (shared.patch);
        synth.setOpSwitch (shared.patch[155]);
    }
    else if (shared.patchEdited)
    {
        const auto& cur = synth.getPatch();
        for (int i = 0; i < 155; ++i)
            if (cur[(size_t) i] != shared.patch[(size_t) i]) synth.editPatch (i, shared.patch[(size_t) i]);
        if (cur[155] != shared.patch[155]) synth.setOpSwitch (shared.patch[155]);
    }
    shared.patchReset = shared.patchEdited = false;
}

void B7Processor::selectSource (int src)
{
    if (src == FILE_CART && ! fileCartLoaded) return;
    source = (src == FILE_CART || src == INTERNAL) ? src : juce::jlimit (0, numCarts() - 1, src);
    edit = bankOf (source)[(size_t) voiceNum]; edit[155] = 0x3f;
    publish (true, true);
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void B7Processor::selectVoice (int i)
{
    voiceNum = juce::jlimit (0, 31, i);
    edit = bankOf (source)[(size_t) voiceNum]; edit[155] = 0x3f;
    publish (false, true);
}

bool B7Processor::isEdited() const
{
    const auto& o = bankOf (source)[(size_t) voiceNum];
    return ! std::equal (o.begin(), o.begin() + 155, edit.begin()) || edit[155] != 0x3f;
}

void B7Processor::setParam (int index, int value)
{
    if (index < 0 || index >= 155) return;
    const auto v = (uint8_t) juce::jlimit (0, (int) b7::patchMax()[(size_t) index], value);
    if (edit[(size_t) index] == v) return;
    edit[(size_t) index] = v;
    publish (false, false);
}

void B7Processor::setOpEnabled (int op, bool on)
{
    if (op < 1 || op > 6) return;
    const int bit = 1 << (6 - op);
    edit[155] = (uint8_t) (on ? (edit[155] | bit) : (edit[155] & ~bit));
    publish (false, false);
}

void B7Processor::setVoiceName (const juce::String& n)
{
    b7::setPatchName (edit, n.toUpperCase().substring (0, 10).toStdString());
    publish (false, false);
}

void B7Processor::revert() { selectVoice (voiceNum); }

void B7Processor::storeTo (int slot)
{
    slot = juce::jlimit (0, 31, slot);
    internal[(size_t) slot] = edit; internal[(size_t) slot][155] = 0x3f;
    source = INTERNAL; voiceNum = slot;
    publish (true, false);
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void B7Processor::copySourceToInternal()
{
    if (source == INTERNAL) return;
    internal = bankOf (source);
    source = INTERNAL;
    publish (true, false);
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void B7Processor::initVoice()
{
    edit = b7::initPatch();
    publish (false, true);
}

// ================================================================ ficheiros .syx
bool B7Processor::loadSyxFile (const juce::File& f, juce::String& message)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb) || mb.getSize() == 0 || mb.getSize() > 1024 * 1024)
    {
        message = U ("Non se puido ler o ficheiro.");
        return false;
    }
    const auto* bytes = static_cast<const uint8_t*> (mb.getData());
    const auto voices = b7::parseSysex (std::vector<uint8_t> (bytes, bytes + mb.getSize()));
    if (voices.empty())
    {
        message = U ("Non parece un ficheiro de sons de DX7 (.syx de 32 sons ou dun só son).");
        return false;
    }
    if (voices.size() == 1)
    {
        edit = voices[0];
        publish (false, true);
        message = U ("Son cargado: ") + juce::String (b7::patchName (edit)) + U (". Para gardalo, usa GARDAR.");
        return true;
    }
    for (size_t k = 0; k < 32; ++k) fileCart[k] = k < voices.size() ? voices[k] : b7::initPatch();
    fileCartName = f.getFileNameWithoutExtension().toUpperCase().substring (0, 10);
    fileCartLoaded = true;
    selectSource (FILE_CART);
    message = U ("Cartucho aberto: ") + f.getFileName() + " (" + juce::String ((int) juce::jmin<size_t> (voices.size(), 32)) + " sons)";
    return true;
}

bool B7Processor::saveBankFile (const juce::File& f, int src) const
{
    const auto& b = bankOf (src);
    const auto d = b7::bankToSysex (std::vector<b7::Patch> (b.begin(), b.end()));
    return f.replaceWithData (d.data(), d.size());
}

bool B7Processor::saveVoiceFile (const juce::File& f) const
{
    const auto d = b7::voiceToSysex (edit);
    return f.replaceWithData (d.data(), d.size());
}

// ================================================================ SysEx que chega polo MIDI (p. ex. desde un DX7)
void B7Processor::pushSysex (const uint8_t* data, int size)
{
    if (size <= 0 || size > 8192) return;
    if (sysexFifo.getFreeSpace() < size + 4) return;
    uint8_t hdr[4] = { (uint8_t) (size & 0xff), (uint8_t) ((size >> 8) & 0xff), 0, 0 };
    auto put = [this] (const uint8_t* src, int n)
    {
        const auto w = sysexFifo.write (n);
        if (w.blockSize1 > 0) std::memcpy (&sysexBuf[(size_t) w.startIndex1], src, (size_t) w.blockSize1);
        if (w.blockSize2 > 0) std::memcpy (&sysexBuf[(size_t) w.startIndex2], src + w.blockSize1, (size_t) w.blockSize2);
    };
    put (hdr, 4);
    put (data, size);
}

void B7Processor::setParamFromHost (const char* id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void B7Processor::handleSysex (const std::vector<uint8_t>& d)
{
    if (d.size() < 4 || d[0] != 0x43) return;
    const int sub = d[1] & 0xf0;
    if (sub == 0x00)   // volcado: un son (formato 0) ou un banco de 32 (formato 9)
    {
        std::vector<uint8_t> full; full.reserve (d.size() + 2);
        full.push_back (0xF0); full.insert (full.end(), d.begin(), d.end()); full.push_back (0xF7);
        const auto voices = b7::parseSysex (full);
        if (voices.empty()) return;
        if (d[2] == 9 && voices.size() >= 32)
        {
            // como no DX7: o banco recibido vai á memoria interna
            for (size_t k = 0; k < 32; ++k) internal[k] = voices[k];
            source = INTERNAL;
            edit = internal[(size_t) voiceNum]; edit[155] = 0x3f;
            publish (true, true);
            notice = U ("RECIBIDO BANCO");
        }
        else
        {
            edit = voices[0];
            publish (false, true);
            notice = U ("RECIBIDO SON");
        }
        noticeTime = juce::Time::getMillisecondCounter();
        updateHostDisplay (ChangeDetails().withProgramChanged (true));
    }
    else if (sub == 0x10 && d.size() >= 5)   // cambio dun parámetro (o DX7 mándao ao mover o deslizador de datos)
    {
        const int group = (d[2] >> 2) & 0x1f, param = ((d[2] & 3) << 7) | d[3], value = d[4] & 0x7f;
        if (group == 0)
        {
            if (param < 155) setParam (param, value);
            else if (param == 155) { edit[155] = (uint8_t) (value & 0x3f); publish (false, false); }
        }
        else if (group == 2)   // parámetros de función
        {
            if (param == 65) setParamFromHost ("bend", (float) juce::jlimit (0, 12, value));
            else if (param == 70) setParamFromHost ("wheelRange", (float) juce::jlimit (0, 99, value));
            else if (param == 71)
            {
                setParamFromHost ("wheelPitch", (value & 1) ? 1.0f : 0.0f);
                setParamFromHost ("wheelAmp", (value & 2) ? 1.0f : 0.0f);
                setParamFromHost ("wheelEg", (value & 4) ? 1.0f : 0.0f);
            }
        }
    }
}

void B7Processor::timerCallback()
{
    const int p = pendingProgram.exchange (-1);
    if (p >= 0)
    {
        // o fío de audio xa cambiou o son; aquí só se pon ao día o búfer de edición
        voiceNum = p;
        edit = bankOf (source)[(size_t) voiceNum]; edit[155] = 0x3f;
        publish (false, false);
        updateHostDisplay (ChangeDetails().withProgramChanged (true));
    }
    while (sysexFifo.getNumReady() >= 4)
    {
        uint8_t hdr[4];
        auto get = [this] (uint8_t* dst, int n)
        {
            const auto r = sysexFifo.read (n);
            if (r.blockSize1 > 0) std::memcpy (dst, &sysexBuf[(size_t) r.startIndex1], (size_t) r.blockSize1);
            if (r.blockSize2 > 0) std::memcpy (dst + r.blockSize1, &sysexBuf[(size_t) r.startIndex2], (size_t) r.blockSize2);
        };
        get (hdr, 4);
        const int size = hdr[0] | (hdr[1] << 8);
        std::vector<uint8_t> msg ((size_t) size);
        get (msg.data(), size);
        handleSysex (msg);
    }
}

// ================================================================ audio
bool B7Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void B7Processor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    setLatencySamples (0);
    synth.prepare (sampleRate);
    {
        const juce::SpinLock::ScopedLockType lk (sharedLock);
        shared.bank = bankOf (source); shared.bankDirty = true;
        shared.patch = edit; shared.patchReset = true;
    }
    pullShared();
    teito.preparar (sampleRate, 0.080, 0.891);
    volS = juce::Decibels::decibelsToGain ((double) volumeParam->load());
    lastBend = lastWheel = -1;
    keyboardState.reset();
}

// O MIDI vai ao firmware do DX7 emulado: o pedal de sostemento e o reparto das 16 voces son
// os seus (por iso aquí non se usan os do núcleo, Sostido e Voces: cambiarían o son).
void B7Processor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) synth.noteOn (m.getNoteNumber(), m.getVelocity());
    else if (m.isNoteOff()) synth.noteOff (m.getNoteNumber());
    else if (m.isSustainPedalOn()) synth.setSustain (true);
    else if (m.isSustainPedalOff()) synth.setSustain (false);
    else if (m.isAllSoundOff()) synth.allSoundOff();
    else if (m.isAllNotesOff()) synth.allNotesOff();
    else if (m.isPitchWheel()) synth.pitchBend (m.getPitchWheelValue());
    else if (m.isResetAllControllers()) { synth.modWheel (0); synth.breath (0); synth.foot (0); synth.aftertouch (0); synth.pitchBend (8192); synth.setSustain (false); }
    else if (m.isControllerOfType (1)) synth.modWheel (m.getControllerValue());
    else if (m.isControllerOfType (2)) synth.breath (m.getControllerValue());
    else if (m.isControllerOfType (4)) synth.foot (m.getControllerValue());
    else if (m.isChannelPressure()) synth.aftertouch (m.getChannelPressureValue());
    else if (m.isProgramChange())
    {
        const int p = m.getProgramChangeNumber() & 31;   // como o DX7: 32 memorias
        auto patch = audioBank[(size_t) p]; patch[155] = 0x3f;
        synth.setPatch (patch);
        pendingProgram.store (p);
    }
    else if (m.isSysEx()) pushSysex (m.getSysExData(), m.getSysExDataSize());
}

void B7Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    pullShared();

    const int bend = (int) bendParam->load();
    if (bend != lastBend) { synth.setBendRange (bend); lastBend = bend; }
    const int wheel = (int) wheelRangeParam->load() | (wheelPitchParam->load() > 0.5f ? 0x100 : 0)
                    | (wheelAmpParam->load() > 0.5f ? 0x200 : 0) | (wheelEgParam->load() > 0.5f ? 0x400 : 0);
    if (wheel != lastWheel) { synth.setWheel (wheel & 0xff, wheel & 0x100, wheel & 0x200, wheel & 0x400); lastWheel = wheel; }

    float* L = buffer.getWritePointer (0);
    int pos = 0;
    bool anyMidi = false;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, n, meta.samplePosition);
        if (t > pos) { synth.render (L + pos, t - pos); pos = t; }
        handleMidi (meta.getMessage());
        anyMidi = true;
    }
    if (pos < n) synth.render (L + pos, n - pos);
    if (anyMidi) midiActivity.fetch_add (1);

    // volume (suavizado) e limitador de seguridade a −1 dBFS, sen anticipación
    const double volT = juce::Decibels::decibelsToGain ((double) volumeParam->load());
    const double sm = 1.0 - std::exp (-1.0 / (0.02 * sr));
    float peak = 0;
    for (int i = 0; i < n; ++i)
    {
        volS += (volT - volS) * sm;
        const double x = teito.procesar ((double) L[i] * volS);
        L[i] = (float) x;
        peak = std::max (peak, std::abs ((float) x));
    }
    for (int c = 1; c < buffer.getNumChannels(); ++c) buffer.copyFrom (c, 0, L, n);   // o DX7 é mono

    activeVoices.store (synth.activeVoices());
    float lv[6]; synth.opLevels (lv);
    for (int i = 0; i < 6; ++i) opLevel[(size_t) i].store (lv[i]);
    outLevel.store (peak);
}

// ================================================================ estado
// a memoria enteira (o «Memoria» do estado do proxecto): a fonte, o son, a memoria interna, o
// búfer de edición e o último .syx aberto. A fonte gárdase pola súa pegada: os cartuchos da
// carpeta poden cambiar de orde.
juce::ValueTree B7Processor::memoria() const
{
    juce::ValueTree m ("Memoria");
    auto b64 = [] (const std::vector<uint8_t>& d) { return juce::Base64::toBase64 (d.data(), d.size()); };
    m.setProperty ("source", source == INTERNAL ? juce::String ("int") : source == FILE_CART ? juce::String ("file")
                             : juce::String (std::to_string (carts[(size_t) source].hash)), nullptr);
    m.setProperty ("voice", voiceNum, nullptr);
    m.setProperty ("internal", b64 (b7::bankToSysex (std::vector<b7::Patch> (internal.begin(), internal.end()))), nullptr);
    m.setProperty ("edit", b64 (std::vector<uint8_t> (edit.begin(), edit.end())), nullptr);
    if (fileCartLoaded)
    {
        m.setProperty ("file", b64 (b7::bankToSysex (std::vector<b7::Patch> (fileCart.begin(), fileCart.end()))), nullptr);
        m.setProperty ("fileName", fileCartName, nullptr);
    }
    return m;
}

void B7Processor::setMemoria (const juce::ValueTree& m)
{
    if (! m.isValid()) return;
    auto unb64 = [] (const juce::String& s)
    {
        juce::MemoryOutputStream out;
        if (! juce::Base64::convertFromBase64 (out, s)) return std::vector<uint8_t>();
        const auto* p = static_cast<const uint8_t*> (out.getData());
        return std::vector<uint8_t> (p, p + out.getDataSize());
    };
    const auto in = b7::parseSysex (unb64 (m.getProperty ("internal").toString()));
    if (in.size() >= 32) for (size_t k = 0; k < 32; ++k) internal[k] = in[k];
    fileCartLoaded = false;
    if (m.hasProperty ("file"))
    {
        const auto fc = b7::parseSysex (unb64 (m.getProperty ("file").toString()));
        if (fc.size() >= 32) { for (size_t k = 0; k < 32; ++k) fileCart[k] = fc[k]; fileCartLoaded = true; fileCartName = m.getProperty ("fileName").toString(); }
    }
    const auto key = m.getProperty ("source").toString();
    if (key == "int") source = INTERNAL;
    else if (key == "file") source = fileCartLoaded ? FILE_CART : 0;
    else source = juce::jmax (0, sourceForHash ((uint64_t) std::strtoull (key.toRawUTF8(), nullptr, 10)));
    voiceNum = juce::jlimit (0, 31, (int) m.getProperty ("voice", 0));
    const auto e = unb64 (m.getProperty ("edit").toString());
    if (e.size() == 156)
    {
        std::copy (e.begin(), e.end(), edit.begin());
        const uint8_t mask = edit[155] & 0x3f;
        b7::sanitize (edit); edit[155] = mask;
    }
    else { edit = bankOf (source)[(size_t) voiceNum]; edit[155] = 0x3f; }
    publish (true, true);
}

void B7Processor::setEditBuffer (const b7::Patch& p)
{
    edit = p;
    publish (false, false);
}

escoita::Estado::Opcions B7Processor::opcionsEstado()
{
    escoita::Estado::Opcions o;
    o.extension = "b7preset";
    o.extra = [this] { return memoria(); };
    o.aplicarExtra = [this] (const juce::ValueTree& m)
    {
        setMemoria (m);
        updateHostDisplay (ChangeDetails().withProgramChanged (true));
    };
    return o;
}

// un cambio que se pode desfacer: gárdanse o antes e o despois. «facer» non se repite a
// primeira vez (o cambio xa está feito); si ao refacer.
void B7Processor::editar (const juce::String& nome, const std::function<void()>& f)
{
    const auto antes = edit;
    f();
    const auto despois = edit;
    if (antes == despois) return;
    auto primeira = std::make_shared<bool> (true);
    estado.cambio (nome, [this, despois, primeira] { if (*primeira) *primeira = false; else setEditBuffer (despois); },
                         [this, antes] { setEditBuffer (antes); });
}

void B7Processor::cambiarMemoria (const juce::String& nome, const std::function<void()>& f)
{
    const auto antes = memoria();
    f();
    const auto despois = memoria();
    if (antes.isEquivalentTo (despois)) return;
    auto primeira = std::make_shared<bool> (true);
    estado.cambio (nome, [this, despois, primeira] { if (*primeira) *primeira = false; else { setMemoria (despois); updateHostDisplay (ChangeDetails().withProgramChanged (true)); } },
                         [this, antes] { setMemoria (antes); updateHostDisplay (ChangeDetails().withProgramChanged (true)); });
}

void B7Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = apvts.copyState();
    st.removeChild (st.getChildWithName ("Memoria"), nullptr);
    st.appendChild (memoria(), nullptr);
    st.setProperty ("uiScale", uiScale, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, destData);
}

void B7Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (! xml || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    uiScale = juce::jlimit (0.6f, 2.0f, (float) tree.getProperty ("uiScale", 1.0f));
    const auto m = tree.getChildWithName ("Memoria");
    setMemoria (m);
    tree.removeChild (m, nullptr);
    apvts.replaceState (tree);
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

juce::AudioProcessorEditor* B7Processor::createEditor() { return new B7Editor (*this); }

#ifndef BARULLO_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new B7Processor(); }
#endif
