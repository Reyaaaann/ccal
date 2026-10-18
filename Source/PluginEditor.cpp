#include "PluginEditor.h"
#include "Theory.h"
#include <BinaryData.h>

namespace
{
    constexpr int kW = 900;
    constexpr int kH = 1096;

    float gPulse = 0.5f;   // 0..1 slow breathing, drives all glows
    float gFlash = 0.0f;   // 1 -> 0 after pressing CONVERT

    namespace Col
    {
        const juce::Colour bgTop    { 0xff180412 };
        const juce::Colour bgBot    { 0xff2a0a20 };
        const juce::Colour panelA   { 0xff32102a };
        const juce::Colour panelB   { 0xff1e0819 };
        const juce::Colour field    { 0xff180613 };
        const juce::Colour pink     { 0xffff4fb0 };
        const juce::Colour pinkHot  { 0xffff3da5 };
        const juce::Colour pinkSoft { 0xffffa3d8 };
        const juce::Colour text     { 0xfffbeaf4 };
        const juce::Colour dim      { 0xffc593ad };
    }

    // ---- cursive font: first installed from this list, otherwise a serif italic
    struct FontPick { juce::String name; float scale; bool italic; };

    const FontPick& fontPick()
    {
        static const FontPick fp = []
        {
            const auto avail = juce::Font::findAllTypefaceNames();
            const char* prefs[] = { "Monotype Corsiva", "Apple Chancery", "Snell Roundhand",
                                    "Palatino Linotype", "Book Antiqua", "Georgia", "Times New Roman" };
            for (auto* p : prefs)
            {
                if (avail.contains (juce::String (p), true))
                {
                    const juce::String n (p);
                    const bool script = (n == "Monotype Corsiva" || n == "Apple Chancery" || n == "Snell Roundhand");
                    return FontPick { n, n == "Monotype Corsiva" ? 1.2f : (script ? 1.1f : 1.0f), ! script };
                }
            }
            return FontPick { juce::Font::getDefaultSerifFontName(), 1.0f, true };
        }();
        return fp;
    }

    juce::Font F (float h, int style = juce::Font::plain)
    {
        const auto& fp = fontPick();
        const int st = style | (fp.italic ? (int) juce::Font::italic : 0);
        return juce::Font (juce::FontOptions (fp.name, h * fp.scale, st));
    }

