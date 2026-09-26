using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Threading;

namespace ModularVoiceStudio.App.Controls;

/// <summary>
/// Real-time visual DSP display with dedicated visualizers for Pitch, Reverb, Delay, Robot, Doubler, and Saturation.
/// </summary>
public class DSPVisualizerControl : Control
{
    public static readonly StyledProperty<string> EffectTypeProperty =
        AvaloniaProperty.Register<DSPVisualizerControl, string>(nameof(EffectType), string.Empty);

    public static readonly StyledProperty<double> Param1Property =
        AvaloniaProperty.Register<DSPVisualizerControl, double>(nameof(Param1), 0.0);

    public static readonly StyledProperty<double> Param2Property =
        AvaloniaProperty.Register<DSPVisualizerControl, double>(nameof(Param2), 0.0);

    public static readonly StyledProperty<double> Param3Property =
        AvaloniaProperty.Register<DSPVisualizerControl, double>(nameof(Param3), 0.0);

    public string EffectType
    {
        get => GetValue(EffectTypeProperty);
        set => SetValue(EffectTypeProperty, value);
    }

    public double Param1
    {
        get => GetValue(Param1Property);
        set => SetValue(Param1Property, value);
    }

    public double Param2
    {
        get => GetValue(Param2Property);
        set => SetValue(Param2Property, value);
    }

    public double Param3
    {
        get => GetValue(Param3Property);
        set => SetValue(Param3Property, value);
    }

    private readonly DispatcherTimer _animTimer;
    private double _phase;

    static DSPVisualizerControl()
    {
        AffectsRender<DSPVisualizerControl>(EffectTypeProperty, Param1Property, Param2Property, Param3Property);
    }

    public DSPVisualizerControl()
    {
        _animTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(33) // ~30 FPS
        };
        _animTimer.Tick += (s, e) =>
        {
            _phase += 0.08;
            if (_phase > Math.PI * 2) _phase -= Math.PI * 2;
            InvalidateVisual();
        };

