// Proba sen interface: sons de todos os cartuchos, independencia do tamaño de bloque,
// SysEx recibido polo MIDI, gardar/recuperar o estado e capturas da interface.
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, (unsigned) b.getNumChannels(), 24, {}, 0)))
        {
            os.release();
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            std::printf ("WAV: %s\n", f.getFullPathName().toRawUTF8());
        }
}

// toca unha secuencia (nota, acorde, pedal, bend, roda) en cada son do cartucho escollido
static juce::AudioBuffer<float> run (int cart, int blockSize, double sr, double* cpu, float* peakPerVoice = nullptr, bool clean = false)
{
    B7Processor p;
    p.setPlayConfigDetails (0, 2, sr, blockSize);
    p.prepareToPlay (sr, blockSize);
    p.selectSource (cart);
    struct Ev { double t; int type; int a; int b; };   // 0 on, 1 off, 2 prog, 3 cc, 5 bend
    std::vector<Ev> seq;
    double t = 0.02;
    const double per = 1.6;
    for (int v = 0; v < 32; ++v)
    {
        const double t0 = v * per + 0.02;
        seq.push_back ({ t0, 2, v, 0 });
        seq.push_back ({ t0 + 0.01, 0, 60, 100 });
        seq.push_back ({ t0 + 0.5, 1, 60, 0 });
        for (int n : { 55, 64, 71 }) seq.push_back ({ t0 + 0.6, 0, n, 70 });
        if (! clean)   // roda de modulación e bend, para probalos
        {
            seq.push_back ({ t0 + 0.7, 3, 1, 90 });
            seq.push_back ({ t0 + 0.8, 5, 12000, 0 });
            seq.push_back ({ t0 + 0.95, 5, 8192, 0 });
            seq.push_back ({ t0 + 1.0, 3, 1, 0 });
        }
        for (int n : { 55, 64, 71 }) seq.push_back ({ t0 + 1.1, 1, n, 0 });
        seq.push_back ({ t0 + 1.55, 3, 120, 0 });   // todo apagado antes do seguinte
    }
    t = 32 * per + 0.1;
    const int total = (int) (t * sr);
    juce::AudioBuffer<float> out (2, total), buf (2, blockSize);
    size_t k = 0;
    const auto c0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < total; pos += blockSize)
    {
        const int n = juce::jmin (blockSize, total - pos);
        juce::MidiBuffer midi;
        while (k < seq.size() && (int) (seq[k].t * sr) < pos + n)
        {
            const int at = juce::jmax (0, (int) (seq[k].t * sr) - pos);
            const auto& e = seq[k++];
            juce::MidiMessage m = e.type == 0 ? juce::MidiMessage::noteOn (1, e.a, (juce::uint8) e.b)
                               : e.type == 1 ? juce::MidiMessage::noteOff (1, e.a)
                               : e.type == 2 ? juce::MidiMessage::programChange (1, e.a)
                               : e.type == 3 ? juce::MidiMessage::controllerEvent (1, e.a, e.b)
                               : juce::MidiMessage::pitchWheel (1, e.a);
            midi.addEvent (m, at);
        }
        buf.setSize (2, n, false, false, true);
        buf.clear();
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
    }
    *cpu = (juce::Time::getMillisecondCounterHiRes() - c0) / 1000.0;
    if (peakPerVoice)
        for (int v = 0; v < 32; ++v)
        {
            const int a = (int) ((v * per) * sr), b = juce::jmin (total, (int) (((v + 1) * per) * sr));
            peakPerVoice[v] = out.getMagnitude (0, a, b - a);
        }
    return out;
}

