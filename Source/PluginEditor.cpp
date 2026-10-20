#include "PluginEditor.h"
#include "BinaryData.h"
#include "Algo.h"
#include "Lcd5x7.h"

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
}

namespace ui
{
// ================================================================ tipografía
static juce::Typeface::Ptr tf (int which)
{
    static juce::Typeface::Ptr t[3] = {
        juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedMedium_ttf, BinaryData::BarlowSemiCondensedMedium_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedSemiBold_ttf, BinaryData::BarlowSemiCondensedSemiBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize) };
    return t[which];
}
juce::Font body (float s) { return juce::Font (juce::FontOptions (tf (0)).withHeight (s)); }
juce::Font bold (float s) { return juce::Font (juce::FontOptions (tf (1)).withHeight (s)); }
juce::Font cond (float s) { return juce::Font (juce::FontOptions (tf (2)).withHeight (s)); }

static juce::String upper (const juce::String& s)
{
    return s.toUpperCase().replace (U ("á"), U ("Á")).replace (U ("é"), U ("É")).replace (U ("í"), U ("Í"))
            .replace (U ("ó"), U ("Ó")).replace (U ("ú"), U ("Ú")).replace (U ("ñ"), U ("Ñ"));
}

// a LCD só ten ASCII: sen tiles
static juce::String lcdText (const juce::String& s)
{
    juce::String o;
    for (auto c : upper (s))
    {
        switch (c)
        {
            case 0xC1: c = 'A'; break; case 0xC9: c = 'E'; break; case 0xCD: c = 'I'; break;
            case 0xD3: c = 'O'; break; case 0xDA: c = 'U'; break; case 0xD1: c = 'N'; break;
            case 0xDC: c = 'U'; break; case 0x2212: c = '-'; break;
            default: break;
        }
        o += (c >= 32 && c < 128) ? juce::String::charToString (c) : juce::String ("?");
    }
    return o;
}

void printed (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float size, juce::Colour c, juce::Justification j, float tracking)
{
    g.setColour (c);
    g.setFont (juce::Font (juce::FontOptions (tf (1)).withHeight (size).withKerningFactor (tracking)));
    g.drawText (t, r, j, false);
}

juce::String noteName (int midi)
{
    static const char* n[12] = { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
    return juce::String (n[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 2);
}

// barra verde de grupo co nome en crema, como no panel
static void groupBar (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour c = col::green)
{
    g.setColour (c);
    g.fillRect (r.withHeight (3.0f).withY (r.getCentreY() - 1.5f));
    const auto f = juce::Font (juce::FontOptions (tf (1)).withHeight (12.0f).withKerningFactor (0.14f));
    const float w = juce::GlyphArrangement::getStringWidth (f, t) + 14.0f;
    auto lab = r.withSizeKeepingCentre (juce::jmin (w, r.getWidth()), r.getHeight());
    g.setColour (col::body); g.fillRect (lab);
    g.setColour (c.brighter (0.25f)); g.setFont (f);
    g.drawText (t, lab, juce::Justification::centred, false);
}

// ================================================================ aspecto
Look::Look()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::body);
    setColour (juce::TextButton::buttonColourId, col::membrane);
    setColour (juce::TextButton::textColourOffId, col::print);
    setColour (juce::TextButton::textColourOnId, col::print);
    setColour (juce::Label::textColourId, col::printDim);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::backgroundColourId, col::bodyLo);
    setColour (juce::TextEditor::textColourId, col::print);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, col::green);
    setColour (juce::TextEditor::highlightColourId, col::green.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, col::print);
    setColour (juce::ListBox::backgroundColourId, col::bodyLo);
    setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, col::membraneHi);
    setColour (juce::PopupMenu::backgroundColourId, col::bodyHi);
    setColour (juce::PopupMenu::textColourId, col::print);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::greenDim);
    setColour (juce::AlertWindow::backgroundColourId, col::bodyHi);
    setColour (juce::AlertWindow::textColourId, col::print);
}

juce::Typeface::Ptr Look::getTypefaceForFont (const juce::Font& f) { return tf (f.isBold() ? 1 : 0); }

// botón de membrana: case plano, cun marco fino; ao premer escurece. Os «tint» = "green" levan a cara verde.
void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto tint = b.getProperties()["tint"].toString();
    const bool on = b.getToggleState();
    juce::Colour c = tint == "green" ? col::greenDim : tint == "maroon" ? col::maroon : col::membrane;
    if (on) c = tint == "maroon" ? col::maroon.brighter (0.25f) : col::green.darker (0.35f);
    if (down) c = c.darker (0.25f); else if (over) c = c.brighter (0.07f);
    if (! b.isEnabled()) c = c.withMultipliedSaturation (0.4f).darker (0.3f);
    g.setColour (col::bodyLo); g.fillRoundedRectangle (r.translated (0, 1.5f), 2.5f);
    g.setColour (c); g.fillRoundedRectangle (r, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 1.0f);
    if (on) { g.setColour (col::green); g.fillRect (r.removeFromBottom (3.0f).reduced (4.0f, 0)); }
}

void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat();
    printed (g, upper (b.getButtonText()), r.reduced (6.0f, 0), 13.0f,
             col::print.withAlpha (b.isEnabled() ? 1.0f : 0.4f), juce::Justification::centred, 0.07f);
}

// deslizador como os do panel: ranura negra e capuchón escuro cunha raia crema
void Look::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cx = (float) x + (float) w * 0.5f;
    auto slot = juce::Rectangle<float> (cx - 3.5f, (float) y, 7.0f, (float) h);
    g.setColour (juce::Colour (0xff0a0807)); g.fillRoundedRectangle (slot, 3.0f);
    g.setColour (col::printDim.withAlpha (0.6f));
    for (int t = 0; t <= 10; ++t)
    {
        const float ty = (float) y + (float) t * (float) (h - 1) / 10.0f;
        const float len = (t % 5 == 0) ? 9.0f : 5.0f;
        g.fillRect (cx + 12.0f, ty, len, 1.0f);
        g.fillRect (cx - 12.0f - len, ty, len, 1.0f);
    }
    auto cap = juce::Rectangle<float> (cx - 11.0f, pos - 12.0f, 22.0f, 24.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (cap.translated (0, 2.0f), 2.5f);
    g.setColour (juce::Colour (0xff221b17)); g.fillRoundedRectangle (cap, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    for (int i = 0; i < 4; ++i) g.fillRect (cap.getX() + 3.0f, cap.getY() + 3.0f + (float) i * 2.4f, cap.getWidth() - 6.0f, 1.0f);
    g.setColour (col::print); g.fillRect (cap.getX() + 2.0f, pos - 1.0f, cap.getWidth() - 4.0f, 2.0f);
}

juce::Label* Look::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (bold (13.0f));
    l->setColour (juce::Label::textColourId, col::print);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

void Look::drawLabel (juce::Graphics& g, juce::Label& l)
{
    if (l.isBeingEdited()) return;
    g.setColour (l.findColour (juce::Label::textColourId));
    g.setFont (getLabelFont (l));
    g.drawFittedText (l.getText(), l.getLocalBounds().reduced (2, 0), l.getJustificationType(), 2, 0.9f);
}

void Look::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    auto r = juce::Rectangle<int> (w, h).toFloat();
    g.setColour (col::bodyHi); g.fillRoundedRectangle (r, 4.0f);
    g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    juce::AttributedString s; s.setJustification (juce::Justification::topLeft);
    s.append (text, body (14.0f), col::print);
    juce::TextLayout tl; tl.createLayoutWithBalancedLineLengths (s, 300.0f);
    tl.draw (g, r.reduced (10.0f, 7.0f));
}

juce::Rectangle<int> Look::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::AttributedString s; s.append (text, body (14.0f));
    juce::TextLayout tl; tl.createLayoutWithBalancedLineLengths (s, 300.0f);
    const int w = (int) tl.getWidth() + 22, h = (int) tl.getHeight() + 16;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h).constrainedWithin (parent);
}

void Look::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down)
{
    auto t = vertical ? juce::Rectangle<int> (x + 2, thumbStart, w - 4, thumbSize) : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, h - 4);
    g.setColour ((over || down) ? col::green.withAlpha (0.7f) : col::membraneHi);
    g.fillRoundedRectangle (t.toFloat(), 3.0f);
}

void Look::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (col::bodyLo); g.fillRoundedRectangle (juce::Rectangle<int> (w, h).toFloat(), 2.5f);
}

void Look::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? col::green : col::line);
    g.drawRoundedRectangle (juce::Rectangle<int> (w, h).toFloat().reduced (0.5f), 2.5f, 1.0f);
}

// ================================================================ LCD
void Lcd::setLines (const juce::String& a, const juce::String& b)
{
    if (a == l1 && b == l2) return;
    l1 = a; l2 = b; repaint();
}

