using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;

namespace ModularVoiceStudio.App.Controls;

public class KnobControl : Control
{
    public static readonly StyledProperty<double> ValueProperty =
        AvaloniaProperty.Register<KnobControl, double>(nameof(Value), 0.0);

    public static readonly StyledProperty<double> MinimumProperty =
        AvaloniaProperty.Register<KnobControl, double>(nameof(Minimum), 0.0);

    public static readonly StyledProperty<double> MaximumProperty =
        AvaloniaProperty.Register<KnobControl, double>(nameof(Maximum), 1.0);

    public static readonly StyledProperty<double> DefaultValueProperty =
        AvaloniaProperty.Register<KnobControl, double>(nameof(DefaultValue), 0.0);

    public static readonly StyledProperty<string> UnitProperty =
        AvaloniaProperty.Register<KnobControl, string>(nameof(Unit), string.Empty);

    public static readonly StyledProperty<string> KnobLabelProperty =
        AvaloniaProperty.Register<KnobControl, string>(nameof(KnobLabel), string.Empty);

    public static readonly StyledProperty<IBrush> AccentBrushProperty =
        AvaloniaProperty.Register<KnobControl, IBrush>(nameof(AccentBrush), new SolidColorBrush(Color.Parse("#38BDF8")));

    public double Value
    {
        get => GetValue(ValueProperty);
        set => SetValue(ValueProperty, Math.Clamp(value, Minimum, Maximum));
    }

    public double Minimum
    {
        get => GetValue(MinimumProperty);
        set => SetValue(MinimumProperty, value);
    }

    public double Maximum
    {
        get => GetValue(MaximumProperty);
        set => SetValue(MaximumProperty, value);
    }

    public double DefaultValue
    {
        get => GetValue(DefaultValueProperty);
        set => SetValue(DefaultValueProperty, value);
    }

    public string Unit
    {
        get => GetValue(UnitProperty);
        set => SetValue(UnitProperty, value);
    }

    public string KnobLabel
    {
        get => GetValue(KnobLabelProperty);
        set => SetValue(KnobLabelProperty, value);
    }

    public IBrush AccentBrush
    {
        get => GetValue(AccentBrushProperty);
        set => SetValue(AccentBrushProperty, value);
    }

    public event EventHandler<double>? ValueChanged;

    private bool _isDragging;
    private Point _lastDragPoint;

    static KnobControl()
    {
        AffectsRender<KnobControl>(ValueProperty, MinimumProperty, MaximumProperty, AccentBrushProperty, KnobLabelProperty, UnitProperty);
    }

    public KnobControl()
    {
        ClipToBounds = false;
        Cursor = new Cursor(StandardCursorType.SizeNorthSouth);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        base.OnPointerPressed(e);
        if (e.ClickCount >= 2)
        {
            Value = DefaultValue;
            ValueChanged?.Invoke(this, Value);
            e.Handled = true;
            return;
        }

        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
        {
            _isDragging = true;
            _lastDragPoint = e.GetPosition(this);
            e.Pointer.Capture(this);
            e.Handled = true;
        }
    }

    protected override void OnPointerMoved(PointerEventArgs e)
    {
        base.OnPointerMoved(e);
        if (!_isDragging) return;

        var currentPoint = e.GetPosition(this);
        double deltaY = _lastDragPoint.Y - currentPoint.Y; // Dragging UP increases value
        _lastDragPoint = currentPoint;

        double range = Maximum - Minimum;
        if (range <= 0.0) return;

        double sensitivity = (e.KeyModifiers.HasFlag(KeyModifiers.Shift)) ? 0.001 : 0.005;
        double change = deltaY * range * sensitivity;

        Value = Math.Clamp(Value + change, Minimum, Maximum);
        ValueChanged?.Invoke(this, Value);
        e.Handled = true;
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        base.OnPointerReleased(e);
        if (_isDragging)
        {
            _isDragging = false;
            e.Pointer.Capture(null);
            e.Handled = true;
        }
    }

    protected override void OnPointerWheelChanged(PointerWheelEventArgs e)
    {
        base.OnPointerWheelChanged(e);
        double range = Maximum - Minimum;
        if (range <= 0.0) return;

        double change = e.Delta.Y * (range * 0.02);
        Value = Math.Clamp(Value + change, Minimum, Maximum);
        ValueChanged?.Invoke(this, Value);
        e.Handled = true;
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);

        double w = Bounds.Width;
        double h = Bounds.Height;
        if (w <= 0 || h <= 0) return;

        double size = Math.Min(w, h);
        double radius = (size / 2.0) - 8.0;
        if (radius <= 6) return;

        Point center = new Point(w / 2.0, (h / 2.0) - (string.IsNullOrEmpty(KnobLabel) ? 0 : 6));

