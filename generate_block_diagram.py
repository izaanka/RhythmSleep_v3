import os
import sys
from PIL import Image, ImageDraw, ImageFont

def generate_svg():
    svg_content = """<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1400 950" width="1400" height="950" style="background:#ffffff; font-family:Arial, Helvetica, sans-serif;">
  <defs>
    <marker id="arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
      <path d="M 0 1 L 10 5 L 0 9 z" fill="#000000" />
    </marker>
    <style>
      .title { font-size: 26px; font-weight: bold; fill: #000000; text-anchor: middle; }
      .subtitle { font-size: 15px; fill: #333333; text-anchor: middle; }
      .group-title { font-size: 16px; font-weight: bold; fill: #000000; }
      .node-title { font-size: 14px; font-weight: bold; fill: #000000; text-anchor: middle; }
      .node-desc { font-size: 12px; fill: #222222; text-anchor: middle; }
      .group-box { fill: #fafafa; stroke: #000000; stroke-width: 1.5; stroke-dasharray: 4,4; rx: 8; ry: 8; }
      .node-box { fill: #ffffff; stroke: #000000; stroke-width: 1.8; rx: 6; ry: 6; }
      .highlight-box { fill: #f5f5f5; stroke: #000000; stroke-width: 2.2; rx: 6; ry: 6; }
      .flow-line { stroke: #000000; stroke-width: 2; fill: none; marker-end: url(#arrow); }
      .tag { font-size: 11px; font-weight: bold; fill: #000000; }
    </style>
  </defs>

  <!-- Title Banner -->
  <text x="700" y="45" class="title">RHYTHMSLEEP v3 — FUNCTIONAL BLOCK DIAGRAM</text>
  <text x="700" y="70" class="subtitle">Distributed Architecture: Wearable Neural Edge Node (ESP32-S3) &amp; Telemetry Gateway Server (Arduino UNO Q)</text>
  <line x1="50" y1="85" x2="1350" y2="85" stroke="#000000" stroke-width="1.5" />

  <!-- SECTION 1: SIGNAL ACQUISITION -->
  <rect x="50" y="105" width="280" height="380" class="group-box" />
  <text x="70" y="130" class="group-title">1. SIGNAL ACQUISITION</text>

  <rect x="70" y="150" width="240" height="65" class="node-box" />
  <text x="190" y="175" class="node-title">Forehead EEG Electrodes</text>
  <text x="190" y="195" class="node-desc">Differential Biopotential Channels</text>

  <line x1="190" y1="215" x2="190" y2="245" class="flow-line" />

  <rect x="70" y="245" width="240" height="65" class="node-box" />
  <text x="190" y="270" class="node-title">Analog Front-End (AFE)</text>
  <text x="190" y="290" class="node-desc">Gain Amp &amp; Active Filtering</text>

  <line x1="190" y1="310" x2="190" y2="340" class="flow-line" />

  <rect x="70" y="340" width="240" height="65" class="node-box" />
  <text x="190" y="365" class="node-title">ESP32-S3 ADC Sampling</text>
  <text x="190" y="385" class="node-desc">GPIO 9 | 256 Hz Continuous</text>

  <!-- Connect Section 1 -> Section 2 -->
  <line x1="310" y1="372" x2="370" y2="372" class="flow-line" />

  <!-- SECTION 2: DIGITAL SIGNAL PROCESSING -->
  <rect x="370" y="105" width="310" height="380" class="group-box" />
  <text x="390" y="130" class="group-title">2. DSP &amp; FEATURE EXTRACTION</text>

  <rect x="390" y="150" width="270" height="65" class="node-box" />
  <text x="525" y="175" class="node-title">Motion Artifact Rejection</text>
  <text x="525" y="195" class="node-desc">Rail-clip Check (&lt;30 or &gt;4065)</text>

  <line x1="525" y1="215" x2="525" y2="245" class="flow-line" />

  <rect x="390" y="245" width="270" height="65" class="node-box" />
  <text x="525" y="270" class="node-title">Digital IIR Bandpass Filter</text>
  <text x="525" y="290" class="node-desc">0.5 Hz - 45.0 Hz Butterworth</text>

  <line x1="525" y1="310" x2="525" y2="340" class="flow-line" />

  <rect x="390" y="340" width="270" height="65" class="node-box" />
  <text x="525" y="365" class="node-title">512-Point Real FFT Engine</text>
  <text x="525" y="385" class="node-desc">δ, θ, α, β, γ Power Bands (Δf=0.5Hz)</text>

  <line x1="525" y1="405" x2="525" y2="425" class="flow-line" />

  <rect x="390" y="425" width="270" height="50" class="highlight-box" />
  <text x="525" y="446" class="node-title">16-Feature Input Vector</text>
  <text x="525" y="462" class="node-desc">Powers, Ratios, Entropy, Peak Freq</text>

  <!-- Connect Section 2 -> Section 3 -->
  <line x1="680" y1="450" x2="730" y2="450" class="flow-line" />

  <!-- SECTION 3: ON-DEVICE NEURAL NETWORK -->
  <rect x="730" y="105" width="310" height="380" class="group-box" />
  <text x="750" y="130" class="group-title">3. ON-DEVICE AI INFERENCE</text>

  <rect x="750" y="150" width="270" height="75" class="highlight-box" />
  <text x="885" y="175" class="node-title">16 → 32 → 16 → 4 MLP</text>
  <text x="885" y="195" class="node-desc">Dense Layers + ReLU Activations</text>
  <text x="885" y="212" class="node-desc">1,140 Trainable Parameters</text>

  <line x1="885" y1="225" x2="885" y2="255" class="flow-line" />

  <rect x="750" y="255" width="270" height="65" class="node-box" />
  <text x="885" y="280" class="node-title">Softmax Probability &amp; Voting</text>
  <text x="885" y="300" class="node-desc">30s Temporal Majority Voting Window</text>

  <line x1="885" y1="320" x2="885" y2="350" class="flow-line" />

  <rect x="750" y="350" width="270" height="65" class="node-box" />
  <text x="885" y="375" class="node-title">Classified Sleep Stage</text>
  <text x="885" y="395" class="node-desc">0:WAKE | 1:LIGHT | 2:DEEP | 3:REM</text>

  <!-- SECTION 4: LOCAL ACTUATION & STORAGE -->
  <rect x="1090" y="105" width="260" height="380" class="group-box" />
  <text x="1110" y="130" class="group-title">4. LOCAL EDGE ACTIONS</text>

  <rect x="1110" y="150" width="220" height="65" class="node-box" />
  <text x="1220" y="175" class="node-title">Dual Hardware Displays</text>
  <text x="1220" y="195" class="node-desc">ST7789 TFT + SSD1306 OLED</text>

  <rect x="1110" y="245" width="220" height="65" class="node-box" />
  <text x="1220" y="270" class="node-title">Smart Haptic Alarm</text>
  <text x="1220" y="290" class="node-desc">Vibration Motor (GPIO 14)</text>

  <rect x="1110" y="340" width="220" height="65" class="node-box" />
  <text x="1220" y="365" class="node-title">On-Device Learning (NVS)</text>
  <text x="1220" y="385" class="node-desc">Backpropagation Personalization</text>

  <!-- Connect Section 3 -> Section 4 -->
  <line x1="1020" y1="382" x2="1090" y2="382" class="flow-line" />
  <line x1="1020" y1="280" x2="1060" y2="280" stroke="#000000" stroke-width="2" />
  <line x1="1060" y1="280" x2="1060" y2="182" stroke="#000000" stroke-width="2" />
  <line x1="1060" y1="182" x2="1110" y2="182" class="flow-line" />
  <line x1="1060" y1="280" x2="1110" y2="280" class="flow-line" />

  <!-- SECTION 5: DUAL-CHANNEL TELEMETRY -->
  <rect x="50" y="520" width="380" height="380" class="group-box" />
  <text x="70" y="545" class="group-title">5. DUAL-CHANNEL TELEMETRY</text>

  <rect x="70" y="570" width="340" height="70" class="node-box" />
  <text x="240" y="598" class="node-title">Channel A: USB Serial Interface</text>
  <text x="240" y="618" class="node-desc">/dev/ttyACM0 | 115200 baud | 3s Rate</text>

  <rect x="70" y="670" width="340" height="70" class="node-box" />
  <text x="240" y="698" class="node-title">Channel B: Wi-Fi HTTP POST</text>
  <text x="240" y="718" class="node-desc">REST /api/sleep-data | Port 3000 | 5s Rate</text>

  <rect x="70" y="770" width="340" height="70" class="node-box" />
  <text x="240" y="798" class="node-title">Channel C: UDP Discovery &amp; Sync</text>
  <text x="240" y="818" class="node-desc">Port 8888 Discovery | GMT+5:30 RTC Sync</text>

  <!-- Connect Section 3 down to Section 5 -->
  <path d="M 885 415 L 885 500 L 240 500 L 240 570" class="flow-line" />

  <!-- SECTION 6: GATEWAY SERVER (ARDUINO UNO Q) -->
  <rect x="480" y="520" width="410" height="380" class="group-box" />
  <text x="500" y="545" class="group-title">6. GATEWAY SERVER (ARDUINO UNO Q)</text>

  <rect x="500" y="570" width="370" height="65" class="node-box" />
  <text x="685" y="595" class="node-title">Telemetry Ingestion &amp; Router</text>
  <text x="685" y="615" class="node-desc">Unified Stream Parser (Serial + HTTP + UDP)</text>

  <line x1="685" y1="635" x2="685" y2="665" class="flow-line" />

  <rect x="500" y="665" width="370" height="65" class="highlight-box" />
  <text x="685" y="690" class="node-title">Session Qualification Filter</text>
  <text x="685" y="710" class="node-desc">Requires: ≥90% Sleep Waves &amp; ≥60m Duration</text>

  <line x1="685" y1="730" x2="685" y2="760" class="flow-line" />

  <rect x="500" y="760" width="370" height="65" class="node-box" />
  <text x="685" y="785" class="node-title">Async Store Worker &amp; Scoring</text>
  <text x="685" y="805" class="node-desc">10s Non-blocking Flush | Sleep Score (0-100)</text>

  <!-- Connect Section 5 -> Section 6 -->
  <line x1="410" y1="605" x2="500" y2="605" class="flow-line" />
  <line x1="410" y1="705" x2="500" y2="705" class="flow-line" />
  <line x1="410" y1="805" x2="500" y2="805" class="flow-line" />

  <!-- SECTION 7: WEB DASHBOARD -->
  <rect x="940" y="520" width="410" height="380" class="group-box" />
  <text x="960" y="545" class="group-title">7. REAL-TIME WEB DASHBOARD</text>

  <rect x="960" y="570" width="370" height="65" class="node-box" />
  <text x="1145" y="595" class="node-title">Stepped Hypnogram Timeline</text>
  <text x="1145" y="615" class="node-desc">Interactive Sleep Stage Chart (Chart.js)</text>

  <rect x="960" y="665" width="370" height="65" class="node-box" />
  <text x="1145" y="690" class="node-title">Live Neural State &amp; Spectral Bars</text>
  <text x="1145" y="710" class="node-desc">Power Bars (δ, θ, α, β, γ) + Confidence %</text>

  <rect x="960" y="760" width="370" height="65" class="node-box" />
  <text x="1145" y="785" class="node-title">Live Hardware Serial Console</text>
  <text x="1145" y="805" class="node-desc">WebSocket Stream (/dev/ttyACM0) + Commands</text>

  <!-- Connect Section 6 -> Section 7 -->
  <line x1="870" y1="605" x2="960" y2="605" class="flow-line" />
  <line x1="870" y1="700" x2="960" y2="700" class="flow-line" />
  <line x1="870" y1="795" x2="960" y2="795" class="flow-line" />

</svg>
"""
    svg_path = "/home/izaan/Documents/rhy/rhythmsleep_block_diagram.svg"
    with open(svg_path, "w", encoding="utf-8") as f:
        f.write(svg_content)
    print(f"Generated SVG: {svg_path}")