void Lcd::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::lcdBg); g.fillRoundedRectangle (r, 3.0f);
    // brillo e sombra interior do cristal
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), 0, r.getY(), juce::Colours::black.withAlpha (0.10f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);

    const int cols = 16 * 6 - 1, rows = 7 * 2 + 3;
    auto in = r.reduced (10.0f, 9.0f);
    const float p = juce::jmin (in.getWidth() / (float) cols, in.getHeight() / (float) rows);
    const float x0 = in.getCentreX() - p * (float) cols * 0.5f, y0 = in.getCentreY() - p * (float) rows * 0.5f;
    const float dot = p * 0.86f;
    auto drawLine = [&] (const juce::String& s, int line)
    {
        for (int c = 0; c < 16; ++c)
        {
            const int ch = c < s.length() ? (int) s[c] : ' ';
            const auto* gl = b7::LCD5X7[(ch >= 32 && ch < 128 ? ch : '?') - 32];
            for (int x = 0; x < 5; ++x)
                for (int y = 0; y < 7; ++y)
                {
                    const bool on = (gl[x] >> y) & 1;
                    const float px = x0 + (float) (c * 6 + x) * p, py = y0 + (float) (line * 10 + y) * p;
                    g.setColour (on ? col::lcdPix : col::lcdPix.withAlpha (0.06f));
                    g.fillRect (px, py, dot, dot);
                }
        }
    };
    drawLine (l1, 0);
    drawLine (l2, 1);
}

// ================================================================ caixa numérica
NumBox::NumBox (int mn, int mx, std::function<juce::String (int)> f) : minV (mn), maxV (mx), fmt (std::move (f))
{
    setRepaintsOnMouseActivity (false);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void NumBox::setValue (int v)
{
    v = juce::jlimit (minV, maxV, v);
    if (v != value) { value = v; repaint(); }
}

void NumBox::change (int v)
{
    v = juce::jlimit (minV, maxV, v);
    if (v == value) return;
    value = v; repaint();
    if (onChange) onChange (v);
    if (onTouch) onTouch (*this);
}

void NumBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f, 1.0f);
    const bool toggle = maxV - minV == 1;
    if (toggle && value == maxV)
    {
        g.setColour (col::green.darker (0.45f)); g.fillRoundedRectangle (r, 2.5f);
        g.setColour (col::green); g.fillRect (r.withTop (r.getBottom() - 2.5f).reduced (3.0f, 0));
    }
    else
    {
        g.setColour (col::bodyLo); g.fillRoundedRectangle (r, 2.5f);
    }
    g.setColour ((hover || active) ? col::green : col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 1.0f);
    juce::Colour ink = active ? col::green.brighter (0.4f) : col::print;
    if (dim || ! isEnabled()) ink = ink.withAlpha (0.4f);
    g.setColour (ink);
    g.setFont (cond (small ? 13.5f : 15.5f));
    g.drawFittedText (text(), getLocalBounds().reduced (2, 0), juce::Justification::centred, 1, 0.8f);
}

void NumBox::mouseEnter (const juce::MouseEvent&) { hover = true; repaint(); if (onTouch) onTouch (*this); }
void NumBox::mouseExit (const juce::MouseEvent&) { hover = false; repaint(); }

void NumBox::mouseDown (const juce::MouseEvent&)
{
    startValue = value; acc = 0; dragged = false; active = true; repaint();
    if (onGestureStart) onGestureStart();
    if (onTouch) onTouch (*this);
}

void NumBox::mouseDrag (const juce::MouseEvent& e)
{
    const int range = maxV - minV;
    if (range <= 1) return;
    const float pxPerStep = juce::jlimit (2.0f, 14.0f, 220.0f / (float) range) * (e.mods.isShiftDown() ? 4.0f : 1.0f);
    const float dy = (float) -e.getDistanceFromDragStartY();
    if (std::abs (dy) > 2.0f) dragged = true;
    change (startValue + (int) std::round (dy / pxPerStep));
}

void NumBox::mouseUp (const juce::MouseEvent& e)
{
    active = false;
    if (! dragged && maxV - minV == 1 && ! e.mods.isPopupMenu() && e.getNumberOfClicks() == 1) change (value == maxV ? minV : maxV);
    if (onGestureEnd) onGestureEnd();
    repaint();
}

void NumBox::mouseDoubleClick (const juce::MouseEvent&)
{
    if (maxV - minV <= 1) return;
    editor = std::make_unique<juce::TextEditor>();
    editor->setFont (cond (15.0f));
    editor->setJustification (juce::Justification::centred);
    editor->setText (juce::String (value), false);
    editor->selectAll();
    editor->setBounds (getLocalBounds());
    addAndMakeVisible (*editor);
    editor->grabKeyboardFocus();
    const juce::Component::SafePointer<NumBox> safe (this);
    auto finish = [this, safe] (bool apply)
    {
        if (! editor) return;
        const auto t = editor->getText().trim();
        if (apply && t.containsAnyOf ("0123456789"))
        {
            if (onGestureStart) onGestureStart();
            change (t.getIntValue());
            if (onGestureEnd) onGestureEnd();
        }
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->editor.reset(); });
    };
    editor->onReturnKey = [finish] { finish (true); };
    editor->onEscapeKey = [finish] { finish (false); };
    editor->onFocusLost = [finish] { finish (true); };
}

void NumBox::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float d = std::abs (w.deltaY) > 1.0e-6f ? w.deltaY : -w.deltaX;
    if (std::abs (d) < 1.0e-4f) return;
    acc += d * 10.0f;
    int steps = (int) acc;
    if (steps == 0 && ! w.isSmooth) steps = d > 0 ? 1 : -1;
    if (steps == 0) return;
    acc = 0;
    if (onGestureStart) onGestureStart();
    change (value + juce::jlimit (-4, 4, steps));
    if (onGestureEnd) onGestureEnd();
}

// ================================================================ debuxo dos algoritmos
void drawAlgorithm (juce::Graphics& g, juce::Rectangle<float> area, int alg, const float* levels, const uint8_t* opOn, bool compact, juce::Colour ink)
{
    const auto G = b7::algoGraph (alg);
    float maxX = 0, maxY = 0;
    for (int o = 1; o <= 6; ++o) { maxX = juce::jmax (maxX, G.x[(size_t) o]); maxY = juce::jmax (maxY, G.y[(size_t) o]); }
    const float bus = compact ? 0.45f : 0.6f;
    const float cw = area.getWidth() / (maxX + 1.6f), ch = area.getHeight() / (maxY + 1.0f + bus);
    const float cell = juce::jmin (cw, ch * 1.15f);
    const float s = juce::jmin (cell * 0.62f, ch * 0.62f);
    const float gw = cell * (maxX + 1.0f);
    const float left = area.getCentreX() - gw * 0.5f;
    const float busY = area.getBottom() - ch * bus * 0.55f;
    auto centre = [&] (int o) { return juce::Point<float> (left + (G.x[(size_t) o] + 0.5f) * cell, busY - ch * bus * 0.45f - (G.y[(size_t) o] + 0.5f) * ch); };
    const float lw = compact ? 1.0f : 1.6f;
    const float gap = s * 0.5f;

    g.setColour (ink.withAlpha (0.85f));
    // conexións modulador → destino
    for (auto& e : G.edges)
    {
        const auto a = centre (e.first), b = centre (e.second);
        juce::Path p;
        p.startNewSubPath (a.x, a.y + gap);
        const float midY = b.y - gap - (b.y - gap - (a.y + gap)) * 0.5f;
        if (std::abs (a.x - b.x) < 0.5f) p.lineTo (b.x, b.y - gap);
        else { p.lineTo (a.x, midY); p.lineTo (b.x, midY); p.lineTo (b.x, b.y - gap); }
        g.strokePath (p, juce::PathStrokeType (lw));
    }
    // portadores → saída
    float bx0 = 1e9f, bx1 = -1e9f;
    for (int o = 1; o <= 6; ++o)
        if (G.carrier[(size_t) o])
        {
            const auto c = centre (o);
            g.drawLine (c.x, c.y + gap, c.x, busY, lw);
            bx0 = juce::jmin (bx0, c.x); bx1 = juce::jmax (bx1, c.x);
        }
    if (bx1 >= bx0) g.drawLine (bx0 - (compact ? 0.0f : 4.0f), busY, bx1 + (compact ? 0.0f : 4.0f), busY, lw * 1.4f);
    // realimentación
    if (G.fbFrom > 0 && G.fbTo > 0)
    {
        const auto a = centre (G.fbFrom), b = centre (G.fbTo);
        const float xr = juce::jmax (a.x, b.x) + s * 0.5f + cell * 0.18f;
        const float yb = a.y + gap + ch * 0.12f, yt = b.y - gap - ch * 0.14f;
        juce::Path p;
        p.startNewSubPath (a.x, a.y + gap); p.lineTo (a.x, yb); p.lineTo (xr, yb); p.lineTo (xr, yt); p.lineTo (b.x, yt); p.lineTo (b.x, b.y - gap);
        g.setColour (col::green.withAlpha (0.9f));
        g.strokePath (p, juce::PathStrokeType (lw));
    }
    // caixas
    for (int o = 1; o <= 6; ++o)
    {
        const auto c = centre (o);
        auto box = juce::Rectangle<float> (s, s).withCentre (c);
        const bool carrier = G.carrier[(size_t) o];
        const bool on = opOn == nullptr || ((*opOn >> (6 - o)) & 1);
        const float lv = levels ? juce::jlimit (0.0f, 1.0f, levels[o - 1]) : 0.0f;
        g.setColour (carrier ? col::green.withAlpha (on ? 1.0f : 0.3f) : col::body);
        g.fillRect (box);
        if (lv > 0.001f)
        {
            g.setColour ((carrier ? juce::Colours::white : col::green).withAlpha (0.25f + 0.6f * lv));
            g.fillRect (box.withTop (box.getBottom() - box.getHeight() * lv));
        }
        g.setColour (on ? ink : ink.withAlpha (0.35f));
        g.drawRect (box, lw);
        g.setColour (carrier ? col::bodyLo : (on ? ink : ink.withAlpha (0.35f)));
        g.setFont (cond (s * (compact ? 0.78f : 0.6f)));
        g.drawText (juce::String (o), box, juce::Justification::centred, false);
    }
}

