# VIFF Format – Implementierungsdokumentation für C++/Qt

## 1. Format-Übersicht

### Was ist VIFF/XV?

VIFF steht für **Khoros Versatile Image File Format** – das native Dateiformat des Khoros-Systems (UNIX-basierte Bildverarbeitungsumgebung mit X Window System). Das Format wird auch als `.xv`-Format bezeichnet.

### Verwendungszweck in match3d_v2

VIFF-Dateien speichern **Tiefenbilder (Range Images)** von 3D-Scannern:
- Z-Werte (Höhendaten) als 32-bit Float pro Pixel
- Pixel-Größe in Metern (XPixelSize, YPixelSize)
- Typische Daten: Zahnoberflächen-Scans zu verschiedenen Zeitpunkten

Testdateien im Projekt:
- `1_17_16_I.xv`, `1_17_16_II.xv` (Stereopaar 1)
- `27_46_47_I.xv`, `27_46_47_II.xv` (Stereopaar 2)
- `dif-*.xv` (vorberechnete Differenzbilder)

### Dateiendungen
- `.xv` – Primäre Endung im match3d-Kontext
- `.viff` – Alternative (selten)

---

## 2. Dateistruktur

### Header-Größe: exakt 1024 Bytes

| Offset | Hex | Feldname | Typ | Größe | Bedeutung |
|--------|-----|----------|-----|-------|-----------|
| 0 | 0x000 | FileId | uint8 | 1 | Magic: 0xAB (171) |
| 1 | 0x001 | FileType | uint8 | 1 | Typ: 0x01 (VIFF) |
| 2 | 0x002 | Release | uint8 | 1 | Release-Nr. (z.B. 3) |
| 3 | 0x003 | Version | uint8 | 1 | Versions-Nr. (z.B. 1) |
| 4 | 0x004 | MachineDep | uint8 | 1 | Byte-Order (s.u.) |
| 5–7 | 0x005 | Padding | uint8[3] | 3 | 0x00 0x00 0x00 |
| 8–519 | 0x008 | Comment | char[512] | 512 | ASCII-Kommentar, null-terminiert |
| 520–523 | 0x208 | NumberOfRows | uint32 | 4 | Bildbreite in Pixeln |
| 524–527 | 0x20C | NumberOfColumns | uint32 | 4 | Bildhöhe in Pixeln |
| 528–531 | 0x210 | LengthOfSubrow | uint32 | 4 | Sub-Row-Länge (meist 0) |
| 532–535 | 0x214 | StartX | int32 | 4 | X-Offset für Sub-Bild |
| 536–539 | 0x218 | StartY | int32 | 4 | Y-Offset für Sub-Bild |
| 540–543 | 0x21C | XPixelSize | float32 | 4 | Pixelbreite in Metern |
| 544–547 | 0x220 | YPixelSize | float32 | 4 | Pixelhöhe in Metern |
| 548–551 | 0x224 | LocationType | uint32 | 4 | 1=implizit, 2=explizit |
| 552–555 | 0x228 | LocationDim | uint32 | 4 | Anzahl Location-Dimensionen |
| 556–559 | 0x22C | NumberOfImages | uint32 | 4 | Anzahl Bilder (meist 1) |
| 560–563 | 0x230 | NumberOfBands | uint32 | 4 | Anzahl Kanäle (meist 1) |
| 564–567 | 0x234 | DataStorageType | uint32 | 4 | Datentyp (5 = Float32) |
| 568–571 | 0x238 | DataEncodingScheme | uint32 | 4 | Kompression (0 = keine) |
| 572–575 | 0x23C | MapScheme | uint32 | 4 | Mapping-Typ |
| 576–579 | 0x240 | MapStorageType | uint32 | 4 | Datentyp Map-Elemente |
| 580–583 | 0x244 | MapRowSize | uint32 | 4 | Map-Breite |
| 584–587 | 0x248 | MapColumnSize | uint32 | 4 | Map-Höhe |
| 588–591 | 0x24C | MapSubrowSize | uint32 | 4 | Map Sub-row |
| 592–595 | 0x250 | MapEnable | uint32 | 4 | Map benötigt? |
| 596–599 | 0x254 | MapsPerCycle | uint32 | 4 | Anzahl Sequenz-Maps |
| 600–603 | 0x258 | ColorSpaceModel | uint32 | 4 | 0=kein, 1=RGB |
| 604–607 | 0x25C | ISpare1 | uint32 | 4 | Benutzerdefiniert |
| 608–611 | 0x260 | ISpare2 | uint32 | 4 | Benutzerdefiniert |
| 612–615 | 0x264 | FSpare1 | float32 | 4 | Benutzerdefiniert (Origin-X) |
| 616–619 | 0x268 | FSpare2 | float32 | 4 | Benutzerdefiniert (Origin-Y) |
| 620–1023 | 0x26C | Reserve | uint8[404] | 404 | Padding (0x00) |