        AttachedToVisualTree += (s, e) => _animTimer.Start();
        DetachedFromVisualTree += (s, e) => _animTimer.Stop();
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);

        double w = Bounds.Width;
        double h = Bounds.Height;
        if (w <= 0 || h <= 0) return;

        // Background Slate Screen
        var bgBrush = new SolidColorBrush(Color.Parse("#0F1318"));
        var borderPen = new Pen(new SolidColorBrush(Color.Parse("#202733")), 1.0);
        context.DrawRectangle(bgBrush, borderPen, new Rect(0, 0, w, h), 4, 4);

        // Subtle Grid Lines
        var gridPen = new Pen(new SolidColorBrush(Color.Parse("#171D26")), 1.0);
        for (double x = 20; x < w; x += 25)
            context.DrawLine(gridPen, new Point(x, 0), new Point(x, h));
        for (double y = 15; y < h; y += 20)
            context.DrawLine(gridPen, new Point(0, y), new Point(w, y));

        string norm = EffectType.ToLowerInvariant();
        if (norm.Contains("pitch"))
        {
            DrawPitchVisualizer(context, w, h);
        }
        else if (norm.Contains("reverb"))
        {
            DrawReverbVisualizer(context, w, h);
        }
        else if (norm.Contains("delay"))
        {
            DrawDelayVisualizer(context, w, h);
        }
        else if (norm.Contains("robot"))
        {
            DrawRobotVisualizer(context, w, h);
        }
        else if (norm.Contains("double"))
        {
            DrawDoublerVisualizer(context, w, h);
        }
        else if (norm.Contains("saturat"))
        {
            DrawSaturationVisualizer(context, w, h);
        }
        else
        {
            DrawGenericSpectrum(context, w, h);
        }
    }

    // 1. Pitch Shifter Visualizer
    private void DrawPitchVisualizer(DrawingContext context, double w, double h)
    {
        double semitones = Param1; // -12 to +12
        double midY = h / 2.0;

        // Center 0 semitone reference line
        var refPen = new Pen(new SolidColorBrush(Color.Parse("#384252")), 1.0, new DashStyle(new double[] { 4, 4 }, 0));
        context.DrawLine(refPen, new Point(10, midY), new Point(w - 10, midY));

        // Pitch Shift Offset Target
        double targetY = midY - (semitones / 12.0) * (h * 0.38);
        var targetPen = new Pen(new SolidColorBrush(Color.Parse("#06B6D4")), 1.5);
        context.DrawLine(targetPen, new Point(10, targetY), new Point(w - 10, targetY));

        // Formant Shift Waveform
        var waveGeometry = new StreamGeometry();
        using (var ctx = waveGeometry.Open())
        {
            ctx.BeginFigure(new Point(10, midY), false);
            for (double x = 10; x <= w - 10; x += 4)
            {
                double normX = (x - 10) / (w - 20);
                double freq = 3.0 + (semitones * 0.15);
                double y = targetY + Math.Sin(normX * Math.PI * freq + _phase) * (h * 0.22);
                ctx.LineTo(new Point(x, y));
            }
            ctx.EndFigure(false);
        }
        var wavePen = new Pen(new SolidColorBrush(Color.Parse("#38BDF8")), 2.0);
        context.DrawGeometry(null, wavePen, waveGeometry);

        // Readout text
        DrawReadout(context, $"PITCH SHIFT: {semitones:+0.0;-0.0;0.0} SEMITONES", new Point(12, 8));
    }

    // 2. Reverb 3D Room Visualizer
    private void DrawReverbVisualizer(DrawingContext context, double w, double h)
    {
        double roomSize = Math.Clamp(Param1, 0.1, 1.0); // 0..1
        double damping = Math.Clamp(Param2, 0.0, 1.0);

        double cx = w / 2.0;
        double cy = h / 2.0;
        double maxDim = Math.Min(w, h) * 0.45;
        double size = maxDim * (0.35 + roomSize * 0.65);

        // Isometric 3D Room Wireframe
        Point fTopLeft = new Point(cx - size * 0.8, cy - size * 0.5);
        Point fTopRight = new Point(cx + size * 0.8, cy - size * 0.5);
        Point fBotLeft = new Point(cx - size * 0.8, cy + size * 0.5);
        Point fBotRight = new Point(cx + size * 0.8, cy + size * 0.5);

        Point bTopLeft = new Point(cx - size * 0.4, cy - size * 0.8);
        Point bTopRight = new Point(cx + size * 0.4, cy - size * 0.8);
        Point bBotLeft = new Point(cx - size * 0.4, cy + size * 0.2);
        Point bBotRight = new Point(cx + size * 0.4, cy + size * 0.2);

        var roomPen = new Pen(new SolidColorBrush(Color.Parse("#3B82F6")), 1.5);
        var backPen = new Pen(new SolidColorBrush(Color.Parse("#1D4ED8")), 1.0, new DashStyle(new double[] { 2, 2 }, 0));

        // Back wall
        context.DrawLine(backPen, bTopLeft, bTopRight);
        context.DrawLine(backPen, bTopRight, bBotRight);
        context.DrawLine(backPen, bBotRight, bBotLeft);
        context.DrawLine(backPen, bBotLeft, bTopLeft);

        // Connecting struts
        context.DrawLine(roomPen, fTopLeft, bTopLeft);
        context.DrawLine(roomPen, fTopRight, bTopRight);
        context.DrawLine(roomPen, fBotLeft, bBotLeft);
        context.DrawLine(roomPen, fBotRight, bBotRight);

        // Front wall
        context.DrawLine(roomPen, fTopLeft, fTopRight);
        context.DrawLine(roomPen, fTopRight, fBotRight);
        context.DrawLine(roomPen, fBotRight, fBotLeft);
        context.DrawLine(roomPen, fBotLeft, fTopLeft);

        // Animated Acoustic Particle Reflections
        int numParticles = (int)(16 + roomSize * 24);
        var particleBrush = new SolidColorBrush(Color.Parse("#60A5FA"));
        for (int i = 0; i < numParticles; i++)
        {
            double pPhase = _phase * 1.5 + (i * 1.37);
            double px = cx + Math.Sin(pPhase) * (size * 0.65);
            double py = cy + Math.Cos(pPhase * 0.8) * (size * 0.4);
            double pr = 1.5 + Math.Sin(pPhase * 2.0) * 0.8;
            context.DrawEllipse(particleBrush, null, new Point(px, py), pr, pr);
        }

        DrawReadout(context, $"SPACE: {roomSize * 100:0}% • DAMPING: {damping * 100:0}%", new Point(12, 8));
    }

    // 3. Stereo Delay Multi-Tap Visualizer
    private void DrawDelayVisualizer(DrawingContext context, double w, double h)
    {
        double timeMs = Math.Clamp(Param1, 10.0, 1000.0);
        double feedback = Math.Clamp(Param2, 0.0, 90.0) / 100.0;

        double baseY = h - 16;
        double startX = 20;
        double availW = w - 40;

        // Baseline
        var axisPen = new Pen(new SolidColorBrush(Color.Parse("#2D3748")), 1.0);
        context.DrawLine(axisPen, new Point(10, baseY), new Point(w - 10, baseY));

        // Dry Initial Impulse
        var dryBrush = new SolidColorBrush(Color.Parse("#FFFFFF"));
        context.DrawRectangle(dryBrush, null, new Rect(startX, baseY - (h * 0.7), 4, h * 0.7));

        // Echo Repeat Taps
        int maxTaps = 8;
        double tapSpacing = Math.Clamp((timeMs / 1000.0) * (availW / 3.5), 18.0, availW / 4.0);
        double currentHeight = h * 0.7 * feedback;

        var tapBrushL = new SolidColorBrush(Color.Parse("#38BDF8"));
        var tapBrushR = new SolidColorBrush(Color.Parse("#818CF8"));

        for (int i = 1; i <= maxTaps; i++)
        {
            double tapX = startX + (i * tapSpacing);
            if (tapX > w - 15) break;

            double tapH = currentHeight * Math.Pow(feedback, i - 1);
            if (tapH < 3) break;

            var brush = (i % 2 == 1) ? tapBrushL : tapBrushR;
            context.DrawRectangle(brush, null, new Rect(tapX, baseY - tapH, 3.5, tapH), 1.5, 1.5);
        }

        DrawReadout(context, $"DELAY: {timeMs:0} MS • FEEDBACK: {feedback * 100:0}%", new Point(12, 8));
    }

    // 4. Robot Voice Ring-Modulation Scope
    private void DrawRobotVisualizer(DrawingContext context, double w, double h)
    {
        double freq = Math.Clamp(Param1, 30.0, 800.0);
        double crush = Math.Clamp(Param2, 0.0, 100.0);

        double midY = h / 2.0;
        var ringPen = new Pen(new SolidColorBrush(Color.Parse("#10B981")), 2.0);

        var geom = new StreamGeometry();
        using (var ctx = geom.Open())
        {
            ctx.BeginFigure(new Point(10, midY), false);
            double step = crush > 10.0 ? 8.0 : 3.0; // Bitcrush quantization step

            for (double x = 10; x <= w - 10; x += step)
            {
                double normX = (x - 10) / (w - 20);
                double carrier = Math.Sin(normX * (freq * 0.08) + _phase * 3.0);
                double modulator = Math.Sin(normX * Math.PI * 2.0 + _phase);
                double sample = carrier * modulator;

                if (crush > 1.0)
                {
                    double levels = Math.Max(2.0, 16.0 - (crush * 0.14));
                    sample = Math.Round(sample * levels) / levels;
                }

                double y = midY + sample * (h * 0.35);
                ctx.LineTo(new Point(x, y));
            }
            ctx.EndFigure(false);
        }

        context.DrawGeometry(null, ringPen, geom);
        DrawReadout(context, $"CARRIER: {freq:0} HZ • BITCRUSH: {crush:0}%", new Point(12, 8));
    }

    // 5. Vocal Doubler Stereo Lissajous Scope
    private void DrawDoublerVisualizer(DrawingContext context, double w, double h)
    {
        double spread = Math.Clamp(Param1, 0.0, 100.0) / 100.0;
        double detune = Math.Clamp(Param2, 0.0, 50.0);

        double cx = w / 2.0;
        double cy = h / 2.0;
        double rx = (w * 0.38) * (0.3 + spread * 0.7);
        double ry = h * 0.35;

        // Center Axis
        var axisPen = new Pen(new SolidColorBrush(Color.Parse("#252E3B")), 1.0);
        context.DrawLine(axisPen, new Point(cx, 10), new Point(cx, h - 10));
        context.DrawLine(axisPen, new Point(10, cy), new Point(w - 10, cy));

        // Lissajous Stereo Field
        var lissajousPen = new Pen(new SolidColorBrush(Color.Parse("#A855F7")), 1.8);
        var geom = new StreamGeometry();
        using (var ctx = geom.Open())
        {
            double initX = cx + Math.Sin(_phase) * rx;
            double initY = cy + Math.Cos(_phase * (1.0 + detune * 0.01)) * ry;
            ctx.BeginFigure(new Point(initX, initY), false);

            for (int i = 1; i <= 64; i++)
            {
                double t = _phase + (i * 0.12);
                double x = cx + Math.Sin(t * 1.0) * rx;
                double y = cy + Math.Cos(t * (1.0 + detune * 0.01)) * ry;
                ctx.LineTo(new Point(x, y));
            }
            ctx.EndFigure(false);
        }
        context.DrawGeometry(null, lissajousPen, geom);

        DrawReadout(context, $"STEREO SPREAD: {spread * 100:0}% • DETUNE: {detune:0} CT", new Point(12, 8));
    }

    // 6. Saturation Transfer Function Curve
    private void DrawSaturationVisualizer(DrawingContext context, double w, double h)
    {
        double drive = Math.Clamp(Param1, 0.0, 100.0);
        double warmth = Math.Clamp(Param2, 0.0, 100.0);

        double midX = w / 2.0;
        double midY = h / 2.0;

        // Diagonal Linear Unity Reference
        var refPen = new Pen(new SolidColorBrush(Color.Parse("#283240")), 1.0, new DashStyle(new double[] { 3, 3 }, 0));
        context.DrawLine(refPen, new Point(15, h - 15), new Point(w - 15, 15));

        // Tanh Drive Soft-Clipping Curve
        double driveGain = 1.0 + (drive * 0.07);
        double bias = warmth * 0.002;

        var curveGeom = new StreamGeometry();
        using (var ctx = curveGeom.Open())
        {
            ctx.BeginFigure(new Point(15, h - 15), false);
            for (double x = 15; x <= w - 15; x += 4)
            {
                double inNorm = ((x - midX) / (w * 0.38));
                double outNorm = Math.Tanh((inNorm + bias) * driveGain);
                double y = midY - (outNorm * (h * 0.38));
                ctx.LineTo(new Point(x, Math.Clamp(y, 8, h - 8)));
            }
            ctx.EndFigure(false);
        }

        var curvePen = new Pen(new SolidColorBrush(Color.Parse("#F59E0B")), 2.0);
        context.DrawGeometry(null, curvePen, curveGeom);

        DrawReadout(context, $"DRIVE: {drive:0}% • TUBE WARMTH: {warmth:0}%", new Point(12, 8));
    }

    private void DrawGenericSpectrum(DrawingContext context, double w, double h)
    {
        double midY = h / 2.0;
        var geom = new StreamGeometry();
        using (var ctx = geom.Open())
        {
            ctx.BeginFigure(new Point(10, midY), false);
            for (double x = 10; x <= w - 10; x += 4)
            {
                double normX = (x - 10) / (w - 20);
                double y = midY + Math.Sin(normX * Math.PI * 4.0 + _phase) * (h * 0.25);
                ctx.LineTo(new Point(x, y));
            }
            ctx.EndFigure(false);
        }
        var pen = new Pen(new SolidColorBrush(Color.Parse("#38BDF8")), 1.5);
        context.DrawGeometry(null, pen, geom);
        DrawReadout(context, "ACTIVE DSP EFFECT MONITOR", new Point(12, 8));
    }

    private static void DrawReadout(DrawingContext context, string text, Point pt)
    {
        var formatted = new FormattedText(
            text,
            System.Globalization.CultureInfo.InvariantCulture,
            FlowDirection.LeftToRight,
            new Typeface("Inter, Segoe UI, sans-serif", FontStyle.Normal, FontWeight.Bold),
            8.0,
            new SolidColorBrush(Color.Parse("#94A3B8")));

        context.DrawText(formatted, pt);
    }
}