// envolvente: catro taxas e catro niveis (a de ton ten o centro en 50)
void drawEnvelope (juce::Graphics& g, juce::Rectangle<float> r, const uint8_t* rates, const uint8_t* levels, bool pitch, juce::Colour ink)
{
    auto seg = [] (int rate) { return 0.12f + 0.88f * std::pow ((99.0f - (float) rate) / 99.0f, 1.6f); };
    const float sus = 0.55f;
    const float total = seg (rates[0]) + seg (rates[1]) + seg (rates[2]) + sus + seg (rates[3]);
    auto yOf = [&] (int l) { return r.getBottom() - r.getHeight() * ((float) l / 99.0f); };
    juce::Path p;
    float x = r.getX();
    const float k = r.getWidth() / total;
    p.startNewSubPath (x, yOf (levels[3]));
    for (int i = 0; i < 3; ++i) { x += seg (rates[i]) * k; p.lineTo (x, yOf (levels[i])); }
    x += sus * k; p.lineTo (x, yOf (levels[2]));
    const float keyOff = x;
    x += seg (rates[3]) * k; p.lineTo (x, yOf (levels[3]));
    g.setColour (col::bodyLo); g.fillRect (r);
    if (pitch) { g.setColour (col::line); g.drawHorizontalLine ((int) yOf (50), r.getX(), r.getRight()); }
    g.setColour (col::line); g.drawVerticalLine ((int) keyOff, r.getY(), r.getBottom());
    g.setColour (ink); g.strokePath (p, juce::PathStrokeType (1.3f));
}

// ================================================================ táboa de algoritmos
int AlgoStrip::indexAt (juce::Point<int> p) const
{
    const float cw = (float) getWidth() / 16.0f, chh = (float) getHeight() / 2.0f;
    const int c = juce::jlimit (0, 15, (int) ((float) p.x / cw)), r = juce::jlimit (0, 1, (int) ((float) p.y / chh));
    return r * 16 + c;
}

void AlgoStrip::paint (juce::Graphics& g)
{
    const float cw = (float) getWidth() / 16.0f, chh = (float) getHeight() / 2.0f;
    for (int i = 0; i < 32; ++i)
    {
        auto cell = juce::Rectangle<float> ((float) (i % 16) * cw, (float) (i / 16) * chh, cw, chh).reduced (1.5f);
        const bool sel = i == selected;
        g.setColour (sel ? col::green.withAlpha (0.18f) : (i == hover ? col::bodyHi : col::body));
        g.fillRect (cell);
        if (sel) { g.setColour (col::green); g.drawRect (cell, 1.2f); }
        g.setColour (sel ? col::green.brighter (0.3f) : col::printDim);
        g.setFont (cond (12.5f));
        g.drawText (juce::String (i + 1), cell.reduced (3.0f, 1.0f), juce::Justification::topLeft, false);
        drawAlgorithm (g, cell.reduced (2.0f, 3.0f).withTrimmedLeft (8.0f), i, nullptr, nullptr, true, sel ? col::print : col::printDim);
    }
}

void AlgoStrip::mouseDown (const juce::MouseEvent& e) { if (onPick) onPick (indexAt (e.getPosition())); }
void AlgoStrip::mouseMove (const juce::MouseEvent& e)
{
    const int h = indexAt (e.getPosition());
    if (h != hover) { hover = h; repaint(); setTooltip (U ("Algoritmo ") + juce::String (h + 1)); }
}

// ================================================================ botón de memoria
void MemButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    juce::Colour c = selected ? col::green.darker (0.5f) : col::membrane;
    if (storeMode) c = over ? col::maroon.brighter (0.2f) : col::membrane;
    if (down) c = c.darker (0.2f); else if (over && ! storeMode) c = c.brighter (0.07f);
    g.setColour (col::bodyLo); g.fillRoundedRectangle (r.translated (0, 1.5f), 2.5f);
    g.setColour (c); g.fillRoundedRectangle (r, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 1.0f);
    if (selected) { g.setColour (col::green); g.fillRect (r.withTop (r.getBottom() - 3.0f).reduced (4.0f, 0)); }
    auto in = r.reduced (7.0f, 5.0f);
    g.setColour (selected ? col::green.brighter (0.5f) : col::green);
    g.setFont (cond (15.0f));
    g.drawText (juce::String (index + 1), in.removeFromTop (16.0f), juce::Justification::topLeft, false);
    g.setColour (col::print);
    g.setFont (cond (15.5f));
    g.drawFittedText (name, in.toNearestInt(), juce::Justification::bottomLeft, 1, 0.75f);
}

// ================================================================ páxina de sons
SoundsPage::SoundsPage (B7Processor& p, MainView& m) : proc (p), main (m)
{
    list.setModel (this);
    list.setRowHeight (36);
    list.setColour (juce::ListBox::backgroundColourId, col::bodyLo);
    addAndMakeVisible (list);

    for (int i = 0; i < 32; ++i)
    {
        auto* b = mems.add (new MemButton (i));
        b->onClick = [this, i]
        {
            if (storeMode)
            {
                proc.storeTo (i);
                storeMode = false; storeBtn.setToggleState (false, juce::dontSendNotification);
                main.showMessage (U ("Gardado na memoria interna, posición ") + juce::String (i + 1) + ".");
            }
            else proc.selectVoice (i);
            refresh();
            main.keys().grabKeyboardFocus();
        };
        addAndMakeVisible (b);
    }

    openBtn.setButtonText (U ("Abrir .syx"));
    openBtn.setTooltip (U ("Abre un ficheiro .syx: un banco de 32 sons queda como cartucho (FICHEIRO); un só son vai ao búfer de edición."));
    openBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Abrir sons de DX7"), juce::File(), "*.syx;*.SYX");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            juce::String msg;
            proc.loadSyxFile (f, msg);
            main.showMessage (msg);
            refresh();
        });
    };
    saveBankBtn.setButtonText (U ("Gardar banco"));
    saveBankBtn.setTooltip (U ("Garda os 32 sons da memoria que estás vendo nun .syx (o mesmo formato que manda o DX7)."));
    saveBankBtn.onClick = [this]
    {
        const int src = proc.getSource();
        chooser = std::make_unique<juce::FileChooser> (U ("Gardar banco"), juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                         .getChildFile (proc.sourceName (src).trim() + ".syx"), "*.syx");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this, src] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (! f.hasFileExtension ("syx")) f = f.withFileExtension ("syx");
            main.showMessage (proc.saveBankFile (f, src) ? U ("Banco gardado: ") + f.getFileName() : U ("Non se puido gardar o ficheiro."));
        });
    };
    copyBtn.setButtonText (U ("Copiar na interna"));
    copyBtn.setTooltip (U ("Copia os 32 sons deste cartucho na memoria interna (substitúe o que houbese)."));
    copyBtn.onClick = [this]
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, U ("Copiar na memoria interna"),
            U ("Os 32 sons de ") + proc.sourceName (proc.getSource()) + U (" substituirán os da memoria interna."),
            U ("Copiar"), U ("Cancelar"), this, juce::ModalCallbackFunction::create ([this] (int r)
            {
                if (r == 0) return;
                proc.copySourceToInternal();
                main.showMessage (U ("Copiado na memoria interna."));
                refresh();
            }));
    };
    storeBtn.setButtonText (U ("Gardar na interna"));
    storeBtn.getProperties().set ("tint", "maroon");
    storeBtn.setClickingTogglesState (true);
    storeBtn.setTooltip (U ("Garda o son actual (coas túas edicións) na memoria interna: preme este botón e despois a posición 1–32."));
    storeBtn.onClick = [this]
    {
        storeMode = storeBtn.getToggleState();
        main.showMessage (storeMode ? U ("Escolle a posición da memoria interna onde gardar o son.") : juce::String());
        refresh();
    };
    folderBtn.setButtonText (U ("Carpeta"));
    folderBtn.setTooltip (U ("Carpeta de cartuchos: todos os bancos .syx que haxa nela (e nas súas subcarpetas) aparecen nesta lista. "
                             "Barullo recoñece os bancos de fábrica (ROM1A–ROM4B, VRC101–VRC112) e amósaos co seu nome. "
                             "Se tes un DX7, podes volcar a súa memoria: FUNCTION, botón 8 ata «MIDI TRANSMIT ?» e YES."));
    folderBtn.onClick = [this] { chooseFolder(); };
    for (auto* b : { &openBtn, &folderBtn, &saveBankBtn, &copyBtn, &storeBtn }) { b->setWantsKeyboardFocus (false); addAndMakeVisible (b); }
}