def generate_png():
    width = 1600
    height = 1100
    image = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(image)

    # Try loading system fonts or default
    try:
        font_title = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 26)
        font_subtitle = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 15)
        font_header = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 15)
        font_node = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 13)
        font_desc = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 11)
    except Exception:
        font_title = ImageFont.load_default()
        font_subtitle = ImageFont.load_default()
        font_header = ImageFont.load_default()
        font_node = ImageFont.load_default()
        font_desc = ImageFont.load_default()

    # Draw Title
    title_text = "RHYTHMSLEEP v3 — FUNCTIONAL BLOCK DIAGRAM"
    subtitle_text = "Distributed Architecture: Wearable Neural Edge Node (ESP32-S3) & Telemetry Gateway Server (Arduino UNO Q)"
    
    draw.text((width // 2, 40), title_text, fill="black", font=font_title, anchor="mt")
    draw.text((width // 2, 75), subtitle_text, fill="#333333", font=font_subtitle, anchor="mt")
    draw.line([(50, 105), (width - 50, 105)], fill="black", width=2)

    def draw_box(x, y, w, h, title, desc="", fill_color="#FFFFFF", border_color="black", border_width=2):
        draw.rectangle([x, y, x + w, y + h], fill=fill_color, outline=border_color, width=border_width)
        if title and desc:
            draw.text((x + w // 2, y + h // 2 - 10), title, fill="black", font=font_node, anchor="mm")
            draw.text((x + w // 2, y + h // 2 + 10), desc, fill="#222222", font=font_desc, anchor="mm")
        elif title:
            draw.text((x + w // 2, y + h // 2), title, fill="black", font=font_node, anchor="mm")

    def draw_arrow(x1, y1, x2, y2):
        draw.line([(x1, y1), (x2, y2)], fill="black", width=2)
        # Arrowhead
        if x1 == x2: # Vertical
            if y2 > y1:
                draw.polygon([(x2 - 5, y2 - 8), (x2 + 5, y2 - 8), (x2, y2)], fill="black")
            else:
                draw.polygon([(x2 - 5, y2 + 8), (x2 + 5, y2 + 8), (x2, y2)], fill="black")
        elif y1 == y2: # Horizontal
            if x2 > x1:
                draw.polygon([(x2 - 8, y2 - 5), (x2 - 8, y2 + 5), (x2, y2)], fill="black")
            else:
                draw.polygon([(x2 + 8, y2 - 5), (x2 + 8, y2 + 5), (x2, y2)], fill="black")

    # ROW 1 SECTIONS
    y_row1 = 130
    h_row1 = 410

    # 1. Acquisition
    draw.rectangle([50, y_row1, 360, y_row1 + h_row1], outline="black", width=2)
    draw.text((70, y_row1 + 15), "1. SIGNAL ACQUISITION", fill="black", font=font_header)
    draw_box(70, y_row1 + 50, 270, 75, "Forehead EEG Electrodes", "Differential Biopotential Channels")
    draw_arrow(205, y_row1 + 125, 205, y_row1 + 165)
    draw_box(70, y_row1 + 165, 270, 75, "Analog Front-End (AFE)", "Biopotential Gain & Filtering")
    draw_arrow(205, y_row1 + 240, 205, y_row1 + 280)
    draw_box(70, y_row1 + 280, 270, 75, "ESP32-S3 ADC Sampling", "GPIO 9 | 256 Hz Continuous")

    draw_arrow(340, y_row1 + 317, 410, y_row1 + 317)

    # 2. DSP
    draw.rectangle([410, y_row1, 750, y_row1 + h_row1], outline="black", width=2)
    draw.text((430, y_row1 + 15), "2. DSP & FEATURE EXTRACTION", fill="black", font=font_header)
    draw_box(430, y_row1 + 50, 300, 65, "Motion Artifact Filter", "Rail Clipping (<30 or >4065)")
    draw_arrow(580, y_row1 + 115, 580, y_row1 + 145)
    draw_box(430, y_row1 + 145, 300, 65, "Digital IIR Bandpass", "0.5 - 45.0 Hz Butterworth")
    draw_arrow(580, y_row1 + 210, 580, y_row1 + 240)
    draw_box(430, y_row1 + 240, 300, 65, "512-Point Real FFT", "δ, θ, α, β, γ Bands (Δf=0.5Hz)")
    draw_arrow(580, y_row1 + 305, 580, y_row1 + 335)
    draw_box(430, y_row1 + 335, 300, 60, "16-Feature Extraction", "Bands, Ratios, Entropy, Freq", fill_color="#F0F0F0", border_width=3)

    draw_arrow(730, y_row1 + 365, 800, y_row1 + 365)

    # 3. AI Inference
    draw.rectangle([800, y_row1, 1170, y_row1 + h_row1], outline="black", width=2)
    draw.text((820, y_row1 + 15), "3. ON-DEVICE AI INFERENCE", fill="black", font=font_header)
    draw_box(820, y_row1 + 50, 330, 80, "16 → 32 → 16 → 4 MLP", "ReLU + Softmax | 1,140 Weights", fill_color="#F0F0F0", border_width=3)
    draw_arrow(985, y_row1 + 130, 985, y_row1 + 165)
    draw_box(820, y_row1 + 165, 330, 75, "30s Majority Voting", "Smooths Transition Artifacts")
    draw_arrow(985, y_row1 + 240, 985, y_row1 + 275)
    draw_box(820, y_row1 + 275, 330, 75, "Sleep Stage Classification", "0:WAKE | 1:LIGHT | 2:DEEP | 3:REM")

    draw_arrow(1150, y_row1 + 312, 1220, y_row1 + 312)

    # 4. Local Actions
    draw.rectangle([1220, y_row1, 1550, y_row1 + h_row1], outline="black", width=2)
    draw.text((1240, y_row1 + 15), "4. LOCAL EDGE ACTIONS", fill="black", font=font_header)
    draw_box(1240, y_row1 + 50, 290, 75, "Dual Hardware Displays", "ST7789 TFT + SSD1306 OLED")
    draw_box(1240, y_row1 + 165, 290, 75, "Smart Haptic Alarm", "Vibration Motor (GPIO 14)")
    draw_box(1240, y_row1 + 280, 290, 75, "On-Device Learning", "Backprop Optimization (NVS)")

    # Connect to local actions
    draw.line([(1170, y_row1 + 87), (1240, y_row1 + 87)], fill="black", width=2)
    draw.polygon([(1240 - 8, y_row1 + 87 - 5), (1240 - 8, y_row1 + 87 + 5), (1240, y_row1 + 87)], fill="black")
    draw.line([(1170, y_row1 + 202), (1240, y_row1 + 202)], fill="black", width=2)
    draw.polygon([(1240 - 8, y_row1 + 202 - 5), (1240 - 8, y_row1 + 202 + 5), (1240, y_row1 + 202)], fill="black")

    # ROW 2 SECTIONS
    y_row2 = 580
    h_row2 = 470

    # Line from stage output to Row 2
    draw.line([(985, y_row1 + 350), (985, 560)], fill="black", width=2)
    draw.line([(985, 560), (250, 560)], fill="black", width=2)
    draw_arrow(250, 560, 250, y_row2 + 40)

    # 5. Telemetry
    draw.rectangle([50, y_row2, 450, y_row2 + h_row2], outline="black", width=2)
    draw.text((70, y_row2 + 20), "5. DUAL-CHANNEL TELEMETRY", fill="black", font=font_header)
    draw_box(70, y_row2 + 60, 360, 85, "Channel A: USB Serial", "/dev/ttyACM0 @ 115200 baud | 3s Rate")
    draw_box(70, y_row2 + 180, 360, 85, "Channel B: Wi-Fi HTTP POST", "REST /api/sleep-data | Port 3000 | 5s Rate")
    draw_box(70, y_row2 + 300, 360, 85, "Channel C: UDP Discovery", "Port 8888 Broadcast | GMT+5:30 Sync")

    # 6. Gateway Server
    draw.rectangle([500, y_row2, 1020, y_row2 + h_row2], outline="black", width=2)
    draw.text((520, y_row2 + 20), "6. GATEWAY SERVER (ARDUINO UNO Q)", fill="black", font=font_header)
    draw_box(530, y_row2 + 60, 460, 85, "Telemetry Ingestion & Router", "Unified Parser (Serial + HTTP + UDP)")
    draw_arrow(760, y_row2 + 145, 760, y_row2 + 180)
    draw_box(530, y_row2 + 180, 460, 85, "Session Qualification Gate", "Requires: >=90% Sleep Waves & >=60m Duration", fill_color="#F0F0F0", border_width=3)
    draw_arrow(760, y_row2 + 265, 760, y_row2 + 300)
    draw_box(530, y_row2 + 300, 460, 85, "Async Storage & Scoring", "10s Non-blocking Flush | Sleep Score 0-100")

    # Connect Section 5 -> 6
    draw_arrow(430, y_row2 + 102, 530, y_row2 + 102)
    draw_arrow(430, y_row2 + 222, 530, y_row2 + 222)
    draw_arrow(430, y_row2 + 342, 530, y_row2 + 342)

    # 7. Dashboard
    draw.rectangle([1070, y_row2, 1550, y_row2 + h_row2], outline="black", width=2)
    draw.text((1090, y_row2 + 20), "7. REAL-TIME WEB DASHBOARD", fill="black", font=font_header)
    draw_box(1100, y_row2 + 60, 420, 85, "Stepped Hypnogram Timeline", "Interactive Sleep Stage Chart (Chart.js)")
    draw_box(1100, y_row2 + 180, 420, 85, "Live Neural State & Spectral Bars", "Power Bars (δ, θ, α, β, γ) + Confidence %")
    draw_box(1100, y_row2 + 300, 420, 85, "Live Hardware Serial Console", "WebSocket Terminal (/dev/ttyACM0) + Commands")

    # Connect Section 6 -> 7
    draw_arrow(990, y_row2 + 102, 1100, y_row2 + 102)
    draw_arrow(990, y_row2 + 222, 1100, y_row2 + 222)
    draw_arrow(990, y_row2 + 342, 1100, y_row2 + 342)

    png_path = "/home/izaan/Documents/rhy/rhythmsleep_block_diagram.png"
    image.save(png_path, "PNG", dpi=(300, 300))
    print(f"Generated PNG: {png_path}")

if __name__ == "__main__":
    generate_svg()
    generate_png()