    float textW (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, false).getWidth();
    }

    void glowText (juce::Graphics& g, const juce::String& txt, juce::Rectangle<float> a,
                   const juce::Font& f, juce::Colour c, float glow, juce::Justification j,
                   const juce::ColourGradient* grad = nullptr)
    {
        juce::GlyphArrangement ga;
        ga.addFittedText (f, txt, a.getX(), a.getY(), a.getWidth(), a.getHeight(), j, 1, 1.0f);
        juce::Path p;
        ga.createPath (p);

        if (glow > 0.0f)
        {
            for (int k = 4; k >= 1; --k)
            {
                g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, glow * 0.07f)));
                g.strokePath (p, juce::PathStrokeType ((float) k * 2.6f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
            }
        }
        if (grad != nullptr) g.setGradientFill (*grad); else g.setColour (c);
        g.fillPath (p);
    }

    void glowRect (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour c, float amount)
    {
        for (int k = 3; k >= 1; --k)
        {
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, amount * 0.045f)));
            g.drawRoundedRectangle (r.expanded ((float) k), corner + (float) k, (float) k * 1.3f);
        }
        g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, 0.30f + amount * 0.25f)));
        g.drawRoundedRectangle (r, corner, 1.1f);
    }

    void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float glow)
    {
        juce::ColourGradient grad (Col::panelA.withAlpha (0.85f), r.getX(), r.getY(),
                                   Col::panelB.withAlpha (0.90f), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 10.0f);
        glowRect (g, r, 10.0f, Col::pink, glow);
    }

    // small pink bar + cursive heading, like the Apprehension panels
    void heading (juce::Graphics& g, const juce::String& title, float x, float y,
                  const juce::String& sub, float pulse)
    {
        g.setColour (Col::pink);
        g.fillRoundedRectangle (x, y + 4.0f, 3.0f, 19.0f, 1.5f);
        glowText (g, title, { x + 12.0f, y, 320.0f, 28.0f }, F (23.0f), Col::pinkSoft,
                  0.4f + 0.5f * pulse, juce::Justification::centredLeft);
        if (sub.isNotEmpty())
        {
            g.setColour (Col::dim);
            g.setFont (F (14.0f));
            g.drawText (sub, (int) x + 12, (int) y + 27, 340, 18, juce::Justification::centredLeft);
        }
    }

    // ----------------------------------------------------------------------------------
    class GlowButton : public juce::Button
    {
    public:
        enum Style { Tab, Key, Pill, Quick };
        static constexpr float pad = 5.0f;

        GlowButton (const juce::String& n, Style s) : juce::Button (n), style (s)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void setSelected (bool s) { if (selected != s) { selected = s; repaint(); } }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (pad);
            const float cr = (style == Pill) ? r.getHeight() * 0.5f : (style == Tab ? r.getHeight() * 0.5f : 8.0f);

            if (style == Pill)
            {
                const float amt = 0.5f + 0.5f * gPulse + 1.4f * gFlash + (over ? 0.3f : 0.0f);
                for (int k = 3; k >= 1; --k)
                {
                    g.setColour (Col::pink.withAlpha (juce::jlimit (0.0f, 1.0f, amt * 0.06f)));
                    g.drawRoundedRectangle (r.expanded ((float) k), cr + (float) k, (float) k * 1.4f);
                }
                juce::ColourGradient grad (Col::pinkHot, r.getX(), r.getY(),
                                           Col::pinkSoft.interpolatedWith (Col::pinkHot, 0.4f),
                                           r.getRight(), r.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (r, cr);
            }
            else if (selected)
            {
                const float amt = 0.5f + 0.7f * gPulse + (over ? 0.3f : 0.0f);
                for (int k = 3; k >= 1; --k)
                {
                    g.setColour (Col::pink.withAlpha (juce::jlimit (0.0f, 1.0f, amt * 0.06f)));
                    g.drawRoundedRectangle (r.expanded ((float) k), cr + (float) k, (float) k * 1.3f);
                }
                juce::ColourGradient grad (juce::Colour (0xff5e1648), r.getX(), r.getY(),
                                           juce::Colour (0xff3a0c2d), r.getX(), r.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (r, cr);
                g.setColour (Col::pinkHot);
                g.drawRoundedRectangle (r, cr, 1.5f);
            }
            else
            {
                g.setColour (Col::field);
                g.fillRoundedRectangle (r, cr);
                g.setColour (Col::pink.withAlpha (over ? 0.85f : 0.35f));
                g.drawRoundedRectangle (r, cr, 1.1f);
            }

            if (down)
            {
                g.setColour (juce::Colours::white.withAlpha (0.16f));
                g.fillRoundedRectangle (r, cr);
            }

            if (style == Pill)
            {
                g.setColour (juce::Colour (0xff2a0620));
                g.setFont (F (26.0f, juce::Font::bold));
                g.drawText (getButtonText(), r.toNearestInt(), juce::Justification::centred);

                const float ax = r.getRight() - 46.0f, ay = r.getCentreY();
                juce::Path a;
                a.startNewSubPath (ax, ay);
                a.lineTo (ax + 24.0f, ay);
                a.startNewSubPath (ax + 16.0f, ay - 8.0f);
                a.lineTo (ax + 24.0f, ay);
                a.lineTo (ax + 16.0f, ay + 8.0f);
                g.strokePath (a, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
                return;
            }

            g.setColour (selected ? Col::text : Col::pinkSoft);
            if (style == Quick)
            {
                g.setFont (F (16.0f, juce::Font::bold));
                g.drawFittedText (getButtonText(), r.toNearestInt(), juce::Justification::centred, 2);
            }
            else
            {
                g.setFont (F (style == Tab ? 21.0f : 19.0f, juce::Font::bold));
                g.drawText (getButtonText(), r.toNearestInt(), juce::Justification::centred);
            }
        }

    private:
        Style style;
        bool selected = false;
    };

    // ----------------------------------------------------------------------------------
    class PinkLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        PinkLookAndFeel()
        {
            setColour (juce::ComboBox::backgroundColourId, Col::field);
            setColour (juce::ComboBox::textColourId, Col::text);
            setColour (juce::ComboBox::outlineColourId, Col::pink.withAlpha (0.6f));
            setColour (juce::ComboBox::arrowColourId, Col::pink);
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff200a1b));
            setColour (juce::PopupMenu::textColourId, Col::text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, Col::pink);
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
            setColour (juce::TextEditor::backgroundColourId, Col::field);
            setColour (juce::TextEditor::textColourId, Col::text);
            setColour (juce::TextEditor::highlightColourId, Col::pink.withAlpha (0.5f));
            setColour (juce::TextEditor::outlineColourId, Col::pink);
            setColour (juce::TextEditor::focusedOutlineColourId, Col::pinkSoft);
            setColour (juce::CaretComponent::caretColourId, Col::pinkSoft);
        }
        juce::Font getComboBoxFont (juce::ComboBox&) override { return F (19.0f, juce::Font::bold); }
        juce::Font getPopupMenuFont() override { return F (19.0f); }
    };

    struct Particle { float x, y, speed, phase, size; };
}