void SoundsPage::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> (U ("Carpeta de cartuchos"), proc.getCartFolder().isDirectory() ? proc.getCartFolder()
                                                     : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.isDirectory()) return;
        proc.setCartFolder (f);
        const int n = proc.numFolderCarts();
        main.showMessage (n > 0 ? juce::String (n) + U (" bancos atopados en ") + f.getFileName()
                                : U ("Non hai bancos .syx de 32 sons en ") + f.getFileName());
        refresh();
    });
}

int SoundsPage::getNumRows() { return proc.numCarts() + 3; }

void SoundsPage::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    const int src = rowToSource (row);
    if (src == FOLDER_ROW)
    {
        auto r = juce::Rectangle<int> (w, h).toFloat().reduced (12.0f, 3.0f);
        g.setColour (col::green.brighter (0.2f)); g.setFont (cond (15.0f));
        g.drawText (proc.numFolderCarts() > 0 ? U ("+ CAMBIAR A CARPETA…") : U ("+ OS TEUS BANCOS .SYX"), r.removeFromTop (17.0f), juce::Justification::centredLeft, false);
        g.setColour (col::printDim); g.setFont (body (12.0f));
        g.drawText (U ("Escolle unha carpeta: aparecen aquí, os de fábrica co seu nome"), r, juce::Justification::centredLeft, true);
        return;
    }
    const bool sel = src == proc.getSource();
    const bool avail = src != B7Processor::FILE_CART || proc.hasFileCart();
    auto r = juce::Rectangle<int> (w, h).toFloat();
    if (sel) { g.setColour (col::green.withAlpha (0.16f)); g.fillRect (r); g.setColour (col::green); g.fillRect (r.withWidth (4.0f)); }
    g.setColour (col::line.withAlpha (0.5f)); g.drawHorizontalLine (h - 1, 0, (float) w);
    auto in = r.reduced (12.0f, 3.0f);
    g.setColour (avail ? (sel ? col::green.brighter (0.4f) : col::print) : col::printDim.withAlpha (0.5f));
    g.setFont (cond (16.5f));
    g.drawText (proc.sourceName (src), in.removeFromTop (17.0f), juce::Justification::centredLeft, false);
    g.setColour (col::printDim.withAlpha (avail ? 1.0f : 0.5f));
    g.setFont (body (12.5f));
    g.drawText (proc.sourceTitle (src), in, juce::Justification::centredLeft, true);
}

void SoundsPage::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    const int src = rowToSource (row);
    if (src == FOLDER_ROW) { chooseFolder(); return; }
    if (src == B7Processor::FILE_CART && ! proc.hasFileCart()) { openBtn.triggerClick(); return; }
    if (storeMode) { storeMode = false; storeBtn.setToggleState (false, juce::dontSendNotification); main.showMessage ({}); }
    proc.selectSource (src);
    refresh();
    main.keys().grabKeyboardFocus();
}

juce::String SoundsPage::getTooltipForRow (int row)
{
    const int src = rowToSource (row);
    if (src == B7Processor::INTERNAL) return U ("A memoria interna: 32 sons que podes gardar e editar. Gárdase co proxecto.");
    if (src == B7Processor::FILE_CART) return U ("O último banco .syx que abriches.");
    if (src == FOLDER_ROW) return folderBtn.getTooltip();
    return {};
}

void SoundsPage::refresh()
{
    const int src = storeMode ? B7Processor::INTERNAL : proc.getSource();
    for (auto* b : mems)
    {
        b->name = juce::String (b7::patchName (proc.voiceAt (src, b->index)));
        b->selected = ! storeMode && b->index == proc.getVoiceNum();
        b->storeMode = storeMode;
        b->repaint();
    }
    copyBtn.setEnabled (proc.getSource() != B7Processor::INTERNAL);
    if (shownCarts != proc.numCarts()) { shownCarts = proc.numCarts(); list.updateContent(); }
    list.repaint();
    repaint();
}

void SoundsPage::tick()
{
    bool changed = false;
    for (int i = 0; i < 6; ++i)
    {
        const float v = juce::jlimit (0.0f, 1.0f, std::sqrt (proc.opLevel[(size_t) (6 - (i + 1))].load()));
        const float nv = juce::jmax (v, lv[i] * 0.8f);
        if (std::abs (nv - lv[i]) > 0.01f) changed = true;
        lv[i] = nv;
    }
    if (changed) repaint (algoArea);
}

void SoundsPage::paint (juce::Graphics& g)
{
    groupBar (g, list.getBounds().toFloat().withHeight (16.0f).translated (0, -24.0f), "CARTUCHOS");
    const int src = storeMode ? B7Processor::INTERNAL : proc.getSource();
    groupBar (g, memArea.toFloat().withHeight (16.0f).translated (0, -24.0f),
              storeMode ? U ("GARDAR EN: MEMORIA INTERNA") : U ("MEMORIA · ") + proc.sourceName (src), storeMode ? col::maroon.brighter (0.4f) : col::green);
    const auto& e = proc.getEdit();
    groupBar (g, algoArea.toFloat().withHeight (16.0f).translated (0, -24.0f), U ("ALGORITMO ") + juce::String (e[134] + 1));
    g.setColour (col::bodyLo); g.fillRect (algoArea);
    drawAlgorithm (g, algoArea.toFloat().reduced (14.0f, 18.0f), e[134], lv, &e[155], false, col::print);

    // descrición / información debaixo da grella
    auto d = descArea.toFloat();
    juce::String txt = proc.voiceDesc (proc.getSource(), proc.getVoiceNum());
    if (txt.isEmpty()) txt = proc.sourceTitle (proc.getSource());
    g.setColour (col::printDim); g.setFont (body (14.0f));
    g.drawFittedText (txt, d.toNearestInt(), juce::Justification::centredLeft, 2, 0.95f);
}

void SoundsPage::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (36);
    top.removeFromLeft (300);
    storeBtn.setBounds (top.removeFromRight (150)); top.removeFromRight (8);
    copyBtn.setBounds (top.removeFromRight (150)); top.removeFromRight (8);
    saveBankBtn.setBounds (top.removeFromRight (130)); top.removeFromRight (8);
    folderBtn.setBounds (top.removeFromRight (96)); top.removeFromRight (8);
    openBtn.setBounds (top.removeFromRight (112));
    r.removeFromTop (36);
    list.setBounds (r.removeFromLeft (250));
    r.removeFromLeft (20);
    algoArea = r.removeFromRight (220);
    r.removeFromRight (20);
    memArea = r;
    auto grid = r.withTrimmedBottom (46);
    descArea = r.removeFromBottom (40);
    const float cw = (float) grid.getWidth() / 8.0f, chh = (float) grid.getHeight() / 4.0f;
    for (int i = 0; i < 32; ++i)
        mems[i]->setBounds (juce::Rectangle<float> ((float) grid.getX() + (float) (i % 8) * cw, (float) grid.getY() + (float) (i / 8) * chh, cw, chh).toNearestInt());
}

// ================================================================ páxina de edición
namespace
{
    const char* CURVES[4] = { "-LIN", "-EXP", "+EXP", "+LIN" };
    const char* LFOW[6] = { "TRIANG.", "SERRA -", "SERRA +", "CADRADA", "SENO", "ALEAT." };

    juce::String freqText (const uint8_t* op)
    {
        const int mode = op[17], c = op[18], f = op[19];
        if (mode == 0)
        {
            const double ratio = (c == 0 ? 0.5 : (double) c) * (1.0 + f / 100.0);
            return juce::String (ratio, 2);
        }
        const double hz = std::pow (10.0, (double) (c & 3)) * std::exp (std::log (10.0) * f / 100.0);
        return hz >= 1000 ? juce::String (hz / 1000.0, 2) + " kHz" : juce::String (hz, hz < 10 ? 3 : (hz < 100 ? 2 : 1)) + " Hz";
    }
}

NumBox* EditPage::add (int index, int mn, int mx, const juce::String& lcd, const juce::String& tip, std::function<juce::String (int)> fmt)
{
    auto* b = boxes.add (new NumBox (mn, mx, std::move (fmt)));
    b->lcdName = lcd;
    b->setTooltip (tip);
    b->small = true;
    if (index >= 0)
    {
        bound.push_back ({ b, index });
        b->onChange = [this, index] (int v) { proc.setParam (index, v); };
    }
    b->onTouch = [this] (NumBox& nb) { main.showParam (nb.lcdName, nb.text()); };
    addAndMakeVisible (b);
    return b;
}