### Magic Number / Dateikennung
- Byte 0: `0xAB` (171 dezimal) → Khoros-Datei
- Byte 1: `0x01` → VIFF-Typ

### Byte-Order (MachineDep, Byte 4)
| Wert | Bedeutung |
|------|-----------|
| 0x02 | Big-Endian (Motorola/SPARC) |
| 0x04 | DEC/VAX |
| **0x08** | **Little-Endian (x86/PC)** – Standard im match3d |
| 0x0A | Cray |

### Datentypen (DataStorageType)
| Wert | Typ | Bytes |
|------|-----|-------|
| 0x01 | uint8 | 1 |
| 0x02 | uint16 | 2 |
| 0x04 | uint32 | 4 |
| **0x05** | **float32** | **4** – Standard für Tiefendaten |
| 0x06 | complex float | 8 |
| 0x09 | float64 | 8 |

---

## 3. Datenlayout

**Pixeldaten beginnen bei Byte-Offset 1024 (0x400).**

- Reihenfolge: Zeile für Zeile (Row-Major)
- Pro Pixel: 1 Float32 (bei NumberOfBands=1)
- Pixelindex: `idx = y * NumberOfRows + x`
- Datenmenge: `NumberOfRows × NumberOfColumns × NumberOfBands × 4` Bytes

### Ungültige Pixel
- `0.0f` – häufig als "kein Messwert" verwendet
- `NaN` – IEEE 754 Not-a-Number möglich
- Negative Werte – als ungültig behandeln

### Physikalische Koordinaten eines Pixels (x, y)
```
X_world = x * XPixelSize + FSpare1   (FSpare1 = Origin-X)
Y_world = y * YPixelSize + FSpare2   (FSpare2 = Origin-Y)
Z_world = pixelData[y * width + x]   (direkt in Scanner-Einheiten)
```

---

## 4. C++ Header-Struct

```cpp
#pragma pack(push, 1)
struct ViffHeader {
    uint8_t  fileId;              // 0xAB
    uint8_t  fileType;            // 0x01
    uint8_t  release;
    uint8_t  version;
    uint8_t  machineDep;          // 0x02=Big, 0x08=Little Endian
    uint8_t  padding[3];          // 0x00
    char     comment[512];
    uint32_t numberOfRows;        // Bildbreite
    uint32_t numberOfColumns;     // Bildhöhe
    uint32_t lengthOfSubrow;
    int32_t  startX;
    int32_t  startY;
    float    xPixelSize;          // Meter
    float    yPixelSize;          // Meter
    uint32_t locationType;        // 1=implizit
    uint32_t locationDim;
    uint32_t numberOfImages;      // 1
    uint32_t numberOfBands;       // 1
    uint32_t dataStorageType;     // 5=float32
    uint32_t dataEncodingScheme;  // 0=unkomprimiert
    uint32_t mapScheme;
    uint32_t mapStorageType;
    uint32_t mapRowSize;
    uint32_t mapColumnSize;
    uint32_t mapSubrowSize;
    uint32_t mapEnable;
    uint32_t mapsPerCycle;
    uint32_t colorSpaceModel;
    uint32_t iSpare1;
    uint32_t iSpare2;
    float    fSpare1;             // Origin-X
    float    fSpare2;             // Origin-Y
    uint8_t  reserve[404];
};
#pragma pack(pop)
static_assert(sizeof(ViffHeader) == 1024, "ViffHeader must be exactly 1024 bytes");
```

---

## 5. ViffReader – Implementierung

