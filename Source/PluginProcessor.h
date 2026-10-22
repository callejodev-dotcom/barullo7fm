#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <escoita_nucleo/escoita_nucleo.h>
#include "Synth7.h"
#include "Cartuchos.h"

// Como no DX7: unha memoria interna de 32 sons que se pode gravar, os cartuchos e un búfer de edición
// co son que está a soar. O son que se toca é sempre o do búfer de edición.
// Cartuchos: os que van dentro (CLASICOS, BARULLO), os bancos .syx da carpeta de cartuchos do usuario
// (os coñecidos aparecen co seu nome) e o último .syx aberto.
class B7Processor : public juce::AudioProcessor, private juce::Timer
{
public:
    static constexpr int INTERNAL = -1;    // fontes de sons: −1 memoria interna, −2 o último .syx aberto,
    static constexpr int FILE_CART = -2;   // 0..numCarts()−1 os cartuchos (os de dentro e os da carpeta)

    B7Processor();
    ~B7Processor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Barullo 7FM"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 32; }
    int getCurrentProgram() override { return voiceNum; }
    void setCurrentProgram (int index) override { selectVoice (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::UndoManager undo;   // desfacer e refacer (declárase antes ca o apvts)
    juce::AudioProcessorValueTreeState apvts;
    // presets propios (.b7preset en Documentos/Barullo 7FM/Presets), A/B e desfacer, do núcleo
    // común: os parámetros e mais a memoria (fonte, son, memoria interna, búfer de edición e o .syx aberto)
    escoita::Estado estado { apvts, &undo, opcionsEstado() };
    juce::MidiKeyboardState keyboardState;

    // ---- memoria e edición (só desde o fío da interface) ----
    int getSource() const { return source; }
    int getVoiceNum() const { return voiceNum; }
    const b7::Patch& voiceAt (int src, int i) const;
    juce::String sourceName (int src) const;     // «INTERNA», «ROM1A», …
    juce::String sourceTitle (int src) const;    // descrición
    juce::String voiceDesc (int src, int i) const;   // só os sons propios teñen descrición
    bool hasFileCart() const { return fileCartLoaded; }
    int numCarts() const { return (int) carts.size(); }
    int numFolderCarts() const { return (int) carts.size() - b7::NUM_CARTRIDGES; }

    // carpeta de cartuchos (lémbrase para todos os proxectos)
    juce::File getCartFolder() const { return cartFolder; }
    void setCartFolder (const juce::File&);
    void rescanCartFolder();

    void selectSource (int src);                 // cambia de memoria e carga o son co mesmo número
    void selectVoice (int i);                    // carga un son no búfer de edición
    const b7::Patch& getEdit() const { return edit; }
    bool isEdited() const;
    void setParam (int index, int value);        // 0..154 como no formato VCED
    void setOpEnabled (int opNumber, bool on);   // 1..6
    bool isOpEnabled (int opNumber) const { return (edit[155] >> (6 - opNumber)) & 1; }
    void setVoiceName (const juce::String&);
    void revert();                               // volve ao son gardado
    void storeTo (int slot);                     // garda o búfer de edición na memoria interna
    void copySourceToInternal();                 // copia o cartucho enteiro na memoria interna
    void initVoice();

    // cambios desde a interface que se poden desfacer: «f» fai o cambio cos métodos de enriba
    void editar (const juce::String& nome, const std::function<void()>& f);          // só o búfer de edición
    void cambiarMemoria (const juce::String& nome, const std::function<void()>& f);  // fonte, son e bancos

    bool loadSyxFile (const juce::File&, juce::String& message);
    bool saveBankFile (const juce::File&, int src) const;
    bool saveVoiceFile (const juce::File&) const;

    // cambia cada vez que muda algo da memoria ou do búfer de edición (a interface mira isto)
    std::atomic<uint32_t> changes { 1 };
    // aviso curto para a pantalla (p. ex. «RECIBIDO BANCO»)
    juce::String notice; juce::uint32 noticeTime = 0;

    // ---- para a pantalla (escríbese desde o fío de audio) ----
    std::atomic<int> activeVoices { 0 };
    std::array<std::atomic<float>, 6> opLevel {};
    std::atomic<float> outLevel { 0 };
    std::atomic<uint32_t> midiActivity { 0 };

    float uiScale = 1.0f;
    void serviceMessages() { timerCallback(); }   // o mesmo que fai o temporizador (para as probas)

private:
    b7::Synth7 synth;

    // memoria (fío da interface)
    struct Cart
    {
        juce::String name, title;
        uint64_t hash = 0;
        int descKind = 0;            // 1 CLASICOS, 2 BARULLO (teñen descrición de cada son)
        std::array<b7::Patch, 32> voices {};
    };
    std::vector<Cart> carts;          // primeiro os de dentro, despois os da carpeta
    juce::File cartFolder;
    std::unique_ptr<juce::PropertiesFile> settings;
    int sourceForHash (uint64_t h) const;
    std::array<b7::Patch, 32> internal {}, fileCart {};
    juce::String fileCartName;
    bool fileCartLoaded = false;
    int source = 0, voiceNum = 0;
    b7::Patch edit {};

    // paso ao fío de audio: o fío da interface deixa aquí o estado e o de audio cólleo cando pode (sen bloquearse)
    struct Shared
    {
        std::array<b7::Patch, 32> bank {};
        b7::Patch patch {};
        bool bankDirty = false, patchReset = false, patchEdited = false;
    };
    juce::SpinLock sharedLock;
    Shared shared;
    std::array<b7::Patch, 32> audioBank {};
    void publish (bool bank, bool reset);
    void pullShared();

    // do fío de audio á interface
    std::atomic<int> pendingProgram { -1 };
    juce::AbstractFifo sysexFifo { 1 << 16 };
    std::vector<uint8_t> sysexBuf = std::vector<uint8_t> (1 << 16);
    void pushSysex (const uint8_t* data, int size);
    void timerCallback() override;
    void handleSysex (const std::vector<uint8_t>&);
    void setParamFromHost (const char* id, float value);

    // a memoria enteira nun ValueTree (o «Memoria» do estado do proxecto) e volta
    juce::ValueTree memoria() const;
    void setMemoria (const juce::ValueTree&);
    void setEditBuffer (const b7::Patch&);
    escoita::Estado::Opcions opcionsEstado();

    std::array<b7::Patch, 32>& bankOf (int src);
    const std::array<b7::Patch, 32>& bankOf (int src) const;

    std::atomic<float>* volumeParam = nullptr;
    std::atomic<float>* bendParam = nullptr;
    std::atomic<float>* wheelRangeParam = nullptr;
    std::atomic<float>* wheelPitchParam = nullptr;
    std::atomic<float>* wheelAmpParam = nullptr;
    std::atomic<float>* wheelEgParam = nullptr;

    double sr = 48000;
    double volS = 1;
    escoita::Teito teito;   // limitador de seguridade a −1 dBFS, do núcleo
    int lastBend = -1, lastWheel = -1;

    void handleMidi (const juce::MidiMessage&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (B7Processor)
};