EditPage::EditPage (B7Processor& p, MainView& m) : proc (p), main (m)
{
    strip.onPick = [this] (int a) { proc.setParam (134, a); main.showParam ("ALGORITMO", juce::String (a + 1)); };
    strip.setTooltip (U ("Os 32 algoritmos: como se conectan os seis operadores. Os verdes soan (portadores); os outros modulan os de debaixo. A liña verde é a realimentación."));
    addAndMakeVisible (strip);

    // ---- globais ----
    add (134, 0, 31, "ALGORITMO", U ("Algoritmo (1–32): a forma en que se conectan os operadores."), [] (int v) { return juce::String (v + 1); });
    add (135, 0, 7, "REALIMENTACION", U ("Realimentación (0–7): o operador marcado modúlase a si mesmo; aos valores altos vai cara a serra e ruído."));
    add (136, 0, 1, "SINCRONIA OSC.", U ("Sincronía dos osciladores: se está acesa, todos comezan en fase en cada nota (ataques sempre iguais)."), [] (int v) { return v ? U ("SI") : U ("NON"); });
    add (142, 0, 5, "LFO ONDA", U ("Forma de onda do LFO."), [] (int v) { return juce::String (LFOW[juce::jlimit (0, 5, v)]); });
    add (137, 0, 99, "LFO VELOCIDADE", U ("Velocidade do LFO."));
    add (138, 0, 99, "LFO RETARDO", U ("Retardo do LFO: canto tarda en entrar despois de premer."));
    add (139, 0, 99, "LFO PROF. TON", U ("Profundidade do LFO sobre a afinación (vibrato)."));
    add (140, 0, 99, "LFO PROF. AMP", U ("Profundidade do LFO sobre a amplitude (trémolo ou wah, segundo os operadores con AM)."));
    add (141, 0, 1, "LFO SINCRONIA", U ("Sincronía do LFO: se está acesa, a onda empeza desde o comezo en cada nota."), [] (int v) { return v ? U ("SI") : U ("NON"); });
    add (143, 0, 7, "SENS. TON (PMS)", U ("Sensibilidade á modulación de ton: canto vibrato chega ao son (do LFO e da roda)."));
    for (int i = 0; i < 4; ++i) add (126 + i, 0, 99, "EG TON TAXA " + juce::String (i + 1), U ("Envolvente de ton: velocidade do tramo ") + juce::String (i + 1) + ".");
    for (int i = 0; i < 4; ++i) add (130 + i, 0, 99, "EG TON NIVEL " + juce::String (i + 1), U ("Envolvente de ton: nivel ") + juce::String (i + 1) + U (" (50 = afinación normal)."));
    add (144, 0, 48, "TRANSPOSICION", U ("Transposición do son (Do3 = sen transpoñer)."), [] (int v) { return noteName (v + 36); });

    nameEd.setFont (cond (17.0f));
    nameEd.setInputRestrictions (10, " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{|}");
    nameEd.setTooltip (U ("Nome do son (10 caracteres, como no DX7)."));
    nameEd.onTextChange = [this] { proc.setVoiceName (nameEd.getText()); };
    nameEd.onReturnKey = [this] { main.keys().grabKeyboardFocus(); };
    addAndMakeVisible (nameEd);

    // ---- operadores (fila 0 = OP1) ----
    for (int row = 0; row < 6; ++row)
    {
        const int opn = row + 1, base = (6 - opn) * 21;
        const juce::String o = "OP" + juce::String (opn) + " ";
        auto* sw = add (-1, 0, 1, o + "ACESO", U ("Acende ou apaga este operador (para escoitar cada parte do son)."),
                        [opn] (int v) { return "OP" + juce::String (opn) + (v ? "" : " -"); });
        sw->small = false;
        sw->onChange = [this, opn] (int v) { proc.setOpEnabled (opn, v != 0); };
        opSwitch[(size_t) row] = sw;
        add (base + 17, 0, 1, o + "MODO", U ("Modo de frecuencia: RAZÓN segue o teclado (múltiplo da nota); FIXA dá sempre a mesma frecuencia."),
             [] (int v) { return v ? U ("FIXA") : U ("RAZÓN"); });
        add (base + 18, 0, 31, o + "GROSO", U ("Frecuencia grosa: en RAZÓN, o múltiplo da nota (0 = ×0,5); en FIXA, 1, 10, 100 ou 1000 Hz."));
        add (base + 19, 0, 99, o + "FINO", U ("Frecuencia fina."));
        add (base + 20, 0, 14, o + "DESAFINAR", U ("Desafinación lixeira (−7…+7): engade batemento entre operadores."),
             [] (int v) { return v == 7 ? juce::String ("0") : (v > 7 ? "+" + juce::String (v - 7) : juce::String (v - 7)); });
        for (int i = 0; i < 4; ++i) add (base + i, 0, 99, o + "TAXA " + juce::String (i + 1),
             U ("Envolvente: velocidade do tramo ") + juce::String (i + 1) + (i == 3 ? U (" (ao soltar).") : "."));
        for (int i = 0; i < 4; ++i) add (base + 4 + i, 0, 99, o + "NIVEL " + juce::String (i + 1),
             U ("Envolvente: nivel ") + juce::String (i + 1) + (i == 2 ? U (" (o que se mantén mentres premes).") : i == 3 ? U (" (onde acaba ao soltar).") : "."));
        add (base + 16, 0, 99, o + "NIVEL SAIDA", U ("Nivel de saída do operador: nos portadores, volume; nos moduladores, canto brillo dan."));
        add (base + 15, 0, 7, o + "SENS. VELOC.", U ("Sensibilidade á velocidade: canto cambia este operador segundo a forza coa que premes."));
        add (base + 14, 0, 3, o + "SENS. AM", U ("Sensibilidade á modulación de amplitude (LFO e controis)."));
        add (base + 13, 0, 7, o + "ESCALA TAXA", U ("Escala de taxa: as notas agudas fan a envolvente máis rápida."));
        add (base + 8, 0, 99, o + "PUNTO RUPTURA", U ("Punto de ruptura: a nota desde a que se aplica a escala de nivel cara a esquerda e cara a dereita."),
             [] (int v) { return noteName (v + 21); });
        add (base + 9, 0, 99, o + "PROF. ESQUERDA", U ("Profundidade da escala de nivel cara ás notas graves."));
        add (base + 10, 0, 99, o + "PROF. DEREITA", U ("Profundidade da escala de nivel cara ás notas agudas."));
        add (base + 11, 0, 3, o + "CURVA ESQUERDA", U ("Curva da escala de nivel cara aos graves."), [] (int v) { return juce::String (CURVES[v & 3]); });
        add (base + 12, 0, 3, o + "CURVA DEREITA", U ("Curva da escala de nivel cara aos agudos."), [] (int v) { return juce::String (CURVES[v & 3]); });
    }

    initBtn.setButtonText (U ("Son inicial"));
    initBtn.setTooltip (U ("Empeza desde cero: só o OP1, onda senoidal (o «INIT VOICE»)."));
    initBtn.onClick = [this] { proc.initVoice(); main.showMessage (U ("Son inicial cargado.")); };
    revertBtn.setButtonText (U ("Desfacer cambios"));
    revertBtn.setTooltip (U ("Volve ao son tal e como está gardado na memoria."));
    revertBtn.onClick = [this] { proc.revert(); main.showMessage (U ("Cambios desfeitos.")); };
    saveVoiceBtn.setButtonText (U ("Gardar son .syx"));
    saveVoiceBtn.setTooltip (U ("Garda só este son nun .syx (formato de un son do DX7)."));
    saveVoiceBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Gardar son"), juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                         .getChildFile (juce::String (b7::patchName (proc.getEdit())).trim() + ".syx"), "*.syx");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (! f.hasFileExtension ("syx")) f = f.withFileExtension ("syx");
            main.showMessage (proc.saveVoiceFile (f) ? U ("Son gardado: ") + f.getFileName() : U ("Non se puido gardar o ficheiro."));
        });
    };
    openVoiceBtn.setButtonText (U ("Abrir .syx"));
    openVoiceBtn.setTooltip (U ("Abre un .syx: un son vai ao búfer de edición; un banco queda como cartucho."));
    openVoiceBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Abrir sons de DX7"), juce::File(), "*.syx;*.SYX");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            juce::String msg;
            proc.loadSyxFile (f, msg);
            main.showMessage (msg);
        });
    };
    for (auto* b : { &openVoiceBtn, &saveVoiceBtn, &revertBtn, &initBtn }) { b->setWantsKeyboardFocus (false); addAndMakeVisible (b); }
}