```cpp
class ViffReader {
public:
    struct DepthImage {
        uint32_t width;
        uint32_t height;
        float xPixelSize;
        float yPixelSize;
        float originX;
        float originY;
        std::vector<float> data;  // Row-major, width*height Elemente

        float at(uint32_t x, uint32_t y) const {
            return data[y * width + x];
        }
        bool isValid(uint32_t x, uint32_t y) const {
            float v = at(x, y);
            return !std::isnan(v) && v > 0.0f;
        }
    };

    bool load(const std::string& path, DepthImage& out);
    std::string lastError() const { return errorMsg_; }

private:
    std::string errorMsg_;

    static bool isLittleEndianSystem() {
        uint16_t t = 1;
        return *reinterpret_cast<uint8_t*>(&t) == 1;
    }

    static void byteSwap4(void* p) {
        auto* b = static_cast<uint8_t*>(p);
        std::swap(b[0], b[3]);
        std::swap(b[1], b[2]);
    }

    void swapHeaderFields(ViffHeader& h) {
        byteSwap4(&h.numberOfRows);
        byteSwap4(&h.numberOfColumns);
        byteSwap4(&h.lengthOfSubrow);
        byteSwap4(&h.startX);
        byteSwap4(&h.startY);
        byteSwap4(&h.xPixelSize);
        byteSwap4(&h.yPixelSize);
        byteSwap4(&h.locationType);
        byteSwap4(&h.locationDim);
        byteSwap4(&h.numberOfImages);
        byteSwap4(&h.numberOfBands);
        byteSwap4(&h.dataStorageType);
        byteSwap4(&h.dataEncodingScheme);
        byteSwap4(&h.fSpare1);
        byteSwap4(&h.fSpare2);
    }
};

bool ViffReader::load(const std::string& path, DepthImage& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { errorMsg_ = "Cannot open: " + path; return false; }

    ViffHeader h;
    f.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (!f) { errorMsg_ = "Cannot read header"; return false; }

    // Validate magic
    if (h.fileId != 0xAB || h.fileType != 0x01) {
        errorMsg_ = "Not a VIFF file (wrong magic)";
        return false;
    }

    // Determine if byte swap is needed
    bool fileIsLittle = (h.machineDep == 0x08);
    bool sysIsLittle  = isLittleEndianSystem();
    bool needSwap     = (fileIsLittle != sysIsLittle);

    if (needSwap) swapHeaderFields(h);

    if (h.dataStorageType != 0x05) {
        errorMsg_ = "Unsupported data type: " + std::to_string(h.dataStorageType);
        return false;
    }
    if (h.dataEncodingScheme != 0x00) {
        errorMsg_ = "Compressed VIFF not supported";
        return false;
    }

    out.width       = h.numberOfRows;
    out.height      = h.numberOfColumns;
    out.xPixelSize  = h.xPixelSize;
    out.yPixelSize  = h.yPixelSize;
    out.originX     = h.fSpare1;
    out.originY     = h.fSpare2;

    uint32_t n = out.width * out.height * h.numberOfBands;
    out.data.resize(n);

    f.seekg(1024, std::ios::beg);
    f.read(reinterpret_cast<char*>(out.data.data()), n * sizeof(float));
    if (!f) { errorMsg_ = "Cannot read pixel data"; return false; }

    if (needSwap) {
        for (auto& v : out.data) byteSwap4(&v);
    }

    return true;
}
```

---

## 6. ViffWriter – Implementierung

```cpp
class ViffWriter {
public:
    bool save(const std::string& path,
              uint32_t width, uint32_t height,
              const std::vector<float>& data,
              float xPixelSize = 0.0f, float yPixelSize = 0.0f,
              float originX = 0.0f, float originY = 0.0f);
    std::string lastError() const { return errorMsg_; }

private:
    std::string errorMsg_;
};

bool ViffWriter::save(const std::string& path,
                      uint32_t width, uint32_t height,
                      const std::vector<float>& data,
                      float xPixelSize, float yPixelSize,
                      float originX, float originY) {
    if (data.size() != static_cast<size_t>(width * height)) {
        errorMsg_ = "Data size mismatch";
        return false;
    }

    ViffHeader h;
    std::memset(&h, 0, sizeof(h));

    h.fileId             = 0xAB;
    h.fileType           = 0x01;
    h.release            = 0x03;
    h.version            = 0x01;
    h.machineDep         = 0x08;  // Little-Endian
    h.numberOfRows       = width;
    h.numberOfColumns    = height;
    h.numberOfBands      = 1;
    h.numberOfImages     = 1;
    h.dataStorageType    = 0x05;  // float32
    h.dataEncodingScheme = 0x00;
    h.locationType       = 0x01;
    h.xPixelSize         = xPixelSize;
    h.yPixelSize         = yPixelSize;
    h.fSpare1            = originX;
    h.fSpare2            = originY;
    std::strncpy(h.comment, "match3d_v2", 511);

    std::ofstream f(path, std::ios::binary);
    if (!f) { errorMsg_ = "Cannot create: " + path; return false; }

    f.write(reinterpret_cast<const char*>(&h), sizeof(h));
    f.write(reinterpret_cast<const char*>(data.data()),
            data.size() * sizeof(float));

    if (!f) { errorMsg_ = "Write error"; return false; }
    return true;
}
```

---

## 7. Qt-Integration

### Konvertierung zu QImage für Anzeige

```cpp
QImage depthImageToQImage(const ViffReader::DepthImage& img,
                           float zMin, float zMax) {
    QImage qimg(img.width, img.height, QImage::Format_Grayscale8);
    float range = (zMax > zMin) ? (zMax - zMin) : 1.0f;

    for (uint32_t y = 0; y < img.height; ++y) {
        uint8_t* line = qimg.scanLine(y);
        for (uint32_t x = 0; x < img.width; ++x) {
            float v = img.at(x, y);
            if (!img.isValid(x, y)) {
                line[x] = 0;
            } else {
                float norm = std::clamp((v - zMin) / range, 0.0f, 1.0f);
                line[x] = static_cast<uint8_t>(norm * 255.0f);
            }
        }
    }
    return qimg;
}
```