// ======================================================================================
class MainContent : public juce::Component, private juce::Timer
{
public:
    explicit MainContent (CentsHzAudioProcessor& p)
        : proc (p), apvts (p.apvts)
    {
        compass     = juce::ImageCache::getFromMemory (BinaryData::compass_pink_png,  BinaryData::compass_pink_pngSize);
        compassGlow = juce::ImageCache::getFromMemory (BinaryData::compass_glow_png,  BinaryData::compass_glow_pngSize);
        zinc        = juce::ImageCache::getFromMemory (BinaryData::zinc_logo_png,     BinaryData::zinc_logo_pngSize);

        juce::Random rng (7);
        for (int i = 0; i < 22; ++i)
            particles.push_back ({ rng.nextFloat() * kW, rng.nextFloat() * kH,
                                   0.15f + rng.nextFloat() * 0.4f,
                                   rng.nextFloat() * 6.28f, 1.2f + rng.nextFloat() * 2.2f });

        setLookAndFeel (&laf);

        for (auto* b : { &tabCentsToHz, &tabHzToCents })
            addAndMakeVisible (*b);
        tabCentsToHz.onClick = [this] { setChoice ("mode", 0); };
        tabHzToCents.onClick = [this] { setChoice ("mode", 1); };

        addAndMakeVisible (convertBtn);
        convertBtn.onClick = [this] { replayConvert(); };

        for (auto* l : { &leftLabel, &refLabel })
        {
            addAndMakeVisible (*l);
            l->setEditable (true);
            l->setFont (F (32.0f));
            l->setJustificationType (juce::Justification::centredLeft);
            l->setColour (juce::Label::textColourId, Col::text);
            l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
            l->setColour (juce::Label::textWhenEditingColourId, Col::text);
            l->setColour (juce::Label::backgroundWhenEditingColourId, Col::field);
            l->setColour (juce::Label::outlineWhenEditingColourId, Col::pink);
            l->setMouseCursor (juce::MouseCursor::IBeamCursor);
        }
        leftLabel.onTextChange = [this]
        {
            const double v = parseNumber (leftLabel.getText());
            if (getChoice ("mode") == 0) setFloat ("cents", (float) v);
            else                         setFloat ("targetHz", (float) v);
            replayConvert();
        };
        refLabel.onTextChange = [this]
        {
            setFloat ("refHz", (float) parseNumber (refLabel.getText()));
            replayConvert();
        };

        for (int i = 0; i < 12; ++i)
        {
            rootBtns.add (new GlowButton (Theory::noteName (i), GlowButton::Key));
            auto* b = rootBtns.getLast();
            b->setButtonText (Theory::noteName (i));
            addAndMakeVisible (b);
            b->onClick = [this, i] { setChoice ("rootKey", i); };
        }

        addAndMakeVisible (scaleBox);
        {
            int id = 1;
            for (auto& s : Theory::scales()) scaleBox.addItem (s.name, id++);
        }
        scaleBox.setJustificationType (juce::Justification::centredLeft);
        scaleBox.onChange = [this]
        {
            if (! updatingUI) setChoice ("scale", scaleBox.getSelectedItemIndex());
        };

        addAndMakeVisible (modeSame);
        addAndMakeVisible (modeRelative);
        modeSame.onClick     = [this] { setBool ("relative", false); };
        modeRelative.onClick = [this] { setBool ("relative", true); };

        const char* qNames[]  = { "A4\n440 Hz", "A3\n220 Hz", "A5\n880 Hz", "C4\n261.63 Hz" };
        for (int i = 0; i < 4; ++i)
        {
            quickBtns.add (new GlowButton ("q" + juce::String (i), GlowButton::Quick));
            auto* b = quickBtns.getLast();
            b->setButtonText (qNames[i]);
            addAndMakeVisible (b);
            b->onClick = [this, i] { setFloat ("refHz", quickValues[i]); replayConvert(); };
        }

        setSize (kW, kH);
        pollParameters (true);
        displayed = (float) currentResult();
        startTimerHz (30);
    }

    ~MainContent() override
    {
        stopTimer();
        scaleBox.setLookAndFeel (nullptr);
        setLookAndFeel (nullptr);
    }

    // ------------------------------------------------------------------ layout
    void resized() override
    {
        auto place = [] (juce::Component& c, juce::Rectangle<int> visual)
        {
            c.setBounds (visual.expanded ((int) GlowButton::pad));
        };

        place (tabCentsToHz,  { 230, 294, 220, 38 });
        place (tabHzToCents,  { 460, 294, 220, 38 });

        leftField = { 56.0f,  418.0f, 368.0f, 48.0f };
        refField  = { 476.0f, 418.0f, 368.0f, 48.0f };
        leftLabel.setBounds (leftField.toNearestInt().withTrimmedRight (86).withTrimmedLeft (14).reduced (0, 2));
        refLabel .setBounds (refField .toNearestInt().withTrimmedRight (86).withTrimmedLeft (14).reduced (0, 2));

        place (convertBtn, { 40, 492, 820, 56 });

        for (int i = 0; i < rootBtns.size(); ++i)
            place (*rootBtns[i], { 160 + i * 56, 752, 52, 38 });

        scaleBox.setBounds (160, 806, 262, 34);
        place (modeSame,     { 566, 804, 126, 36 });
        place (modeRelative, { 700, 804, 126, 36 });

        for (int i = 0; i < quickBtns.size(); ++i)
            place (*quickBtns[i], { 476 + i * 94, 1006, 88, 42 });
    }