void EditPage::refresh()
{
    const auto& e = proc.getEdit();
    for (auto& [b, idx] : bound) b->setValue (e[(size_t) idx]);
    for (int row = 0; row < 6; ++row) opSwitch[(size_t) row]->setValue (proc.isOpEnabled (row + 1) ? 1 : 0);
    // os do operador apagado, en gris
    for (int row = 0; row < 6; ++row)
    {
        const bool on = proc.isOpEnabled (row + 1);
        for (auto& [b, idx] : bound)
            if (idx < 126 && idx / 21 == 6 - (row + 1)) { if (b->dim == on) { b->dim = ! on; b->repaint(); } }
    }
    strip.selected = e[134]; strip.repaint();
    const auto nm = juce::String (b7::patchName (e));
    if (! nameEd.hasKeyboardFocus (true) && nameEd.getText() != nm) nameEd.setText (nm, false);
    revertBtn.setEnabled (proc.isEdited());
    repaint();
}

void EditPage::tick()
{
    for (int i = 0; i < 6; ++i)
    {
        const float v = juce::jlimit (0.0f, 1.0f, std::sqrt (proc.opLevel[(size_t) (6 - (i + 1))].load()));
        const float nv = juce::jmax (v, lv[i] * 0.8f);
        if (std::abs (nv - lv[i]) > 0.01f) { lv[i] = nv; repaint (meterArea[(size_t) i]); }
    }
}

void EditPage::paint (juce::Graphics& g)
{
    const auto& e = proc.getEdit();
    // grupos globais
    const char* gn[5] = { "ALGORITMO", "LFO", "ENVOLVENTE DE TON", "TRANSP.", "NOME" };
    for (int i = 0; i < 4; ++i) groupBar (g, groupArea[(size_t) i].toFloat().withHeight (14.0f), U (gn[i]));
    printed (g, gn[4], groupArea[4].toFloat(), 12.0f, col::green.brighter (0.25f), juce::Justification::centredLeft, 0.14f);
    // nomes das caixas globais
    auto capt = [&] (int from, int to, const char* const* names)
    {
        for (int i = from; i < to; ++i)
        {
            auto r = boxes[i]->getBounds().toFloat();
            printed (g, U (names[i - from]), r.withY (r.getY() - 15.0f).withHeight (14.0f), 11.0f, col::printDim, juce::Justification::centred, 0.04f);
        }
    };
    static const char* gl[] = { "ALG", "REALIM.", "SINC. OSC", "ONDA", "VELOC.", "RETARDO", "PROF. TON", "PROF. AMP", "SINC.", "SENS. TON",
                                "T1", "T2", "T3", "T4", "N1", "N2", "N3", "N4", "NOTA" };
    capt (0, 19, gl);
    drawEnvelope (g, pegArea.toFloat(), &e[126], &e[130], true, col::green);

    // táboa de operadores
    const float hy = (float) tableHead.getY();
    auto colX = [&] (int boxIndex) { return boxes[boxIndex]->getBounds().toFloat(); };
    const int b0 = 19;   // primeira caixa do OP1
    auto span = [&] (int a, int b) { auto ra = colX (b0 + a), rb = colX (b0 + b); return ra.getUnion (rb).withY (hy).withHeight (14.0f); };
    // índices dentro da fila: 0 sw, 1 modo, 2 groso, 3 fino, 4 desaf, 5..8 T, 9..12 N, 13 OL, 14 KVS, 15 AMS, 16 RS, 17 BP, 18 LD, 19 RD, 20 LC, 21 RC
    groupBar (g, span (1, 4), "OSCILADOR");
    groupBar (g, span (5, 12).withRight ((float) egArea[0].getRight()), "ENVOLVENTE");
    groupBar (g, span (13, 16), U ("NIVEL E SENSIBILIDADE"));
    groupBar (g, span (17, 21), "ESCALA DE TECLADO");
    static const char* names[] = { "", "MODO", "GROSO", "FINO", "DESAF.", "T1", "T2", "T3", "T4", "N1", "N2", "N3", "N4",
                                   "NIVEL", "VELOC.", "AM", "ESC. T.", "PUNTO", "PROF. E", "PROF. D", "CURVA E", "CURVA D" };
    for (int i = 1; i < 22; ++i)
        printed (g, U (names[i]), colX (b0 + i).withY (hy + 17.0f).withHeight (13.0f), 11.0f, col::printDim, juce::Justification::centred, 0.04f);
    printed (g, U ("FRECUENCIA"), freqArea[0].toFloat().withY (hy + 17.0f).withHeight (13.0f), 11.0f, col::printDim, juce::Justification::centred, 0.04f);

    const auto G = b7::algoGraph (e[134]);
    for (int row = 0; row < 6; ++row)
    {
        const int opn = row + 1, base = (6 - opn) * 21;
        const bool on = proc.isOpEnabled (opn);
        if (row % 2 == 0) { g.setColour (col::bodyHi.withAlpha (0.45f)); g.fillRect (rowArea[(size_t) row]); }
        // frecuencia
        auto fr = freqArea[(size_t) row].toFloat().reduced (1.0f);
        g.setColour (col::bodyLo); g.fillRoundedRectangle (fr, 2.5f);
        g.setColour (G.carrier[(size_t) opn] ? col::green.withAlpha (on ? 1.0f : 0.4f) : col::print.withAlpha (on ? 1.0f : 0.4f));
        g.setFont (cond (15.0f));
        g.drawText (freqText (&e[(size_t) base]), fr, juce::Justification::centred, false);
        // envolvente
        drawEnvelope (g, egArea[(size_t) row].toFloat().reduced (1.0f, 2.0f), &e[(size_t) base], &e[(size_t) base + 4], false,
                      on ? (G.carrier[(size_t) opn] ? col::green : col::print) : col::printDim.withAlpha (0.4f));
        // medidor
        auto mr = meterArea[(size_t) row].toFloat();
        g.setColour (col::bodyLo); g.fillRect (mr);
        g.setColour (G.carrier[(size_t) opn] ? col::green : col::print);
        g.fillRect (mr.withTop (mr.getBottom() - mr.getHeight() * lv[row]));
    }
}

void EditPage::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (36);
    top.removeFromLeft (300);
    initBtn.setBounds (top.removeFromRight (120)); top.removeFromRight (8);
    revertBtn.setBounds (top.removeFromRight (140)); top.removeFromRight (8);
    saveVoiceBtn.setBounds (top.removeFromRight (140)); top.removeFromRight (8);
    openVoiceBtn.setBounds (top.removeFromRight (112));
    groupArea[4] = { 290, top.getY() + 3, 48, 30 };
    nameEd.setBounds (340, top.getY() + 3, 140, 30);
    r.removeFromTop (8);
    strip.setBounds (r.removeFromTop (132));
    r.removeFromTop (10);

    // globais
    auto gr = r.removeFromTop (64);
    int x = gr.getX();
    const int by = gr.getY() + 34, bh = 26;
    auto place = [&] (int i, int w) { boxes[i]->setBounds (x, by, w, bh); x += w + 4; };
    const int g0 = x;
    place (0, 44); place (1, 50); place (2, 58);
    groupArea[0] = { g0, gr.getY(), x - 4 - g0, 14 };
    x += 14;
    const int g1 = x;
    place (3, 62); for (int i = 4; i <= 9; ++i) place (i, i == 9 ? 50 : 46);
    groupArea[1] = { g1, gr.getY(), x - 4 - g1, 14 };
    x += 14;
    const int g2 = x;
    for (int i = 10; i <= 17; ++i) place (i, 28);
    pegArea = { x, by, 72, bh }; x += 76;
    groupArea[2] = { g2, gr.getY(), x - 4 - g2, 14 };
    x += 14;
    const int g3 = x;
    place (18, gr.getRight() - x);
    groupArea[3] = { g3, gr.getY(), x - 4 - g3, 14 };

    r.removeFromTop (6);
    tableHead = r.removeFromTop (34);
    const int rowH = juce::jmin (34, r.getHeight() / 6);
    struct C { int w, gap; };
    // 0 sw, 1 modo, 2 groso, 3 fino, [freq], 4 desaf, 5..8 T, 9..12 N, [eg], 13 OL, 14 KVS, 15 AMS, 16 RS, 17 BP, 18 LD, 19 RD, 20 LC, 21 RC
    const C cols[22] = { { 48, 0 }, { 50, 14 }, { 36, 3 }, { 36, 3 }, { 40, 70 }, { 28, 8 }, { 28, 2 }, { 28, 2 }, { 28, 2 },
                         { 28, 6 }, { 28, 2 }, { 28, 2 }, { 28, 2 }, { 38, 74 }, { 32, 3 }, { 30, 3 }, { 30, 3 },
                         { 44, 8 }, { 32, 3 }, { 32, 3 }, { 42, 3 }, { 42, 3 } };
    for (int row = 0; row < 6; ++row)
    {
        auto rr = r.removeFromTop (rowH);
        rowArea[(size_t) row] = rr.withX (0).withWidth (getWidth());
        const int b0 = 19 + row * 22;
        int cx = rr.getX();
        const int y = rr.getY() + (rowH - 28) / 2;
        for (int c = 0; c < 22; ++c)
        {
            cx += cols[c].gap;
            if (c == 4) freqArea[(size_t) row] = { cx - cols[c].gap + 3, y, cols[c].gap - 6, 28 };
            if (c == 13) egArea[(size_t) row] = { cx - cols[c].gap + 4, y, cols[c].gap - 8, 28 };
            boxes[b0 + c]->setBounds (cx, y, cols[c].w, 28);
            if (c == 0) meterArea[(size_t) row] = { cx + cols[c].w + 3, y + 2, 5, 24 };
            cx += cols[c].w;
        }
    }
}

