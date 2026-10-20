#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
    // Aspecto: panel dun sintetizador FM dixital de 1983. Corpo marrón escuro, rótulos en crema,
    // barras de grupo en verde vivo, botóns de membrana planos, pantalla LCD de 2×16 caracteres
    // e díxitos de LED vermellos co número do son.
    namespace col
    {
        const juce::Colour body      { 0xff2a221d };
        const juce::Colour bodyHi    { 0xff352b24 };
        const juce::Colour bodyLo    { 0xff1a1411 };
        const juce::Colour line      { 0xff4a3d33 };
        const juce::Colour membrane  { 0xff4b3a31 };
        const juce::Colour membraneHi{ 0xff5a463b };
        const juce::Colour print     { 0xffece6da };
        const juce::Colour printDim  { 0xffa89c8c };
        const juce::Colour green     { 0xff3dbb8e };   // o verde das barras de grupo
        const juce::Colour greenDim  { 0xff2a7a5e };
        const juce::Colour maroon    { 0xff8a3b33 };
        const juce::Colour led       { 0xffff3b2f };
        const juce::Colour ledOff    { 0xff2b0806 };
        const juce::Colour ledBg     { 0xff120302 };
        const juce::Colour lcdBg     { 0xffa3b18c };
        const juce::Colour lcdPix    { 0xff2d3a26 };
        const juce::Colour ivory     { 0xfff4f1e8 };
        const juce::Colour blackKey  { 0xff1c1714 };
    }

    juce::Font body (float size);
    juce::Font bold (float size);
    juce::Font cond (float size);
    void printed (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size, juce::Colour, juce::Justification, float tracking = 0.08f);
    juce::String noteName (int midi);          // «Do3» (Do central = Do3, como no DX7: C3)

    struct Look : juce::LookAndFeel_V4
    {
        Look();
        juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;
        juce::Label* createSliderTextBox (juce::Slider&) override;
        void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
        void drawLabel (juce::Graphics&, juce::Label&) override;
        void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                            int thumbStart, int thumbSize, bool over, bool down) override;
        int getDefaultScrollbarWidth() override { return 10; }
        void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
        void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    };

    // teclado cos nomes en galego (Do central = Do3)
    struct Keys : juce::MidiKeyboardComponent
    {
        using MidiKeyboardComponent::MidiKeyboardComponent;
        juce::String getWhiteNoteText (int note) override { return note % 12 == 0 ? "Do" + juce::String (note / 12 - 2) : juce::String(); }
    };

    // pantalla LCD de 2 liñas × 16 caracteres, con puntos de 5×7
    struct Lcd : juce::Component
    {
        void setLines (const juce::String& a, const juce::String& b);
        void paint (juce::Graphics&) override;
        juce::String l1, l2;
    };

    // un número que se arrastra (arriba/abaixo), coa roda do rato ou con dobre clic para escribilo
    struct NumBox : juce::Component, juce::SettableTooltipClient
    {
        NumBox (int mn, int mx, std::function<juce::String (int)> f = {});
        void setValue (int v);
        int getValue() const { return value; }
        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        juce::String text() const { return fmt ? fmt (value) : juce::String (value); }

        int value = 0, minV = 0, maxV = 99;
        std::function<juce::String (int)> fmt;
        std::function<void (int)> onChange;
        std::function<void (NumBox&)> onTouch;     // para amosar o parámetro na LCD
        std::function<void()> onGestureStart, onGestureEnd;
        juce::String lcdName;                      // como se chama na LCD (máx. 16)
        bool small = false, dim = false;
    private:
        void change (int v);
        int startValue = 0; float acc = 0; bool dragged = false, hover = false, active = false;
        std::unique_ptr<juce::TextEditor> editor;
    };

    void drawAlgorithm (juce::Graphics&, juce::Rectangle<float>, int alg, const float* levels, const uint8_t* opOn, bool compact, juce::Colour ink);
    void drawEnvelope (juce::Graphics&, juce::Rectangle<float>, const uint8_t* rates, const uint8_t* levels, bool pitch, juce::Colour ink);

    // os 32 algoritmos en dúas filas, como na táboa impresa no panel; prémese un para escollelo
    struct AlgoStrip : juce::Component, juce::SettableTooltipClient
    {
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        int indexAt (juce::Point<int>) const;
        int selected = 0, hover = -1;
        std::function<void (int)> onPick;
    };

    struct MemButton : juce::Button
    {
        MemButton (int i) : juce::Button (juce::String (i + 1)), index (i) {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
        int index; juce::String name; bool selected = false, storeMode = false;
    };

    class MainView;

    class SoundsPage : public juce::Component, private juce::ListBoxModel
    {
    public:
        SoundsPage (B7Processor&, MainView&);
        void refresh();
        void tick();
        void paint (juce::Graphics&) override;
        void resized() override;
    private:
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        juce::String getTooltipForRow (int row) override;
        static constexpr int FOLDER_ROW = -3;   // a última fila: escoller a carpeta de cartuchos
        int rowToSource (int row) const
        {
            if (row == 0) return B7Processor::INTERNAL;
            if (row <= proc.numCarts()) return row - 1;
            return row == proc.numCarts() + 1 ? B7Processor::FILE_CART : FOLDER_ROW;
        }
        void chooseFolder();
        int shownCarts = -1;

        B7Processor& proc;
        MainView& main;
        juce::ListBox list;
        juce::OwnedArray<MemButton> mems;
        juce::TextButton openBtn, folderBtn, saveBankBtn, copyBtn, storeBtn;
        juce::Rectangle<int> algoArea, descArea, memArea;
        bool storeMode = false;
        float lv[6] {};
        std::unique_ptr<juce::FileChooser> chooser;
    };

    class EditPage : public juce::Component
    {
    public:
        EditPage (B7Processor&, MainView&);
        void refresh();
        void tick();
        void paint (juce::Graphics&) override;
        void resized() override;
    private:
        NumBox* add (int index, int mn, int mx, const juce::String& lcd, const juce::String& tip, std::function<juce::String (int)> fmt = {});
        B7Processor& proc;
        MainView& main;
        AlgoStrip strip;
        juce::OwnedArray<NumBox> boxes;
        std::vector<std::pair<NumBox*, int>> bound;          // caixa ↔ índice VCED
        std::array<NumBox*, 6> opSwitch {};
        std::array<juce::Rectangle<int>, 6> rowArea, egArea, freqArea, meterArea;
        juce::Rectangle<int> pegArea, globalsArea, tableHead;
        std::array<juce::Rectangle<int>, 5> groupArea;
        juce::TextEditor nameEd;
        juce::TextButton initBtn, revertBtn, saveVoiceBtn, openVoiceBtn;
        std::unique_ptr<juce::FileChooser> chooser;
        float lv[6] {};
    };

    class MainView : public juce::Component, private juce::Timer
    {
    public:
        static constexpr int W = 1040, H = 840;
        explicit MainView (B7Processor&);
        ~MainView() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void showPage (int p);
        void showParam (const juce::String& a, const juce::String& b);   // a LCD amosa isto un momento
        void showMessage (const juce::String& m);                         // aviso de texto baixo a LCD
        juce::MidiKeyboardComponent& keys() { return keyboard; }
        void timerCallback() override;

    private:
        void paintPanel (juce::Graphics&);
        void updateLcd();

        B7Processor& proc;
        Look look;
        juce::TooltipWindow tooltips { this, 500 };
        Lcd lcd;
        juce::TextButton tabPlay, tabEdit;
        juce::HyperlinkButton escoita;
        juce::Slider volume { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
        NumBox bendBox { 0, 12 }, wheelBox { 0, 99 }, wheelP { 0, 1 }, wheelA { 0, 1 }, wheelE { 0, 1 };
        std::unique_ptr<juce::ParameterAttachment> bendAtt, wheelAtt, wpAtt, waAtt, weAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volAtt;
        Keys keyboard;
        SoundsPage sounds;
        EditPage editor;
        juce::Rectangle<int> ledArea, lcdFrame, voicesArea, funcArea, volArea, keysFrame, editLed, msgArea;
        int page = 0;
        uint32_t seenChanges = 0;
        juce::String paramA, paramB, message; juce::uint32 paramTime = 0, messageTime = 0;
        int shownVoices = -1;
        uint32_t lastMidi = 0; float midiFlash = 0;
    };
}

class B7Editor : public juce::AudioProcessorEditor
{
public:
    explicit B7Editor (B7Processor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (ui::col::body); }
    ui::MainView& view() { return main; }
private:
    B7Processor& proc;
    ui::MainView main;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (B7Editor)
};