    // ------------------------------------------------------------------ painting
    void paint (juce::Graphics& g) override
    {
        const float t = time;
        const float pulse = gPulse;
        const bool centsMode = (getChoice ("mode") == 0);

        // ---- background (dark plum, like the Apprehension skin)
        g.setGradientFill (juce::ColourGradient (Col::bgTop, 0.0f, 0.0f, Col::bgBot, 0.0f, (float) kH, false));
        g.fillRect (0, 0, kW, kH);
        {
            juce::ColourGradient tl (Col::pink.withAlpha (0.10f + 0.07f * pulse), 0.0f, 0.0f,
                                     Col::pink.withAlpha (0.0f), 420.0f, 420.0f, true);
            g.setGradientFill (tl);
            g.fillRect (0, 0, kW, kH);
            juce::ColourGradient br (Col::pink.withAlpha (0.09f + 0.07f * (1.0f - pulse)), (float) kW, (float) kH,
                                     Col::pink.withAlpha (0.0f), kW - 460.0f, kH - 460.0f, true);
            g.setGradientFill (br);
            g.fillRect (0, 0, kW, kH);
        }
        for (auto& pt : particles)
        {
            const float a = 0.05f + 0.20f * (0.5f + 0.5f * std::sin (t * 1.6f + pt.phase));
            g.setColour (Col::pinkSoft.withAlpha (a));
            g.fillEllipse (pt.x, pt.y, pt.size, pt.size);
        }

        // ---- waves beside the main title
        drawWaves (g, { 30.0f, 150.0f, 250.0f, 90.0f }, true);
        drawWaves (g, { 620.0f, 150.0f, 250.0f, 90.0f }, false);

        // ---- header: compass + CENTRIX + credit
        {
            auto logoRect = juce::Rectangle<float> (34.0f, 22.0f, 104.0f, 104.0f);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

            juce::Graphics::ScopedSaveState ss (g);
            g.addTransform (juce::AffineTransform::rotation (std::sin (t * 0.5f) * 0.08f,
                                                             logoRect.getCentreX(), logoRect.getCentreY()));
            g.setOpacity (0.30f + 0.45f * pulse);
            g.drawImage (compassGlow, logoRect.expanded (logoRect.getWidth() * 0.125f), juce::RectanglePlacement::stretchToFit);
            g.setOpacity (1.0f);
            g.drawImage (compass, logoRect, juce::RectanglePlacement::stretchToFit);
        }
        {
            const float sweep = std::fmod (t * 90.0f, 900.0f) - 200.0f;
            juce::ColourGradient shimmer (juce::Colour (0xfffff4fa), 150.0f + sweep * 0.0f, 0.0f,
                                          Col::pinkSoft, 420.0f, 0.0f, false);
            shimmer.addColour (0.55, juce::Colour (0xffffd2ec));
            glowText (g, "Centrix", { 156.0f, 20.0f, 460.0f, 84.0f }, F (80.0f, juce::Font::bold),
                      Col::pink, 0.7f + 1.1f * pulse, juce::Justification::centredLeft, &shimmer);
        }
        glowText (g, "Designed by Reyaan.", { 160.0f, 96.0f, 420.0f, 30.0f }, F (26.0f),
                  Col::pinkSoft, 0.5f + 0.9f * pulse, juce::Justification::centredLeft);
        {
            // flourish line under the credit, with a small diamond
            g.setColour (Col::pink.withAlpha (0.45f));
            g.drawLine (160.0f, 132.0f, 560.0f, 132.0f, 1.0f);
            juce::Path d;
            d.addRectangle (-3.5f, -3.5f, 7.0f, 7.0f);
            g.setColour (Col::pink);
            g.fillPath (d, juce::AffineTransform::rotation (juce::MathConstants<float>::pi * 0.25f)
                                .translated (360.0f, 132.0f));
        }

        // ---- zinc logo (right) - big, transparent centre, ringed in pink
        {
            auto z = juce::Rectangle<float> (762.0f, 20.0f, 112.0f, 112.0f);
            for (int k = 5; k >= 1; --k)
            {
                g.setColour (Col::pink.withAlpha (juce::jlimit (0.0f, 1.0f, (0.35f + 0.65f * pulse) * 0.09f)));
                g.drawEllipse (z.expanded (6.0f + (float) k * 1.3f), (float) k * 1.5f);
            }
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.setOpacity (1.0f);
            g.drawImage (zinc, z, juce::RectanglePlacement::stretchToFit);
            g.setColour (Col::pink.withAlpha (0.55f + 0.4f * pulse));
            g.drawEllipse (z.expanded (6.0f), 1.5f);
        }

        // ---- main title (cursive)
        {
            const juce::String from = centsMode ? "Cents" : "Hz";
            const juce::String to   = centsMode ? "Hz" : "Cents";
            const juce::Font tf = F (66.0f, juce::Font::bold);
            const juce::Font mf = F (54.0f);
            const float gap = 20.0f;
            const float w1 = textW (tf, from), w2 = textW (mf, "to"), w3 = textW (tf, to);
            float x = ((float) kW - (w1 + w2 + w3 + gap * 2.0f)) * 0.5f;
            const float y = 142.0f, h = 80.0f;

            const float gl = 0.6f + 1.0f * pulse;
            glowText (g, from, { x, y, w1 + 30.0f, h }, tf, Col::text, 0.3f * gl, juce::Justification::centredLeft);
            x += w1 + gap;
            glowText (g, "to", { x, y + 4.0f, w2 + 30.0f, h }, mf, Col::pinkSoft, 0.2f * gl, juce::Justification::centredLeft);
            x += w2 + gap;
            glowText (g, to, { x, y, w3 + 30.0f, h }, tf, Col::pinkHot, 1.1f * gl, juce::Justification::centredLeft);
        }
        glowText (g, "Frequency Calculator", { 0.0f, 218.0f, (float) kW, 30.0f },
                  F (25.0f), Col::pinkSoft, 0.3f, juce::Justification::centred);
        g.setColour (Col::dim);
        g.setFont (F (16.0f));
        g.drawText (centsMode ? "Convert musical cents to frequency (Hz) with precision."
                              : "Convert a frequency (Hz) to musical cents with precision.",
                    0, 248, kW, 24, juce::Justification::centred);

        // ---- input panels
        drawPanel (g, { 40.0f, 352.0f, 400.0f, 126.0f }, 0.4f + 0.6f * pulse);
        drawPanel (g, { 460.0f, 352.0f, 400.0f, 126.0f }, 0.4f + 0.6f * pulse);

        heading (g, centsMode ? "Cents" : "Target Frequency", 56.0f, 362.0f,
                 centsMode ? "Enter the number of musical cents" : "Frequency you want to land on (Hz)", pulse);
        heading (g, "Reference Frequency", 476.0f, 362.0f, "Base frequency (typically A4 = 440 Hz)", pulse);

        drawField (g, leftField, leftLabel.isBeingEdited() ? 1.0f : 0.3f + 0.4f * pulse, centsMode ? "" : "Hz");
        drawField (g, refField, refLabel.isBeingEdited() ? 1.0f : 0.3f + 0.4f * pulse, "Hz");

        // ---- result panel
        {
            auto r = juce::Rectangle<float> (40.0f, 562.0f, 820.0f, 124.0f);
            drawPanel (g, r, 0.5f + 0.9f * pulse + gFlash * 2.0f);
            heading (g, "Result", 56.0f, 572.0f, centsMode ? "Converted frequency" : "Converted pitch offset", pulse);

            const juce::String num = centsMode ? juce::String (displayed, 2)
                                               : ((displayed > 0.0005f ? "+" : "") + juce::String (displayed, 2));
            const juce::String unit = centsMode ? "Hz" : "cents";
            const juce::Font nf = F (76.0f, juce::Font::bold), uf = F (32.0f, juce::Font::bold);
            const float nw = textW (nf, num), uw = textW (uf, unit);
            const float total = nw + 14.0f + uw;
            const float sx = 490.0f - total * 0.5f;
            const float gl = 1.0f + 1.4f * pulse + gFlash * 3.0f;
            glowText (g, num, { sx, 570.0f, nw + 30.0f, 84.0f }, nf, Col::pinkHot, gl, juce::Justification::centredLeft);
            glowText (g, unit, { sx + nw + 14.0f, 616.0f, uw + 30.0f, 38.0f }, uf, Col::pinkSoft, gl * 0.7f,
                      juce::Justification::centredLeft);

            const double cents = currentCents();
            g.setColour (Col::dim);
            g.setFont (F (16.0f));
            g.drawText ("= " + juce::String (cents / 100.0, 3) + " semitones      |      ratio "
                            + juce::String (std::pow (2.0, cents / 1200.0), 5),
                        r.toNearestInt().withTop (656).withHeight (22), juce::Justification::centred);

            for (int i = 0; i < 9; ++i)
            {
                const float h = 12.0f + 38.0f * (0.5f + 0.5f * std::sin (t * (2.5f + 0.3f * (float) i) + (float) i));
                auto bar = juce::Rectangle<float> (3.0f, h).withCentre ({ 764.0f + (float) i * 10.0f, 622.0f });
                g.setColour (Col::pink.withAlpha (0.20f + 0.30f * pulse));
                g.fillRoundedRectangle (bar, 1.5f);
            }
        }

        // ---- key & scale panel
        drawKeyPanel (g, pulse);

        // ---- formula + quick reference
        drawPanel (g, { 40.0f, 950.0f, 400.0f, 122.0f }, 0.4f + 0.6f * pulse);
        drawPanel (g, { 460.0f, 950.0f, 400.0f, 122.0f }, 0.4f + 0.6f * pulse);

        heading (g, "Formula", 56.0f, 958.0f, "Conversion formula used", pulse);
        g.setColour (Col::field);
        g.fillRoundedRectangle (56.0f, 1010.0f, 368.0f, 34.0f, 8.0f);
        g.setColour (Col::pink.withAlpha (0.3f));
        g.drawRoundedRectangle (56.0f, 1010.0f, 368.0f, 34.0f, 8.0f, 1.0f);
        g.setColour (Col::text);
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 17.0f, juce::Font::plain)));
        g.drawText (centsMode ? "Hz = f_ref x 2^(cents / 1200)" : "cents = 1200 x log2(Hz / f_ref)",
                    68, 1010, 350, 34, juce::Justification::centredLeft);
        g.setColour (Col::dim);
        g.setFont (F (13.0f));
        g.drawText ("f_ref = reference frequency (typically 440 Hz)", 56, 1048, 368, 16, juce::Justification::centredLeft);

        heading (g, "Quick Reference", 476.0f, 958.0f, "Common reference frequencies", pulse);
        g.setColour (Col::dim);
        g.setFont (F (13.0f));
        g.drawText ("Tap one to set the reference. You can also type 432, 444, etc.", 476, 1052, 372, 16,
                    juce::Justification::centredLeft);
    }

    // ------------------------------------------------------------------ input
    void mouseDown (const juce::MouseEvent& e) override
    {
        const bool centsMode = (getChoice ("mode") == 0);
        const auto p = e.position;
        const float mult = e.mods.isShiftDown() ? 10.0f : (e.mods.isAltDown() ? 0.1f : 1.0f);

        auto arrowHit = [&] (juce::Rectangle<float> field, float& dir)
        {
            auto a = field.removeFromRight (40.0f);
            if (! a.contains (p)) return false;
            dir = (p.y < a.getCentreY()) ? 1.0f : -1.0f;
            return true;
        };

        float dir = 0.0f;
        if (arrowHit (leftField, dir)) { step (centsMode ? "cents" : "targetHz", dir * mult); return; }
        if (arrowHit (refField,  dir)) { step ("refHz", dir * mult); return; }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const bool centsMode = (getChoice ("mode") == 0);
        const float mult = e.mods.isShiftDown() ? 10.0f : (e.mods.isAltDown() ? 0.1f : 1.0f);
        const float dir = w.deltaY > 0 ? 1.0f : (w.deltaY < 0 ? -1.0f : 0.0f);
        if (dir == 0.0f) return;
        if (leftField.contains (e.position)) step (centsMode ? "cents" : "targetHz", dir * mult);
        else if (refField.contains (e.position)) step ("refHz", dir * mult);
    }