// ================================================================ vista principal
MainView::MainView (B7Processor& p)
    : proc (p), keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard), sounds (p, *this), editor (p, *this)
{
    setLookAndFeel (&look);
    addAndMakeVisible (lcd);

    tabPlay.setButtonText (U ("Sons"));
    tabEdit.setButtonText (U ("Editar"));
    tabPlay.setTooltip (U ("Os sons: cartuchos e memorias."));
    tabEdit.setTooltip (U ("Edita o son: os seis operadores, o algoritmo, o LFO e as envolventes."));
    tabPlay.onClick = [this] { showPage (0); };
    tabEdit.onClick = [this] { showPage (1); };
    for (auto* b : { &tabPlay, &tabEdit }) { b->setWantsKeyboardFocus (false); addAndMakeVisible (b); }

    volume.setTooltip ("Volume");
    volAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "volume", volume);
    volume.setDoubleClickReturnValue (true, 0.0);
    volume.onValueChange = [this] { showParam ("VOLUME", juce::String (volume.getValue(), 1) + " dB"); };
    addAndMakeVisible (volume);

    auto attach = [this] (NumBox& b, const char* id, std::unique_ptr<juce::ParameterAttachment>& att, const juce::String& lcdName, const juce::String& tip)
    {
        auto* prm = proc.apvts.getParameter (id);
        att = std::make_unique<juce::ParameterAttachment> (*prm, [&b, prm] (float v) { b.setValue ((int) std::round (prm->convertFrom0to1 (prm->convertTo0to1 (v)))); });
        b.onChange = [&att] (int v) { att->setValueAsPartOfGesture ((float) v); };
        b.onGestureStart = [&att] { att->beginGesture(); };
        b.onGestureEnd = [&att] { att->endGesture(); };
        b.onTouch = [this] (NumBox& nb) { showParam (nb.lcdName, nb.text()); };
        b.lcdName = lcdName;
        b.setTooltip (tip);
        att->sendInitialUpdate();
        addAndMakeVisible (b);
    };
    bendBox.fmt = [] (int v) { return juce::String (v) + " st"; };
    attach (bendBox, "bend", bendAtt, "ALCANCE BEND", U ("Alcance da roda de afinación, en semitóns."));
    attach (wheelBox, "wheelRange", wheelAtt, "RODA MOD. ALCANCE", U ("Canto actúa a roda de modulación (0–99)."));
    wheelP.fmt = [] (int) { return U ("TON"); };
    wheelA.fmt = [] (int) { return U ("AMP"); };
    wheelE.fmt = [] (int) { return U ("EG"); };
    for (auto* b : { &wheelP, &wheelA, &wheelE }) b->small = true;
    attach (wheelP, "wheelPitch", wpAtt, "RODA MOD. -> TON", U ("A roda de modulación dá vibrato (ton)."));
    attach (wheelA, "wheelAmp", waAtt, "RODA MOD. -> AMP", U ("A roda de modulación dá trémolo (amplitude, nos operadores con AM)."));
    attach (wheelE, "wheelEg", weAtt, "RODA MOD. -> EG", U ("A roda de modulación controla o nivel das envolventes dos operadores con AM."));

    keyboard.setAvailableRange (36, 96);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyPressBaseOctave (5);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, col::ivory);
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, col::blackKey);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, col::green.withAlpha (0.55f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, col::green.withAlpha (0.18f));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xffb9b5aa));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.35f));
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, col::printDim);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, col::membrane);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, col::print);
    addAndMakeVisible (keyboard);

    addChildComponent (sounds);
    addChildComponent (editor);

    escoita.setButtonText ("ESCOITA.");
    escoita.setURL (juce::URL ("https://www.escoita.link"));
    escoita.setFont (juce::Font (juce::FontOptions (tf (1)).withHeight (17.0f).withKerningFactor (0.16f)), false, juce::Justification::centredRight);
    escoita.setColour (juce::HyperlinkButton::textColourId, col::print);
    escoita.setTooltip (U ("Barullo 7FM é unha ferramenta de Escoita · escoita.link"));
    escoita.setWantsKeyboardFocus (false);
    addAndMakeVisible (escoita);

    setSize (W, H);
    showPage (0);
    sounds.refresh(); editor.refresh();
    seenChanges = proc.changes.load();
    startTimerHz (30);
    timerCallback();
}

MainView::~MainView() { stopTimer(); setLookAndFeel (nullptr); }

void MainView::showPage (int p)
{
    page = p;
    sounds.setVisible (p == 0);
    editor.setVisible (p == 1);
    tabPlay.setToggleState (p == 0, juce::dontSendNotification);
    tabEdit.setToggleState (p == 1, juce::dontSendNotification);
    if (p == 0) sounds.refresh(); else editor.refresh();
    repaint();
}

void MainView::showParam (const juce::String& a, const juce::String& b)
{
    paramA = a; paramB = b; paramTime = juce::Time::getMillisecondCounter();
    updateLcd();
}

void MainView::showMessage (const juce::String& m)
{
    message = m; messageTime = juce::Time::getMillisecondCounter();
    repaint (msgArea);
}

void MainView::updateLcd()
{
    const auto now = juce::Time::getMillisecondCounter();
    if (paramTime != 0 && now - paramTime < 2500)
    {
        lcd.setLines (lcdText (paramA).substring (0, 16), "= " + lcdText (paramB).substring (0, 14));
        return;
    }
    if (proc.notice.isNotEmpty() && now - proc.noticeTime < 2500)
    {
        lcd.setLines (lcdText (proc.notice), juce::String (b7::patchName (proc.getEdit())));
        return;
    }
    const int src = proc.getSource();
    const auto s = lcdText (proc.sourceName (src)).substring (0, 9).paddedRight (' ', 9);
    lcd.setLines (s + " SON " + juce::String (proc.getVoiceNum() + 1).paddedLeft ('0', 2),
                  juce::String (b7::patchName (proc.getEdit())).paddedRight (' ', 10) + (proc.isEdited() ? "  EDIT" : ""));
}

void MainView::timerCallback()
{
    const auto ch = proc.changes.load();
    if (ch != seenChanges)
    {
        seenChanges = ch;
        if (page == 0) sounds.refresh(); else editor.refresh();
        repaint (ledArea.expanded (4)); repaint (editLed.expanded (6));
    }
    updateLcd();
    if (page == 0) sounds.tick(); else editor.tick();

    const int nv = proc.activeVoices.load();
    const auto mi = proc.midiActivity.load();
    if (mi != lastMidi) { lastMidi = mi; midiFlash = 1.0f; }
    else midiFlash *= 0.75f;
    if (nv != shownVoices || midiFlash > 0.02f) { shownVoices = nv; repaint (voicesArea); }
    if (message.isNotEmpty() && juce::Time::getMillisecondCounter() - messageTime > 6000) { message = {}; repaint (msgArea); }
}