### Datei-Filter für QFileDialog

```cpp
QString viffFilter = "VIFF/XV Files (*.xv *.viff);;All Files (*)";
QString path = QFileDialog::getOpenFileName(this, "Open VIFF", "", viffFilter);
```

---

## 8. Fallstricke

| Problem | Ursache | Lösung |
|---------|---------|--------|
| Falsche Werte | Endianness nicht berücksichtigt | MachineDep prüfen, Byte-Swap |
| Header falsch | Struct-Padding durch Compiler | `#pragma pack(push,1)` |
| Leere Pixel | 0.0f als "kein Wert" | `isValid()` Methode nutzen |
| Falsche Größe | NumberOfRows = Breite (nicht Höhe!) | Feldnamen sind irreführend |
| Einheiten-Mismatch | Pixel in Metern, Z in µm | Automatische Konvertierung (s.u.) |

> **Wichtig:** `NumberOfRows` = Bildbreite (Spalten), `NumberOfColumns` = Bildhöhe (Zeilen). Diese Benennung entspricht dem Khoros-Konvention (Zeilen = Spalten eines Bildes) und ist für Bildverarbeitung ungewöhnlich.

---

## 8a. Automatische Einheitenkonvertierung

### Problem: Inkonsistente Einheiten in Zahnscan-Daten

Die VIFF-Spezifikation definiert `XPixelSize` und `YPixelSize` in **Metern**. Einige Zahnscanner speichern jedoch die Z-Werte (Höhendaten) in **Mikrometern (µm)**, während die Pixel-Größen korrekt in Metern angegeben sind.

**Beispiel aus realen Daten:**
- `xPixelSize = 2.78e-5` (≈ 28 µm, gespeichert in Metern)
- `yPixelSize = 3.0e-5` (≈ 30 µm, gespeichert in Metern)
- Z-Werte: `10000 – 15000` (tatsächlich µm, nicht Meter!)

Ohne Konvertierung würden Gradienten falsch berechnet:
- `dz/dx = 5000 µm / (2 × 2.78e-5 m) ≈ 90,000,000` → 90° Steigung (unmöglich!)

### Lösung: Heuristik-basierte Auto-Konvertierung

`ViffReader::load()` erkennt automatisch dieses Szenario und konvertiert alle Werte nach **Millimetern (mm)**:

```cpp
// Erkennungskriterien:
const bool pixelSizeInMeters = (img.xPixelSize < 0.001f && img.xPixelSize > 0.0f);
const bool zLikelyMicrons = (zMax > 1000.0f);

if (pixelSizeInMeters && zLikelyMicrons) {
    // Pixel-Größen: Meter → mm (×1000)
    img.xPixelSize *= 1000.0f;
    img.yPixelSize *= 1000.0f;
    img.originX *= 1000.0f;
    img.originY *= 1000.0f;

    // Z-Werte: µm → mm (÷1000)
    for (auto& v : img.data) {
        if (v != 0.0f) v /= 1000.0f;
    }
}
```

### Ergebnis nach Konvertierung

| Feld | Vor Konvertierung | Nach Konvertierung |
|------|-------------------|-------------------|
| xPixelSize | 2.78e-5 m | 0.0278 mm |
| yPixelSize | 3.0e-5 m | 0.030 mm |
| Z-Werte | 10000–15000 (µm) | 10–15 mm |
| Gradienten | ~90° (falsch) | ~30° (realistisch) |

### Wann wird NICHT konvertiert?

- `xPixelSize >= 0.001` (bereits in mm oder größer)
- `zMax <= 1000` (Z-Werte bereits in mm-Bereich)
- Differenzbilder (können negative Werte haben) → `isDiffImage = true`

### Interne Einheit: Millimeter

Nach dem Laden arbeitet match3d_v2 intern **ausschließlich in Millimetern**:
- Koordinaten im Status-Bar: mm
- Registrierungs-Parameter: mm (Translation), Grad (Rotation)
- Statistiken: mm

---

## 9. Testdaten-Validierung

```cpp
void printViffInfo(const ViffReader::DepthImage& img) {
    auto [minIt, maxIt] = std::minmax_element(img.data.begin(), img.data.end());
    int invalidCount = std::count_if(img.data.begin(), img.data.end(),
        [](float v){ return std::isnan(v) || v <= 0.0f; });

    qDebug() << "Size:" << img.width << "x" << img.height;
    qDebug() << "Pixel:" << img.xPixelSize*1000 << "x" << img.yPixelSize*1000 << "mm";
    qDebug() << "Z range:" << *minIt << "to" << *maxIt;
    qDebug() << "Invalid pixels:" << invalidCount << "/" << img.data.size();
}
```
