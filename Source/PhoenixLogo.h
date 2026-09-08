#pragma once

#include <juce_graphics/juce_graphics.h>
#include <vector>

// Vector stand-in for the phoenix mark (so the repo needs no artwork files). Drawn in a unit square
// as separate shapes, so overlapping feathers merge instead of cancelling each other out.
namespace ember
{
    inline std::vector<juce::Path> phoenixShapes()
    {
        using P = juce::Point<float>;
        std::vector<juce::Path> shapes;

        auto feather = [&] (P s, float angleDeg, float len, float bend, float width, bool mirror)
        {
            const float a = juce::degreesToRadians (angleDeg);
            const P dir { std::cos (a), -std::sin (a) };
            const P perp { -dir.y, dir.x };
            juce::Path path;
            path.startNewSubPath (s);
            path.quadraticTo (s + dir * (len * 0.55f) + perp * bend, s + dir * len);
            path.quadraticTo (s + dir * (len * 0.5f) + perp * (bend + width), s);
            path.closeSubPath();
            if (mirror)
                path.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (1.0f, 0.0f));
            shapes.push_back (path);
        };

        // wings: staggered crescent blades; the outer ones start beside the body and sweep up around the head
        struct Blade { float rx, ry, angle, len, bend; };
        const Blade blades[] {
            { 0.38f, 0.52f,  98.0f, 0.52f, 0.30f }, { 0.41f, 0.53f, 112.0f, 0.50f, 0.26f },
            { 0.44f, 0.54f, 128.0f, 0.48f, 0.22f }, { 0.46f, 0.55f, 146.0f, 0.45f, 0.18f },
            { 0.47f, 0.56f, 164.0f, 0.40f, 0.14f }, { 0.47f, 0.57f, 184.0f, 0.34f, 0.10f },
            { 0.47f, 0.58f, 204.0f, 0.27f, 0.06f } };
        for (bool mirror : { false, true })
            for (const auto& bl : blades)
                feather ({ bl.rx, bl.ry }, bl.angle, bl.len, bl.len * bl.bend, bl.len * 0.30f, mirror);

        // tail
        const float tailAngles[]  { 236.0f, 253.0f, 270.0f, 287.0f, 304.0f };
        const float tailLengths[] { 0.26f, 0.32f, 0.36f, 0.32f, 0.26f };
        for (size_t i = 0; i < std::size (tailAngles); ++i)
            feather ({ 0.5f, 0.62f }, tailAngles[i], tailLengths[i], 0.0f, tailLengths[i] * 0.34f, false);

        // body, head, beak and crest
        juce::Path body;
        body.startNewSubPath (0.5f, 0.27f);
        body.cubicTo (0.62f, 0.40f, 0.60f, 0.64f, 0.5f, 0.70f);
        body.cubicTo (0.40f, 0.64f, 0.38f, 0.40f, 0.5f, 0.27f);
        body.closeSubPath();
        shapes.push_back (body);

        juce::Path head;
        head.addEllipse (0.455f, 0.205f, 0.09f, 0.085f);
        head.addTriangle (0.53f, 0.225f, 0.62f, 0.265f, 0.53f, 0.275f);
        head.addTriangle (0.47f, 0.215f, 0.43f, 0.15f, 0.51f, 0.205f);
        shapes.push_back (head);
        return shapes;
    }

    inline void drawPhoenix (juce::Graphics& g, juce::Rectangle<float> area, float alpha = 1.0f)
    {
        static const auto shapes = phoenixShapes();
        const auto t = juce::AffineTransform::scale (area.getWidth(), area.getHeight()).translated (area.getX(), area.getY());

        g.beginTransparencyLayer (alpha);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeef2f6), area.getCentreX(), area.getY(),
                                                 juce::Colour (0xff7d8794), area.getCentreX(), area.getBottom(), false));
        for (const auto& s : shapes)
            g.fillPath (s, t);
        g.endTransparencyLayer();
    }
}