static void snapshot (const juce::File& png, int page, int src, int voice)
{
    B7Processor p;
    p.prepareToPlay (48000, 512);
    p.selectSource (src);
    p.selectVoice (voice);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* ce = dynamic_cast<B7Editor*> (ed.get());
    ed->setSize (ui::MainView::W, ui::MainView::H);
    for (int n : { 60, 64, 67 }) p.keyboardState.noteOn (1, n, 1.0f);
    ce->view().showPage (page);
    juce::AudioBuffer<float> buf (2, 512);
    for (int blk = 0; blk < 12; ++blk) { juce::MidiBuffer m; buf.clear(); p.processBlock (buf, m); ce->view().timerCallback(); }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
    png.deleteFile();
    juce::FileOutputStream os (png);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("PNG: %s\n", png.getFullPathName().toRawUTF8());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        snapshot (juce::File (juce::String (argv[2])), argc > 3 ? atoi (argv[3]) : 0, argc > 4 ? atoi (argv[4]) : 0, argc > 5 ? atoi (argv[5]) : 0);
        return 0;
    }
    if (argc > 4 && juce::String (argv[1]) == "--midi")   // --midi banco.syx ficheiro.mid saida.wav
    {
        const double sr = 48000; const int bs = 256;
        B7Processor p;
        p.setPlayConfigDetails (0, 2, sr, bs);
        p.prepareToPlay (sr, bs);
        juce::String msg;
        if (! p.loadSyxFile (juce::File (juce::String (argv[2])), msg)) { std::printf ("%s\n", msg.toRawUTF8()); return 1; }
        juce::MidiFile mf;
        const juce::File midiFile { juce::String (argv[3]) };
        juce::FileInputStream in { midiFile };
        if (! mf.readFrom (in)) { std::printf ("MIDI?\n"); return 1; }
        mf.convertTimestampTicksToSeconds();
        juce::MidiMessageSequence seq;
        for (int t = 0; t < mf.getNumTracks(); ++t) seq.addSequence (*mf.getTrack (t), 0);
        seq.updateMatchedPairs();
        const double len = seq.getEndTime() + 1.0;
        const int total = (int) (len * sr);
        juce::AudioBuffer<float> out (1, total), buf (2, bs);
        int k = 0;
        for (int pos = 0; pos < total; pos += bs)
        {
            const int n = juce::jmin (bs, total - pos);
            juce::MidiBuffer midi;
            while (k < seq.getNumEvents() && (int) (seq.getEventPointer (k)->message.getTimeStamp() * sr) < pos + n)
            {
                const auto& m = seq.getEventPointer (k)->message;
                if (! m.isMetaEvent()) midi.addEvent (m, juce::jmax (0, (int) (m.getTimeStamp() * sr) - pos));
                ++k;
            }
            buf.setSize (2, n, false, false, true); buf.clear();
            p.processBlock (buf, midi);
            p.serviceMessages();
            out.copyFrom (0, pos, buf, 0, 0, n);
        }
        writeWav (juce::File (juce::String (argv[4])), out, sr);
        return 0;
    }
    if (argc > 2 && juce::String (argv[1]) == "--render")
    {
        double cpu = 0;
        const int cart = argc > 3 ? atoi (argv[3]) : 0;
        auto a = run (cart, 512, 48000, &cpu, nullptr, true);
        writeWav (juce::File (juce::String (argv[2])), a, 48000);
        return 0;
    }
    int fails = 0;
    {   // SysEx: un banco e un cambio de parámetro, como os manda un DX7
        B7Processor p;
        p.prepareToPlay (48000, 512);
        std::vector<b7::Patch> bank;
        for (int k = 0; k < 32; ++k) bank.push_back (b7::unpackVmem (b7::CARTRIDGES[1].vmem + k * 128));
        auto sx = b7::bankToSysex (bank);
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage (sx.data(), (int) sx.size()), 0);
        const uint8_t pc[] = { 0xF0, 0x43, 0x10, 0x01, 0x06, 0x05, 0xF7 };   // parámetro 134 (algoritmo) = 5 → algoritmo 6
        m.addEvent (juce::MidiMessage (pc, (int) sizeof (pc)), 10);
        juce::AudioBuffer<float> buf (2, 512);
        p.processBlock (buf, m);
        p.serviceMessages();
        const bool ok = p.getSource() == B7Processor::INTERNAL && b7::patchName (p.voiceAt (B7Processor::INTERNAL, 0)) == b7::patchName (bank[0])
                        && p.getEdit()[134] == 5 && p.isEdited();
        std::printf ("SysEx recibido (banco + parámetro): %s  [interna 1 = %s]\n", ok ? "OK" : "FALLO", b7::patchName (p.voiceAt (B7Processor::INTERNAL, 0)).c_str());
        fails += ok ? 0 : 1;
    }
    {   // estado
        B7Processor a, b;
        a.selectSource (1); a.selectVoice (7); a.setParam (3, 12); a.setOpEnabled (2, false);
        a.storeTo (9);
        a.selectSource (0); a.selectVoice (4); a.setParam (135, 1);
        a.uiScale = 0.8f;
        juce::MemoryBlock mb; a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        bool ok = b.getSource() == 0 && b.getVoiceNum() == 4 && b.getEdit() == a.getEdit() && std::abs (b.uiScale - 0.8f) < 1e-6f
               && b.voiceAt (B7Processor::INTERNAL, 9) == a.voiceAt (B7Processor::INTERNAL, 9) && b.isEdited();
        std::printf ("estado gardado/recuperado: %s (%d bytes)\n", ok ? "OK" : "FALLO", (int) mb.getSize());
        fails += ok ? 0 : 1;
    }
    {   // o aviso das versións novas compara os números, non o texto (0.10 vai despois de 0.9)
        const auto isNewer = escoita::versionNova;
        const bool ok = isNewer ("v0.1.1", "0.1.0") && isNewer ("0.10.0", "0.9.9") && isNewer ("1.0", "0.9.9")
                        && ! isNewer ("v0.1.0", "0.1.0") && ! isNewer ("0.0.9", "0.1.0") && ! isNewer ("", "0.1.0") && ! isNewer ("lixo", "0.1.0");
        std::printf ("versións novas: comparación dos números: %s\n", ok ? "OK" : "FALLO");
        fails += ok ? 0 : 1;
    }
    {   // tamaños de bloque
        double c1 = 0, c2 = 0;
        auto a = run (0, 512, 48000, &c1);
        auto b = run (0, 37, 48000, &c2);
        float d = 0;
        for (int i = 0; i < a.getNumSamples(); ++i) d = juce::jmax (d, std::abs (a.getSample (0, i) - b.getSample (0, i)));
        std::printf ("ROM1A: %.2f s de CPU para %.1f s de audio (%.0fx tempo real); diferenza entre bloques 512/37: %g\n",
                     c1, a.getNumSamples() / 48000.0, a.getNumSamples() / 48000.0 / c1, d);
        fails += d < 1.0e-5f ? 0 : 1;
        if (argc > 1) writeWav (juce::File (juce::String (argv[1])), a, 48000);
    }
    int numCarts = b7::NUM_CARTRIDGES;
    if (const char* dir = std::getenv ("B7_CARTUCHOS"))   // carpeta de cartuchos: recoñecemento dos bancos coñecidos
    {
        B7Processor p;
        p.setCartFolder (juce::File (juce::String (dir)));
        numCarts = p.numCarts();
        const bool ok = p.numFolderCarts() == 32 && p.sourceName (b7::NUM_CARTRIDGES) == "ROM1A" && p.sourceName (p.numCarts() - 1) == "VRC112B";
        std::printf ("carpeta de cartuchos: %d bancos, primeiro %s: %s\n", p.numFolderCarts(), p.sourceName (b7::NUM_CARTRIDGES).toRawUTF8(), ok ? "OK" : "FALLO");
        fails += ok ? 0 : 1;
    }
    {   // todos os cartuchos: nada de NaN, e os niveis
        float worst = 0, quiet = 1; int nan = 0; juce::String worstName, quietName;
        std::vector<float> all;
        B7Processor names;
        for (int c = 0; c < numCarts; ++c)
        {
            double cpu = 0; float pk[32];
            auto o = run (c, 512, 48000, &cpu, pk);
            for (int i = 0; i < o.getNumSamples(); ++i) if (! std::isfinite (o.getSample (0, i))) { ++nan; break; }
            for (int v = 0; v < 32; ++v)
            {
                const auto nm = names.sourceName (c) + " " + juce::String (b7::patchName (names.voiceAt (c, v)));
                all.push_back (pk[v]);
                if (pk[v] > worst) { worst = pk[v]; worstName = nm; }
                if (pk[v] < quiet) { quiet = pk[v]; quietName = nm; }
            }
        }
        std::printf ("%d sons: NaN=%d  pico máis alto %.3f (%s)  máis baixo %.4f (%s)\n", (int) all.size(), nan, worst, worstName.toRawUTF8(), quiet, quietName.toRawUTF8());
        std::sort (all.begin(), all.end());
        int lim = 0; for (float x : all) lim += x > 0.85f ? 1 : 0;
        std::printf ("picos: p10 %.3f  mediana %.3f  p90 %.3f  p99 %.3f; no limitador (>0.85): %d\n",
                     all[all.size() / 10], all[all.size() / 2], all[all.size() * 9 / 10], all[all.size() * 99 / 100], lim);
        fails += nan;
    }
    return fails;
}