static void drawSevenSeg (juce::Graphics& g, juce::Rectangle<float> r, int digit)
{
    static const uint8_t map[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
    const float w = r.getWidth(), h = r.getHeight(), t = w * 0.17f, gp = t * 0.18f;
    const float x0 = r.getX(), y0 = r.getY(), ym = y0 + h / 2;
    auto hseg = [&] (float y) { juce::Path p; const float a = x0 + gp + t / 2, b = x0 + w - gp - t / 2;
        p.startNewSubPath (a, y); p.lineTo (a + t / 2, y - t / 2); p.lineTo (b - t / 2, y - t / 2); p.lineTo (b, y); p.lineTo (b - t / 2, y + t / 2); p.lineTo (a + t / 2, y + t / 2); p.closeSubPath(); return p; };
    auto vseg = [&] (float x, float ya, float yb) { juce::Path p; const float a = ya + gp + t / 2, b = yb - gp - t / 2;
        p.startNewSubPath (x, a); p.lineTo (x + t / 2, a + t / 2); p.lineTo (x + t / 2, b - t / 2); p.lineTo (x, b); p.lineTo (x - t / 2, b - t / 2); p.lineTo (x - t / 2, a + t / 2); p.closeSubPath(); return p; };
    const float yt = y0 + t / 2, yb = y0 + h - t / 2, xl = x0 + t / 2, xr = x0 + w - t / 2;
    juce::Path segs[7] = { hseg (yt), vseg (xr, yt, ym), vseg (xr, ym, yb), hseg (yb), vseg (xl, ym, yb), vseg (xl, yt, ym), hseg (ym) };
    const auto shear = juce::AffineTransform::shear (-0.08f, 0.0f).translated (0.08f * (y0 + h), 0.0f);
    for (int s = 0; s < 7; ++s)
    {
        const bool on = (map[digit % 10] >> s) & 1;
        segs[s].applyTransform (shear);
        if (on) { g.setColour (col::led.withAlpha (0.25f)); g.strokePath (segs[s], juce::PathStrokeType (4.0f)); }
        g.setColour (on ? col::led : col::ledOff);
        g.fillPath (segs[s]);
    }
}

static void lamp (juce::Graphics& g, juce::Rectangle<float> r, bool on, juce::Colour c, float glow = 1.0f)
{
    if (on) { g.setColour (c.withAlpha (0.3f * glow)); g.fillEllipse (r.expanded (2.5f)); }
    g.setColour (on ? c.withMultipliedAlpha (juce::jlimit (0.35f, 1.0f, glow)) : c.withMultipliedBrightness (0.18f));
    g.fillEllipse (r);
}

void MainView::paintPanel (juce::Graphics& g)
{
    // cabeceira
    printed (g, "BARULLO 7FM", { 28.0f, 14.0f, 260.0f, 40.0f }, 32.0f, col::print, juce::Justification::centredLeft, 0.04f);
    printed (g, U ("SÍNTESE FM DIXITAL · 6 OPERADORES · 32 ALGORITMOS · 16 VOCES"), { 248.0f, 24.0f, 560.0f, 24.0f }, 13.0f, col::green, juce::Justification::centredLeft, 0.12f);
    g.setColour (col::line); g.drawHorizontalLine (62, 20.0f, (float) getWidth() - 20.0f);

    // volume
    printed (g, "VOLUME", volArea.toFloat().removeFromTop (14.0f), 11.5f, col::print, juce::Justification::centred, 0.12f);

    // díxitos de LED
    groupBar (g, ledArea.toFloat().withHeight (14.0f).translated (0, -22.0f), "SON");
    auto lr = ledArea.toFloat();
    g.setColour (juce::Colour (0xff0a0706)); g.fillRoundedRectangle (lr.expanded (4.0f), 5.0f);
    g.setColour (col::ledBg); g.fillRoundedRectangle (lr, 3.0f);
    const int n = proc.getVoiceNum() + 1;
    const float dh = lr.getHeight() * 0.7f, dw = dh * 0.56f;
    auto d1 = juce::Rectangle<float> (lr.getCentreX() - dw - 6.0f, lr.getCentreY() - dh / 2, dw, dh);
    drawSevenSeg (g, d1, n / 10);
    drawSevenSeg (g, d1.translated (dw + 12.0f, 0), n % 10);

    // marco da LCD
    groupBar (g, lcdFrame.toFloat().withHeight (14.0f).translated (0, -22.0f), "PANTALLA");
    g.setColour (juce::Colour (0xff0a0706)); g.fillRoundedRectangle (lcdFrame.toFloat(), 5.0f);

    // voces, MIDI e editado
    groupBar (g, voicesArea.toFloat().withHeight (14.0f).translated (0, -22.0f), "VOCES");
    auto va = voicesArea.toFloat();
    const int active = juce::jmax (0, shownVoices);
    for (int i = 0; i < 16; ++i)
    {
        auto r = juce::Rectangle<float> (va.getX() + 4.0f + (float) (i % 8) * 17.0f, va.getY() + 6.0f + (float) (i / 8) * 17.0f, 8.0f, 8.0f);
        lamp (g, r, i < active, col::led);
    }
    auto row = juce::Rectangle<float> (va.getX() + 4.0f, va.getY() + 50.0f, 8.0f, 8.0f);
    lamp (g, row, midiFlash > 0.05f, col::green, midiFlash);
    printed (g, "MIDI", row.withWidth (50.0f).translated (13.0f, -3.0f).withHeight (14.0f), 11.5f, col::printDim, juce::Justification::centredLeft, 0.1f);
    lamp (g, editLed.toFloat(), proc.isEdited(), col::led);
    printed (g, "EDITADO", editLed.toFloat().withWidth (70.0f).translated (13.0f, -3.0f).withHeight (14.0f), 11.5f, col::printDim, juce::Justification::centredLeft, 0.1f);

    // función
    groupBar (g, funcArea.toFloat().withHeight (14.0f).translated (0, -22.0f), U ("FUNCIÓN"));
    auto lab = [&] (juce::Component& c, const char* t)
    {
        auto r = c.getBounds().toFloat();
        printed (g, U (t), r.withY (r.getY() - 16.0f).withHeight (14.0f), 11.0f, col::printDim, juce::Justification::centred, 0.05f);
    };
    lab (bendBox, "BEND");
    lab (wheelBox, "RODA MOD.");
    {
        auto r = wheelP.getBounds().getUnion (wheelE.getBounds()).toFloat();
        printed (g, U ("A RODA ACTÚA EN"), r.withY (r.getY() - 16.0f).withHeight (14.0f), 11.0f, col::printDim, juce::Justification::centred, 0.05f);
    }

    // aviso de texto
    if (message.isNotEmpty())
    {
        g.setColour (col::print.withAlpha (0.85f)); g.setFont (body (13.5f));
        g.drawText (message, msgArea, juce::Justification::centredLeft, true);
    }
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (col::body);
    // textura moi leve do plástico
    g.setColour (juce::Colours::white.withAlpha (0.015f));
    for (int y = 0; y < getHeight(); y += 3) g.drawHorizontalLine (y, 0, (float) getWidth());
    paintPanel (g);
    groupBar (g, tabPlay.getBounds().getUnion (tabEdit.getBounds()).toFloat().withHeight (14.0f).translated (0, -18.0f), "MODO");
    g.setColour (col::line); g.drawHorizontalLine (keysFrame.getY() - 10, 20.0f, (float) getWidth() - 20.0f);
    auto kf = keysFrame.toFloat();
    g.setColour (juce::Colour (0xff0a0706)); g.fillRoundedRectangle (kf, 4.0f);
}

void MainView::resized()
{
    escoita.setBounds (getWidth() - 28 - 160, 18, 160, 30);
    // fila do panel: y 90..190
    const int py = 92, ph = 96;
    volArea = { 26, 74, 56, 124 };
    volume.setBounds (volArea.withTrimmedTop (16).reduced (4, 2));
    ledArea = { 104, py, 104, ph };
    lcdFrame = { 230, py, 372, ph };
    lcd.setBounds (lcdFrame.reduced (8));
    msgArea = { 230, py + ph + 6, 600, 18 };
    voicesArea = { 626, py, 140, ph };
    editLed = { 702, py + 50, 8, 8 };
    funcArea = { 782, py, getWidth() - 28 - 782, ph };
    {
        int x = funcArea.getX();
        const int y = py + 22;
        bendBox.setBounds (x, y, 54, 28); x += 62;
        wheelBox.setBounds (x, y, 54, 28); x += 62;
        wheelP.setBounds (x, y, 34, 28); wheelA.setBounds (x + 36, y, 34, 28); wheelE.setBounds (x + 72, y, 30, 28);
    }

    // páxinas: comezan na fila dos botóns de modo
    const int pageY = 226;
    tabPlay.setBounds (28, pageY, 120, 34);
    tabEdit.setBounds (154, pageY, 120, 34);
    keysFrame = { 20, getHeight() - 118, getWidth() - 40, 106 };
    keyboard.setBounds (keysFrame.reduced (6));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 36.0f);
    auto pageArea = juce::Rectangle<int> (28, pageY, getWidth() - 56, keysFrame.getY() - 18 - pageY);
    sounds.setBounds (pageArea);
    editor.setBounds (pageArea);
}
} // namespace ui

// ================================================================ xanela (escalable)
B7Editor::B7Editor (B7Processor& p) : AudioProcessorEditor (&p), proc (p), main (p)
{
    addAndMakeVisible (main);
    setResizable (true, true);
    setResizeLimits (ui::MainView::W * 5 / 10, ui::MainView::H * 5 / 10, ui::MainView::W * 2, ui::MainView::H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) ui::MainView::W / ui::MainView::H);
    float s = proc.uiScale;
    // a primeira vez, que colla na pantalla
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto ua = d->userArea;
        const float fit = juce::jmin ((float) (ua.getWidth() - 40) / (float) ui::MainView::W, (float) (ua.getHeight() - 90) / (float) ui::MainView::H);
        if (s > fit && fit > 0.4f) s = fit;
    }
    setSize (juce::roundToInt (ui::MainView::W * s), juce::roundToInt (ui::MainView::H * s));
}

void B7Editor::resized()
{
    const float s = (float) getWidth() / (float) ui::MainView::W;
    main.setBounds (0, 0, ui::MainView::W, ui::MainView::H);
    main.setTransform (juce::AffineTransform::scale (s));
    proc.uiScale = s;
}