private:
    // ------------------------------------------------------------------ parameter helpers
    float getRaw (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    int   getChoice (const char* id) const { return (int) std::lround (getRaw (id)); }

    void setFloat (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    }
    void setChoice (const char* id, int idx) { setFloat (id, (float) idx); }
    void setBool (const char* id, bool b)    { setFloat (id, b ? 1.0f : 0.0f); }

    void step (const char* id, float delta)
    {
        setFloat (id, getRaw (id) + delta);
        replayConvert (false);
    }

    static double parseNumber (juce::String s)
    {
        s = s.retainCharacters ("0123456789.-+eE");
        return s.getDoubleValue();
    }

    double currentCents() const
    {
        if (getChoice ("mode") == 0) return getRaw ("cents");
        return Theory::hzToCents (getRaw ("targetHz"), getRaw ("refHz"));
    }

    double currentResult() const
    {
        if (getChoice ("mode") == 0) return Theory::centsToHz (getRaw ("cents"), getRaw ("refHz"));
        return Theory::hzToCents (getRaw ("targetHz"), getRaw ("refHz"));
    }

    void replayConvert (bool restart = true)
    {
        gFlash = 1.0f;
        if (restart) displayed = 0.0f;
    }

    // ------------------------------------------------------------------ timer / polling
    void timerCallback() override
    {
        time += 1.0f / 30.0f;
        gPulse = 0.5f + 0.5f * std::sin (time * 2.2f);
        gFlash *= 0.90f;
        if (gFlash < 0.01f) gFlash = 0.0f;

        for (auto& pt : particles)
        {
            pt.y -= pt.speed;
            pt.x += std::sin (time * 0.6f + pt.phase) * 0.2f;
            if (pt.y < -6.0f) { pt.y = (float) kH + 4.0f; }
        }

        pollParameters (false);

        const float target = (float) currentResult();
        displayed += (target - displayed) * 0.22f;
        if (std::abs (target - displayed) < 0.0005f) displayed = target;

        repaint();
    }

    void pollParameters (bool force)
    {
        const bool centsMode = (getChoice ("mode") == 0);

        tabCentsToHz.setSelected (centsMode);
        tabHzToCents.setSelected (! centsMode);
        convertBtn.setButtonText (centsMode ? "Convert to Hz" : "Convert to Cents");

        const juce::String leftTxt = centsMode ? juce::String (getRaw ("cents"), 2)
                                               : juce::String (getRaw ("targetHz"), 2);
        if ((force || ! leftLabel.isBeingEdited()) && leftLabel.getText() != leftTxt)
            leftLabel.setText (leftTxt, juce::dontSendNotification);

        const juce::String refTxt = juce::String (getRaw ("refHz"), 2);
        if ((force || ! refLabel.isBeingEdited()) && refLabel.getText() != refTxt)
            refLabel.setText (refTxt, juce::dontSendNotification);

        const int root = getChoice ("rootKey");
        for (int i = 0; i < rootBtns.size(); ++i) rootBtns[i]->setSelected (i == root);

        const int sc = getChoice ("scale");
        if (scaleBox.getSelectedItemIndex() != sc)
        {
            updatingUI = true;
            scaleBox.setSelectedItemIndex (sc, juce::dontSendNotification);
            updatingUI = false;
        }

        const bool rel = getRaw ("relative") > 0.5f;
        modeSame.setSelected (! rel);
        modeRelative.setSelected (rel);

        const float ref = getRaw ("refHz");
        for (int i = 0; i < quickBtns.size(); ++i)
            quickBtns[i]->setSelected (std::abs (ref - quickValues[i]) < 0.005f);
    }

    // ------------------------------------------------------------------ drawing helpers
    void drawField (juce::Graphics& g, juce::Rectangle<float> r, float glow, const juce::String& unit)
    {
        g.setColour (Col::field);
        g.fillRoundedRectangle (r, 10.0f);
        glowRect (g, r, 10.0f, Col::pink, glow);

        auto a = r.removeFromRight (40.0f);
        g.setColour (Col::pink.withAlpha (0.25f));
        g.drawLine (a.getX(), r.getY() + 8.0f, a.getX(), r.getBottom() - 8.0f, 1.0f);
        g.setColour (Col::pink);
        juce::Path up, down;
        const float cx = a.getCentreX(), cy = a.getCentreY();
        up.addTriangle (cx - 6.0f, cy - 4.0f, cx + 6.0f, cy - 4.0f, cx, cy - 12.0f);
        down.addTriangle (cx - 6.0f, cy + 4.0f, cx + 6.0f, cy + 4.0f, cx, cy + 12.0f);
        g.fillPath (up);
        g.fillPath (down);

        if (unit.isNotEmpty())
        {
            g.setColour (Col::dim);
            g.setFont (F (18.0f));
            g.drawText (unit, juce::Rectangle<float> (r.getRight() - 80.0f, r.getY(), 64.0f, r.getHeight()),
                        juce::Justification::centredRight);
        }
    }

    void drawWaves (juce::Graphics& g, juce::Rectangle<float> r, bool leftSide)
    {
        const int N = 90;
        const float t = time;
        for (int layer = 0; layer < 3; ++layer)
        {
            juce::Path p;
            const float ph = t * (1.3f + 0.5f * (float) layer) + (float) layer * 1.7f;
            for (int i = 0; i <= N; ++i)
            {
                float u = (float) i / (float) N;
                const float uu = leftSide ? u : 1.0f - u;
                const float env = std::pow (std::sin (juce::MathConstants<float>::pi * uu), 1.6f);
                const float wv = std::sin (uu * 16.0f + ph) * 0.6f + std::sin (uu * 7.0f - ph * 1.3f) * 0.4f;
                const float y = r.getCentreY() + env * r.getHeight() * 0.45f * wv * (1.0f - 0.22f * (float) layer);
                const float x = r.getX() + u * r.getWidth();
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);

                if (layer == 0 && i % 2 == 0)
                {
                    g.setColour (Col::pink.withAlpha (0.07f + 0.07f * gPulse));
                    g.drawLine (x, r.getCentreY() + 34.0f, x, juce::jmin (y, r.getCentreY() + 34.0f), 1.0f);
                }
            }
            g.setColour (Col::pink.withAlpha (0.07f + 0.04f * gPulse));
            g.strokePath (p, juce::PathStrokeType (5.0f - (float) layer * 1.4f));
            g.setColour (Col::pinkSoft.withAlpha (0.75f - 0.2f * (float) layer));
            g.strokePath (p, juce::PathStrokeType (1.3f));
        }
    }

    void drawKeyPanel (juce::Graphics& g, float pulse)
    {
        auto r = juce::Rectangle<float> (40.0f, 696.0f, 820.0f, 240.0f);
        drawPanel (g, r, 0.4f + 0.7f * pulse);
        heading (g, "Key & Scale", 56.0f, 706.0f, "Where your root and scale land after the pitch change", pulse);

        g.setColour (Col::pinkSoft);
        g.setFont (F (19.0f));
        g.drawText ("Root Key", 56, 756, 104, 30, juce::Justification::centredLeft);
        g.drawText ("Scale", 56, 808, 104, 30, juce::Justification::centredLeft);
        g.drawText ("Final Scale", 436, 808, 126, 30, juce::Justification::centredLeft);

        const int root = getChoice ("rootKey"), sc = getChoice ("scale");
        const bool rel = getRaw ("relative") > 0.5f;
        const auto sh = Theory::shiftKey (root, sc, currentCents());
        const auto& scales = Theory::scales();

        const juce::String original = Theory::keyName (root) + " " + scales[(size_t) sc].name;
        const juce::String shifted  = Theory::keyName (sh.newRoot) + " " + scales[(size_t) sc].name;
        const juce::String relName  = Theory::keyName (sh.relRoot) + " " + scales[(size_t) sh.relScale].name;

        struct Col3 { float x, w; const char* label; const juce::String* name; bool hot; };
        const Col3 cols[] = { { 60.0f,  230.0f, "Original",            &original, false },
                              { 330.0f, 250.0f, "After pitch change",  &shifted,  ! rel },
                              { 620.0f, 220.0f, "Relative",            &relName,  rel } };

        for (auto& c : cols)
        {
            g.setColour (c.hot ? Col::pinkSoft : Col::dim);
            g.setFont (F (14.0f));
            g.drawText (c.label, (int) c.x, 850, (int) c.w, 18, juce::Justification::centredLeft);
            glowText (g, *c.name, { c.x, 866.0f, c.w, 38.0f }, F (c.hot ? 32.0f : 28.0f, juce::Font::bold),
                      c.hot ? Col::pinkHot : Col::text, c.hot ? 0.9f + 1.4f * pulse : 0.0f,
                      juce::Justification::centredLeft);
        }

        g.setColour (Col::pink.withAlpha (0.8f));
        for (float ax : { 296.0f, 588.0f })
        {
            juce::Path a;
            a.startNewSubPath (ax, 884.0f);
            a.lineTo (ax + 22.0f, 884.0f);
            a.startNewSubPath (ax + 15.0f, 877.0f);
            a.lineTo (ax + 22.0f, 884.0f);
            a.lineTo (ax + 15.0f, 891.0f);
            g.strokePath (a, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const int fRoot = rel ? sh.relRoot : sh.newRoot;
        const int fScale = rel ? sh.relScale : sc;
        g.setColour (Col::text);
        g.setFont (F (16.0f));
        g.drawText ("Final notes:  " + Theory::scaleNotes (fRoot, fScale), 60, 908, 520, 20,
                    juce::Justification::centredLeft);
        g.setColour (Col::dim);
        juce::String res = (sh.semitones >= 0 ? "+" : "") + juce::String (sh.semitones) + " st";
        res << "   " << (sh.residualCents >= 0 ? "+" : "") << juce::String (sh.residualCents, 2) << " cents detune";
        g.drawText (res, 520, 908, 320, 20, juce::Justification::centredRight);
    }

    // ------------------------------------------------------------------ members
    CentsHzAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    PinkLookAndFeel laf;

    juce::Image compass, compassGlow, zinc;
    std::vector<Particle> particles;

    GlowButton tabCentsToHz { "Cents > Hz", GlowButton::Tab };
    GlowButton tabHzToCents { "Hz > Cents", GlowButton::Tab };
    GlowButton convertBtn   { "Convert to Hz", GlowButton::Pill };
    GlowButton modeSame     { "Same Scale", GlowButton::Key };
    GlowButton modeRelative { "Relative", GlowButton::Key };
    juce::OwnedArray<GlowButton> rootBtns, quickBtns;
    juce::ComboBox scaleBox;
    juce::Label leftLabel, refLabel;
    juce::Rectangle<float> leftField, refField;

    const float quickValues[4] = { 440.0f, 220.0f, 880.0f, 261.63f };

    float time = 0.0f;
    float displayed = 0.0f;
    bool updatingUI = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContent)
};

// ======================================================================================
CentsHzAudioProcessorEditor::CentsHzAudioProcessorEditor (CentsHzAudioProcessor& p)
    : AudioProcessorEditor (&p)
{
    content = std::make_unique<MainContent> (p);
    addAndMakeVisible (*content);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setResizeLimits (450, (int) (450.0 * kH / kW), 1350, (int) (1350.0 * kH / kW));
    setSize (720, (int) (720.0 * kH / kW));
}

CentsHzAudioProcessorEditor::~CentsHzAudioProcessorEditor() = default;

void CentsHzAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    content->setBounds (0, 0, kW, kH);
    content->setTransform (juce::AffineTransform::scale (s));
}