        // Start angle: -135 deg (bottom left), End angle: +135 deg (bottom right). Total span = 270 deg.
        const double startAngle = 135.0;
        const double sweepAngle = 270.0;

        double norm = (Maximum > Minimum) ? Math.Clamp((Value - Minimum) / (Maximum - Minimum), 0.0, 1.0) : 0.0;
        double currentAngleDeg = startAngle + norm * sweepAngle;

        // 1. Background Arc Track (Dark Slate)
        var trackPen = new Pen(new SolidColorBrush(Color.Parse("#252A34")), 4, lineCap: PenLineCap.Round);
        DrawArc(context, trackPen, center, radius, startAngle, sweepAngle);

        // 2. Active Illuminated Arc
        if (norm > 0.001)
        {
            var activePen = new Pen(AccentBrush, 4, lineCap: PenLineCap.Round);
            DrawArc(context, activePen, center, radius, startAngle, norm * sweepAngle);
        }

        // 3. Dial Body (Metallic Circular Gradient Plate)
        double bodyRadius = radius - 5.0;
        var bodyBrush = new LinearGradientBrush
        {
            StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
            EndPoint = new RelativePoint(1, 1, RelativeUnit.Relative),
            GradientStops =
            {
                new GradientStop(Color.Parse("#2B323D"), 0.0),
                new GradientStop(Color.Parse("#1A1D24"), 1.0)
            }
        };
        var bodyBorderPen = new Pen(new SolidColorBrush(Color.Parse("#3D4554")), 1.5);
        context.DrawEllipse(bodyBrush, bodyBorderPen, center, bodyRadius, bodyRadius);

        // 4. Indicator Needle / Dot
        double rad = currentAngleDeg * (Math.PI / 180.0);
        double indDist = bodyRadius - 5.0;
        Point pointerEnd = new Point(center.X + indDist * Math.Cos(rad), center.Y + indDist * Math.Sin(rad));
        Point pointerStart = new Point(center.X + (indDist - 6.0) * Math.Cos(rad), center.Y + (indDist - 6.0) * Math.Sin(rad));

        var needlePen = new Pen(AccentBrush, 2.5, lineCap: PenLineCap.Round);
        context.DrawLine(needlePen, pointerStart, pointerEnd);

        // 5. Center Value Readout
        string formattedVal = FormatValue(Value, Unit);
        var valText = new FormattedText(
            formattedVal,
            System.Globalization.CultureInfo.InvariantCulture,
            FlowDirection.LeftToRight,
            new Typeface("Inter, Segoe UI, sans-serif", FontStyle.Normal, FontWeight.Bold),
            9.0,
            new SolidColorBrush(Color.Parse("#FFFFFF")));

        context.DrawText(valText, new Point(center.X - (valText.Width / 2.0), center.Y - (valText.Height / 2.0)));

        // 6. Optional Label below knob
        if (!string.IsNullOrEmpty(KnobLabel))
        {
            var labelText = new FormattedText(
                KnobLabel.ToUpperInvariant(),
                System.Globalization.CultureInfo.InvariantCulture,
                FlowDirection.LeftToRight,
                new Typeface("Inter, Segoe UI, sans-serif", FontStyle.Normal, FontWeight.ExtraBold),
                7.5,
                new SolidColorBrush(Color.Parse("#8E99A8")));

            context.DrawText(labelText, new Point(center.X - (labelText.Width / 2.0), center.Y + radius + 3.0));
        }
    }

    private static void DrawArc(DrawingContext context, Pen pen, Point center, double radius, double startAngleDeg, double sweepAngleDeg)
    {
        if (sweepAngleDeg <= 0.0) return;

        const int segments = 32;
        var geometry = new StreamGeometry();
        using (var ctx = geometry.Open())
        {
            double startRad = startAngleDeg * (Math.PI / 180.0);
            Point firstPt = new Point(center.X + radius * Math.Cos(startRad), center.Y + radius * Math.Sin(startRad));
            ctx.BeginFigure(firstPt, false);

            for (int i = 1; i <= segments; i++)
            {
                double angleDeg = startAngleDeg + (sweepAngleDeg * (i / (double)segments));
                double angleRad = angleDeg * (Math.PI / 180.0);
                Point pt = new Point(center.X + radius * Math.Cos(angleRad), center.Y + radius * Math.Sin(angleRad));
                ctx.LineTo(pt);
            }
            ctx.EndFigure(false);
        }

        context.DrawGeometry(null, pen, geometry);
    }

    private static string FormatValue(double val, string unit)
    {
        string u = string.IsNullOrWhiteSpace(unit) ? "" : " " + unit;
        if (Math.Abs(val - Math.Round(val)) < 0.01)
            return $"{(int)Math.Round(val)}{u}";
        return $"{val:0.0}{u}";
    }
}
